#pragma once

#include "app_state.h"
#include <cstdint>
#include <string>
#include <vector>

namespace papers3 {

class ReaderService {
public:
    bool open(const BookInfo& book, int progressPercent = 0);
    void close();
    bool isOpen() const;
    bool nextPage();
    bool previousPage();
    bool goToPage(int page);
    bool goToChapter(int chapter);
    void setLayout(int fontSize, int marginLevel, int lineHeightLevel);
    int pageIndex() const;
    int pageCount() const;
    int progressPercent() const;
    const std::string& pageText() const;
    const std::vector<std::string>& chapterTitles() const;
    const std::vector<int>& chapterPages() const;
    std::uint32_t consumeSessionSeconds(std::uint32_t nowMs);
    const std::string& error() const;

private:
    std::vector<std::string> pages_;
    std::vector<std::string> chapterTitles_;
    std::vector<int> chapterPages_;
    int page_ = 0;
    std::uint32_t lastTickMs_ = 0;
    std::uint32_t pendingMs_ = 0;
    std::string error_;
    int cellsPerLine_ = 42;
    int linesPerPage_ = 27;

    bool paginateTxt(const std::string& path);
    bool paginateEpub(const std::string& path);
    bool paginateText(std::string content);
};

}  // namespace papers3
