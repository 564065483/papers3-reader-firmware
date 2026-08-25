#pragma once

#include "app_state.h"
#include "bluetooth_service.h"
#include "device_service.h"
#include "file_manager_service.h"
#include "file_transfer_service.h"
#include "input_method.h"
#include "ota_service.h"
#include "papers3_hal.h"
#include "reader_service.h"
#include "settings_service.h"
#include "storage_service.h"
#include "timer_service.h"
#include "ui_renderer.h"
#include "wifi_service.h"

namespace papers3 {

class AppController {
public:
    void init();
    void update();

private:
    AppState state_;
    UiRenderer ui_;
    InputMethod ime_;
    StorageService storage_;
    SettingsService settings_;
    ReaderService reader_;
    WifiService wifi_;
    BluetoothService bluetooth_;
    FileTransferService transfer_;
    FileManagerService files_;
    TimerService timers_;
    DeviceService device_;
    OtaService ota_;
    bool wasPressed_ = false;
    int touchStartX_ = 0;
    int touchStartY_ = 0;
    int touchLastX_ = 0;
    int touchLastY_ = 0;
    std::uint32_t touchStartMs_ = 0;
    std::uint32_t lastDrawMs_ = 0;
    std::uint32_t lastTimerDrawMs_ = 0;
    std::uint32_t lastReconnectMs_ = 0;

    void handleTouch(const TouchPoint& point);
    void handleEvent(const UiEvent& event, const TouchPoint& point);
    void handleGesture(int endX, int endY, std::uint32_t durationMs);
    void openBook(int index);
    void refreshBooks();
    void syncReaderState();
    void leaveReader();
    void openPage(Page page);
    void saveUi();
    void runDiagnostics();
    void redraw(bool force = false);
};

}  // namespace papers3
