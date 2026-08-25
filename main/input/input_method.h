#pragma once

#include "app_state.h"
#include <string>
#include <vector>

namespace papers3 {

class InputMethod {
public:
    void open(std::string* target);
    void close();
    bool isOpen() const;
    void setMode(InputMode mode);
    InputMode mode() const;

    void key(const std::string& value);
    void backspace();
    void clear();
    void commitCandidate(const std::string& value);

    const std::string& composition() const;
    const std::string& value() const;
    std::vector<std::string> candidates() const;
    std::string title() const;

private:
    std::string* target_ = nullptr;
    InputMode mode_ = InputMode::English;
    std::string composition_;
};

}  // namespace papers3
