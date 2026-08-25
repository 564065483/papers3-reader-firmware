#pragma once

#include "app_state.h"
#include "papers3_hal.h"
#include <esp_http_server.h>
#include <string>

namespace papers3 {

class FileTransferService {
public:
    bool startApUploadServer(AppState& state, PaperS3Hal& hal);
    void stop(AppState& state, PaperS3Hal& hal);
    bool running() const;
    std::string uploadUrl() const;

private:
    httpd_handle_t server_ = nullptr;
    bool running_ = false;
    std::string url_;

#ifdef ESP_PLATFORM
    bool ftpRunning_ = false;
    void* ftpTask_ = nullptr;
    int ftpListenSocket_ = -1;
    bool startFtpServer();
    void stopFtpServer();
    static void ftpTaskEntry(void* context);
    void ftpLoop();
    void ftpClient(int clientSocket);
#endif

    static esp_err_t rootHandler(httpd_req_t* request);
    static esp_err_t filesHandler(httpd_req_t* request);
    static esp_err_t uploadHandler(httpd_req_t* request);
    static esp_err_t deleteHandler(httpd_req_t* request);
};

}  // namespace papers3
