#pragma once

#include <M5GFX.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace papers3 {

struct WifiNetwork {
    std::string ssid;
    int rssi = -100;
};

class PaperS3Hal {
public:
    M5GFX& display();
    void setTextRefresh();
    void setQualityRefresh() {}
    void setOrientation(bool landscape);
    std::uint32_t millis() const
    {
        static const auto start = std::chrono::steady_clock::now();
        return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count());
    }
};

PaperS3Hal& hal();

}  // namespace papers3
