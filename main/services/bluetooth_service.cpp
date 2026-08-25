#include "bluetooth_service.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

namespace papers3 {

namespace {
constexpr const char* TAG = "Bluetooth";
constexpr EventBits_t SCAN_DONE = BIT0;
constexpr EventBits_t CONNECT_DONE = BIT1;
constexpr EventBits_t SYNC_DONE = BIT2;

std::string addressString(const std::uint8_t* address)
{
    char value[18];
    std::snprintf(value, sizeof(value), "%02X:%02X:%02X:%02X:%02X:%02X",
                  address[5], address[4], address[3], address[2], address[1], address[0]);
    return value;
}
}

BluetoothService* BluetoothService::instance_ = nullptr;

bool BluetoothService::init(AppState& state)
{
    state_ = &state;
    if (initialized_) return true;
    instance_ = this;
    events_ = xEventGroupCreate();
    if (!events_) return false;
    const auto result = nimble_port_init();
    if (result != ESP_OK) {
        state.status.message = "蓝牙初始化失败";
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(result));
        return false;
    }
    ble_hs_cfg.reset_cb = onReset;
    ble_hs_cfg.sync_cb = onSync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("PaperS3");
    nimble_port_freertos_init(hostTask);
    initialized_ = true;
    const auto bits = xEventGroupWaitBits(static_cast<EventGroupHandle_t>(events_), SYNC_DONE, pdFALSE, pdTRUE, pdMS_TO_TICKS(3000));
    state.status.bluetoothEnabled = (bits & SYNC_DONE) != 0;
    state.status.message = state.status.bluetoothEnabled ? "蓝牙已就绪" : "蓝牙启动超时";
    return state.status.bluetoothEnabled;
}

void BluetoothService::hostTask(void*)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void BluetoothService::onSync()
{
    if (!instance_) return;
    if (ble_hs_id_infer_auto(0, &instance_->ownAddressType_) == 0) {
        instance_->synced_ = true;
        xEventGroupSetBits(static_cast<EventGroupHandle_t>(instance_->events_), SYNC_DONE);
    }
}

void BluetoothService::onReset(int reason)
{
    ESP_LOGW(TAG, "NimBLE reset: %d", reason);
    if (instance_) instance_->synced_ = false;
}

int BluetoothService::gapEvent(ble_gap_event* event, void* argument)
{
    auto* service = static_cast<BluetoothService*>(argument);
    if (!service) return 0;
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            if (!service->targetDevices_) break;
            ble_hs_adv_fields fields {};
            ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            std::string address = addressString(event->disc.addr.val);
            auto found = std::find_if(service->targetDevices_->begin(), service->targetDevices_->end(),
                                      [&](const BluetoothDevice& item) { return item.address == address; });
            std::string name;
            if (fields.name && fields.name_len) name.assign(reinterpret_cast<const char*>(fields.name), fields.name_len);
            if (name.empty()) name = "BLE " + address.substr(12);
            if (found == service->targetDevices_->end()) {
                service->targetDevices_->push_back({name, address, event->disc.rssi, event->disc.addr.type, false});
            } else if (event->disc.rssi > found->rssi) {
                found->rssi = event->disc.rssi;
                if (!name.empty()) found->name = name;
            }
            break;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            xEventGroupSetBits(static_cast<EventGroupHandle_t>(service->events_), SCAN_DONE);
            break;
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) service->connectionHandle_ = event->connect.conn_handle;
            if (service->state_) service->state_->status.bluetoothConnected = event->connect.status == 0;
            xEventGroupSetBits(static_cast<EventGroupHandle_t>(service->events_), CONNECT_DONE);
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            service->connectionHandle_ = 0xffff;
            if (service->state_) service->state_->status.bluetoothConnected = false;
            break;
        default:
            break;
    }
    return 0;
}

bool BluetoothService::scan(AppState& state, std::uint32_t durationMs)
{
    state_ = &state;
    if (!initialized_ && !init(state)) return false;
    if (!synced_) return false;
    ble_gap_disc_cancel();
    state.bluetoothDevices.clear();
    targetDevices_ = &state.bluetoothDevices;
    ble_gap_disc_params params {};
    params.filter_duplicates = 1;
    params.passive = 0;
    params.itvl = 0;
    params.window = 0;
    params.filter_policy = 0;
    params.limited = 0;
    xEventGroupClearBits(static_cast<EventGroupHandle_t>(events_), SCAN_DONE);
    const int result = ble_gap_disc(ownAddressType_, static_cast<std::int32_t>(durationMs), &params, gapEvent, this);
    if (result != 0) {
        state.status.message = "蓝牙扫描启动失败";
        targetDevices_ = nullptr;
        return false;
    }
    xEventGroupWaitBits(static_cast<EventGroupHandle_t>(events_), SCAN_DONE, pdTRUE, pdTRUE,
                        pdMS_TO_TICKS(durationMs + 1500));
    targetDevices_ = nullptr;
    std::sort(state.bluetoothDevices.begin(), state.bluetoothDevices.end(),
              [](const BluetoothDevice& a, const BluetoothDevice& b) { return a.rssi > b.rssi; });
    state.status.message = state.bluetoothDevices.empty() ? "未发现附近蓝牙设备" : "蓝牙扫描完成";
    return true;
}

bool BluetoothService::connect(AppState& state, std::size_t deviceIndex)
{
    state_ = &state;
    if (!initialized_ && !init(state)) return false;
    if (deviceIndex >= state.bluetoothDevices.size()) return false;
    ble_gap_disc_cancel();
    ble_addr_t address {};
    address.type = state.bluetoothDevices[deviceIndex].addressType;
    unsigned int bytes[6] {};
    if (std::sscanf(state.bluetoothDevices[deviceIndex].address.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
                    &bytes[5], &bytes[4], &bytes[3], &bytes[2], &bytes[1], &bytes[0]) != 6) return false;
    for (int i = 0; i < 6; ++i) address.val[i] = static_cast<std::uint8_t>(bytes[i]);
    xEventGroupClearBits(static_cast<EventGroupHandle_t>(events_), CONNECT_DONE);
    const int result = ble_gap_connect(ownAddressType_, &address, 30000, nullptr, gapEvent, this);
    if (result != 0) {
        state.status.message = "蓝牙连接启动失败";
        return false;
    }
    xEventGroupWaitBits(static_cast<EventGroupHandle_t>(events_), CONNECT_DONE, pdTRUE, pdTRUE, pdMS_TO_TICKS(31000));
    const bool connected = connectionHandle_ != 0xffff;
    for (auto& device : state.bluetoothDevices) device.connected = false;
    state.bluetoothDevices[deviceIndex].connected = connected;
    state.status.bluetoothConnected = connected;
    state.status.message = connected ? "蓝牙连接成功" : "蓝牙连接失败";
    return connected;
}

void BluetoothService::disconnect(AppState& state)
{
    if (connectionHandle_ != 0xffff) ble_gap_terminate(connectionHandle_, BLE_ERR_REM_USER_CONN_TERM);
    connectionHandle_ = 0xffff;
    for (auto& device : state.bluetoothDevices) device.connected = false;
    state.status.bluetoothConnected = false;
    state.status.message = "蓝牙已断开";
}

bool BluetoothService::ready() const { return initialized_ && synced_; }

}  // namespace papers3
