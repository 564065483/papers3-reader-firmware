#pragma once

#include "app_state.h"
#include <cstdint>
#include <vector>

struct ble_gap_event;

namespace papers3 {

class BluetoothService {
public:
    bool init(AppState& state);
    bool scan(AppState& state, std::uint32_t durationMs = 5000);
    bool connect(AppState& state, std::size_t deviceIndex);
    void disconnect(AppState& state);
    bool ready() const;

private:
    bool initialized_ = false;
    bool synced_ = false;
    std::uint8_t ownAddressType_ = 0;
    std::uint16_t connectionHandle_ = 0xffff;
    void* events_ = nullptr;
    std::vector<BluetoothDevice>* targetDevices_ = nullptr;
    AppState* state_ = nullptr;

    static BluetoothService* instance_;
    static void hostTask(void* parameter);
    static void onSync();
    static void onReset(int reason);
    static int gapEvent(ble_gap_event* event, void* argument);
};

}  // namespace papers3
