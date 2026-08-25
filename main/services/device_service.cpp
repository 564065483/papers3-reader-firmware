#include "device_service.h"

#include <M5Unified.hpp>
#include <cstdlib>
#include <ctime>

namespace papers3 {

void DeviceService::init(AppState& state)
{
    imuReady_ = M5.Imu.begin();
    const std::time_t now = std::time(nullptr);
    if (now > 1700000000) {
        std::tm current {};
        localtime_r(&now, &current);
        state.calendar.year = current.tm_year + 1900;
        state.calendar.month = current.tm_mon + 1;
        state.calendar.selectedDay = current.tm_mday;
    }
}

int DeviceService::updateTiltPageTurn(std::uint32_t nowMs, bool enabled)
{
    if (!enabled || !imuReady_ || nowMs - lastImuReadMs_ < 100) return 0;
    lastImuReadMs_ = nowMs;
    float x = 0;
    float y = 0;
    float z = 0;
    if (!M5.Imu.getAccel(&x, &y, &z)) return 0;
    const float axis = std::abs(x) > std::abs(y) ? x : y;
    if (std::abs(axis) < 0.30f) {
        tiltArmed_ = true;
        return 0;
    }
    if (!tiltArmed_ || std::abs(axis) < 0.72f) return 0;
    tiltArmed_ = false;
    return axis > 0 ? 1 : -1;
}

int DeviceService::updateOrientation(std::uint32_t nowMs, bool enabled)
{
    if (!enabled || !imuReady_ || nowMs - lastOrientationReadMs_ < 120) return -1;
    lastOrientationReadMs_ = nowMs;
    float x = 0;
    float y = 0;
    float z = 0;
    if (!M5.Imu.getAccel(&x, &y, &z)) return -1;
    int orientation = lastOrientation_;
    if (std::abs(x) > 0.72f && std::abs(x) > std::abs(y) + 0.10f) orientation = 1;
    else if (std::abs(y) > 0.72f && std::abs(y) > std::abs(x) + 0.10f) orientation = 0;
    if (orientation == lastOrientation_) return -1;
    lastOrientation_ = orientation;
    return orientation;
}

void DeviceService::beep()
{
    if (!M5.Speaker.isEnabled()) M5.Speaker.begin();
    M5.Speaker.setVolume(96);
    M5.Speaker.tone(880, 350);
}

void DeviceService::lightSleep(PaperS3Hal& hal)
{
    hal.display().sleep();
    M5.Power.lightSleep(0, true);
    hal.display().wakeup();
    hal.requestFullRefresh();
}

[[noreturn]] void DeviceService::powerOff()
{
    M5.Power.powerOff();
    while (true) m5gfx::delay(1000);
}

bool DeviceService::imuAvailable() const { return imuReady_; }

}  // namespace papers3
