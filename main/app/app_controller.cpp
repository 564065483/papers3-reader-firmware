#include "app_controller.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <iterator>

namespace papers3 {

namespace {
const Page kSettingPages[] = {Page::Wifi, Page::Bluetooth, Page::Display, Page::Fonts, Page::Wallpapers, Page::Storage, Page::Firmware, Page::General};
const Page kToolPages[] = {Page::Calendar, Page::Pomodoro, Page::Timers, Page::Todos, Page::Album, Page::FontTest, Page::FileManager, Page::FileTransfer, Page::Diagnostics};
const char* kReaderFonts[] = {"System", "Regular", "Bold", "Italic"};
const char* kTurnModes[] = {"swipe", "tap", "simulation", "tilt"};

void cycleBookTag(BookInfo& book)
{
    static const char* tags[] = {"", "待读", "进行中", "已完成"};
    int next = 0;
    for (int i = 0; i < 4; ++i) if (book.tag == tags[i]) next = (i + 1) % 4;
    book.tag = tags[next];
}

void chooseRandomLockWallpaper(SystemSettings& settings)
{
    if (!settings.randomLockWallpaper) return;
    static const char* wallpapers[] = {"paper-morning", "ink-mountain", "minimal-clock"};
    const auto seed = static_cast<unsigned>(std::time(nullptr));
    settings.wallpaperId = wallpapers[seed % 3];
}
}

void AppController::init()
{
    hal().init();
    settings_.init();
    settings_.load(state_);
    char timezone[20] {};
    std::snprintf(timezone, sizeof(timezone), "UTC-%d", state_.system.timezoneMinutes / 60);
    setenv("TZ", timezone, 1);
    tzset();
    // The Paper S3 UI is intentionally authored for the 540×960 portrait
    // panel. Do not let a stale NVS value rotate the hardware into a clipped
    // 960×540 canvas before a dedicated landscape layout exists.
    state_.system.orientation = Orientation::Portrait;
    hal().setOrientation(false);
    hal().setWifiLowPower(state_.system.lowPower);
    ota_.confirmRunningImage();
    files_.ensureDirectories();
    timers_.init(state_);
    device_.init(state_);
    refreshBooks();
    state_.lastInteractionMs = hal().millis();

    std::string savedSsid;
    std::string savedPassword;
    if (state_.system.autoReconnect && settings_.loadWifi(savedSsid, savedPassword)) {
        state_.selectedSsid = savedSsid;
        state_.wifiPassword = savedPassword;
        wifi_.connect(state_, hal(), savedSsid, savedPassword);
        hal().setWifiLowPower(state_.system.lowPower);
    }
    redraw(true);
}

void AppController::refreshBooks()
{
    storage_.refresh(state_, hal());
    for (auto& book : state_.books) settings_.loadBook(book);
}

void AppController::update()
{
    hal().update();
    const auto now = hal().millis();
    state_.status.batteryPercent = hal().batteryPercent();
    if (state_.status.wifiEnabled) wifi_.syncStatus(state_, hal());
    if (state_.status.wifiEnabled && state_.system.autoReconnect && !state_.status.wifiConnected && !state_.selectedSsid.empty() && !transfer_.running() && now - lastReconnectMs_ > 60000) {
        lastReconnectMs_ = now;
        wifi_.connect(state_, hal(), state_.selectedSsid, state_.wifiPassword);
        hal().setWifiLowPower(state_.system.lowPower);
    }

    if (timers_.update(state_)) { device_.beep(); settings_.saveUi(state_); redraw(true); }
    if ((state_.page == Page::Timers || state_.page == Page::Pomodoro) && now - lastTimerDrawMs_ >= 1000) {
        lastTimerDrawMs_ = now;
        redraw(true);
    }

    if (state_.page == Page::Reader && reader_.isOpen()) {
        const auto seconds = reader_.consumeSessionSeconds(now);
        if (seconds) {
            settings_.addReadingSeconds(state_, seconds);
            if (state_.selectedBook >= 0 && state_.selectedBook < static_cast<int>(state_.books.size())) {
                state_.books[state_.selectedBook].readingSeconds += seconds;
                settings_.saveBook(state_.books[state_.selectedBook], seconds);
            }
        }
        if (state_.reader.autoPage && now - state_.autoPageLastMs >= static_cast<std::uint32_t>(state_.reader.autoPageSeconds * 1000)) {
            if (reader_.nextPage()) syncReaderState();
            state_.autoPageLastMs = now;
            redraw(true);
        }
        const int tilt = device_.updateTiltPageTurn(now, state_.reader.pageTurnMode == "tilt");
        if (tilt != 0) {
            tilt > 0 ? reader_.nextPage() : reader_.previousPage();
            syncReaderState();
            redraw(true);
        }
        const int orientation = device_.updateOrientation(now, state_.reader.autoRotate);
        if (orientation >= 0) {
            state_.system.orientation = orientation == 1 ? Orientation::Landscape : Orientation::Portrait;
            hal().setOrientation(orientation == 1);
            redraw(true);
        }
    }

    if (state_.pendingFileRename && !ime_.isOpen()) {
        std::string error;
        state_.status.message = files_.renameEntry(state_.editPath, state_.editBuffer, error) ? "重命名成功" : error;
        state_.pendingFileRename = false;
        files_.list(state_, state_.currentFolder);
        refreshBooks();
        redraw(true);
    }
    if (state_.pendingTimerRename && !ime_.isOpen()) {
        if (state_.selectedTimer >= 0 && state_.selectedTimer < static_cast<int>(state_.timers.size()) && !state_.editBuffer.empty()) state_.timers[state_.selectedTimer].name = state_.editBuffer;
        state_.pendingTimerRename = false;
        saveUi();
        redraw(true);
    }
    if (state_.pendingTodoAdd && !ime_.isOpen()) {
        if (!state_.editBuffer.empty() && state_.todos.size() < 6) {
            state_.todos.push_back({state_.editBuffer, false});
            state_.status.message = "待办已添加";
        }
        state_.editBuffer.clear();
        state_.pendingTodoAdd = false;
        saveUi();
        redraw(true);
    }

    const auto point = hal().touch();
    if (point.pressed) {
        touchLastX_ = point.x;
        touchLastY_ = point.y;
        if (!wasPressed_) {
            touchStartX_ = point.x;
            touchStartY_ = point.y;
            touchStartMs_ = now;
            state_.lastInteractionMs = now;
            handleTouch(point);
        } else {
            const auto event = ui_.hitTest(point.x, point.y);
            if (event.action == UiAction::ReaderPageSlider || event.action == UiAction::ReaderFontSize || event.action == UiAction::ReaderMargin || event.action == UiAction::ReaderLineHeight) handleEvent(event, point);
        }
    } else if (wasPressed_) {
        handleGesture(touchLastX_, touchLastY_, now - touchStartMs_);
    }
    wasPressed_ = point.pressed;

    if (!state_.sleeping && state_.overlay == SystemOverlay::None && state_.system.sleepMinutes > 0 && now - state_.lastInteractionMs > static_cast<std::uint32_t>(state_.system.sleepMinutes) * 60000U) {
        state_.sleeping = true;
        device_.lightSleep(hal());
        state_.sleeping = false;
        state_.lastInteractionMs = hal().millis();
        redraw(true);
    }
    if (state_.system.powerOffHours > 0 && now - state_.lastInteractionMs > static_cast<std::uint32_t>(state_.system.powerOffHours) * 3600000U) {
        chooseRandomLockWallpaper(state_.system);
        state_.overlay = SystemOverlay::LockScreen;
        redraw(true);
        hal().delayMs(800);
        device_.powerOff();
    }
    if ((state_.page != Page::Reader || state_.overlay == SystemOverlay::LockScreen) && now - lastDrawMs_ > 30000) redraw();
    hal().delayMs(8);
}

void AppController::syncReaderState()
{
    state_.readerPage = reader_.pageIndex();
    state_.readerPageCount = reader_.pageCount();
    state_.readerText = reader_.pageText();
    state_.readerChapterTitles = reader_.chapterTitles();
    state_.readerChapterPages = reader_.chapterPages();
    state_.readerBookmarked = false;
    if (state_.selectedBook >= 0 && state_.selectedBook < static_cast<int>(state_.books.size())) {
        auto& book = state_.books[state_.selectedBook];
        book.progressPercent = reader_.progressPercent();
        state_.readerBookmarked = std::find(book.bookmarks.begin(), book.bookmarks.end(), state_.readerPage) != book.bookmarks.end();
        settings_.saveBook(book, 0);
    }
}

void AppController::leaveReader()
{
    syncReaderState();
    reader_.close();
    state_.readerText.clear();
    state_.readerChromeVisible = false;
    state_.readerPanel = ReaderPanel::None;
    state_.page = Page::Bookshelf;
}

void AppController::openBook(int index)
{
    if (index < 0 || index >= static_cast<int>(state_.books.size())) return;
    state_.selectedBook = index;
    state_.books[index].lastOpenedEpoch = static_cast<std::int64_t>(std::time(nullptr));
    settings_.saveBook(state_.books[index], 0);
    reader_.setLayout(state_.reader.fontSize, state_.reader.marginLevel, state_.reader.lineHeightLevel);
    if (reader_.open(state_.books[index], state_.books[index].progressPercent)) {
        state_.page = Page::Reader;
        state_.readerChromeVisible = false;
        state_.readerPanel = ReaderPanel::None;
        state_.autoPageLastMs = hal().millis();
        syncReaderState();
    } else state_.status.message = reader_.error();
}

void AppController::openPage(Page page)
{
    state_.page = page;
    if (page == Page::Bookshelf) refreshBooks();
    if (page == Page::Bluetooth && !bluetooth_.ready()) bluetooth_.init(state_);
    if (page == Page::FileManager) { state_.selectedFile = -1; files_.list(state_, state_.currentFolder); }
    if (page == Page::Album) { state_.selectedFile = -1; state_.albumIndex = -1; files_.list(state_, "/sdcard/wallpapers"); }
    if (page == Page::Wifi) { state_.status.message = "正在扫描…"; redraw(true); wifi_.scan(hal()); state_.status.message = wifi_.networks().empty() ? "没有找到 Wi-Fi" : "请选择网络"; }
}

void AppController::handleTouch(const TouchPoint& point)
{
    if (ime_.isOpen()) {
        if (ui_.keyboardBackspaceHit(point.x, point.y)) ime_.backspace();
        else if (ui_.keyboardClearHit(point.x, point.y)) ime_.clear();
        else if (ui_.keyboardEnterHit(point.x, point.y)) ime_.close();
        else if (ui_.keyboardGlobeHit(point.x, point.y)) ime_.setMode(ime_.mode() == InputMode::Pinyin ? InputMode::English : InputMode::Pinyin);
        else if (ui_.keyboardSymbolsHit(point.x, point.y)) ime_.setMode(ime_.mode() == InputMode::Symbols ? InputMode::English : InputMode::Symbols);
        else if (ui_.keyboardSpaceHit(point.x, point.y)) ime_.key(" ");
        else {
            const int candidate = ui_.candidateIndex(point.x, point.y);
            if (candidate >= 0) { const auto list = ime_.candidates(); if (candidate < static_cast<int>(list.size())) ime_.commitCandidate(list[candidate]); }
            else { const int key = ui_.keyboardKeyIndex(point.x, point.y); static const char* keys = "qwertyuiopasdfghjklzxcvbnm"; if (key >= 0 && key < 26) ime_.key(std::string(1, keys[key])); }
        }
        redraw(true);
        return;
    }
    const auto event = ui_.hitTest(point.x, point.y);
    handleEvent(event, point);
}

void AppController::handleEvent(const UiEvent& event, const TouchPoint& point)
{
    if (state_.overlay == SystemOverlay::LockScreen) return;
    if (state_.overlay == SystemOverlay::ControlCenter) {
        if (event.action == UiAction::OverlayClose) state_.overlay = SystemOverlay::None;
        else if (event.action == UiAction::ControlWifi) {
            state_.status.wifiEnabled = !state_.status.wifiEnabled;
            hal().setWifiEnabled(state_.status.wifiEnabled);
            if (!state_.status.wifiEnabled) { state_.status.wifiConnected = false; state_.status.ssid.clear(); }
            else if (state_.system.autoReconnect && !state_.selectedSsid.empty()) { wifi_.connect(state_, hal(), state_.selectedSsid, state_.wifiPassword); hal().setWifiLowPower(state_.system.lowPower); }
        } else if (event.action == UiAction::ControlBluetooth) {
            if (state_.status.bluetoothEnabled) { bluetooth_.disconnect(state_); state_.status.bluetoothEnabled = false; }
            else state_.status.bluetoothEnabled = bluetooth_.init(state_) || bluetooth_.ready();
        }
        redraw(true); return;
    }

    switch (event.action) {
        case UiAction::Nav: openPage(event.index == 0 ? Page::Home : event.index == 1 ? Page::Bookshelf : event.index == 2 ? Page::Tools : Page::Settings); break;
        case UiAction::Back:
            if (state_.page == Page::Wifi || state_.page == Page::Bluetooth || state_.page == Page::Display || state_.page == Page::Fonts || state_.page == Page::Wallpapers || state_.page == Page::Storage || state_.page == Page::Firmware || state_.page == Page::General) state_.page = Page::Settings;
            else state_.page = Page::Tools;
            break;
        case UiAction::StatusLock: chooseRandomLockWallpaper(state_.system); state_.overlay = SystemOverlay::LockScreen; break;
        case UiAction::StatusControl: state_.overlay = SystemOverlay::ControlCenter; break;
        case UiAction::Search: ime_.setMode(InputMode::Pinyin); ime_.open(&state_.searchText); break;
        case UiAction::BooksFilter: state_.bookshelfFilter = event.index; state_.bookshelfPage = 0; break;
        case UiAction::BooksTagFilter: state_.bookshelfTagFilter = (state_.bookshelfTagFilter + 1) % 4; state_.bookshelfPage = 0; break;
        case UiAction::BooksPrevious: if (state_.bookshelfPage > 0) --state_.bookshelfPage; break;
        case UiAction::BooksNext: ++state_.bookshelfPage; break;
        case UiAction::BookOpen: openBook(event.index); break;
        case UiAction::BookFavorite:
            if (event.index >= 0 && event.index < static_cast<int>(state_.books.size())) { state_.books[event.index].favorite = !state_.books[event.index].favorite; settings_.saveBook(state_.books[event.index], 0); }
            break;
        case UiAction::BookTag:
            if (event.index >= 0 && event.index < static_cast<int>(state_.books.size())) { cycleBookTag(state_.books[event.index]); settings_.saveBook(state_.books[event.index], 0); }
            break;
        case UiAction::SettingsItem: if (event.index >= 0 && event.index < 8) openPage(kSettingPages[event.index]); break;
        case UiAction::ToolsItem: if (event.index >= 0 && event.index < static_cast<int>(std::size(kToolPages))) openPage(kToolPages[event.index]); break;
        case UiAction::WifiRefresh: state_.status.message = "正在扫描…"; redraw(true); wifi_.scan(hal()); state_.status.message = wifi_.networks().empty() ? "没有找到 Wi-Fi" : "请选择网络"; break;
        case UiAction::WifiToggle:
            state_.status.wifiEnabled = !state_.status.wifiEnabled;
            hal().setWifiEnabled(state_.status.wifiEnabled);
            if (!state_.status.wifiEnabled) {
                wifi_.disconnect(state_, hal());
                state_.selectedSsid.clear();
                state_.wifiPassword.clear();
            } else if (state_.system.autoReconnect && !state_.selectedSsid.empty()) {
                wifi_.connect(state_, hal(), state_.selectedSsid, state_.wifiPassword);
                hal().setWifiLowPower(state_.system.lowPower);
            }
            break;
        case UiAction::WifiNetwork:
            if (event.index >= 0 && event.index < static_cast<int>(wifi_.networks().size())) { state_.selectedSsid = wifi_.networks()[event.index].ssid; state_.wifiPassword.clear(); }
            break;
        case UiAction::WifiPassword: ime_.setMode(InputMode::English); ime_.open(&state_.wifiPassword); break;
        case UiAction::WifiConnect:
            if (state_.status.wifiConnected) { wifi_.disconnect(state_, hal()); settings_.clearWifi(); state_.selectedSsid.clear(); state_.wifiPassword.clear(); }
            else if (state_.selectedSsid.empty()) state_.status.message = "请先选择 Wi-Fi";
            else if (wifi_.connect(state_, hal(), state_.selectedSsid, state_.wifiPassword)) { settings_.saveWifi(state_.selectedSsid, state_.wifiPassword); hal().setWifiLowPower(state_.system.lowPower); }
            break;
        case UiAction::BluetoothToggle:
            if (state_.status.bluetoothEnabled) { bluetooth_.disconnect(state_); state_.status.bluetoothEnabled = false; }
            else state_.status.bluetoothEnabled = bluetooth_.init(state_) || bluetooth_.ready();
            break;
        case UiAction::BluetoothScan: state_.status.bluetoothEnabled = bluetooth_.init(state_) || bluetooth_.ready(); state_.status.message = "正在扫描蓝牙设备…"; redraw(true); bluetooth_.scan(state_); break;
        case UiAction::BluetoothDevice:
            if (event.index >= 0 && event.index < static_cast<int>(state_.bluetoothDevices.size())) { if (state_.bluetoothDevices[event.index].connected) bluetooth_.disconnect(state_); else bluetooth_.connect(state_, event.index); }
            break;
        case UiAction::SettingChoice:
            if (state_.page == Page::Display) {
                if (event.index == 0) state_.theme = Theme::Light;
                else if (event.index == 1) state_.theme = Theme::Dark;
                else if (event.index == 2) { state_.system.orientation = Orientation::Portrait; hal().setOrientation(false); }
                else if (event.index == 3) { state_.system.orientation = Orientation::Portrait; state_.status.message = "本机界面固定竖屏"; hal().setOrientation(false); }
                else if (event.index == 4) state_.system.statusBar = !state_.system.statusBar;
                else if (event.index == 5) state_.system.sleepMinutes = 5;
                else if (event.index == 6) state_.system.sleepMinutes = 15;
                else if (event.index == 7) state_.system.powerOffHours = 2;
                else if (event.index == 8) state_.reader.autoRotate = !state_.reader.autoRotate;
            } else if (state_.page == Page::Fonts) {
                const char* ids[] = {"system", "builtin-bold", "builtin-italic"};
                if (event.index >= 0 && event.index < 3) state_.system.systemFontId = ids[event.index];
                else if (event.index == 3) { const auto path = files_.firstFile("/sdcard/fonts", {".vlw"}); if (path.empty()) state_.status.message = "字体目录中没有 VLW 文件"; else state_.system.systemFontId = path; }
            } else if (state_.page == Page::Wallpapers) {
                const char* ids[] = {"paper-morning", "ink-mountain", "minimal-clock"};
                if (event.index >= 0 && event.index < 3) state_.system.wallpaperId = ids[event.index];
                else if (event.index == 3) { const auto path = files_.firstFile("/sdcard/wallpapers", {".jpg", ".jpeg", ".png"}); if (path.empty()) state_.status.message = "壁纸目录中没有图片"; else state_.system.wallpaperId = path; }
                else if (event.index == 4) state_.system.randomLockWallpaper = !state_.system.randomLockWallpaper;
            } else if (state_.page == Page::General) {
                if (event.index == 0) state_.language = SystemLanguage::Chinese;
                else if (event.index == 1) state_.language = SystemLanguage::Vietnamese;
                else if (event.index == 2) { state_.system.ntpEnabled = !state_.system.ntpEnabled; wifi_.applyTimeSettings(state_); }
                else if (event.index == 3) state_.system.autoReconnect = !state_.system.autoReconnect;
                else if (event.index == 4) { state_.system.lowPower = !state_.system.lowPower; hal().setWifiLowPower(state_.system.lowPower); }
                else if (event.index >= 5 && event.index <= 7) { state_.system.timezoneMinutes = 420 + (event.index - 5) * 60; char zone[20] {}; std::snprintf(zone, sizeof(zone), "UTC-%d", state_.system.timezoneMinutes / 60); setenv("TZ", zone, 1); tzset(); wifi_.applyTimeSettings(state_); }
            }
            saveUi(); break;
        case UiAction::TransferToggle: if (transfer_.running()) { transfer_.stop(state_, hal()); refreshBooks(); } else transfer_.startApUploadServer(state_, hal()); break;
        case UiAction::CalendarPrevious:
            if (--state_.calendar.month < 1) { state_.calendar.month = 12; --state_.calendar.year; }
            {
                const int maxDay = state_.calendar.month == 2 ? (((state_.calendar.year % 4 == 0 && state_.calendar.year % 100 != 0) || state_.calendar.year % 400 == 0) ? 29 : 28) :
                    ((state_.calendar.month == 4 || state_.calendar.month == 6 || state_.calendar.month == 9 || state_.calendar.month == 11) ? 30 : 31);
                state_.calendar.selectedDay = std::min(state_.calendar.selectedDay, maxDay);
            }
            break;
        case UiAction::CalendarNext:
            if (++state_.calendar.month > 12) { state_.calendar.month = 1; ++state_.calendar.year; }
            {
                const int maxDay = state_.calendar.month == 2 ? (((state_.calendar.year % 4 == 0 && state_.calendar.year % 100 != 0) || state_.calendar.year % 400 == 0) ? 29 : 28) :
                    ((state_.calendar.month == 4 || state_.calendar.month == 6 || state_.calendar.month == 9 || state_.calendar.month == 11) ? 30 : 31);
                state_.calendar.selectedDay = std::min(state_.calendar.selectedDay, maxDay);
            }
            break;
        case UiAction::CalendarDay: state_.calendar.selectedDay = event.index; break;
        case UiAction::PomodoroMode: timers_.switchPomodoroMode(state_, !state_.pomodoro.focusMode); saveUi(); break;
        case UiAction::PomodoroStartPause: state_.pomodoro.running ? timers_.pausePomodoro(state_) : timers_.startPomodoro(state_); saveUi(); break;
        case UiAction::PomodoroReset: timers_.resetPomodoro(state_); saveUi(); break;
        case UiAction::TimerSelect: state_.selectedTimer = event.index; break;
        case UiAction::TimerStartPause:
            if (state_.selectedTimer >= 0 && state_.selectedTimer < static_cast<int>(state_.timers.size())) { state_.timers[state_.selectedTimer].running ? timers_.pauseTimer(state_, state_.selectedTimer) : timers_.startTimer(state_, state_.selectedTimer); saveUi(); }
            break;
        case UiAction::TimerReset: timers_.resetTimer(state_, state_.selectedTimer); saveUi(); break;
        case UiAction::TimerAdd:
            if (state_.timers.size() < 4) { const int index = static_cast<int>(state_.timers.size()); state_.timers.push_back({"timer-" + std::to_string(index), "新计时器", 300, 300, false, 0}); state_.selectedTimer = index; saveUi(); }
            else state_.status.message = "最多可添加 4 个计时器";
            break;
        case UiAction::TimerRename:
            if (state_.selectedTimer >= 0 && state_.selectedTimer < static_cast<int>(state_.timers.size())) { state_.editBuffer = state_.timers[state_.selectedTimer].name; state_.pendingTimerRename = true; ime_.setMode(InputMode::Pinyin); ime_.open(&state_.editBuffer); }
            break;
        case UiAction::TimerMinus:
            if (state_.selectedTimer >= 0 && state_.selectedTimer < static_cast<int>(state_.timers.size()) && !state_.timers[state_.selectedTimer].running) { auto& timer = state_.timers[state_.selectedTimer]; timer.durationSeconds = std::max<std::uint32_t>(60, timer.durationSeconds > 60 ? timer.durationSeconds - 60 : 60); timer.remainingSeconds = timer.durationSeconds; saveUi(); }
            break;
        case UiAction::TimerPlus:
            if (state_.selectedTimer >= 0 && state_.selectedTimer < static_cast<int>(state_.timers.size()) && !state_.timers[state_.selectedTimer].running) { auto& timer = state_.timers[state_.selectedTimer]; timer.durationSeconds = std::min<std::uint32_t>(24 * 3600, timer.durationSeconds + 60); timer.remainingSeconds = timer.durationSeconds; saveUi(); }
            break;
        case UiAction::TimerDelete:
            if (state_.selectedTimer >= 0 && state_.selectedTimer < static_cast<int>(state_.timers.size())) { state_.timers.erase(state_.timers.begin() + state_.selectedTimer); state_.selectedTimer = std::max(0, std::min(state_.selectedTimer, static_cast<int>(state_.timers.size()) - 1)); saveUi(); }
            break;
        case UiAction::TodoToggle:
            if (event.index >= 0 && event.index < static_cast<int>(state_.todos.size())) { state_.todos[event.index].done = !state_.todos[event.index].done; saveUi(); }
            break;
        case UiAction::TodoAdd:
            if (state_.todos.size() >= 6) state_.status.message = "最多保存 6 条待办";
            else { state_.editBuffer.clear(); state_.pendingTodoAdd = true; ime_.setMode(InputMode::Pinyin); ime_.open(&state_.editBuffer); }
            break;
        case UiAction::TodoDelete:
            if (event.index >= 0 && event.index < static_cast<int>(state_.todos.size())) { state_.todos.erase(state_.todos.begin() + event.index); saveUi(); }
            break;
        case UiAction::FileSelect: state_.selectedFile = event.index; state_.editPath.clear(); break;
        case UiAction::FileOpen:
            if (state_.selectedFile >= 0 && state_.selectedFile < static_cast<int>(state_.files.size())) { const auto entry = state_.files[state_.selectedFile]; if (entry.directory) { state_.selectedFile = -1; files_.list(state_, entry.path); } else { auto found = std::find_if(state_.books.begin(), state_.books.end(), [&](const BookInfo& b) { return b.path == entry.path; }); if (found != state_.books.end()) openBook(static_cast<int>(found - state_.books.begin())); else state_.status.message = "该文件不能在阅读器中打开"; } }
            break;
        case UiAction::FileRename:
            if (state_.selectedFile >= 0 && state_.selectedFile < static_cast<int>(state_.files.size())) { state_.editPath = state_.files[state_.selectedFile].path; state_.editBuffer = state_.files[state_.selectedFile].name; state_.pendingFileRename = true; ime_.open(&state_.editBuffer); }
            break;
        case UiAction::FileDelete:
            if (state_.selectedFile >= 0 && state_.selectedFile < static_cast<int>(state_.files.size())) { const auto path = state_.files[state_.selectedFile].path; if (state_.editPath != path) { state_.editPath = path; state_.status.message = "再次点击删除以确认"; } else { std::string error; state_.status.message = files_.removeEntry(path, error) ? "删除成功" : error; state_.editPath.clear(); state_.selectedFile = -1; files_.list(state_, state_.currentFolder); refreshBooks(); } }
            break;
        case UiAction::FileUp:
            if (state_.currentFolder != "/sdcard") { const auto slash = state_.currentFolder.find_last_of('/'); files_.list(state_, slash == 0 ? "/sdcard" : state_.currentFolder.substr(0, slash)); state_.selectedFile = -1; }
            break;
        case UiAction::AlbumSelect:
            if (event.index >= 0 && event.index < static_cast<int>(state_.files.size())) { state_.albumIndex = event.index; state_.selectedFile = event.index; }
            break;
        case UiAction::StorageRefresh:
            refreshBooks();
            state_.status.message = state_.status.sdMounted ? "存储卡已刷新" : "未检测到 SD 卡";
            break;
        case UiAction::StorageOpenManager:
            state_.currentFolder = "/sdcard/books";
            files_.list(state_, state_.currentFolder);
            state_.selectedFile = -1;
            openPage(Page::FileManager);
            break;
        case UiAction::FirmwareInstall:
            if (state_.otaStatus != "再次点击确认安装并重启") state_.otaStatus = "再次点击确认安装并重启";
            else ota_.installFromSd(state_, "/sdcard/ota/firmware.bin", true);
            break;
        case UiAction::DiagnosticsRun: runDiagnostics(); break;
        case UiAction::ReaderToggleChrome:
            // In tap mode the side thirds remain page-turn targets while the
            // centre toggles the chrome. This keeps the reader usable without
            // making the toolbar a permanent overlay.
            if (!state_.readerChromeVisible && state_.reader.pageTurnMode == "tap" && point.x < 180) {
                reader_.previousPage();
                syncReaderState();
            } else if (!state_.readerChromeVisible && state_.reader.pageTurnMode == "tap" && point.x > 360) {
                reader_.nextPage();
                syncReaderState();
            } else {
                state_.readerChromeVisible = !state_.readerChromeVisible;
                state_.readerPanel = ReaderPanel::None;
            }
            break;
        case UiAction::ReaderBack: leaveReader(); break;
        case UiAction::ReaderAutoPage: state_.reader.autoPage = !state_.reader.autoPage; state_.autoPageLastMs = hal().millis(); saveUi(); break;
        case UiAction::ReaderBookmark:
            if (state_.selectedBook >= 0 && state_.selectedBook < static_cast<int>(state_.books.size())) { auto& marks = state_.books[state_.selectedBook].bookmarks; const auto it = std::find(marks.begin(), marks.end(), state_.readerPage); if (it == marks.end()) marks.push_back(state_.readerPage); else marks.erase(it); settings_.saveBook(state_.books[state_.selectedBook], 0); syncReaderState(); }
            break;
        case UiAction::ReaderTurnPicker: state_.readerPanel = state_.readerPanel == ReaderPanel::PageTurnPicker ? ReaderPanel::None : ReaderPanel::PageTurnPicker; break;
        case UiAction::ReaderContents: state_.readerPanel = state_.readerPanel == ReaderPanel::Contents ? ReaderPanel::None : ReaderPanel::Contents; break;
        case UiAction::ReaderProgress: state_.readerPanel = state_.readerPanel == ReaderPanel::Progress ? ReaderPanel::None : ReaderPanel::Progress; break;
        case UiAction::ReaderTypography: state_.readerPanel = state_.readerPanel == ReaderPanel::Typography ? ReaderPanel::None : ReaderPanel::Typography; break;
        case UiAction::ReaderPageSlider: reader_.goToPage((state_.readerPageCount - 1) * event.value / 100); syncReaderState(); break;
        case UiAction::ReaderChapter: reader_.goToChapter(event.index); syncReaderState(); break;
        case UiAction::ReaderFontPicker: state_.readerPanel = ReaderPanel::FontPicker; break;
        case UiAction::ReaderFontChoice: if (event.index >= 0 && event.index < 4) { state_.reader.fontName = kReaderFonts[event.index]; state_.readerPanel = ReaderPanel::Typography; saveUi(); } break;
        case UiAction::ReaderTurnMode: if (event.index >= 0 && event.index < 4) { state_.reader.pageTurnMode = kTurnModes[event.index]; state_.readerPanel = ReaderPanel::None; saveUi(); } break;
        case UiAction::ReaderFontSize: state_.reader.fontSize = 14 + event.value * 10 / 100; reader_.setLayout(state_.reader.fontSize, state_.reader.marginLevel, state_.reader.lineHeightLevel); if (state_.selectedBook >= 0) { const int progress = reader_.progressPercent(); reader_.open(state_.books[state_.selectedBook], progress); syncReaderState(); } saveUi(); break;
        case UiAction::ReaderMargin: state_.reader.marginLevel = event.value * 4 / 100; reader_.setLayout(state_.reader.fontSize, state_.reader.marginLevel, state_.reader.lineHeightLevel); if (state_.selectedBook >= 0) { const int progress = reader_.progressPercent(); reader_.open(state_.books[state_.selectedBook], progress); syncReaderState(); } saveUi(); break;
        case UiAction::ReaderLineHeight: state_.reader.lineHeightLevel = event.value * 4 / 100; reader_.setLayout(state_.reader.fontSize, state_.reader.marginLevel, state_.reader.lineHeightLevel); if (state_.selectedBook >= 0) { const int progress = reader_.progressPercent(); reader_.open(state_.books[state_.selectedBook], progress); syncReaderState(); } saveUi(); break;
        default:
            if (state_.page == Page::Reader && !state_.readerChromeVisible && state_.reader.pageTurnMode == "tap") { point.x < 180 ? reader_.previousPage() : point.x > 360 ? reader_.nextPage() : false; syncReaderState(); }
            break;
    }
    redraw(true);
}

void AppController::handleGesture(int endX, int endY, std::uint32_t durationMs)
{
    const int dx = endX - touchStartX_;
    const int dy = endY - touchStartY_;
    if (state_.overlay == SystemOverlay::LockScreen) {
        if (touchStartY_ > 700 && dy < -120) { state_.overlay = SystemOverlay::None; state_.lastInteractionMs = hal().millis(); redraw(true); }
        return;
    }
    if (state_.page == Page::Reader && !state_.readerChromeVisible && state_.readerPanel == ReaderPanel::None &&
        std::abs(dx) > 80 && std::abs(dx) > std::abs(dy) &&
        (state_.reader.pageTurnMode == "swipe" || state_.reader.pageTurnMode == "simulation")) {
        dx < 0 ? reader_.nextPage() : reader_.previousPage(); syncReaderState(); redraw(true);
    }
    const auto started = ui_.hitTest(touchStartX_, touchStartY_);
    if (started.action == UiAction::ReaderAutoPage && durationMs > 700) { const int values[] = {5, 8, 15, 30}; int next = 0; for (int i = 0; i < 4; ++i) if (state_.reader.autoPageSeconds == values[i]) next = (i + 1) % 4; state_.reader.autoPageSeconds = values[next]; state_.status.message = "自动翻页间隔 " + std::to_string(state_.reader.autoPageSeconds) + " 秒"; saveUi(); redraw(true); }
}

void AppController::runDiagnostics()
{
    std::uint64_t otaSize = 0;
    state_.diagnosticsText = std::string("显示与触控：已初始化\n") +
        "存储卡：" + (state_.status.sdMounted ? "正常" : "未检测到") + "\n" +
        "图书扫描：" + std::to_string(state_.books.size()) + " 本\n" +
        "Wi-Fi：" + (state_.status.wifiConnected ? ("已连接 " + state_.status.ipAddress) : "未连接") + "\n" +
        "蓝牙控制器：" + (bluetooth_.ready() ? "正常" : "未初始化") + "\n" +
        "姿态传感器：" + (device_.imuAvailable() ? "正常" : "未检测到") + "\n" +
        "OTA 升级包：" + (ota_.packageExists("/sdcard/ota/firmware.bin", &otaSize) ? (std::to_string(otaSize / 1024) + " KB") : "无");
}

void AppController::saveUi() { settings_.saveUi(state_); }

void AppController::redraw(bool force)
{
    if (!force && !hal().consumeFullRefreshRequest() && hal().millis() - lastDrawMs_ < 1000) return;
    ui_.draw(state_, hal(), ime_, wifi_.networks());
    lastDrawMs_ = hal().millis();
}

}  // namespace papers3
