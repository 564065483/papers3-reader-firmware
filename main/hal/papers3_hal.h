#pragma once

#include <M5Unified.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace papers3 {

struct TouchPoint {
    bool pressed = false;
    int x = 0;
    int y = 0;
};

struct WifiNetwork {
    std::string ssid;
    int rssi = -100;
};

class PaperS3Hal {
public:
    void init();
    void update();
    void delayMs(std::uint32_t ms);
    std::uint32_t millis() const;

    M5GFX& display();
    TouchPoint touch();

    void setTextRefresh();
    void setQualityRefresh();
    void requestFullRefresh();
    bool consumeFullRefreshRequest();

    int batteryPercent();
    bool mountSdCard();
    bool isSdMounted() const;
    std::vector<std::string> listBookFiles(const char* root);
    std::vector<WifiNetwork> scanWifi();
    bool connectWifi(const std::string& ssid, const std::string& password, std::uint32_t timeoutMs = 15000);
    void disconnectWifi();
    bool wifiConnected() const;
    std::string wifiIpAddress(bool accessPoint = false) const;
    bool startWifiAccessPoint(const std::string& ssid, const std::string& password);
    void stopWifiAccessPoint();
    void setWifiEnabled(bool enabled);
    void setWifiLowPower(bool enabled);
    void setOrientation(bool landscape);

private:
    bool sdMounted_ = false;
    bool fullRefreshRequested_ = true;
};

PaperS3Hal& hal();

}  // namespace papers3
