#pragma once

#include "app_state.h"
#include <string>

namespace papers3 {

class OtaService {
public:
    void confirmRunningImage();
    bool packageExists(const std::string& path, std::uint64_t* size = nullptr) const;
    bool installFromSd(AppState& state, const std::string& path, bool rebootAfterInstall = true);
};

}  // namespace papers3
