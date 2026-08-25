#include "reader_service.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

extern "C" {
#include <lgfx/utility/lgfx_miniz.h>
}

namespace papers3 {

namespace {
constexpr std::size_t MAX_TEXT_BYTES = 4 * 1024 * 1024;
constexpr std::size_t MAX_CHAPTER_BYTES = 2 * 1024 * 1024;

struct ZipEntry {
    std::string name;
    std::uint16_t method = 0;
    std::uint32_t compressedSize = 0;
    std::uint32_t uncompressedSize = 0;
    std::uint32_t localOffset = 0;
};

std::uint16_t le16(const unsigned char* value) { return static_cast<std::uint16_t>(value[0]) | (static_cast<std::uint16_t>(value[1]) << 8); }
std::uint32_t le32(const unsigned char* value) { return static_cast<std::uint32_t>(value[0]) | (static_cast<std::uint32_t>(value[1]) << 8) | (static_cast<std::uint32_t>(value[2]) << 16) | (static_cast<std::uint32_t>(value[3]) << 24); }

std::size_t utf8Length(unsigned char first)
{
    if ((first & 0x80) == 0) return 1;
    if ((first & 0xE0) == 0xC0) return 2;
    if ((first & 0xF0) == 0xE0) return 3;
    if ((first & 0xF8) == 0xF0) return 4;
    return 1;
}

std::string lower(std::string value)
{
    for (auto& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

bool endsWith(const std::string& value, const char* suffix)
{
    const std::string ending(suffix);
    return value.size() >= ending.size() && lower(value.substr(value.size() - ending.size())) == ending;
}

std::string percentDecode(const std::string& input)
{
    std::string output;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            unsigned int value = 0;
            if (std::sscanf(input.substr(i + 1, 2).c_str(), "%02x", &value) == 1) {
                output.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        }
        output.push_back(input[i]);
    }
    return output;
}

std::string normalizePath(const std::string& base, std::string path)
{
    const auto fragment = path.find('#');
    if (fragment != std::string::npos) path.erase(fragment);
    path = percentDecode(path);
    std::string combined = path.empty() || path[0] == '/' ? path : base + path;
    while (!combined.empty() && combined.front() == '/') combined.erase(combined.begin());
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= combined.size()) {
        const auto slash = combined.find('/', start);
        const auto part = combined.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part == "..") { if (!parts.empty()) parts.pop_back(); }
        else if (!part.empty() && part != ".") parts.push_back(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) { if (i) result += '/'; result += parts[i]; }
    return result;
}

bool loadZipDirectory(FILE* file, std::vector<ZipEntry>& entries)
{
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    if (fileSize < 22) return false;
    const long tailSize = std::min<long>(fileSize, 65557);
    std::vector<unsigned char> tail(static_cast<std::size_t>(tailSize));
    std::fseek(file, fileSize - tailSize, SEEK_SET);
    if (std::fread(tail.data(), 1, tail.size(), file) != tail.size()) return false;
    long eocd = -1;
    for (long i = tailSize - 22; i >= 0; --i) {
        if (le32(tail.data() + i) == 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) return false;
    const auto count = le16(tail.data() + eocd + 10);
    const auto centralOffset = le32(tail.data() + eocd + 16);
    const auto centralSize = le32(tail.data() + eocd + 12);
    if (centralOffset > static_cast<std::uint32_t>(fileSize) ||
        centralSize > static_cast<std::uint32_t>(fileSize) - centralOffset) return false;
    const std::uint64_t centralEnd = static_cast<std::uint64_t>(centralOffset) + centralSize;
    std::fseek(file, centralOffset, SEEK_SET);
    for (std::uint16_t index = 0; index < count; ++index) {
        const long recordStart = std::ftell(file);
        if (recordStart < 0 || static_cast<std::uint64_t>(recordStart) + 46 > centralEnd) return false;
        unsigned char header[46];
        if (std::fread(header, 1, sizeof(header), file) != sizeof(header) || le32(header) != 0x02014b50) return false;
        ZipEntry entry;
        entry.method = le16(header + 10);
        entry.compressedSize = le32(header + 20);
        entry.uncompressedSize = le32(header + 24);
        const auto nameLength = le16(header + 28);
        const auto extraLength = le16(header + 30);
        const auto commentLength = le16(header + 32);
        entry.localOffset = le32(header + 42);
        const std::uint64_t recordEnd = static_cast<std::uint64_t>(recordStart) + 46 + nameLength + extraLength + commentLength;
        if (recordEnd > centralEnd || recordEnd > static_cast<std::uint64_t>(fileSize)) return false;
        entry.name.resize(nameLength);
        if (nameLength && std::fread(entry.name.data(), 1, nameLength, file) != nameLength) return false;
        std::fseek(file, extraLength + commentLength, SEEK_CUR);
        entries.push_back(std::move(entry));
    }
    return true;
}

bool extractEntry(FILE* file, const ZipEntry& entry, std::string& output)
{
    if (entry.uncompressedSize > MAX_CHAPTER_BYTES || entry.compressedSize > MAX_CHAPTER_BYTES) return false;
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    if (fileSize < 30 || static_cast<std::uint64_t>(entry.localOffset) + 30 > static_cast<std::uint64_t>(fileSize)) return false;
    std::fseek(file, entry.localOffset, SEEK_SET);
    unsigned char header[30];
    if (std::fread(header, 1, sizeof(header), file) != sizeof(header) || le32(header) != 0x04034b50) return false;
    const auto nameLength = le16(header + 26);
    const auto extraLength = le16(header + 28);
    const std::uint64_t dataOffset = static_cast<std::uint64_t>(entry.localOffset) + 30 + nameLength + extraLength;
    if (dataOffset > static_cast<std::uint64_t>(fileSize) || entry.compressedSize > static_cast<std::uint64_t>(fileSize) - dataOffset) return false;
    std::fseek(file, static_cast<long>(dataOffset), SEEK_SET);
    std::vector<unsigned char> compressed(entry.compressedSize);
    if (!compressed.empty() && std::fread(compressed.data(), 1, compressed.size(), file) != compressed.size()) return false;
    output.assign(entry.uncompressedSize, '\0');
    if (entry.method == 0) {
        if (compressed.size() != output.size()) return false;
        std::memcpy(output.data(), compressed.data(), output.size());
        return true;
    }
    if (entry.method != 8) return false;
    const auto size = lgfx_tinfl_decompress_mem_to_mem(output.data(), output.size(), compressed.data(), compressed.size(),
                                                       TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (size == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) return false;
    output.resize(size);
    return true;
}

const ZipEntry* findEntry(const std::vector<ZipEntry>& entries, const std::string& name)
{
    for (const auto& entry : entries) if (entry.name == name) return &entry;
    return nullptr;
}

std::string xmlAttribute(const std::string& tag, const std::string& name)
{
    std::size_t position = tag.find(name);
    while (position != std::string::npos) {
        position += name.size();
        while (position < tag.size() && std::isspace(static_cast<unsigned char>(tag[position]))) ++position;
        if (position < tag.size() && tag[position] == '=') {
            ++position;
            while (position < tag.size() && std::isspace(static_cast<unsigned char>(tag[position]))) ++position;
            if (position < tag.size() && (tag[position] == '"' || tag[position] == '\'')) {
                const char quote = tag[position++];
                const auto end = tag.find(quote, position);
                if (end != std::string::npos) return tag.substr(position, end - position);
            }
        }
        position = tag.find(name, position);
    }
    return {};
}

std::string htmlToText(const std::string& html)
{
    std::string text;
    bool hidden = false;
    for (std::size_t i = 0; i < html.size();) {
        if (html[i] == '<') {
            const auto end = html.find('>', i + 1);
            if (end == std::string::npos) break;
            const auto tag = lower(html.substr(i + 1, end - i - 1));
            if (tag.rfind("script", 0) == 0 || tag.rfind("style", 0) == 0) hidden = true;
            if (tag.rfind("/script", 0) == 0 || tag.rfind("/style", 0) == 0) hidden = false;
            if (!hidden && (tag.rfind("br", 0) == 0 || tag.rfind("/p", 0) == 0 || tag.rfind("/div", 0) == 0 ||
                            tag.rfind("/h", 0) == 0 || tag.rfind("/li", 0) == 0)) {
                if (!text.empty() && text.back() != '\n') text.push_back('\n');
            }
            i = end + 1;
            continue;
        }
        if (hidden) { ++i; continue; }
        if (html[i] == '&') {
            const auto end = html.find(';', i + 1);
            if (end != std::string::npos && end - i < 12) {
                const auto entity = html.substr(i, end - i + 1);
                if (entity == "&nbsp;") text.push_back(' ');
                else if (entity == "&amp;") text.push_back('&');
                else if (entity == "&lt;") text.push_back('<');
                else if (entity == "&gt;") text.push_back('>');
                else if (entity == "&quot;") text.push_back('"');
                else if (entity == "&apos;") text.push_back('\'');
                i = end + 1;
                continue;
            }
        }
        text.push_back(html[i++]);
    }
    return text;
}

std::string htmlTitle(const std::string& html)
{
    const auto lowerHtml = lower(html);
    const auto start = lowerHtml.find("<title");
    if (start == std::string::npos) return {};
    const auto content = lowerHtml.find('>', start);
    const auto end = content == std::string::npos ? std::string::npos : lowerHtml.find("</title>", content + 1);
    if (end == std::string::npos) return {};
    return htmlToText(html.substr(content + 1, end - content - 1));
}
}

bool ReaderService::open(const BookInfo& book, int progressPercent)
{
    close();
    const bool loaded = endsWith(book.path, ".txt") ? paginateTxt(book.path) :
                        endsWith(book.path, ".epub") ? paginateEpub(book.path) : false;
    if (!loaded) {
        if (error_.empty()) error_ = "不支持的图书格式";
        return false;
    }
    page_ = std::clamp(progressPercent * static_cast<int>(pages_.size()) / 100, 0, static_cast<int>(pages_.size()) - 1);
    return true;
}

void ReaderService::close()
{
    pages_.clear();
    chapterTitles_.clear();
    chapterPages_.clear();
    page_ = 0;
    lastTickMs_ = 0;
    pendingMs_ = 0;
    error_.clear();
}

bool ReaderService::paginateTxt(const std::string& path)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) { error_ = "无法打开 TXT 文件"; return false; }
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (length <= 0 || static_cast<std::size_t>(length) > MAX_TEXT_BYTES) {
        std::fclose(file);
        error_ = length <= 0 ? "文件为空" : "TXT 文件超过 4 MB";
        return false;
    }
    std::string content(static_cast<std::size_t>(length), '\0');
    content.resize(std::fread(content.data(), 1, content.size(), file));
    std::fclose(file);
    chapterTitles_.push_back("正文");
    chapterPages_.push_back(0);
    return paginateText(std::move(content));
}

bool ReaderService::paginateEpub(const std::string& path)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) { error_ = "无法打开 EPUB 文件"; return false; }
    std::vector<ZipEntry> entries;
    if (!loadZipDirectory(file, entries)) { std::fclose(file); error_ = "EPUB ZIP 目录损坏"; return false; }

    const auto* containerEntry = findEntry(entries, "META-INF/container.xml");
    std::string container;
    if (!containerEntry || !extractEntry(file, *containerEntry, container)) {
        std::fclose(file); error_ = "EPUB 缺少 container.xml"; return false;
    }
    const auto rootStart = container.find("<rootfile");
    const auto rootEnd = rootStart == std::string::npos ? std::string::npos : container.find('>', rootStart);
    const std::string opfPath = rootEnd == std::string::npos ? std::string() : xmlAttribute(container.substr(rootStart, rootEnd - rootStart + 1), "full-path");
    const auto* opfEntry = findEntry(entries, opfPath);
    std::string opf;
    if (!opfEntry || !extractEntry(file, *opfEntry, opf)) { std::fclose(file); error_ = "EPUB 内容清单无法读取"; return false; }
    const auto slash = opfPath.find_last_of('/');
    const std::string base = slash == std::string::npos ? "" : opfPath.substr(0, slash + 1);

    std::map<std::string, std::string> manifest;
    for (std::size_t position = 0; (position = opf.find("<item", position)) != std::string::npos;) {
        if (opf.compare(position, 8, "<itemref") == 0) { position += 8; continue; }
        const auto end = opf.find('>', position);
        if (end == std::string::npos) break;
        const auto tag = opf.substr(position, end - position + 1);
        const auto id = xmlAttribute(tag, "id");
        const auto href = xmlAttribute(tag, "href");
        if (!id.empty() && !href.empty()) manifest[id] = normalizePath(base, href);
        position = end + 1;
    }

    std::vector<std::string> spine;
    for (std::size_t position = 0; (position = opf.find("<itemref", position)) != std::string::npos;) {
        const auto end = opf.find('>', position);
        if (end == std::string::npos) break;
        const auto id = xmlAttribute(opf.substr(position, end - position + 1), "idref");
        const auto found = manifest.find(id);
        if (found != manifest.end()) spine.push_back(found->second);
        position = end + 1;
    }
    if (spine.empty()) {
        for (const auto& entry : entries) if (endsWith(entry.name, ".xhtml") || endsWith(entry.name, ".html") || endsWith(entry.name, ".htm")) spine.push_back(entry.name);
    }

    int chapterNumber = 1;
    for (const auto& chapterPath : spine) {
        const auto* chapterEntry = findEntry(entries, chapterPath);
        std::string chapter;
        if (!chapterEntry || !extractEntry(file, *chapterEntry, chapter)) continue;
        auto plain = htmlToText(chapter);
        if (!plain.empty()) {
            chapterPages_.push_back(static_cast<int>(pages_.size()));
            auto title = htmlTitle(chapter);
            if (title.empty()) title = "第 " + std::to_string(chapterNumber) + " 章";
            chapterTitles_.push_back(std::move(title));
            paginateText(std::move(plain));
            ++chapterNumber;
        }
        std::size_t total = 0;
        for (const auto& page : pages_) total += page.size();
        if (total >= MAX_TEXT_BYTES) break;
    }
    std::fclose(file);
    if (pages_.empty()) { error_ = "EPUB 中没有可读取的章节"; return false; }
    return true;
}

