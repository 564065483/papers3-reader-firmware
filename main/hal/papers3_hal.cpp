#include "papers3_hal.h"

#include <dirent.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_vfs_fat.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <driver/sdspi_host.h>
#include <driver/spi_master.h>
#include <nvs_flash.h>
#include <sdmmc_cmd.h>
#include <lwip/ip4_addr.h>

namespace papers3 {

namespace {
constexpr const char* TAG = "PaperS3Hal";
constexpr const char* SD_MOUNT_POINT = "/sdcard";
constexpr gpio_num_t PIN_SD_MISO = GPIO_NUM_40;
constexpr gpio_num_t PIN_SD_MOSI = GPIO_NUM_38;
constexpr gpio_num_t PIN_SD_SCLK = GPIO_NUM_39;
constexpr gpio_num_t PIN_SD_CS = GPIO_NUM_47;
PaperS3Hal g_hal;
sdmmc_card_t* g_sd_card = nullptr;
bool g_spi_bus_ready = false;
bool g_wifi_ready = false;
esp_netif_t* g_wifi_netif = nullptr;
esp_netif_t* g_ap_netif = nullptr;
EventGroupHandle_t g_wifi_events = nullptr;
constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
bool g_wifi_handlers_ready = false;

void wifiEventHandler(void*, esp_event_base_t base, int32_t id, void*)
{
    if (!g_wifi_events) return;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(g_wifi_events, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED_BIT);
    }
}

bool hasExt(const std::string& value, const char* ext)
{
    const std::string suffix(ext);
    if (value.size() < suffix.size()) return false;
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool initNvs()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase before init: %s", esp_err_to_name(ret));
        ret = nvs_flash_erase();
        if (ret != ESP_OK) return false;
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool initWifiSta()
{
    if (g_wifi_ready) return true;
    if (!initNvs()) return false;

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "event loop init failed: %s", esp_err_to_name(ret));
        return false;
    }

    if (!g_wifi_netif) {
        g_wifi_netif = esp_netif_create_default_wifi_sta();
        if (!g_wifi_netif) {
            ESP_LOGW(TAG, "create default Wi-Fi STA netif failed");
            return false;
        }
    }

    if (!g_wifi_events) g_wifi_events = xEventGroupCreate();
    if (!g_wifi_handlers_ready) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, nullptr);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, nullptr);
        g_wifi_handlers_ready = true;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {
        ESP_LOGW(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return false;
    }

    g_wifi_ready = true;
    return true;
}
}

void PaperS3Hal::init()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(0);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setTextDatum(top_left);
    M5.Display.setFont(&fonts::Font2);
    setQualityRefresh();
    M5.Display.fillScreen(TFT_WHITE);
    sdMounted_ = mountSdCard();
}

void PaperS3Hal::update()
{
    M5.update();
}

void PaperS3Hal::delayMs(std::uint32_t ms)
{
    m5gfx::delay(ms);
}

std::uint32_t PaperS3Hal::millis() const
{
    return m5gfx::millis();
}

M5GFX& PaperS3Hal::display()
{
    return M5.Display;
}

TouchPoint PaperS3Hal::touch()
{
    TouchPoint point;
    point.pressed = M5.Touch.getCount() > 0;
    if (point.pressed) {
        auto detail = M5.Touch.getDetail();
        point.x = detail.x;
        point.y = detail.y;
    }
    return point;
}

void PaperS3Hal::setTextRefresh()
{
    M5.Display.setEpdMode(epd_mode_t::epd_text);
}

void PaperS3Hal::setQualityRefresh()
{
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
}

void PaperS3Hal::requestFullRefresh()
{
    fullRefreshRequested_ = true;
}

bool PaperS3Hal::consumeFullRefreshRequest()
{
    const bool requested = fullRefreshRequested_;
    fullRefreshRequested_ = false;
    return requested;
}

int PaperS3Hal::batteryPercent()
{
    // M5Unified exposes board-specific power APIs where available. Keep a safe
    // fallback until the real unit is verified.
    auto level = M5.Power.getBatteryLevel();
    if (level < 0 || level > 100) return 78;
    return level;
}

bool PaperS3Hal::mountSdCard()
{
    if (sdMounted_) return true;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = PIN_SD_MOSI;
    bus_cfg.miso_io_num = PIN_SD_MISO;
    bus_cfg.sclk_io_num = PIN_SD_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    if (!g_spi_bus_ready) {
        esp_err_t ret = spi_bus_initialize(static_cast<spi_host_device_t>(host.slot), &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "SD SPI bus init failed: %s", esp_err_to_name(ret));
            return false;
        }
        g_spi_bus_ready = true;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_SD_CS;
    slot_config.host_id = static_cast<spi_host_device_t>(host.slot);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 8;
    mount_config.allocation_unit_size = 16 * 1024;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &g_sd_card);
    if (ret == ESP_OK) {
        sdMounted_ = true;
        ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
        return true;
    }
    ESP_LOGW(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
    return false;
}

bool PaperS3Hal::isSdMounted() const
{
    return sdMounted_;
}

