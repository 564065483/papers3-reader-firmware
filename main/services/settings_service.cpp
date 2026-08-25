#include "settings_service.h"

#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <nvs.h>
#include <nvs_flash.h>
#include <utility>

namespace papers3 {

namespace {
constexpr const char* kNamespace = "reader";

std::string readString(nvs_handle_t handle, const char* key)
{
    size_t size = 0;
    if (nvs_get_str(handle, key, nullptr, &size) != ESP_OK || size <= 1) return {};
    std::string value(size, '\0');
    if (nvs_get_str(handle, key, value.data(), &size) != ESP_OK) return {};
    value.resize(size - 1);
    return value;
}

std::uint32_t dayId()
{
    const std::time_t now = std::time(nullptr);
    return now > 1700000000 ? static_cast<std::uint32_t>(now / 86400) : 0;
}

std::uint32_t monthId()
{
    const std::time_t now = std::time(nullptr);
    if (now <= 1700000000) return 0;
    std::tm tm {};
    localtime_r(&now, &tm);
    return static_cast<std::uint32_t>((tm.tm_year + 1900) * 12 + tm.tm_mon);
}
}

bool SettingsService::init()
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) return false;
        result = nvs_flash_init();
    }
    ready_ = result == ESP_OK;
    return ready_;
}

