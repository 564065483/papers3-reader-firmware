#include <M5GFX.h>

#if defined(SDL_h_)

#if defined(_WIN32)
#include <windows.h>
#endif

#include "app_state.h"
#include "input_method.h"
#include "papers3_hal.h"
#include "ui_renderer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace papers3 {

namespace {
M5GFX gDisplay;
PaperS3Hal gHal;
UiRenderer gUi;
InputMethod gIme;
AppState gState;
std::vector<WifiNetwork> gNetworks;
bool gWasTouched = false;
bool gGalleryMode = false;
// Keep the default at the panel's native 540x960 pixels.  A fractional
// scale makes SDL interpolate every glyph and line, which is especially
// noticeable on desktop screenshots.
float gPreviewScale = 1.0f;

void seedPreviewState()
{
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    gState.status.wifiEnabled = true;
    gState.status.wifiConnected = true;
    gState.status.bluetoothEnabled = true;
    gState.status.bluetoothConnected = true;
    gState.status.batteryPercent = 78;
    gState.status.ssid = "Home_5G";
    gState.status.ipAddress = "192.168.1.106";
    gState.status.sdMounted = true;
    gState.totalReadingSeconds = 2 * 3600 + 12 * 60;
    gState.monthlyReadingSeconds = 1 * 3600 + 56 * 60;
    gState.dailyReadingSeconds = 18 * 60;
    gState.books = {
        {"明朝那些事儿", "当年明月", "/sdcard/books/ming.epub", "EPUB", 22, true, {}, 72 * 60, now},
        {"月光下的城市", "林知远", "/sdcard/books/moon.epub", "EPUB", 62, false, {}, 21 * 60, now - 3600},
        {"The Small Machine", "M. Ellis", "/sdcard/books/machine.epub", "EPUB", 44, false, {}, 9 * 60, now - 7200},
        {"바람의 기록", "Han Sujin", "/sdcard/books/wind.epub", "EPUB", 28, true, {}, 15 * 60, now - 10800},
        {"Những ngày yên tĩnh", "Minh Anh", "/sdcard/books/quiet.txt", "TXT", 14, false, {}, 6 * 60, now - 14400},
    };
    for (int i = 0; i < 10; ++i) {
        gState.books.push_back({"示例图书 " + std::to_string(i + 1),
                                "Paper S3",
                                "/sdcard/books/sample" + std::to_string(i) + ".epub",
                                "EPUB",
                                (i + 1) * 6,
                                i % 3 == 0,
                                {},
                                static_cast<std::uint32_t>((i + 1) * 120),
                                now - (i + 5) * 3600});
    }
    if (!gState.books.empty()) gState.books[0].tag = "进行中";
    if (gState.books.size() > 1) gState.books[1].tag = "待读";
    if (gState.books.size() > 2) gState.books[2].tag = "已完成";
    gState.todos = {{"导入一本 EPUB", false}, {"完成今日阅读", true}};
    gState.readerPage = 48;
    gState.readerPageCount = 252;
    gState.readerText =
        "第一章 从这里开始\n\n"
        "这是由固件 UiRenderer 直接绘制的阅读内容。\n"
        "它使用与开发板固件完全相同的坐标、字体和工具栏代码。\n"
        "点击屏幕中央可以显示或隐藏阅读工具栏。\n\n"
        "真机版会使用 EPUB 自带导航生成章节列表，也会记录每本书和每天的阅读时长。"
        "目录、进度、字体和翻页方式都可以打开测试。";
    gState.readerChapterTitles = {"第一章 从这里开始", "第二章 月光下的城市", "第三章 远方", "第四章 尾声"};
    gState.readerChapterPages = {0, 48, 126, 220};
    gState.timers = {
        {"tea", "泡茶", 300, 300, false, 0},
        {"reading", "阅读", 1500, 780, false, 0},
    };
    gState.files = {
        {"..", "/sdcard", "folder", 0, true},
        {"明朝那些事儿.epub", "/sdcard/books/ming.epub", "EPUB", 2621440, false},
        {"quiet.txt", "/sdcard/books/quiet.txt", "TXT", 48230, false},
    };
    gState.transferSsid = "PaperS3-Transfer";
    gState.transferPassword = "papers3s3";
    gState.transferUrl = "http://192.168.4.1";
    gState.diagnosticsText = "DISPLAY  PASS\nTOUCH    PASS\nSD CARD  PASS\nWI-FI    PASS\nBLE      PASS\nIMU      PASS";
    gNetworks = {{"Home_5G", -42}, {"PaperLab", -57}, {"Coffee_WiFi", -72}};
}

void saveScreenshot(const char* name)
{
    std::filesystem::create_directories("screenshots");
    size_t size = 0;
    void* png = gDisplay.createPng(&size, 0, 0, gDisplay.width(), gDisplay.height());
    if (!png || size == 0) return;
    const std::string path = std::string("screenshots/") + name;
    if (FILE* file = std::fopen(path.c_str(), "wb")) {
        std::fwrite(png, 1, size, file);
        std::fclose(file);
    }
    std::free(png);
}

void redraw(const char* screenshot = "last-screen.png")
{
    gHal.setOrientation(gState.system.orientation == Orientation::Landscape);
    gUi.draw(gState, gHal, gIme, gNetworks);
    saveScreenshot(screenshot);
}

void galleryShot(const char* name)
{
    std::printf("gallery: %s\n", name);
    std::fflush(stdout);
    gHal.setOrientation(gState.system.orientation == Orientation::Landscape);
    gUi.draw(gState, gHal, gIme, gNetworks);
    saveScreenshot(name);
    std::printf("saved: %s\n", name);
    std::fflush(stdout);
}

void openBook(int index)
{
    if (index >= 0 && index < static_cast<int>(gState.books.size())) gState.selectedBook = index;
    gState.page = Page::Reader;
    gState.readerChromeVisible = false;
    gState.readerPanel = ReaderPanel::None;
}

void handle(const UiEvent& event)
{
    static const Page nav[] = {Page::Home, Page::Bookshelf, Page::Tools, Page::Settings};
    static const Page tools[] = {Page::Calendar, Page::Pomodoro, Page::Timers, Page::Todos, Page::Album, Page::FontTest, Page::FileManager, Page::FileTransfer, Page::Diagnostics};
    static const Page settings[] = {Page::Wifi, Page::Bluetooth, Page::Display, Page::Fonts, Page::Wallpapers, Page::Storage, Page::Firmware, Page::General};
    switch (event.action) {
        case UiAction::Nav:
            if (event.index >= 0 && event.index < 4) gState.page = nav[event.index];
            break;
        case UiAction::ToolsItem:
            if (event.index >= 0 && event.index < 9) {
                gState.page = tools[event.index];
                if (gState.page == Page::Album) {
                    gState.files = {
                        {"paper-morning", "/sdcard/wallpapers/paper-morning.jpg", "JPG", 0, false},
                        {"ink-mountain", "/sdcard/wallpapers/ink-mountain.jpg", "JPG", 0, false},
                        {"minimal-clock", "/sdcard/wallpapers/minimal-clock.jpg", "JPG", 0, false},
                    };
                    gState.albumIndex = -1;
                }
            }
            break;
        case UiAction::SettingsItem:
            if (event.index >= 0 && event.index < 8) gState.page = settings[event.index];
            break;
        case UiAction::Back:
            gState.page = (gState.page == Page::Calendar || gState.page == Page::Pomodoro || gState.page == Page::Timers || gState.page == Page::Todos || gState.page == Page::Album ||
                           gState.page == Page::FontTest || gState.page == Page::FileManager || gState.page == Page::FileTransfer ||
                           gState.page == Page::Diagnostics)
                              ? Page::Tools
                              : Page::Settings;
            break;
        case UiAction::StatusLock:
            gState.overlay = SystemOverlay::LockScreen;
            break;
        case UiAction::StatusControl:
            gState.overlay = SystemOverlay::ControlCenter;
            break;
        case UiAction::OverlayClose:
            gState.overlay = SystemOverlay::None;
            break;
        case UiAction::ControlWifi:
            gState.status.wifiEnabled = !gState.status.wifiEnabled;
            gState.status.wifiConnected = gState.status.wifiEnabled;
            break;
        case UiAction::ControlBluetooth:
            gState.status.bluetoothEnabled = !gState.status.bluetoothEnabled;
            gState.status.bluetoothConnected = gState.status.bluetoothEnabled;
            break;
        case UiAction::WifiToggle:
            gState.status.wifiEnabled = !gState.status.wifiEnabled;
            gState.status.wifiConnected = gState.status.wifiEnabled;
            break;
        case UiAction::WifiRefresh:
            gState.status.message = "Wi-Fi 扫描完成";
            break;
        case UiAction::WifiNetwork:
            if (event.index >= 0 && event.index < static_cast<int>(gNetworks.size())) {
                gState.selectedSsid = gNetworks[event.index].ssid;
                gState.status.message = "已选择 " + gState.selectedSsid;
            }
            break;
        case UiAction::WifiConnect:
            gState.status.wifiConnected = !gState.status.wifiConnected;
            gState.status.ssid = gState.status.wifiConnected ? gState.selectedSsid : "";
            break;
        case UiAction::BookOpen:
            openBook(event.index);
            break;
        case UiAction::BookFavorite:
            if (event.index >= 0 && event.index < static_cast<int>(gState.books.size())) gState.books[event.index].favorite = !gState.books[event.index].favorite;
            break;
        case UiAction::BookTag:
            if (event.index >= 0 && event.index < static_cast<int>(gState.books.size())) {
                static const char* tags[] = {"", "待读", "进行中", "已完成"};
                int current = 0;
                for (int i = 0; i < 4; ++i) if (gState.books[event.index].tag == tags[i]) current = i;
                gState.books[event.index].tag = tags[(current + 1) % 4];
            }
            break;
        case UiAction::BooksFilter:
            gState.bookshelfFilter = event.index;
            gState.bookshelfPage = 0;
            break;
        case UiAction::BooksTagFilter:
            gState.bookshelfTagFilter = (gState.bookshelfTagFilter + 1) % 4;
            gState.bookshelfPage = 0;
            break;
        case UiAction::BooksPrevious:
            gState.bookshelfPage = std::max(0, gState.bookshelfPage - 1);
            break;
        case UiAction::BooksNext:
            ++gState.bookshelfPage;
            break;
        case UiAction::ReaderToggleChrome:
            if (!gState.readerChromeVisible && gState.reader.pageTurnMode == "tap" && event.index < 0) {
                // The native preview cannot carry the raw x coordinate in its
                // compact event path, so the centre target still exercises
                // the chrome toggle. Physical firmware handles side taps.
                gState.readerChromeVisible = true;
            } else {
                gState.readerChromeVisible = !gState.readerChromeVisible;
                if (!gState.readerChromeVisible) gState.readerPanel = ReaderPanel::None;
            }
            break;
        case UiAction::ReaderBack:
            gState.page = Page::Bookshelf;
            gState.readerChromeVisible = false;
            break;
        case UiAction::ReaderAutoPage:
            gState.reader.autoPage = !gState.reader.autoPage;
            break;
        case UiAction::ReaderBookmark:
            gState.readerBookmarked = !gState.readerBookmarked;
            break;
        case UiAction::ReaderTurnPicker:
            gState.readerPanel = ReaderPanel::PageTurnPicker;
            break;
        case UiAction::ReaderContents:
            gState.readerPanel = ReaderPanel::Contents;
            break;
        case UiAction::ReaderProgress:
            gState.readerPanel = ReaderPanel::Progress;
            break;
        case UiAction::ReaderTypography:
            gState.readerPanel = ReaderPanel::Typography;
            break;
        case UiAction::ReaderFontPicker:
            gState.readerPanel = ReaderPanel::FontPicker;
            break;
        case UiAction::ReaderFontChoice: {
            static const char* fonts[] = {"System", "Regular", "Bold", "Italic"};
            if (event.index >= 0 && event.index < 4) gState.reader.fontName = fonts[event.index];
            gState.readerPanel = ReaderPanel::Typography;
            break;
        }
        case UiAction::ReaderTurnMode: {
            static const char* modes[] = {"swipe", "tap", "simulation", "tilt"};
            if (event.index >= 0 && event.index < 4) gState.reader.pageTurnMode = modes[event.index];
            gState.readerPanel = ReaderPanel::None;
            break;
        }
        case UiAction::BluetoothToggle:
            gState.status.bluetoothEnabled = !gState.status.bluetoothEnabled;
            break;
        case UiAction::BluetoothScan:
            break;
        case UiAction::CalendarPrevious:
            if (--gState.calendar.month < 1) { gState.calendar.month = 12; --gState.calendar.year; }
            break;
        case UiAction::CalendarNext:
            if (++gState.calendar.month > 12) { gState.calendar.month = 1; ++gState.calendar.year; }
            break;
        case UiAction::CalendarDay:
            gState.calendar.selectedDay = event.index;
            break;
        case UiAction::PomodoroMode:
            gState.pomodoro.focusMode = !gState.pomodoro.focusMode;
            gState.pomodoro.running = false;
            gState.pomodoro.remainingSeconds = static_cast<std::uint32_t>((gState.pomodoro.focusMode ? gState.pomodoro.focusMinutes : gState.pomodoro.breakMinutes) * 60);
            break;
        case UiAction::PomodoroStartPause:
            gState.pomodoro.running = !gState.pomodoro.running;
            break;
        case UiAction::PomodoroReset:
            gState.pomodoro.running = false;
            gState.pomodoro.remainingSeconds = static_cast<std::uint32_t>((gState.pomodoro.focusMode ? gState.pomodoro.focusMinutes : gState.pomodoro.breakMinutes) * 60);
            break;
        case UiAction::TimerSelect:
            gState.selectedTimer = event.index;
            break;
        case UiAction::TimerStartPause:
            if (gState.selectedTimer >= 0 && gState.selectedTimer < static_cast<int>(gState.timers.size())) gState.timers[gState.selectedTimer].running = !gState.timers[gState.selectedTimer].running;
            break;
        case UiAction::TimerReset:
            if (gState.selectedTimer >= 0 && gState.selectedTimer < static_cast<int>(gState.timers.size())) gState.timers[gState.selectedTimer].remainingSeconds = gState.timers[gState.selectedTimer].durationSeconds;
            break;
        case UiAction::TimerAdd:
            if (gState.timers.size() < 4) gState.timers.push_back({"preview-timer", "新计时器", 300, 300, false, 0});
            break;
        case UiAction::TimerMinus:
            if (gState.selectedTimer >= 0 && gState.selectedTimer < static_cast<int>(gState.timers.size())) gState.timers[gState.selectedTimer].durationSeconds = std::max<std::uint32_t>(60, gState.timers[gState.selectedTimer].durationSeconds - 60);
            break;
        case UiAction::TimerPlus:
            if (gState.selectedTimer >= 0 && gState.selectedTimer < static_cast<int>(gState.timers.size())) gState.timers[gState.selectedTimer].durationSeconds += 60;
            break;
        case UiAction::TimerDelete:
            if (gState.selectedTimer >= 0 && gState.selectedTimer < static_cast<int>(gState.timers.size())) gState.timers.erase(gState.timers.begin() + gState.selectedTimer);
            gState.selectedTimer = std::max(0, std::min(gState.selectedTimer, static_cast<int>(gState.timers.size()) - 1));
            break;
        case UiAction::TodoToggle:
            if (event.index >= 0 && event.index < static_cast<int>(gState.todos.size())) gState.todos[event.index].done = !gState.todos[event.index].done;
            break;
        case UiAction::TodoAdd:
            if (gState.todos.size() < 6) gState.todos.push_back({"新的待办", false});
            break;
        case UiAction::TodoDelete:
            if (event.index >= 0 && event.index < static_cast<int>(gState.todos.size())) gState.todos.erase(gState.todos.begin() + event.index);
            break;
        case UiAction::FileSelect:
            gState.selectedFile = event.index;
            break;
        case UiAction::FileOpen:
            if (gState.selectedFile >= 0 && gState.selectedFile < static_cast<int>(gState.files.size())) {
                const auto& file = gState.files[gState.selectedFile];
                if (file.directory) gState.currentFolder = file.path;
                else openBook(0);
            }
            break;
        case UiAction::FileRename:
            gState.status.message = "预览中已模拟重命名入口";
            break;
        case UiAction::FileDelete:
            gState.status.message = "预览中已模拟删除入口";
            break;
        case UiAction::FileUp:
            gState.currentFolder = "/sdcard";
            break;
        case UiAction::AlbumSelect:
            if (event.index >= 0 && event.index < static_cast<int>(gState.files.size())) gState.albumIndex = event.index;
            break;
        case UiAction::StorageRefresh:
            gState.status.message = "存储卡已刷新";
            break;
        case UiAction::StorageOpenManager:
            gState.page = Page::FileManager;
            break;
        case UiAction::FirmwareInstall:
            gState.otaStatus = "预览中已模拟安装确认";
            break;
        case UiAction::DiagnosticsRun:
            gState.diagnosticsText = "显示与触控：正常\n存储卡：正常\nWi-Fi：正常\n蓝牙：正常\n姿态传感器：正常";
            break;
        case UiAction::TransferToggle:
            gState.transferUrl = gState.transferUrl.empty() ? "http://192.168.4.1" : "";
            break;
        case UiAction::SettingChoice:
            if (gState.page == Page::Display && event.index == 0) gState.theme = Theme::Light;
            else if (gState.page == Page::Display && event.index == 1) gState.theme = Theme::Dark;
            else if (gState.page == Page::Display && event.index == 2) gState.system.orientation = Orientation::Portrait;
            else if (gState.page == Page::Display && event.index == 3) { gState.system.orientation = Orientation::Portrait; gState.status.message = "本机界面固定竖屏"; }
            else if (gState.page == Page::Display && event.index == 5) gState.system.sleepMinutes = 5;
            else if (gState.page == Page::Display && event.index == 6) gState.system.sleepMinutes = 15;
            else if (gState.page == Page::Display && event.index == 7) gState.system.powerOffHours = 2;
            else if (gState.page == Page::Display && event.index == 8) gState.reader.autoRotate = !gState.reader.autoRotate;
            else if (gState.page == Page::Fonts && event.index < 3) gState.system.systemFontId = event.index == 0 ? "system" : event.index == 1 ? "builtin-bold" : "builtin-italic";
            else if (gState.page == Page::Wallpapers && event.index < 3) gState.system.wallpaperId = event.index == 0 ? "paper-morning" : event.index == 1 ? "ink-mountain" : "minimal-clock";
            else if (gState.page == Page::Wallpapers && event.index == 4) gState.system.randomLockWallpaper = !gState.system.randomLockWallpaper;
            else if (gState.page == Page::General && event.index == 0) gState.language = SystemLanguage::Chinese;
            else if (gState.page == Page::General && event.index == 1) gState.language = SystemLanguage::Vietnamese;
            break;
        default:
            break;
    }
    redraw();
}
}  // namespace