std::vector<std::string> PaperS3Hal::listBookFiles(const char* root)
{
    std::vector<std::string> files;
    if (!sdMounted_) return files;

    DIR* dir = opendir(root);
    if (!dir) return files;
    while (auto* entry = readdir(dir)) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (name.size() >= 4) {
            auto lower = name;
            for (auto& c : lower) c = static_cast<char>(tolower(c));
            if (hasExt(lower, ".txt") || hasExt(lower, ".epub")) {
                files.push_back(std::string(root) + "/" + name);
            }
        }
    }
    closedir(dir);
    return files;
}

std::vector<WifiNetwork> PaperS3Hal::scanWifi()
{
    std::vector<WifiNetwork> result;
    if (!initWifiSta()) return result;

    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = true;

    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(ret));
        return result;
    }

    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK || ap_count == 0) return result;

    if (ap_count > 20) ap_count = 20;
    std::vector<wifi_ap_record_t> records(ap_count);
    ret = esp_wifi_scan_get_ap_records(&ap_count, records.data());
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan read failed: %s", esp_err_to_name(ret));
        return result;
    }

    result.reserve(ap_count);
    for (uint16_t i = 0; i < ap_count; ++i) {
        const auto& record = records[i];
        const auto len = strnlen(reinterpret_cast<const char*>(record.ssid), sizeof(record.ssid));
        if (len == 0) continue;
        result.push_back({
            std::string(reinterpret_cast<const char*>(record.ssid), len),
            static_cast<int>(record.rssi),
        });
    }

    std::sort(result.begin(), result.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
        return a.rssi > b.rssi;
    });
    return result;
}

bool PaperS3Hal::connectWifi(const std::string& ssid, const std::string& password, std::uint32_t timeoutMs)
{
    if (!initWifiSta() || ssid.empty()) return false;
    wifi_config_t config = {};
    const auto ssidLength = std::min(ssid.size(), sizeof(config.sta.ssid) - 1);
    const auto passwordLength = std::min(password.size(), sizeof(config.sta.password) - 1);
    std::memcpy(config.sta.ssid, ssid.data(), ssidLength);
    std::memcpy(config.sta.password, password.data(), passwordLength);
    config.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;

    xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED_BIT);
    esp_wifi_disconnect();
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK) return false;
    if (esp_wifi_connect() != ESP_OK) return false;
    const auto bits = xEventGroupWaitBits(g_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                                          pdMS_TO_TICKS(timeoutMs));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

void PaperS3Hal::disconnectWifi()
{
    if (!g_wifi_ready) return;
    esp_wifi_disconnect();
    if (g_wifi_events) xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED_BIT);
}

bool PaperS3Hal::wifiConnected() const
{
    return g_wifi_events && (xEventGroupGetBits(g_wifi_events) & WIFI_CONNECTED_BIT);
}

std::string PaperS3Hal::wifiIpAddress(bool accessPoint) const
{
    esp_netif_t* netif = accessPoint ? g_ap_netif : g_wifi_netif;
    if (!netif) return {};
    esp_netif_ip_info_t info {};
    if (esp_netif_get_ip_info(netif, &info) != ESP_OK) return {};
    char value[IP4ADDR_STRLEN_MAX] {};
    esp_ip4addr_ntoa(&info.ip, value, sizeof(value));
    return value;
}

bool PaperS3Hal::startWifiAccessPoint(const std::string& ssid, const std::string& password)
{
    if (!initWifiSta() || ssid.empty()) return false;
    if (!g_ap_netif) g_ap_netif = esp_netif_create_default_wifi_ap();
    if (!g_ap_netif) return false;

    wifi_config_t config = {};
    const auto ssidLength = std::min(ssid.size(), sizeof(config.ap.ssid) - 1);
    const auto passwordLength = std::min(password.size(), sizeof(config.ap.password) - 1);
    std::memcpy(config.ap.ssid, ssid.data(), ssidLength);
    std::memcpy(config.ap.password, password.data(), passwordLength);
    config.ap.ssid_len = static_cast<std::uint8_t>(ssidLength);
    config.ap.channel = 1;
    config.ap.max_connection = 4;
    config.ap.authmode = passwordLength >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return false;
    if (esp_wifi_set_config(WIFI_IF_AP, &config) != ESP_OK) return false;
    const auto result = esp_wifi_start();
    // Starting an AP while the STA interface is already running commonly
    // reports WIFI_STATE instead of doing a second start.  In APSTA mode that
    // is a successful, usable state rather than a hard failure.
    return result == ESP_OK || result == ESP_ERR_WIFI_CONN || result == ESP_ERR_WIFI_STATE;
}

void PaperS3Hal::stopWifiAccessPoint()
{
    if (!g_wifi_ready) return;
    esp_wifi_set_mode(WIFI_MODE_STA);
}

void PaperS3Hal::setWifiEnabled(bool enabled)
{
    if (enabled) {
        initWifiSta();
        esp_wifi_start();
    } else if (g_wifi_ready) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        if (g_wifi_events) xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED_BIT);
        g_wifi_ready = false;
    }
}

void PaperS3Hal::setWifiLowPower(bool enabled)
{
    if (g_wifi_ready) esp_wifi_set_ps(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
}

void PaperS3Hal::setOrientation(bool landscape)
{
    M5.Display.setRotation(landscape ? 1 : 0);
    requestFullRefresh();
}

PaperS3Hal& hal()
{
    return g_hal;
}

}  // namespace papers3
