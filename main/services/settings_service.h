#pragma once

#include "app_state.h"
#include <cstdint>
#include <string>

namespace papers3 {

class SettingsService {
public:
    bool init();
    void load(AppState& state);
    void saveUi(const AppState& state);

    bool loadWifi(std::string& ssid, std::string& password);
    void saveWifi(const std::string& ssid, const std::string& password);
    void clearWifi();

    void loadBook(BookInfo& book);
    void saveBook(const BookInfo& book, std::uint32_t seconds);
    void addReadingSeconds(AppState& state, std::uint32_t seconds);
    bool resetAll();

private:
    bool ready_ = false;
    static std::uint32_t hashPath(const std::string& value);
};

}  // namespace papers3
