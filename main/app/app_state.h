#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace papers3 {

enum class Page {
    Home,
    Bookshelf,
    Tools,
    Settings,
    Reader,
    Wifi,
    FileTransfer,
    Bluetooth,
    Display,
    Fonts,
    Wallpapers,
    Storage,
    Firmware,
    General,
    Calendar,
    Pomodoro,
    Timers,
    Todos,
    Album,
    FontTest,
    FileManager,
    Diagnostics,
};

enum class Theme {
    Light,
    Dark,
};

enum class InputMode {
    English,
    Pinyin,
    Symbols,
};

enum class SystemLanguage {
    Chinese,
    Vietnamese,
};

enum class Orientation {
    Portrait,
    Landscape,
};

enum class SystemOverlay {
    None,
    LockScreen,
    ControlCenter,
};

enum class ReaderPanel {
    None,
    Contents,
    Progress,
    Typography,
    FontPicker,
    PageTurnPicker,
};

struct BookInfo {
    std::string title;
    std::string author;
    std::string path;
    std::string type;
    int progressPercent = 0;
    bool favorite = false;
    std::vector<int> bookmarks;
    std::uint32_t readingSeconds = 0;
    std::int64_t lastOpenedEpoch = 0;
    std::string tag;
};

struct TodoItem {
    std::string text;
    bool done = false;
};

struct BluetoothDevice {
    std::string name;
    std::string address;
    int rssi = -100;
    std::uint8_t addressType = 0;
    bool connected = false;
};

struct RuntimeStatus {
    bool wifiEnabled = true;
    bool wifiConnected = false;
    bool bluetoothEnabled = false;
    bool bluetoothConnected = false;
    bool sdMounted = false;
    int batteryPercent = 78;
    std::string ssid;
    std::string ipAddress;
    std::string message;
};

struct ReaderSettings {
    int fontSize = 22;
    int marginLevel = 1;
    int lineHeightLevel = 1;
    std::string fontName = "System";
    std::string pageTurnMode = "swipe";
    bool autoPage = false;
    int autoPageSeconds = 8;
    bool autoRotate = false;
};

struct SystemSettings {
    Orientation orientation = Orientation::Portrait;
    bool statusBar = true;
    bool ntpEnabled = true;
    bool autoReconnect = true;
    bool lowPower = true;
    int sleepMinutes = 15;
    int powerOffHours = 2;
    int timezoneMinutes = 480;
    std::string systemFontId = "system";
    std::string wallpaperId = "paper-morning";
    std::string refreshMode = "text";
    bool randomLockWallpaper = true;
};

struct CountdownTimer {
    std::string id;
    std::string name;
    std::uint32_t durationSeconds = 0;
    std::uint32_t remainingSeconds = 0;
    bool running = false;
    std::int64_t endsAtEpoch = 0;
};

struct PomodoroState {
    bool focusMode = true;
    int focusMinutes = 25;
    int breakMinutes = 5;
    std::uint32_t remainingSeconds = 1500;
    bool running = false;
    std::int64_t endsAtEpoch = 0;
    int cycles = 0;
};

struct CalendarState {
    int year = 2026;
    int month = 8;
    int selectedDay = 10;
};

struct FileEntry {
    std::string name;
    std::string path;
    std::string type;
    std::uint64_t size = 0;
    bool directory = false;
};

struct AppState {
    Page page = Page::Home;
    Theme theme = Theme::Light;
    InputMode inputMode = InputMode::English;
    SystemLanguage language = SystemLanguage::Chinese;
    RuntimeStatus status;
    ReaderSettings reader;
    SystemSettings system;
    std::vector<BookInfo> books;
    std::vector<BluetoothDevice> bluetoothDevices;
    std::vector<CountdownTimer> timers;
    PomodoroState pomodoro;
    CalendarState calendar;
    std::vector<TodoItem> todos;
    std::vector<FileEntry> files;
    std::string searchText;
    std::string imeComposition;
    std::string wifiPassword;
    std::string selectedSsid;
    std::string transferSsid;
    std::string transferPassword;
    std::string transferUrl;
    int selectedBook = -1;
    int readerPage = 0;
    int readerPageCount = 0;
    std::string readerText;
    std::vector<std::string> readerChapterTitles;
    std::vector<int> readerChapterPages;
    bool readerChromeVisible = false;
    ReaderPanel readerPanel = ReaderPanel::None;
    SystemOverlay overlay = SystemOverlay::None;
    bool readerBookmarked = false;
    std::uint32_t autoPageLastMs = 0;
    std::string currentFolder = "/sdcard/books";
    int bookshelfFilter = 0;
    int bookshelfPage = 0;
    int bookshelfTagFilter = 0;
    int albumIndex = -1;
    bool pendingTodoAdd = false;
    int selectedBluetoothDevice = -1;
    int selectedTimer = 0;
    std::string otaStatus;
    std::string diagnosticsText;
    int selectedFile = -1;
    std::string editBuffer;
    std::string editPath;
    bool pendingFileRename = false;
    bool pendingTimerRename = false;
    std::uint32_t lastInteractionMs = 0;
    bool sleeping = false;
    std::uint32_t totalReadingSeconds = 0;
    std::uint32_t monthlyReadingSeconds = 0;
    std::uint32_t dailyReadingSeconds = 0;
    std::uint32_t lastFullRefreshMs = 0;
    bool needsFullRefresh = true;
};

const char* pageName(Page page);

}  // namespace papers3
