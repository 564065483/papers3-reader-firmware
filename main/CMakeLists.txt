file(GLOB_RECURSE APP_SRCS
    "./app/*.cpp"
    "./hal/*.cpp"
    "./input/*.cpp"
    "./services/*.cpp"
    "./ui/*.cpp"
)

# Keep generated LVGL assets explicit so temporary converter outputs cannot
# accidentally become firmware objects.
list(APPEND APP_SRCS
    "./ui/lv_font_ui_16.c"
    "./ui/lv_font_ui_24.c"
    "./ui/lv_font_ui_fallback_16.c"
    "./ui/lv_font_reader_20.c"
    "./ui/lv_font_reader_24.c"
    "./ui/lv_font_ui_kr_16.c"
    "./ui/lv_font_ui_latin_16.c"
    "./ui/papers3_cover.c"
)

set(APP_INCLUDE_DIRS
    "."
    "./app"
    "./hal"
    "./input"
    "./services"
    "./ui"
)

idf_component_register(
    SRCS "main.cpp" ${APP_SRCS}
    INCLUDE_DIRS ${APP_INCLUDE_DIRS}
    REQUIRES
        M5GFX
        M5Unified
        app_update
        bt
        lvgl__lvgl
        esp_event
        esp_netif
        esp_http_server
        esp_wifi
        fatfs
        nvs_flash
        sdmmc
        vfs
)
