#include "file_manager_service.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace papers3 {

bool FileManagerService::ensureDirectories()
{
    bool okay = true;
    for (const char* path : {"/sdcard/books", "/sdcard/fonts", "/sdcard/wallpapers", "/sdcard/ota"}) {
        if (mkdir(path, 0775) != 0) {
            struct stat info {};
            okay = okay && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
        }
    }
    return okay;
}

bool FileManagerService::list(AppState& state, const std::string& folder)
{
    state.files.clear();
    DIR* directory = opendir(folder.c_str());
    if (!directory) { state.status.message = "无法打开目录"; return false; }
    while (auto* entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        const std::string path = folder + (folder.back() == '/' ? "" : "/") + name;
        struct stat info {};
        if (stat(path.c_str(), &info) != 0) continue;
        std::string type = "FILE";
        const auto dot = name.find_last_of('.');
        if (dot != std::string::npos) {
            type = name.substr(dot + 1);
            for (auto& ch : type) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        state.files.push_back({name, path, S_ISDIR(info.st_mode) ? "DIR" : type,
                               static_cast<std::uint64_t>(info.st_size), S_ISDIR(info.st_mode)});
    }
    closedir(directory);
    std::sort(state.files.begin(), state.files.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.directory != b.directory) return a.directory > b.directory;
        return a.name < b.name;
    });
    state.currentFolder = folder;
    state.status.message = "目录已刷新";
    return true;
}

bool FileManagerService::safeName(const std::string& name)
{
    return !name.empty() && name != "." && name != ".." && name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos && name.find("..") == std::string::npos;
}

bool FileManagerService::renameEntry(const std::string& path, const std::string& newName, std::string& error)
{
    if (!safeName(newName)) { error = "文件名不安全"; return false; }
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos || path.rfind("/sdcard/", 0) != 0) { error = "路径不安全"; return false; }
    const std::string target = path.substr(0, slash + 1) + newName;
    if (rename(path.c_str(), target.c_str()) != 0) { error = "重命名失败"; return false; }
    return true;
}

bool FileManagerService::removeEntry(const std::string& path, std::string& error)
{
    if (path.rfind("/sdcard/", 0) != 0 || path == "/sdcard/books" || path == "/sdcard/fonts" ||
        path == "/sdcard/wallpapers" || path == "/sdcard/ota") { error = "禁止删除系统目录"; return false; }
    struct stat info {};
    if (stat(path.c_str(), &info) != 0) { error = "文件不存在"; return false; }
    const int result = S_ISDIR(info.st_mode) ? rmdir(path.c_str()) : unlink(path.c_str());
    if (result != 0) { error = S_ISDIR(info.st_mode) ? "目录非空或删除失败" : "删除失败"; return false; }
    return true;
}

bool FileManagerService::createFolder(const std::string& parent, const std::string& name, std::string& error)
{
    if (parent.rfind("/sdcard", 0) != 0 || !safeName(name)) { error = "目录名称不安全"; return false; }
    if (mkdir((parent + "/" + name).c_str(), 0775) != 0) { error = "创建目录失败"; return false; }
    return true;
}

std::string FileManagerService::firstFile(const std::string& folder, const std::vector<std::string>& extensions) const
{
    DIR* directory = opendir(folder.c_str());
    if (!directory) return {};
    std::string result;
    while (auto* entry = readdir(directory)) {
        std::string name = entry->d_name;
        std::string lowerName = name;
        for (auto& ch : lowerName) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        for (const auto& extension : extensions) {
            if (lowerName.size() >= extension.size() && lowerName.compare(lowerName.size() - extension.size(), extension.size(), extension) == 0) {
                result = folder + "/" + name;
                break;
            }
        }
        if (!result.empty()) break;
    }
    closedir(directory);
    return result;
}

}  // namespace papers3
