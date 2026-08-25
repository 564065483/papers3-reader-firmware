#pragma once

#include "app_state.h"
#include "input_method.h"
#include "papers3_hal.h"
#include <vector>

namespace papers3 {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

enum class UiAction {
    None,
    Nav,
    Back,
    StatusLock,
    StatusControl,
    OverlayClose,
    ControlWifi,
    ControlBluetooth,
    Search,
    BooksFilter,
    BooksTagFilter,
    BooksPrevious,
    BooksNext,
    BookOpen,
    BookFavorite,
    BookTag,
    SettingsItem,
    ToolsItem,
    WifiRefresh,
    WifiToggle,
    WifiNetwork,
    WifiPassword,
    WifiConnect,
    BluetoothToggle,
    BluetoothScan,
    BluetoothDevice,
    SettingChoice,
    TransferToggle,
    CalendarPrevious,
    CalendarNext,
    CalendarDay,
    TodoToggle,
    TodoAdd,
    TodoDelete,
    PomodoroMode,
    PomodoroStartPause,
    PomodoroReset,
    TimerSelect,
    TimerStartPause,
    TimerReset,
    TimerAdd,
    TimerRename,
    TimerMinus,
    TimerPlus,
    TimerDelete,
    FileSelect,
    FileOpen,
    FileRename,
    FileDelete,
    FileUp,
    AlbumSelect,
    StorageRefresh,
    StorageOpenManager,
    FirmwareInstall,
    DiagnosticsRun,
    ReaderToggleChrome,
    ReaderBack,
    ReaderAutoPage,
    ReaderBookmark,
    ReaderTurnPicker,
    ReaderContents,
    ReaderProgress,
    ReaderTypography,
    ReaderPageSlider,
    ReaderChapter,
    ReaderFontPicker,
    ReaderFontChoice,
    ReaderTurnMode,
    ReaderFontSize,
    ReaderMargin,
    ReaderLineHeight,
};

struct UiEvent {
    UiAction action = UiAction::None;
    int index = -1;
    int value = 0;
};

class UiRenderer {
public:
    void draw(AppState& state, PaperS3Hal& hal, InputMethod& ime, const std::vector<WifiNetwork>& networks);
    UiEvent hitTest(int x, int y) const;
    int keyboardKeyIndex(int x, int y) const;
    int candidateIndex(int x, int y) const;
    bool keyboardBackspaceHit(int x, int y) const;
    bool keyboardEnterHit(int x, int y) const;
    bool keyboardClearHit(int x, int y) const;
    bool keyboardSpaceHit(int x, int y) const;
    bool keyboardGlobeHit(int x, int y) const;
    bool keyboardSymbolsHit(int x, int y) const;

public:
    struct HitTarget { Rect rect; UiAction action; int index; };
    Theme currentTheme_ = Theme::Light;
    std::vector<HitTarget> targets_;
    Rect keyRects_[40] {};
    int keyCount_ = 0;
    Rect candidateRects_[5] {};
    int candidateCount_ = 0;
    Rect backspace_ {};
    Rect enter_ {};
    Rect clear_ {};
    Rect globe_ {};
    Rect symbols_ {};
    Rect space_ {};

    void add(const Rect& rect, UiAction action, int index = -1);
    void resetHitTargets();
    void drawStatus(AppState& state, M5GFX& gfx);
    void drawNav(AppState& state, M5GFX& gfx);
    void drawHome(AppState& state, M5GFX& gfx);
    void drawBooks(AppState& state, M5GFX& gfx);
    void drawTools(AppState& state, M5GFX& gfx);
    void drawSettings(AppState& state, M5GFX& gfx);
    void drawSubpage(AppState& state, M5GFX& gfx, const std::vector<WifiNetwork>& networks);
    void drawReader(AppState& state, M5GFX& gfx);
    void drawOverlay(AppState& state, M5GFX& gfx);
    void drawKeyboard(InputMethod& ime, M5GFX& gfx);
    void drawButton(M5GFX& gfx, const Rect& rect, const char* label, bool active = false);
    void drawRow(M5GFX& gfx, const Rect& rect, const char* title, const char* value = nullptr, bool active = false);
};

}  // namespace papers3
