#include "storage_service.h"

#include <algorithm>
#include <cctype>
#include <sys/stat.h>

namespace papers3 {

namespace {
std::string fileName(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    const auto start = slash == std::string::npos ? 0 : slash + 1;
    const auto dot = path.find_last_of('.');
    return path.substr(start, dot == std::string::npos || dot < start ? std::string::npos : dot - start);
}

std::string fileType(const std::string& path)
{
    std::string lower = path;
    for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".txt") == 0 ? "TXT" : "EPUB";
}
}

void StorageService::refresh(AppState& state, PaperS3Hal& hal)
{
    state.status.sdMounted = hal.isSdMounted() || hal.mountSdCard();
    state.books.clear();
    if (!state.status.sdMounted) {
        state.status.message = "未检测到 SD 卡";
        return;
    }
    mkdir("/sdcard/books", 0775);
    auto files = hal.listBookFiles("/sdcard/books");
    std::sort(files.begin(), files.end());
    for (const auto& path : files) {
        state.books.push_back({fileName(path), "", path, fileType(path), 0, false, {}, 0, 0});
    }
    state.status.message = state.books.empty() ? "SD 卡中暂无图书" : "已读取 SD 卡图书";
}

}  // namespace papers3
