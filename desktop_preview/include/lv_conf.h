#pragma once

/* Native preview configuration.  LVGL supplies the remaining defaults from
 * lv_conf_internal.h; these switches mirror the fonts enabled by the ESP-IDF
 * firmware so the desktop renderer uses the same glyph set. */
#define LV_COLOR_DEPTH 16
#define LV_FONT_SOURCE_HAN_SANS_SC_14_CJK 1
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
