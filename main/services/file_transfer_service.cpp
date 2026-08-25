#include "file_transfer_service.h"
#include "upload_page.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <esp_log.h>

#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace papers3 {

namespace {
constexpr const char* TAG = "FileTransfer";
constexpr const char* BOOK_DIR = "/sdcard/books";
constexpr std::size_t MAX_UPLOAD_BYTES = 32 * 1024 * 1024;

const char kUploadPage[] = R"HTML(<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Paper S3 文件传输</title><style>body{font-family:system-ui;margin:0;background:#f2f2ef;color:#171717}.wrap{max-width:680px;margin:auto;padding:24px}.card{background:white;border:1px solid #ccc;border-radius:22px;padding:22px;margin:14px 0}button,.pick{display:inline-block;border:1px solid #222;border-radius:999px;padding:12px 20px;background:#171717;color:white;font-weight:700}input{display:none}.row{display:flex;justify-content:space-between;gap:12px;padding:12px 0;border-bottom:1px solid #ddd}.muted{color:#666;font-size:14px}progress{width:100%;height:12px}</style></head><body><main class="wrap"><h1>Paper S3</h1><p class="muted">手机与设备连接后，可直接管理 SD 卡中的图书。</p><section class="card"><label class="pick">选择 EPUB / TXT<input id="file" type="file" accept=".epub,.txt"></label><p id="name" class="muted">尚未选择文件</p><progress id="progress" value="0" max="100"></progress><p id="status"></p></section><section class="card"><h2>设备文件</h2><div id="files">正在读取…</div></section></main><script>const f=document.querySelector('#file'),s=document.querySelector('#status'),p=document.querySelector('#progress'),n=document.querySelector('#name');async function refresh(){const r=await fetch('/files');const a=await r.json();document.querySelector('#files').innerHTML=a.length?a.map(x=>`<div class="row"><span>${x.name}<br><small class="muted">${Math.ceil(x.size/1024)} KB</small></span><button onclick="del('${encodeURIComponent(x.name)}')">删除</button></div>`).join(''):'暂无文件'}async function del(x){if(!confirm('删除这个文件？'))return;await fetch('/delete?name='+x,{method:'POST'});refresh()}f.onchange=()=>{const x=f.files[0];if(!x)return;n.textContent=x.name;s.textContent='正在上传…';const q=new XMLHttpRequest;q.open('POST','/upload?name='+encodeURIComponent(x.name));q.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded/e.total*100};q.onload=()=>{s.textContent=q.status===200?'上传完成':'上传失败：'+q.responseText;if(q.status===200)refresh()};q.onerror=()=>s.textContent='网络中断';q.send(x)};refresh()</script></body></html>)HTML";

std::string percentDecode(const std::string& input)
{
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            unsigned int value = 0;
            if (std::sscanf(input.substr(i + 1, 2).c_str(), "%02x", &value) == 1) {
                output.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        }
        output.push_back(input[i] == '+' ? ' ' : input[i]);
    }
    return output;
}

std::string queryFilename(httpd_req_t* request)
{
    const auto length = httpd_req_get_url_query_len(request);
    if (!length || length > 512) return {};
    std::string query(length + 1, '\0');
    if (httpd_req_get_url_query_str(request, query.data(), query.size()) != ESP_OK) return {};
    char encoded[384] {};
    if (httpd_query_key_value(query.c_str(), "name", encoded, sizeof(encoded)) != ESP_OK) return {};
    std::string name = percentDecode(encoded);
    const auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.empty() || name == "." || name == ".." || name.find("..") != std::string::npos) return {};
    return name;
}

bool allowedBook(const std::string& name)
{
    auto lower = name;
    for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const bool txt = lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".txt") == 0;
    const bool epub = lower.size() > 5 && lower.compare(lower.size() - 5, 5, ".epub") == 0;
    return txt || epub;
}