M5GFX& PaperS3Hal::display() { return gDisplay; }
void PaperS3Hal::setTextRefresh() {}
void PaperS3Hal::setOrientation(bool landscape)
{
    gDisplay.setRotation(landscape ? 1 : 0);
    if (auto* panel = static_cast<lgfx::Panel_sdl*>(gDisplay.panel())) panel->setFrameRotation(landscape ? 0 : 1);
}
PaperS3Hal& hal() { return gHal; }

void setupPreview()
{
    gDisplay.init();
    gHal.setOrientation(false);
    auto* panel = static_cast<lgfx::Panel_sdl*>(gDisplay.panel());
    panel->setScaling(gPreviewScale, gPreviewScale);
    seedPreviewState();
    redraw("firmware-home.png");
}

void enableGalleryMode() { gGalleryMode = true; }
bool galleryMode() { return gGalleryMode; }

void setPreviewScale(float scale)
{
    gPreviewScale = std::clamp(scale, 0.55f, 1.25f);
}

void createGallery()
{
    gState.overlay = SystemOverlay::None;
    gState.theme = Theme::Light;
    gState.page = Page::Home;
    galleryShot("01-home-light.png");
    gState.page = Page::Bookshelf;
    galleryShot("02-bookshelf.png");
    gState.page = Page::Tools;
    galleryShot("03-tools.png");
    gState.page = Page::Settings;
    galleryShot("04-settings.png");
    gState.page = Page::Reader;
    gState.readerChromeVisible = false;
    gState.readerPanel = ReaderPanel::None;
    galleryShot("05-reader-fullscreen.png");
    gState.readerChromeVisible = true;
    galleryShot("06-reader-toolbar.png");
    gState.system.orientation = Orientation::Landscape;
    galleryShot("06b-reader-landscape.png");
    gState.system.orientation = Orientation::Portrait;
    gState.readerPanel = ReaderPanel::Progress;
    galleryShot("07-reader-progress.png");
    gState.page = Page::Home;
    gState.readerPanel = ReaderPanel::None;
    gState.overlay = SystemOverlay::LockScreen;
    galleryShot("08-lock-screen.png");
    gState.system.wallpaperId = "ink-mountain";
    galleryShot("08b-lock-ink-mountain.png");
    gState.system.wallpaperId = "minimal-clock";
    galleryShot("08c-lock-minimal-clock.png");
    gState.system.wallpaperId = "paper-morning";
    gState.overlay = SystemOverlay::ControlCenter;
    galleryShot("09-control-center.png");
    gState.overlay = SystemOverlay::None;
    gState.theme = Theme::Dark;
    galleryShot("10-home-dark.png");
    gState.theme = Theme::Light;
    gState.page = Page::Calendar;
    galleryShot("11-calendar.png");
    gState.page = Page::Pomodoro;
    galleryShot("12-pomodoro.png");
    gState.page = Page::Timers;
    galleryShot("13-timers.png");
    gState.page = Page::Todos;
    galleryShot("13b-todos.png");
    gState.page = Page::Album;
    gState.files = {
        {"paper-morning", "/sdcard/wallpapers/paper-morning.jpg", "JPG", 0, false},
        {"ink-mountain", "/sdcard/wallpapers/ink-mountain.jpg", "JPG", 0, false},
        {"minimal-clock", "/sdcard/wallpapers/minimal-clock.jpg", "JPG", 0, false},
    };
    gState.albumIndex = 0;
    galleryShot("13c-album.png");
    gState.page = Page::FileManager;
    galleryShot("14-file-manager.png");
    gState.page = Page::FileTransfer;
    galleryShot("15-file-transfer.png");
    gState.page = Page::Diagnostics;
    galleryShot("16-diagnostics.png");
    gState.page = Page::Storage;
    galleryShot("17-storage.png");
    gState.page = Page::Fonts;
    galleryShot("18-fonts.png");
    gState.page = Page::Display;
    galleryShot("19-display.png");
    gState.page = Page::General;
    galleryShot("20-general.png");
    gState.language = SystemLanguage::Vietnamese;
    gState.page = Page::Home;
    galleryShot("21-home-vietnamese.png");
    gState.page = Page::Settings;
    galleryShot("22-settings-vietnamese.png");
    gState.language = SystemLanguage::Chinese;
}

void loopPreview()
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    const bool touched = gDisplay.getTouch(&x, &y);
    if (touched && !gWasTouched) handle(gUi.hitTest(x, y));
    gWasTouched = touched;
    SDL_Delay(12);
}

}  // namespace papers3

int user_func(bool* running)
{
    papers3::setupPreview();
    if (papers3::galleryMode()) {
        papers3::createGallery();
        *running = false;
        std::exit(0);
    }
    while (*running) papers3::loopPreview();
    return 0;
}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    // Prevent Windows from bitmap-scaling the SDL window on 125%/150% DPI
    // displays.  The preview should remain a crisp, pixel-accurate device
    // frame instead of a softened desktop thumbnail.
    SetProcessDPIAware();
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--gallery") {
            papers3::enableGalleryMode();
        } else if (arg.rfind("--scale=", 0) == 0) {
            papers3::setPreviewScale(std::strtof(arg.substr(8).c_str(), nullptr));
        }
    }
    return lgfx::Panel_sdl::main(user_func, 16);
}

#endif
