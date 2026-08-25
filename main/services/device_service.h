#pragma once

#include "app_state.h"
#include "papers3_hal.h"
#include <cstdint>

namespace papers3 {

class DeviceService {
public:
    void init(AppState& state);
    int updateTiltPageTurn(std::uint32_t nowMs, bool enabled);
    // Returns -1 when unchanged, 0 for portrait, and 1 for landscape.
    int updateOrientation(std::uint32_t nowMs, bool enabled);
    void beep();
    void lightSleep(PaperS3Hal& hal);
    [[noreturn]] void powerOff();
    bool imuAvailable() const;

private:
    bool imuReady_ = false;
    bool tiltArmed_ = true;
    std::uint32_t lastImuReadMs_ = 0;
    std::uint32_t lastOrientationReadMs_ = 0;
    int lastOrientation_ = 0;
};

}  // namespace papers3
