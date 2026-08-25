#include "ota_service.h"

#include <cstdio>
#include <sys/stat.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace papers3 {

void OtaService::confirmRunningImage()
{
    esp_ota_mark_app_valid_cancel_rollback();
}

bool OtaService::packageExists(const std::string& path, std::uint64_t* size) const
{
    struct stat info {};
    const bool exists = stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) && info.st_size > 0;
    if (exists && size) *size = static_cast<std::uint64_t>(info.st_size);
    return exists;
}

bool OtaService::installFromSd(AppState& state, const std::string& path, bool rebootAfterInstall)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) { state.otaStatus = "无法打开固件文件"; return false; }
    const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition) { std::fclose(file); state.otaStatus = "没有 OTA 分区"; return false; }
    esp_ota_handle_t handle = 0;
    esp_err_t result = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &handle);
    if (result != ESP_OK) { std::fclose(file); state.otaStatus = "OTA 初始化失败"; return false; }
    char buffer[8192];
    std::size_t total = 0;
    bool okay = true;
    while (true) {
        const auto count = std::fread(buffer, 1, sizeof(buffer), file);
        if (!count) break;
        if (esp_ota_write(handle, buffer, count) != ESP_OK) { okay = false; break; }
        total += count;
    }
    std::fclose(file);
    if (!okay || esp_ota_end(handle) != ESP_OK) {
        esp_ota_abort(handle);
        state.otaStatus = "固件写入或校验失败";
        return false;
    }
    if (esp_ota_set_boot_partition(partition) != ESP_OK) {
        state.otaStatus = "无法设置启动分区";
        return false;
    }
    state.otaStatus = "升级完成，共写入 " + std::to_string(total) + " 字节";
    if (rebootAfterInstall) {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    return true;
}

}  // namespace papers3