bool allowedFirmware(const std::string& name)
{
    auto value = name;
    for (auto& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value.size() > 4 && value.compare(value.size() - 4, 4, ".bin") == 0;
}

bool allowedWallpaper(const std::string& name)
{
    auto value = name;
    for (auto& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return (value.size() > 4 && value.compare(value.size() - 4, 4, ".jpg") == 0) ||
           (value.size() > 5 && value.compare(value.size() - 5, 5, ".jpeg") == 0) ||
           (value.size() > 4 && value.compare(value.size() - 4, 4, ".png") == 0);
}

bool allowedFont(const std::string& name)
{
    auto value = name;
    for (auto& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value.size() > 4 && value.compare(value.size() - 4, 4, ".vlw") == 0;
}
#ifdef ESP_PLATFORM
bool safeFtpName(const std::string& input, std::string& output)
{
    output = input;
    const auto slash = output.find_last_of("/\\");
    if (slash != std::string::npos) output = output.substr(slash + 1);
    if (output.empty() || output == "." || output == ".." || output.find("..") != std::string::npos) return false;
    return allowedBook(output);
}

bool sendFtpLine(int socket, const char* line)
{
    const std::string payload = std::string(line) + "\r\n";
    return send(socket, payload.data(), payload.size(), 0) == static_cast<int>(payload.size());
}

std::string readFtpLine(int socket)
{
    std::string line;
    char ch = 0;
    while (line.size() < 512) {
        const int count = recv(socket, &ch, 1, 0);
        if (count <= 0) return {};
        if (ch == '\n') break;
        if (ch != '\r') line.push_back(ch);
    }
    return line;
}
#endif
}

bool FileTransferService::startApUploadServer(AppState& state, PaperS3Hal& hal)
{
    if (running_) return true;
    if (!state.status.sdMounted) {
        state.status.message = "未检测到 SD 卡";
        return false;
    }
    mkdir(BOOK_DIR, 0775);
    state.transferSsid = "PaperS3-Transfer";
    state.transferPassword = "papers3s3";
    if (!hal.startWifiAccessPoint(state.transferSsid, state.transferPassword)) {
        state.status.message = "热点启动失败";
        return false;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    if (httpd_start(&server_, &config) != ESP_OK) {
        server_ = nullptr;
        hal.stopWifiAccessPoint();
        state.status.message = "文件服务启动失败";
        return false;
    }

    const httpd_uri_t root { .uri = "/", .method = HTTP_GET, .handler = rootHandler, .user_ctx = this };
    const httpd_uri_t files { .uri = "/files", .method = HTTP_GET, .handler = filesHandler, .user_ctx = this };
    const httpd_uri_t upload { .uri = "/upload", .method = HTTP_POST, .handler = uploadHandler, .user_ctx = this };
    const httpd_uri_t remove { .uri = "/delete", .method = HTTP_POST, .handler = deleteHandler, .user_ctx = this };
    httpd_register_uri_handler(server_, &root);
    httpd_register_uri_handler(server_, &files);
    httpd_register_uri_handler(server_, &upload);
    httpd_register_uri_handler(server_, &remove);

    url_ = "http://" + hal.wifiIpAddress(true);
    if (url_ == "http://") url_ = "http://192.168.4.1";
    state.transferUrl = url_;
    state.status.message = "文件传输已开启";
    running_ = true;
#ifdef ESP_PLATFORM
    if (!startFtpServer()) ESP_LOGW(TAG, "FTP server could not start; HTTP upload remains available");
#endif
    return true;
}

void FileTransferService::stop(AppState& state, PaperS3Hal& hal)
{
#ifdef ESP_PLATFORM
    stopFtpServer();
#endif
    if (server_) httpd_stop(server_);
    hal.stopWifiAccessPoint();
    server_ = nullptr;
    running_ = false;
    url_.clear();
    state.transferUrl.clear();
    state.status.message = "文件传输已关闭";
}

bool FileTransferService::running() const { return running_; }
std::string FileTransferService::uploadUrl() const { return url_; }

#ifdef ESP_PLATFORM
bool FileTransferService::startFtpServer()
{
    if (ftpRunning_) return true;
    ftpListenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (ftpListenSocket_ < 0) return false;
    int reuse = 1;
    setsockopt(ftpListenSocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(21);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ftpListenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(ftpListenSocket_, 2) != 0) {
        close(ftpListenSocket_);
        ftpListenSocket_ = -1;
        return false;
    }
    const int flags = fcntl(ftpListenSocket_, F_GETFL, 0);
    fcntl(ftpListenSocket_, F_SETFL, flags | O_NONBLOCK);
    ftpRunning_ = true;
    TaskHandle_t handle = nullptr;
    if (xTaskCreate(&FileTransferService::ftpTaskEntry, "papers3_ftp", 6144, this, 4, &handle) != pdPASS) {
        ftpRunning_ = false;
        close(ftpListenSocket_);
        ftpListenSocket_ = -1;
        return false;
    }
    ftpTask_ = handle;
    return true;
}

void FileTransferService::stopFtpServer()
{
    if (!ftpRunning_) return;
    ftpRunning_ = false;
    if (ftpListenSocket_ >= 0) {
        shutdown(ftpListenSocket_, SHUT_RDWR);
        close(ftpListenSocket_);
        ftpListenSocket_ = -1;
    }
    vTaskDelay(pdMS_TO_TICKS(30));
    ftpTask_ = nullptr;
}

void FileTransferService::ftpTaskEntry(void* context)
{
    auto* self = static_cast<FileTransferService*>(context);
    self->ftpLoop();
    vTaskDelete(nullptr);
}

void FileTransferService::ftpLoop()
{
    while (ftpRunning_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        if (ftpListenSocket_ < 0) break;
        FD_SET(ftpListenSocket_, &readSet);
        timeval timeout{1, 0};
        const int ready = select(ftpListenSocket_ + 1, &readSet, nullptr, nullptr, &timeout);
        if (!ftpRunning_) break;
        if (ready <= 0 || !FD_ISSET(ftpListenSocket_, &readSet)) continue;
        sockaddr_in client{};
        socklen_t length = sizeof(client);
        const int socket = accept(ftpListenSocket_, reinterpret_cast<sockaddr*>(&client), &length);
        if (socket >= 0) {
            ftpClient(socket);
            shutdown(socket, SHUT_RDWR);
            close(socket);
        }
    }
}

void FileTransferService::ftpClient(int clientSocket)
{
    sendFtpLine(clientSocket, "220 Paper S3 FTP ready");
    bool loggedIn = false;
    int passiveSocket = -1;
    auto closePassive = [&]() {
        if (passiveSocket >= 0) { close(passiveSocket); passiveSocket = -1; }
    };
    while (ftpRunning_) {
        const std::string line = readFtpLine(clientSocket);
        if (line.empty()) break;
        const auto split = line.find(' ');
        std::string command = line.substr(0, split);
        std::string argument = split == std::string::npos ? std::string() : line.substr(split + 1);
        for (auto& ch : command) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (command == "USER") {
            sendFtpLine(clientSocket, "331 Password required");
        } else if (command == "PASS") {
            loggedIn = true;
            sendFtpLine(clientSocket, "230 Logged in");
        } else if (command == "SYST") {
            sendFtpLine(clientSocket, "215 UNIX Type: L8");
        } else if (command == "TYPE") {
            sendFtpLine(clientSocket, "200 Type set");
        } else if (command == "PWD") {
            sendFtpLine(clientSocket, "257 \"/\" is current directory");
        } else if (command == "CWD") {
            sendFtpLine(clientSocket, "250 Directory changed");
        } else if (command == "NOOP") {
            sendFtpLine(clientSocket, "200 OK");
        } else if (command == "QUIT") {
            sendFtpLine(clientSocket, "221 Bye");
            break;
        } else if (!loggedIn) {
            sendFtpLine(clientSocket, "530 Please login with USER and PASS");
        } else if (command == "PASV") {
            closePassive();
            passiveSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = 0;
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            if (passiveSocket < 0 || bind(passiveSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(passiveSocket, 1) != 0) {
                closePassive();
                sendFtpLine(clientSocket, "425 Cannot open data connection");
                continue;
            }
            socklen_t length = sizeof(address);
            getsockname(passiveSocket, reinterpret_cast<sockaddr*>(&address), &length);
            const int port = ntohs(address.sin_port);
            char response[96]{};
            std::snprintf(response, sizeof(response), "227 Entering Passive Mode (192,168,4,1,%d,%d)", port / 256, port % 256);
            sendFtpLine(clientSocket, response);
        } else if (command == "LIST") {
            if (passiveSocket < 0) { sendFtpLine(clientSocket, "425 Use PASV first"); continue; }
            sendFtpLine(clientSocket, "150 Opening data connection");
            const int data = accept(passiveSocket, nullptr, nullptr);
            closePassive();
            if (data < 0) { sendFtpLine(clientSocket, "425 Data connection failed"); continue; }
            DIR* dir = opendir(BOOK_DIR);
            if (dir) {
                while (auto* entry = readdir(dir)) {
                    if (!allowedBook(entry->d_name)) continue;
                    struct stat info{};
                    const std::string path = std::string(BOOK_DIR) + "/" + entry->d_name;
                    stat(path.c_str(), &info);
                    char listing[384]{};
                    std::snprintf(listing, sizeof(listing), "-rw-r--r-- 1 root root %ld Jan 1 00:00 %s\r\n", static_cast<long>(info.st_size), entry->d_name);
                    send(data, listing, std::strlen(listing), 0);
                }
                closedir(dir);
            }
            shutdown(data, SHUT_RDWR);
            close(data);
            sendFtpLine(clientSocket, "226 Transfer complete");
        } else if (command == "RETR" || command == "STOR" || command == "DELE") {
            std::string name;
            if (!safeFtpName(argument, name)) { sendFtpLine(clientSocket, "550 Invalid file name"); continue; }
            const std::string path = std::string(BOOK_DIR) + "/" + name;
            if (command == "DELE") {
                sendFtpLine(clientSocket, unlink(path.c_str()) == 0 ? "250 File deleted" : "550 File not found");
                continue;
            }
            if (passiveSocket < 0) { sendFtpLine(clientSocket, "425 Use PASV first"); continue; }
            sendFtpLine(clientSocket, command == "RETR" ? "150 Opening binary connection" : "150 Ok to send data");
            const int data = accept(passiveSocket, nullptr, nullptr);
            closePassive();
            if (data < 0) { sendFtpLine(clientSocket, "425 Data connection failed"); continue; }
            bool okay = true;
            if (command == "RETR") {
                FILE* file = std::fopen(path.c_str(), "rb");
                if (!file) okay = false;
                char buffer[4096];
                while (okay && file) {
                    const std::size_t count = std::fread(buffer, 1, sizeof(buffer), file);
                    if (count && send(data, buffer, count, 0) < 0) okay = false;
                    if (count < sizeof(buffer)) break;
                }
                if (file) std::fclose(file);
            } else {
                const std::string temporary = std::string(BOOK_DIR) + "/." + name + ".upload";
                FILE* file = std::fopen(temporary.c_str(), "wb");
                if (!file) okay = false;
                std::size_t receivedTotal = 0;
                char buffer[4096];
                while (okay) {
                    const int count = recv(data, buffer, sizeof(buffer), 0);
                    if (count <= 0) break;
                    receivedTotal += count;
                    if (receivedTotal > MAX_UPLOAD_BYTES || std::fwrite(buffer, 1, count, file) != static_cast<std::size_t>(count)) { okay = false; break; }
                }
                if (file) std::fclose(file);
                if (okay) { unlink(path.c_str()); if (rename(temporary.c_str(), path.c_str()) != 0) okay = false; }
                else unlink(temporary.c_str());
            }
            shutdown(data, SHUT_RDWR);
            close(data);
            sendFtpLine(clientSocket, okay ? "226 Transfer complete" : "550 Transfer failed");
        } else {
            sendFtpLine(clientSocket, "502 Command not implemented");
        }
    }
    closePassive();
}
#endif

esp_err_t FileTransferService::rootHandler(httpd_req_t* request)
{
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, kUploadPageV2, HTTPD_RESP_USE_STRLEN);
}

esp_err_t FileTransferService::filesHandler(httpd_req_t* request)
{
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    std::string json = "[";
    DIR* dir = opendir(BOOK_DIR);
    bool first = true;
    if (dir) {
        while (auto* entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (!allowedBook(name)) continue;
            struct stat info {};
            const std::string path = std::string(BOOK_DIR) + "/" + name;
            stat(path.c_str(), &info);
            if (!first) json += ',';
            first = false;
            std::string safe = name;
            for (auto& ch : safe) if (ch == '"' || ch == '\\') ch = '_';
            json += "{\"name\":\"" + safe + "\",\"size\":" + std::to_string(info.st_size) + "}";
        }
        closedir(dir);
    }
    json += ']';
    return httpd_resp_send(request, json.c_str(), json.size());
}

esp_err_t FileTransferService::uploadHandler(httpd_req_t* request)
{
    const std::string name = queryFilename(request);
    const bool firmware = allowedFirmware(name);
    const bool wallpaper = allowedWallpaper(name);
    const bool font = allowedFont(name);
    if (!allowedBook(name) && !firmware && !wallpaper && !font) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Unsupported file type");
    if (request->content_len <= 0 || static_cast<std::size_t>(request->content_len) > MAX_UPLOAD_BYTES) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid file size");
    }
    if (firmware) mkdir("/sdcard/ota", 0775);
    if (wallpaper) mkdir("/sdcard/wallpapers", 0775);
    if (font) mkdir("/sdcard/fonts", 0775);
    const std::string directory = firmware ? "/sdcard/ota" : wallpaper ? "/sdcard/wallpapers" : font ? "/sdcard/fonts" : BOOK_DIR;
    const std::string savedName = firmware ? "firmware.bin" : name;
    const std::string temporary = directory + "/." + savedName + ".upload";
    const std::string target = directory + "/" + savedName;
    FILE* file = std::fopen(temporary.c_str(), "wb");
    if (!file) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot write SD card");

    char buffer[4096];
    int remaining = request->content_len;
    bool okay = true;
    while (remaining > 0) {
        const int received = httpd_req_recv(request, buffer, std::min(remaining, static_cast<int>(sizeof(buffer))));
        if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (received <= 0 || std::fwrite(buffer, 1, received, file) != static_cast<std::size_t>(received)) {
            okay = false;
            break;
        }
        remaining -= received;
    }
    std::fclose(file);
    if (!okay || remaining != 0) {
        unlink(temporary.c_str());
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload interrupted");
    }
    unlink(target.c_str());
    if (rename(temporary.c_str(), target.c_str()) != 0) {
        unlink(temporary.c_str());
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot save file");
    }
    ESP_LOGI(TAG, "Uploaded %s (%d bytes)", name.c_str(), request->content_len);
    return httpd_resp_sendstr(request, "OK");
}

esp_err_t FileTransferService::deleteHandler(httpd_req_t* request)
{
    const std::string name = queryFilename(request);
    if (!allowedBook(name)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid file name");
    const std::string path = std::string(BOOK_DIR) + "/" + name;
    if (unlink(path.c_str()) != 0) return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "File not found");
    return httpd_resp_sendstr(request, "OK");
}

}  // namespace papers3
