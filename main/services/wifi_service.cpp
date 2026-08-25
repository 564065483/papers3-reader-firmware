#include "wifi_service.h"

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <esp_sntp.h>

namespace papers3 {

void WifiService::scan(PaperS3Hal& hal)
{
    networks_ = hal.scanWifi();
}

const std::vector<WifiNetwork>& WifiService::networks() const
{
    return networks_;
}

bool WifiService::connect(AppState& state, PaperS3Hal& hal, const std::string& ssid, const std::string& password)
{
    state.status.wifiEnabled = true;
    state.status.message = "正在连接…";
    const bool connected = hal.connectWifi(ssid, password);
    state.status.wifiConnected = connected;
    state.status.ssid = connected ? ssid : "";
    state.status.ipAddress = connected ? hal.wifiIpAddress() : "";
    state.status.message = connected ? "连接成功" : "连接失败，请检查密码";
    if (connected) applyTimeSettings(state);
    return connected;
}

void WifiService::applyTimeSettings(const AppState& state)
{
    if (!state.system.ntpEnabled) {
        if (esp_sntp_enabled()) esp_sntp_stop();
        return;
    }
    if (!esp_sntp_enabled()) {
        char timezone[20] {};
        const int minutes = std::abs(state.system.timezoneMinutes);
        std::snprintf(timezone, sizeof(timezone), "UTC%c%d:%02d", state.system.timezoneMinutes >= 0 ? '-' : '+', minutes / 60, minutes % 60);
        setenv("TZ", timezone, 1);
        tzset();
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.cloudflare.com");
        esp_sntp_init();
    }
}

void WifiService::disconnect(AppState& state, PaperS3Hal& hal)
{
    hal.disconnectWifi();
    state.status.wifiConnected = false;
    state.status.ssid.clear();
    state.status.ipAddress.clear();
    state.status.message = "Wi-Fi 已断开";
}

void WifiService::syncStatus(AppState& state, const PaperS3Hal& hal)
{
    state.status.wifiConnected = hal.wifiConnected();
    state.status.ipAddress = state.status.wifiConnected ? hal.wifiIpAddress() : "";
}

}  // namespace papers3