void SettingsService::load(AppState& state)
{
    if (!ready_) return;
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return;
    std::uint8_t value = 0;
    if (nvs_get_u8(handle, "theme", &value) == ESP_OK) state.theme = value ? Theme::Dark : Theme::Light;
    if (nvs_get_u8(handle, "lang", &value) == ESP_OK) state.language = value ? SystemLanguage::Vietnamese : SystemLanguage::Chinese;
    std::int32_t setting = 0;
    if (nvs_get_i32(handle, "font_size", &setting) == ESP_OK) state.reader.fontSize = setting;
    if (nvs_get_i32(handle, "margin", &setting) == ESP_OK) state.reader.marginLevel = setting;
    if (nvs_get_i32(handle, "line_h", &setting) == ESP_OK) state.reader.lineHeightLevel = setting;
    if (nvs_get_i32(handle, "auto_sec", &setting) == ESP_OK) state.reader.autoPageSeconds = setting;
    if (nvs_get_u8(handle, "auto_page", &value) == ESP_OK) state.reader.autoPage = value != 0;
    if (nvs_get_u8(handle, "auto_rotate", &value) == ESP_OK) state.reader.autoRotate = value != 0;
    state.reader.fontName = readString(handle, "font_name");
    if (state.reader.fontName.empty()) state.reader.fontName = "System";
    state.reader.pageTurnMode = readString(handle, "turn_mode");
    if (state.reader.pageTurnMode.empty()) state.reader.pageTurnMode = "tap";
    if (nvs_get_u8(handle, "orient", &value) == ESP_OK) state.system.orientation = value ? Orientation::Landscape : Orientation::Portrait;
    if (nvs_get_u8(handle, "statusbar", &value) == ESP_OK) state.system.statusBar = value != 0;
    if (nvs_get_u8(handle, "ntp", &value) == ESP_OK) state.system.ntpEnabled = value != 0;
    if (nvs_get_u8(handle, "auto_rec", &value) == ESP_OK) state.system.autoReconnect = value != 0;
    if (nvs_get_u8(handle, "low_power", &value) == ESP_OK) state.system.lowPower = value != 0;
    if (nvs_get_i32(handle, "sleep_min", &setting) == ESP_OK) state.system.sleepMinutes = setting;
    if (nvs_get_i32(handle, "off_hours", &setting) == ESP_OK) state.system.powerOffHours = setting;
    if (nvs_get_i32(handle, "tz_min", &setting) == ESP_OK) state.system.timezoneMinutes = setting;
    state.system.systemFontId = readString(handle, "sys_font");
    if (state.system.systemFontId.empty()) state.system.systemFontId = "system";
    state.system.wallpaperId = readString(handle, "wallpaper");
    if (state.system.wallpaperId.empty()) state.system.wallpaperId = "paper-morning";
    state.system.refreshMode = readString(handle, "refresh");
    if (state.system.refreshMode.empty()) state.system.refreshMode = "text";
    if (nvs_get_u8(handle, "random_wall", &value) == ESP_OK) state.system.randomLockWallpaper = value != 0;

    // Values in NVS can outlive the firmware that wrote them. Keep stale or
    // corrupted settings from producing invalid layout, timer, or power data.
    state.reader.fontSize = std::clamp(state.reader.fontSize, 14, 24);
    state.reader.marginLevel = std::clamp(state.reader.marginLevel, 0, 4);
    state.reader.lineHeightLevel = std::clamp(state.reader.lineHeightLevel, 0, 4);
    state.reader.autoPageSeconds = std::clamp(state.reader.autoPageSeconds, 5, 60);
    if (state.reader.pageTurnMode != "swipe" && state.reader.pageTurnMode != "tap" &&
        state.reader.pageTurnMode != "simulation" && state.reader.pageTurnMode != "tilt") {
        state.reader.pageTurnMode = "swipe";
    }
    state.system.sleepMinutes = std::clamp(state.system.sleepMinutes, 0, 24 * 60);
    state.system.powerOffHours = std::clamp(state.system.powerOffHours, 0, 24 * 7);
    state.system.timezoneMinutes = std::clamp(state.system.timezoneMinutes, -12 * 60, 14 * 60);

    std::int32_t count = 0;
    if (nvs_get_i32(handle, "timer_count", &count) == ESP_OK) {
        count = std::max<std::int32_t>(0, std::min<std::int32_t>(count, 4));
        for (std::int32_t index = 0; index < count; ++index) {
            char key[24];
            std::snprintf(key, sizeof(key), "tm%d_name", static_cast<int>(index));
            CountdownTimer timer;
            timer.id = "timer-" + std::to_string(index);
            timer.name = readString(handle, key);
            std::snprintf(key, sizeof(key), "tm%d_dur", static_cast<int>(index));
            nvs_get_u32(handle, key, &timer.durationSeconds);
            std::snprintf(key, sizeof(key), "tm%d_rem", static_cast<int>(index));
            nvs_get_u32(handle, key, &timer.remainingSeconds);
            std::snprintf(key, sizeof(key), "tm%d_end", static_cast<int>(index));
            nvs_get_i64(handle, key, &timer.endsAtEpoch);
            std::snprintf(key, sizeof(key), "tm%d_run", static_cast<int>(index));
            if (nvs_get_u8(handle, key, &value) == ESP_OK) timer.running = value != 0;
            timer.durationSeconds = std::clamp<std::uint32_t>(timer.durationSeconds, 60, 24 * 3600);
            timer.remainingSeconds = std::clamp<std::uint32_t>(timer.remainingSeconds, 0, timer.durationSeconds);
            if (!timer.name.empty()) state.timers.push_back(timer);
        }
    }
    if (nvs_get_i32(handle, "pom_focus", &setting) == ESP_OK) state.pomodoro.focusMinutes = setting;
    if (nvs_get_i32(handle, "pom_break", &setting) == ESP_OK) state.pomodoro.breakMinutes = setting;
    if (nvs_get_i32(handle, "pom_cycles", &setting) == ESP_OK) state.pomodoro.cycles = setting;
    state.pomodoro.focusMinutes = std::clamp(state.pomodoro.focusMinutes, 1, 120);
    state.pomodoro.breakMinutes = std::clamp(state.pomodoro.breakMinutes, 1, 60);
    state.pomodoro.cycles = std::clamp(state.pomodoro.cycles, 0, 100000);

    std::int32_t todoCount = 0;
    if (nvs_get_i32(handle, "todo_count", &todoCount) == ESP_OK) {
        todoCount = std::clamp<std::int32_t>(todoCount, 0, 6);
        for (std::int32_t index = 0; index < todoCount; ++index) {
            char textKey[24];
            char doneKey[24];
            std::snprintf(textKey, sizeof(textKey), "todo%d_text", static_cast<int>(index));
            std::snprintf(doneKey, sizeof(doneKey), "todo%d_done", static_cast<int>(index));
            TodoItem item;
            item.text = readString(handle, textKey);
            if (nvs_get_u8(handle, doneKey, &value) == ESP_OK) item.done = value != 0;
            if (!item.text.empty()) state.todos.push_back(std::move(item));
        }
    }

    nvs_get_u32(handle, "rd_total", &state.totalReadingSeconds);
    std::uint32_t savedDay = 0;
    std::uint32_t savedMonth = 0;
    nvs_get_u32(handle, "rd_dayid", &savedDay);
    nvs_get_u32(handle, "rd_monid", &savedMonth);
    if (savedDay == dayId()) nvs_get_u32(handle, "rd_day", &state.dailyReadingSeconds);
    if (savedMonth == monthId()) nvs_get_u32(handle, "rd_month", &state.monthlyReadingSeconds);
    nvs_close(handle);
}

