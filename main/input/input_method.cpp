#include "input_method.h"

#include <map>

namespace papers3 {

namespace {
const std::map<std::string, std::vector<std::string>> kPinyin {
    {"nihao", {"你好"}},
    {"yuedu", {"阅读"}},
    {"shu", {"书", "数", "输"}},
    {"shujia", {"书架"}},
    {"sousuo", {"搜索"}},
    {"zhongwen", {"中文"}},
    {"xitong", {"系统"}},
    {"ming", {"明", "名"}},
    {"wenjian", {"文件"}},
    {"mima", {"密码"}},
};
}

void InputMethod::open(std::string* target) { target_ = target; composition_.clear(); }
void InputMethod::close() { target_ = nullptr; composition_.clear(); }
bool InputMethod::isOpen() const { return target_ != nullptr; }
void InputMethod::setMode(InputMode mode) { mode_ = mode; composition_.clear(); }
InputMode InputMethod::mode() const { return mode_; }

void InputMethod::key(const std::string& value)
{
    if (!target_) return;
    if (mode_ == InputMode::Pinyin && value.size() == 1 && value[0] >= 'a' && value[0] <= 'z') {
        composition_ += value;
        return;
    }
    if (mode_ == InputMode::Pinyin && value == " " && !composition_.empty()) {
        const auto list = candidates();
        commitCandidate(list.empty() ? composition_ : list.front());
        return;
    }
    *target_ += value;
}

void InputMethod::backspace()
{
    if (!target_) return;
    if (!composition_.empty()) { composition_.pop_back(); return; }
    if (target_->empty()) return;
    std::size_t index = target_->size() - 1;
    while (index > 0 && (static_cast<unsigned char>((*target_)[index]) & 0xC0) == 0x80) --index;
    target_->erase(index);
}

void InputMethod::clear() { composition_.clear(); if (target_) target_->clear(); }
void InputMethod::commitCandidate(const std::string& value) { if (target_) *target_ += value; composition_.clear(); }
const std::string& InputMethod::composition() const { return composition_; }
const std::string& InputMethod::value() const { static const std::string empty; return target_ ? *target_ : empty; }

std::vector<std::string> InputMethod::candidates() const
{
    if (mode_ != InputMode::Pinyin) return mode_ == InputMode::Symbols ? std::vector<std::string>{"@", ".com", "_", "-", "/"} : std::vector<std::string>{"book", "wifi", "epub"};
    if (composition_.empty()) return {"中文", "阅读", "书架", "搜索"};
    const auto exact = kPinyin.find(composition_);
    if (exact != kPinyin.end()) return exact->second;
    std::vector<std::string> output;
    for (const auto& item : kPinyin) {
        if (item.first.rfind(composition_, 0) == 0) {
            output.insert(output.end(), item.second.begin(), item.second.end());
            if (output.size() >= 4) break;
        }
    }
    return output;
}

std::string InputMethod::title() const
{
    if (mode_ == InputMode::Pinyin) return "拼音输入" + (composition_.empty() ? std::string() : " · " + composition_);
    return mode_ == InputMode::Symbols ? "符号输入" : "英文输入";
}

}  // namespace papers3
