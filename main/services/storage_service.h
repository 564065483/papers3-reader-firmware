#pragma once

#include "app_state.h"
#include "papers3_hal.h"
#include <vector>

namespace papers3 {

class StorageService {
public:
    void refresh(AppState& state, PaperS3Hal& hal);
};

}  // namespace papers3

