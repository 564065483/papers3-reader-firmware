#pragma once

#include "app_state.h"
#include "papers3_hal.h"
#include <vector>

namespace papers3 {

class WifiService {
public:
    void scan(PaperS3Hal& hal);
    const std::vector<WifiNetwork>& networks() const;
    bool connect(AppState& state, PaperS3Hal& hal, const std::string& ssid, const std::string& password);
    void disconnect(AppState& state, PaperS3Hal& hal);
    void syncStatus(AppState& state, const PaperS3Hal& hal);
    void applyTimeSettings(const AppState& state);

private:
    std::vector<WifiNetwork> networks_;
};

}  // namespace papers3