bool ReaderService::paginateText(std::string content)
{
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF && static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) content.erase(0, 3);
    std::string page;
    int lineCells = 0;
    int lineCount = 1;
    auto finishPage = [&]() { if (!page.empty()) pages_.push_back(page); page.clear(); lineCells = 0; lineCount = 1; };
    for (std::size_t i = 0; i < content.size();) {
        if (content[i] == '\r') { ++i; continue; }
        if (content[i] == '\n') {
            if (!page.empty() && page.back() != '\n') page.push_back('\n');
            ++i; lineCells = 0;
            if (++lineCount > linesPerPage_) finishPage();
            continue;
        }
        const auto length = std::min(utf8Length(static_cast<unsigned char>(content[i])), content.size() - i);
        const int cells = length == 1 ? 1 : 2;
        if (lineCells + cells > cellsPerLine_) { page.push_back('\n'); lineCells = 0; if (++lineCount > linesPerPage_) finishPage(); }
        page.append(content, i, length);
        lineCells += cells;
        i += length;
    }
    if (!page.empty()) pages_.push_back(page);
    if (pages_.empty()) error_ = "没有可显示的 UTF-8 文本";
    return !pages_.empty();
}

bool ReaderService::isOpen() const { return !pages_.empty(); }
bool ReaderService::nextPage() { if (page_ + 1 >= pageCount()) return false; ++page_; return true; }
bool ReaderService::previousPage() { if (page_ <= 0) return false; --page_; return true; }
bool ReaderService::goToPage(int page) { if (page < 0 || page >= pageCount()) return false; page_ = page; return true; }
bool ReaderService::goToChapter(int chapter) { return chapter >= 0 && chapter < static_cast<int>(chapterPages_.size()) && goToPage(chapterPages_[chapter]); }
void ReaderService::setLayout(int fontSize, int marginLevel, int lineHeightLevel)
{
    const int safeFont = std::clamp(fontSize, 14, 24);
    const int margin = 20 + std::clamp(marginLevel, 0, 4) * 14;
    const int usableWidth = std::max(220, 540 - margin * 2);
    const int lineHeight = safeFont + 8 + std::clamp(lineHeightLevel, 0, 4) * 5;
    cellsPerLine_ = std::max(18, usableWidth * 2 / safeFont);
    linesPerPage_ = std::max(12, 900 / lineHeight);
}
int ReaderService::pageIndex() const { return page_; }
int ReaderService::pageCount() const { return static_cast<int>(pages_.size()); }
int ReaderService::progressPercent() const { return pages_.empty() ? 0 : (page_ + 1) * 100 / pageCount(); }
const std::string& ReaderService::pageText() const { static const std::string empty; return pages_.empty() ? empty : pages_[page_]; }
const std::vector<std::string>& ReaderService::chapterTitles() const { return chapterTitles_; }
const std::vector<int>& ReaderService::chapterPages() const { return chapterPages_; }
const std::string& ReaderService::error() const { return error_; }

std::uint32_t ReaderService::consumeSessionSeconds(std::uint32_t nowMs)
{
    if (!isOpen()) return 0;
    if (!lastTickMs_) { lastTickMs_ = nowMs; return 0; }
    pendingMs_ += nowMs - lastTickMs_;
    lastTickMs_ = nowMs;
    if (pendingMs_ < 30000) return 0;
    const auto result = pendingMs_ / 1000;
    pendingMs_ %= 1000;
    return result;
}

}  // namespace papers3