void SettingsService::saveUi(const AppState& state)
{
    if (!ready_) return;
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u8(handle, "theme", state.theme == Theme::Dark ? 1 : 0);
    nvs_set_u8(handle, "lang", state.language == SystemLanguage::Vietnamese ? 1 : 0);
    nvs_set_i32(handle, "font_size", state.reader.fontSize);
    nvs_set_i32(handle, "margin", state.reader.marginLevel);
    nvs_set_i32(handle, "line_h", state.reader.lineHeightLevel);
    nvs_set_u8(handle, "auto_page", state.reader.autoPage ? 1 : 0);
    nvs_set_i32(handle, "auto_sec", state.reader.autoPageSeconds);
    nvs_set_u8(handle, "auto_rotate", state.reader.autoRotate ? 1 : 0);
    nvs_set_str(handle, "font_name", state.reader.fontName.c_str());
    nvs_set_str(handle, "turn_mode", state.reader.pageTurnMode.c_str());
    nvs_set_u8(handle, "orient", state.system.orientation == Orientation::Landscape ? 1 : 0);
    nvs_set_u8(handle, "statusbar", state.system.statusBar ? 1 : 0);
    nvs_set_u8(handle, "ntp", state.system.ntpEnabled ? 1 : 0);
    nvs_set_u8(handle, "auto_rec", state.system.autoReconnect ? 1 : 0);
    nvs_set_u8(handle, "low_power", state.system.lowPower ? 1 : 0);
    nvs_set_i32(handle, "sleep_min", state.system.sleepMinutes);
    nvs_set_i32(handle, "off_hours", state.system.powerOffHours);
    nvs_set_i32(handle, "tz_min", state.system.timezoneMinutes);
    nvs_set_str(handle, "sys_font", state.system.systemFontId.c_str());
    nvs_set_str(handle, "wallpaper", state.system.wallpaperId.c_str());
    nvs_set_str(handle, "refresh", state.system.refreshMode.c_str());
    nvs_set_u8(handle, "random_wall", state.system.randomLockWallpaper ? 1 : 0);
    const int timerCount = std::min(4, static_cast<int>(state.timers.size()));
    nvs_set_i32(handle, "timer_count", timerCount);
    for (int index = 0; index < timerCount; ++index) {
        char key[24];
        const auto& timer = state.timers[index];
        std::snprintf(key, sizeof(key), "tm%d_name", index); nvs_set_str(handle, key, timer.name.c_str());
        std::snprintf(key, sizeof(key), "tm%d_dur", index); nvs_set_u32(handle, key, timer.durationSeconds);
        std::snprintf(key, sizeof(key), "tm%d_rem", index); nvs_set_u32(handle, key, timer.remainingSeconds);
        std::snprintf(key, sizeof(key), "tm%d_end", index); nvs_set_i64(handle, key, timer.endsAtEpoch);
        std::snprintf(key, sizeof(key), "tm%d_run", index); nvs_set_u8(handle, key, timer.running ? 1 : 0);
    }
    nvs_set_i32(handle, "pom_focus", state.pomodoro.focusMinutes);
    nvs_set_i32(handle, "pom_break", state.pomodoro.breakMinutes);
    nvs_set_i32(handle, "pom_cycles", state.pomodoro.cycles);
    const int todoCount = std::min(6, static_cast<int>(state.todos.size()));
    nvs_set_i32(handle, "todo_count", todoCount);
    for (int index = 0; index < todoCount; ++index) {
        char textKey[24];
        char doneKey[24];
        std::snprintf(textKey, sizeof(textKey), "todo%d_text", index);
        std::snprintf(doneKey, sizeof(doneKey), "todo%d_done", index);
        nvs_set_str(handle, textKey, state.todos[index].text.c_str());
        nvs_set_u8(handle, doneKey, state.todos[index].done ? 1 : 0);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

bool SettingsService::loadWifi(std::string& ssid, std::string& password)
{
    if (!ready_) return false;
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return false;
    ssid = readString(handle, "wifi_ssid");
    password = readString(handle, "wifi_pass");
    nvs_close(handle);
    return !ssid.empty();
}

void SettingsService::saveWifi(const std::string& ssid, const std::string& password)
{
    if (!ready_) return;
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_str(handle, "wifi_ssid", ssid.c_str());
    nvs_set_str(handle, "wifi_pass", password.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}

void SettingsService::clearWifi()
{
    if (!ready_) return;
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_erase_key(handle, "wifi_ssid");
    nvs_erase_key(handle, "wifi_pass");
    nvs_commit(handle);
    nvs_close(handle);
}

std::uint32_t SettingsService::hashPath(const std::string& value)
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char ch : value) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return hash;
}

void SettingsService::loadBook(BookInfo& book)
{
    if (!ready_) return;
    char progressKey[15];
    char timeKey[15];
    char bookmarkKey[15];
    char favoriteKey[15];
    char openedKey[15];
    char tagKey[15];
    const auto hash = hashPath(book.path);
    std::snprintf(progressKey, sizeof(progressKey), "p%08lx", static_cast<unsigned long>(hash));
    std::snprintf(timeKey, sizeof(timeKey), "t%08lx", static_cast<unsigned long>(hash));
    std::snprintf(bookmarkKey, sizeof(bookmarkKey), "b%08lx", static_cast<unsigned long>(hash));
    std::snprintf(favoriteKey, sizeof(favoriteKey), "f%08lx", static_cast<unsigned long>(hash));
    std::snprintf(openedKey, sizeof(openedKey), "o%08lx", static_cast<unsigned long>(hash));
    std::snprintf(tagKey, sizeof(tagKey), "g%08lx", static_cast<unsigned long>(hash));
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return;
    std::int32_t progress = 0;
    if (nvs_get_i32(handle, progressKey, &progress) == ESP_OK) {
        book.progressPercent = static_cast<int>(std::clamp<std::int32_t>(progress, 0, 100));
    }
    nvs_get_u32(handle, timeKey, &book.readingSeconds);
    std::uint8_t favorite = 0;
    if (nvs_get_u8(handle, favoriteKey, &favorite) == ESP_OK) book.favorite = favorite != 0;
    nvs_get_i64(handle, openedKey, &book.lastOpenedEpoch);
    book.tag = readString(handle, tagKey);
    const auto bookmarks = readString(handle, bookmarkKey);
    std::size_t start = 0;
    while (start < bookmarks.size()) {
        const auto comma = bookmarks.find(',', start);
        const auto token = bookmarks.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!token.empty()) book.bookmarks.push_back(std::atoi(token.c_str()));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    nvs_close(handle);
}

void SettingsService::saveBook(const BookInfo& book, std::uint32_t seconds)
{
    if (!ready_) return;
    char progressKey[15];
    char timeKey[15];
    char bookmarkKey[15];
    char favoriteKey[15];
    char openedKey[15];
    char tagKey[15];
    const auto hash = hashPath(book.path);
    std::snprintf(progressKey, sizeof(progressKey), "p%08lx", static_cast<unsigned long>(hash));
    std::snprintf(timeKey, sizeof(timeKey), "t%08lx", static_cast<unsigned long>(hash));
    std::snprintf(bookmarkKey, sizeof(bookmarkKey), "b%08lx", static_cast<unsigned long>(hash));
    std::snprintf(favoriteKey, sizeof(favoriteKey), "f%08lx", static_cast<unsigned long>(hash));
    std::snprintf(openedKey, sizeof(openedKey), "o%08lx", static_cast<unsigned long>(hash));
    std::snprintf(tagKey, sizeof(tagKey), "g%08lx", static_cast<unsigned long>(hash));
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_i32(handle, progressKey, book.progressPercent);
    nvs_set_u8(handle, favoriteKey, book.favorite ? 1 : 0);
    nvs_set_i64(handle, openedKey, book.lastOpenedEpoch);
    nvs_set_str(handle, tagKey, book.tag.c_str());
    std::uint32_t oldSeconds = 0;
    nvs_get_u32(handle, timeKey, &oldSeconds);
    nvs_set_u32(handle, timeKey, oldSeconds + seconds);
    std::string bookmarks;
    for (std::size_t index = 0; index < book.bookmarks.size(); ++index) {
        if (index) bookmarks += ',';
        bookmarks += std::to_string(book.bookmarks[index]);
    }
    nvs_set_str(handle, bookmarkKey, bookmarks.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}

bool SettingsService::resetAll()
{
    if (!ready_) return false;
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const bool okay = nvs_erase_all(handle) == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return okay;
}

void SettingsService::addReadingSeconds(AppState& state, std::uint32_t seconds)
{
    if (seconds == 0) return;
    const auto currentDay = dayId();
    const auto currentMonth = monthId();
    // Keep the live UI correct even if NVS is temporarily unavailable. The
    // next successful save will persist the accumulated values.
    if (!ready_) {
        state.totalReadingSeconds += seconds;
        state.dailyReadingSeconds += seconds;
        state.monthlyReadingSeconds += seconds;
        return;
    }
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return;
    std::uint32_t savedDay = 0;
    std::uint32_t savedMonth = 0;
    nvs_get_u32(handle, "rd_dayid", &savedDay);
    nvs_get_u32(handle, "rd_monid", &savedMonth);
    if (savedDay != currentDay) state.dailyReadingSeconds = 0;
    if (savedMonth != currentMonth) state.monthlyReadingSeconds = 0;
    state.totalReadingSeconds += seconds;
    state.dailyReadingSeconds += seconds;
    state.monthlyReadingSeconds += seconds;
    nvs_set_u32(handle, "rd_total", state.totalReadingSeconds);
    nvs_set_u32(handle, "rd_dayid", currentDay);
    nvs_set_u32(handle, "rd_day", state.dailyReadingSeconds);
    nvs_set_u32(handle, "rd_monid", currentMonth);
    nvs_set_u32(handle, "rd_month", state.monthlyReadingSeconds);
    nvs_commit(handle);
    nvs_close(handle);
}

}  // namespace papers3
