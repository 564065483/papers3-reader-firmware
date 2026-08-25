#include "ui_renderer.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <string>

#include "lvgl.h"

LV_FONT_DECLARE(lv_font_ui_16);
LV_FONT_DECLARE(lv_font_ui_24);
LV_FONT_DECLARE(lv_font_reader_20);
LV_FONT_DECLARE(lv_font_reader_24);
LV_FONT_DECLARE(lv_font_ui_latin_16);
LV_FONT_DECLARE(lv_font_ui_kr_16);
LV_FONT_DECLARE(lv_font_source_han_sans_sc_16_cjk);
LV_FONT_DECLARE(lv_font_ui_fallback_16);
LV_IMAGE_DECLARE(papers3_cover_small);
LV_IMAGE_DECLARE(papers3_cover_hero);

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace papers3 {

namespace {
constexpr int W = 540;
constexpr int H = 960;
constexpr int STATUS_H = 48;
constexpr int NAV_Y = 858;
constexpr int NAV_H = 72;
constexpr int PAD = 24;

M5GFX* g_gfx = nullptr;
lv_display_t* g_display = nullptr;
void* g_drawBuf = nullptr;
std::uint32_t g_lastTick = 0;
int g_lvWidth = W;
int g_lvHeight = H;

struct Palette {
    lv_color_t bg;
    lv_color_t card;
    lv_color_t card2;
    lv_color_t text;
    lv_color_t muted;
    lv_color_t line;
    lv_color_t active;
    lv_color_t activeText;
};

Palette palette(Theme theme)
{
    if (theme == Theme::Dark) {
        return {
            lv_color_hex(0x0f1110),
            lv_color_hex(0x1a1d1b),
            lv_color_hex(0x252925),
            lv_color_hex(0xf4f4f0),
            lv_color_hex(0xb8bbb5),
            lv_color_hex(0x3a3d39),
            lv_color_hex(0x343734),
            lv_color_hex(0xf4f4f0),
        };
    }
    return {
        lv_color_hex(0xe9edeb),
        lv_color_hex(0xfbfcfa),
        lv_color_hex(0xf0f3f1),
        lv_color_hex(0x151515),
        lv_color_hex(0x666a66),
        lv_color_hex(0xcbd1cd),
        lv_color_hex(0x171717),
        lv_color_hex(0xffffff),
    };
}

SystemLanguage g_language = SystemLanguage::Chinese;

std::string localizeText(const std::string& text)
{
    if (g_language != SystemLanguage::Vietnamese) return text;
    struct Translation { const char* zh; const char* vi; };
    static const Translation translations[] = {
        {"首页", "Trang chủ"}, {"书架", "Tủ sách"}, {"工具", "Công cụ"}, {"设置", "Cài đặt"},
        {"系统", "Hệ thống"}, {"最近阅读", "Đọc gần đây"}, {"继续阅读", "Đọc tiếp"},
        {"累计阅读", "Tổng thời gian đọc"}, {"本月阅读", "Đọc trong tháng"}, {"今日阅读", "Đọc hôm nay"},
        {"Wi‑Fi", "Wi‑Fi"}, {"蓝牙", "Bluetooth"}, {"显示", "Hiển thị"}, {"字体", "Phông chữ"},
        {"壁纸", "Hình nền"}, {"存储", "Lưu trữ"}, {"固件升级", "Nâng cấp firmware"}, {"通用", "Cài đặt chung"},
        {"网络连接", "Kết nối mạng"}, {"设备连接", "Kết nối thiết bị"}, {"主题、休眠", "Chủ đề, nghỉ"},
        {"系统字体", "Phông chữ hệ thống"}, {"待机壁纸", "Hình nền chờ"}, {"SD 卡", "Thẻ SD"},
        {"语言、时间、重置", "Ngôn ngữ, thời gian, đặt lại"}, {"日历", "Lịch"}, {"番茄钟", "Pomodoro"},
        {"倒计时", "Đếm ngược"}, {"字体测试", "Thử phông chữ"}, {"文件管理", "Quản lý tệp"},
        {"文件传输", "Truyền tệp"}, {"固件自检", "Tự kiểm tra"}, {"年月日查看", "Xem ngày tháng"},
        {"专注与休息循环", "Chu kỳ tập trung và nghỉ"}, {"多个计时器", "Nhiều bộ hẹn giờ"},
        {"多语言字体预览", "Xem trước đa ngôn ngữ"}, {"浏览、导入、重命名", "Duyệt, nhập, đổi tên"},
        {"Wi‑Fi 二维码上传", "Tải lên bằng mã QR Wi‑Fi"}, {"模拟功能状态", "Trạng thái mô phỏng"},
        {"最近阅读", "Đọc gần đây"}, {"全部", "Tất cả"}, {"收藏", "Yêu thích"},
        {"搜索书名、作者或格式", "Tìm tên sách, tác giả hoặc định dạng"},
        {"上一页", "Trang trước"}, {"下一页", "Trang sau"}, {"日", "CN"}, {"一", "T2"},
        {"二", "T3"}, {"三", "T4"}, {"四", "T5"}, {"五", "T6"}, {"六", "T7"},
        {"白色主题", "Chủ đề sáng"}, {"黑色主题", "Chủ đề tối"}, {"竖屏", "Dọc"}, {"横屏", "Ngang"},
        {"状态栏", "Thanh trạng thái"}, {"显示", "Hiển thị"}, {"隐藏", "Ẩn"},
        {"休眠时间 · 5 分钟", "Ngủ sau · 5 phút"}, {"休眠时间 · 15 分钟", "Ngủ sau · 15 phút"},
        {"关机时间 · 2 小时", "Tắt máy sau · 2 giờ"}, {"系统语言", "Ngôn ngữ hệ thống"},
        {"中文", "Tiếng Trung"}, {"Ngôn ngữ", "Ngôn ngữ"}, {"网络校时", "Đồng bộ thời gian mạng"},
        {"自动重连", "Tự động kết nối lại"}, {"Wi‑Fi 低功耗", "Wi‑Fi tiết kiệm pin"}, {"时区", "Múi giờ"},
        {"纸感晨雾", "Sương sớm trên giấy"}, {"墨色山影", "Bóng núi mực"}, {"极简时钟", "Đồng hồ tối giản"},
        {"从 SD 卡读取", "Đọc từ thẻ SD"}, {"开启", "Bật"}, {"关闭", "Tắt"}, {"当前", "Hiện tại"},
        {"扫描网络", "Quét mạng"}, {"扫描设备", "Quét thiết bị"}, {"点击刷新附近网络", "Chạm để quét mạng gần đây"},
        {"点击扫描", "Chạm để quét"}, {"密码", "Mật khẩu"}, {"点击输入", "Chạm để nhập"},
        {"连接", "Kết nối"}, {"断开连接", "Ngắt kết nối"}, {"开始", "Bắt đầu"}, {"暂停", "Tạm dừng"},
        {"重置", "Đặt lại"}, {"添加", "Thêm"}, {"调整", "Điều chỉnh"}, {"重命名", "Đổi tên"},
        {"删除", "Xóa"}, {"打开", "Mở"}, {"阅读", "Đọc"}, {"目录", "Thư mục"}, {"上一级", "Cấp trên"},
        {"刷新存储", "Làm mới bộ nhớ"}, {"打开文件管理", "Mở quản lý tệp"}, {"安装并重启", "Cài đặt và khởi động lại"},
        {"开始检测", "Bắt đầu kiểm tra"}, {"设备自检", "Tự kiểm tra thiết bị"}, {"输入内容", "Nội dung nhập"},
        {"阅读进度", "Tiến độ đọc"}, {"章节", "Chương"}, {"选择字体", "Chọn phông chữ"},
        {"翻页方式", "Cách lật trang"}, {"滑动", "Vuốt"}, {"点击", "Chạm"}, {"仿真翻页", "Lật mô phỏng"},
        {"翻转感应", "Cảm biến lật"}, {"字号", "Cỡ chữ"}, {"边距", "Lề"}, {"行距", "Khoảng dòng"},
        {"已同步", "Đã đồng bộ"}, {"已连接", "Đã kết nối"}, {"已开启", "Đã bật"}, {"已关闭", "Đã tắt"},
        {"控制中心", "Trung tâm điều khiển"}, {"文件传输", "Truyền tệp"}, {"开启后生成 Wi‑Fi 上传页面", "Bật để tạo trang tải lên Wi‑Fi"},
        {"设备会创建临时 Wi‑Fi 热点", "Thiết bị sẽ tạo điểm phát Wi‑Fi tạm thời"},
        {"手机连接热点后打开上方地址", "Kết nối điểm phát rồi mở địa chỉ ở trên"},
        {"系统字体", "Phông chữ hệ thống"}, {"内置粗体", "Đậm tích hợp"}, {"内置斜体", "Nghiêng tích hợp"},
    };
    for (const auto& item : translations) if (text == item.zh) return item.vi;
    if (text.rfind("已选择 ", 0) == 0) return "Đã chọn " + text.substr(std::string("已选择 ").size());
    if (text == "Wi‑Fi 开") return "Wi‑Fi Bật";
    if (text == "Wi‑Fi 关") return "Wi‑Fi Tắt";
    const std::string bookSuffix = " 本";
    if (text.size() > bookSuffix.size() && text.compare(text.size() - bookSuffix.size(), bookSuffix.size(), bookSuffix) == 0) {
        return text.substr(0, text.size() - bookSuffix.size()) + " cuốn";
    }
    const std::string pageSuffix = " 页";
    const auto pageAt = text.find(pageSuffix);
    if (pageAt != std::string::npos) {
        std::string translated = text;
        translated.replace(pageAt, pageSuffix.size(), " trang");
        return translated;
    }
    return text;
}

/* A small UI-specific subset keeps every string used by the prototype
 * legible without pulling a full multi-megabyte CJK font into the app. */
const lv_font_t* fontCn() { return &lv_font_ui_16; }
const lv_font_t* fontSmall() { return &lv_font_ui_16; }
const lv_font_t* fontTitle() { return &lv_font_ui_24; }
const lv_font_t* fontNum() { return &lv_font_montserrat_48; }
const lv_font_t* fontLatin20() { return &lv_font_montserrat_20; }
const lv_font_t* fontLatin24() { return &lv_font_montserrat_24; }
const lv_font_t* readerFont(const AppState& state)
{
    if (state.reader.fontSize >= 22) return &lv_font_reader_24;
    if (state.reader.fontSize >= 19) return &lv_font_reader_20;
    return &lv_font_ui_16;
}

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap)
{
    if (!g_gfx) {
        lv_display_flush_ready(disp);
        return;
    }
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;
    g_gfx->startWrite();
    g_gfx->pushImage(area->x1, area->y1, width, height, reinterpret_cast<const uint16_t*>(pxMap));
    g_gfx->endWrite();
    lv_display_flush_ready(disp);
}

void initLvgl(PaperS3Hal& hal, int width, int height)
{
    g_gfx = &hal.display();
    const auto now = hal.millis();
    if (!g_display) {
        lv_init();
#ifdef ESP_PLATFORM
        g_drawBuf = heap_caps_malloc(960 * 60 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!g_drawBuf) g_drawBuf = heap_caps_malloc(960 * 40 * sizeof(lv_color_t), MALLOC_CAP_8BIT);
#else
        static lv_color_t fallback[960 * 40];
        g_drawBuf = fallback;
#endif
        g_display = lv_display_create(width, height);
        lv_display_set_color_format(g_display, LV_COLOR_FORMAT_RGB565_SWAPPED);
        lv_display_set_flush_cb(g_display, flushCb);
        lv_display_set_buffers(g_display, g_drawBuf, nullptr, 960 * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
        g_lastTick = now;
        g_lvWidth = width;
        g_lvHeight = height;
    } else if (g_lvWidth != width || g_lvHeight != height) {
        lv_display_set_resolution(g_display, width, height);
        g_lvWidth = width;
        g_lvHeight = height;
    }
    if (now >= g_lastTick) lv_tick_inc(now - g_lastTick);
    g_lastTick = now;
}

lv_obj_t* obj(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t bg, int radius, lv_color_t border, int borderW = 1)
{
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, bg, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_border_color(o, border, 0);
    lv_obj_set_style_border_width(o, borderW, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* Keep the paper surface quiet but dimensional.  The two-stop gradient and
 * very small shadow reproduce the prototype's depth without relying on
 * photographs or heavy effects that would be expensive on the e-ink panel. */
lv_obj_t* paperCard(lv_obj_t* parent, int x, int y, int w, int h, const Palette& p, int radius = 18)
{
    lv_obj_t* o = obj(parent, x, y, w, h, p.card, radius, p.line);
    lv_obj_set_style_bg_grad_color(o, p.card2, 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_shadow_color(o, lv_color_hex(0x6f7772), 0);
    lv_obj_set_style_shadow_width(o, 6, 0);
    lv_obj_set_style_shadow_opa(o, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(o, 2, 0);
    return o;
}

lv_obj_t* paperInset(lv_obj_t* parent, int x, int y, int w, int h, const Palette& p, int radius = 12)
{
    lv_obj_t* o = paperCard(parent, x, y, w, h, p, radius);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    return o;
}

void coverImage(lv_obj_t* parent, const lv_image_dsc_t* source, int x, int y)
{
    lv_obj_t* image = lv_image_create(parent);
    lv_image_set_src(image, source);
    lv_obj_set_pos(image, x, y);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_SCROLLABLE);
}

std::uint32_t nextCodepoint(const std::string& text, std::size_t& offset)
{
    if (offset >= text.size()) return 0;
    const auto byte = static_cast<unsigned char>(text[offset++]);
    if (byte < 0x80) return byte;
    if ((byte & 0xe0) == 0xc0 && offset < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[offset++]);
        if ((b1 & 0xc0) == 0x80) return ((byte & 0x1f) << 6) | (b1 & 0x3f);
    }
    if ((byte & 0xf0) == 0xe0 && offset + 1 < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[offset++]);
        const auto b2 = static_cast<unsigned char>(text[offset++]);
        if ((b1 & 0xc0) == 0x80 && (b2 & 0xc0) == 0x80) {
            return ((byte & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f);
        }
    }
    if ((byte & 0xf8) == 0xf0 && offset + 2 < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[offset++]);
        const auto b2 = static_cast<unsigned char>(text[offset++]);
        const auto b3 = static_cast<unsigned char>(text[offset++]);
        if ((b1 & 0xc0) == 0x80 && (b2 & 0xc0) == 0x80 && (b3 & 0xc0) == 0x80) {
            return ((byte & 0x07) << 18) | ((b1 & 0x3f) << 12) |
                   ((b2 & 0x3f) << 6) | (b3 & 0x3f);
        }
    }
    return 0xfffd;
}

bool fontHasText(const lv_font_t* font, const std::string& text)
{
    if (!font) return false;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto codepoint = nextCodepoint(text, offset);
        lv_font_glyph_dsc_t glyph{};
        if (!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0) || glyph.is_placeholder) return false;
    }
    return true;
}

const lv_font_t* fontForText(const std::string& text, const lv_font_t* requested)
{
    // The compact UI fonts intentionally contain the most-used labels and
    // icons. For a newly added label, use LVGL's bundled Source Han fallback
    // instead of allowing a missing-glyph box to appear on the device.
    if ((requested == &lv_font_ui_16 || requested == &lv_font_ui_24 ||
         requested == &lv_font_reader_20 || requested == &lv_font_reader_24) && !fontHasText(requested, text)) {
        if (fontHasText(&lv_font_ui_fallback_16, text)) return &lv_font_ui_fallback_16;
        if (fontHasText(&lv_font_source_han_sans_sc_16_cjk, text)) return &lv_font_source_han_sans_sc_16_cjk;
    }
    return requested;
}

lv_obj_t* label(lv_obj_t* parent, const std::string& text, int x, int y, int w, int h, lv_color_t color,
                const lv_font_t* font, lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
{
    const std::string displayText = localizeText(text);
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_size(l, w, h);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_font(l, fontForText(displayText, font), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, displayText.c_str());
    return l;
}

lv_obj_t* line(lv_obj_t* parent, int x, int y, int w, lv_color_t color)
{
    return obj(parent, x, y, w, 1, color, 0, color, 0);
}

void magnifier(lv_obj_t* parent, int x, int y, const Palette& p)
{
    lv_obj_t* ring = obj(parent, x, y, 16, 16, p.card, 8, p.muted, 2);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    static lv_point_precise_t handlePoints[] = {{0, 0}, {9, 9}};
    lv_obj_t* handle = lv_line_create(parent);
    lv_line_set_points(handle, handlePoints, 2);
    lv_obj_set_pos(handle, x + 12, y + 12);
    lv_obj_set_size(handle, 12, 12);
    lv_obj_set_style_line_color(handle, p.muted, 0);
    lv_obj_set_style_line_width(handle, 2, 0);
    lv_obj_set_style_line_rounded(handle, true, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
}

void chevron(lv_obj_t* parent, int x, int y, const Palette& p)
{
    static lv_point_precise_t points[] = {{0, 0}, {6, 6}, {0, 12}};
    lv_obj_t* mark = lv_line_create(parent);
    lv_line_set_points(mark, points, 3);
    lv_obj_set_pos(mark, x, y);
    lv_obj_set_size(mark, 8, 14);
    lv_obj_set_style_line_color(mark, p.muted, 0);
    lv_obj_set_style_line_width(mark, 2, 0);
    lv_obj_set_style_line_rounded(mark, true, 0);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_SCROLLABLE);
}

void directionalChevron(lv_obj_t* parent, int x, int y, bool left, const Palette& p)
{
    static lv_point_precise_t rightPoints[] = {{0, 0}, {6, 6}, {0, 12}};
    static lv_point_precise_t leftPoints[] = {{6, 0}, {0, 6}, {6, 12}};
    lv_obj_t* mark = lv_line_create(parent);
    lv_line_set_points(mark, left ? leftPoints : rightPoints, 3);
    lv_obj_set_pos(mark, x, y);
    lv_obj_set_size(mark, 8, 14);
    lv_obj_set_style_line_color(mark, p.muted, 0);
    lv_obj_set_style_line_width(mark, 2, 0);
    lv_obj_set_style_line_rounded(mark, true, 0);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_SCROLLABLE);
}

void backArrow(lv_obj_t* parent, int x, int y, const Palette& p)
{
    static lv_point_precise_t head[] = {{16, 0}, {6, 10}, {16, 20}};
    static lv_point_precise_t shaft[] = {{6, 10}, {26, 10}};
    lv_obj_t* headLine = lv_line_create(parent);
    lv_line_set_points(headLine, head, 3);
    lv_obj_set_pos(headLine, x, y);
    lv_obj_set_size(headLine, 20, 22);
    lv_obj_set_style_line_color(headLine, p.text, 0);
    lv_obj_set_style_line_width(headLine, 2, 0);
    lv_obj_set_style_line_rounded(headLine, true, 0);
    lv_obj_clear_flag(headLine, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* shaftLine = lv_line_create(parent);
    lv_line_set_points(shaftLine, shaft, 2);
    lv_obj_set_pos(shaftLine, x, y);
    lv_obj_set_size(shaftLine, 28, 22);
    lv_obj_set_style_line_color(shaftLine, p.text, 0);
    lv_obj_set_style_line_width(shaftLine, 2, 0);
    lv_obj_set_style_line_rounded(shaftLine, true, 0);
    lv_obj_clear_flag(shaftLine, LV_OBJ_FLAG_SCROLLABLE);
}

void fileOutline(lv_obj_t* parent, int x, int y, bool folder, const Palette& p)
{
    lv_obj_t* icon = obj(parent, x, y + (folder ? 4 : 0), 22, folder ? 16 : 22,
                         p.card, 3, p.text, 2);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    if (folder) {
        lv_obj_t* tab = obj(parent, x + 3, y + 1, 9, 5, p.card, 2, p.text, 2);
        lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, 0);
    }
}

void vectorLine(lv_obj_t* parent, const lv_point_precise_t* points, std::uint32_t count,
                int x, int y, int w, int h, lv_color_t color, int width = 2)
{
    lv_obj_t* shape = lv_line_create(parent);
    lv_line_set_points(shape, points, count);
    lv_obj_set_pos(shape, x, y);
    lv_obj_set_size(shape, w, h);
    lv_obj_set_style_line_color(shape, color, 0);
    lv_obj_set_style_line_width(shape, width, 0);
    lv_obj_set_style_line_rounded(shape, true, 0);
    lv_obj_clear_flag(shape, LV_OBJ_FLAG_SCROLLABLE);
}

void vectorDot(lv_obj_t* parent, int x, int y, int size, lv_color_t color)
{
    obj(parent, x, y, size, size, color, size / 2, color, 0);
}

void vectorOutline(lv_obj_t* parent, int x, int y, int w, int h, int radius, lv_color_t color)
{
    lv_obj_t* shape = obj(parent, x, y, w, h, color, radius, color, 2);
    lv_obj_set_style_bg_opa(shape, LV_OPA_TRANSP, 0);
}

bool vectorIcon(lv_obj_t* parent, const std::string& name, int x, int y, int w, int h, lv_color_t color)
{
    static lv_point_precise_t homeRoof[] = {{2, 9}, {10, 2}, {18, 9}};
    static lv_point_precise_t homeBody[] = {{4, 8}, {4, 19}, {16, 19}, {16, 8}};
    static lv_point_precise_t homeDoor[] = {{9, 19}, {9, 13}, {11, 13}, {11, 19}};
    static lv_point_precise_t calendarHeader[] = {{3, 7}, {19, 7}};
    static lv_point_precise_t calendarPins[] = {{6, 2}, {6, 6}, {16, 2}, {16, 6}};
    static lv_point_precise_t listRows[] = {{7, 3}, {19, 3}, {7, 10}, {19, 10}, {7, 17}, {19, 17}};
    static lv_point_precise_t wifiTop[] = {{2, 8}, {6, 4}, {10, 3}, {14, 4}, {18, 8}};
    static lv_point_precise_t wifiMid[] = {{5, 12}, {8, 9}, {12, 9}, {15, 12}};
    static lv_point_precise_t bluetooth[] = {{10, 1}, {10, 19}, {17, 7}, {3, 13}, {17, 13}, {3, 7}, {10, 1}};
    static lv_point_precise_t imageMountain[] = {{3, 17}, {8, 11}, {12, 15}, {15, 11}, {20, 17}};
    static lv_point_precise_t refresh[] = {{18, 7}, {18, 3}, {14, 3}, {18, 3}, {18, 7},
                                           {15, 17}, {6, 17}, {3, 14}, {3, 8}, {6, 5}, {11, 5}};
    static lv_point_precise_t uploadArrow[] = {{11, 15}, {11, 3}, {6, 8}, {11, 3}, {16, 8}};
    static lv_point_precise_t uploadTray[] = {{3, 15}, {3, 19}, {19, 19}, {19, 15}};
    static lv_point_precise_t bell[] = {{5, 16}, {5, 9}, {7, 5}, {10, 3}, {13, 5}, {15, 9}, {15, 16}, {5, 16}};
    static lv_point_precise_t bellClapper[] = {{8, 19}, {12, 19}};
    static lv_point_precise_t tool[] = {{4, 18}, {9, 13}, {13, 17}, {19, 11}, {15, 7}, {17, 5}, {14, 2}, {11, 5}, {13, 7}, {8, 12}, {4, 18}};
    static lv_point_precise_t loop[] = {{5, 8}, {8, 4}, {15, 4}, {19, 8}, {15, 8}, {19, 8}, {19, 4},
                                        {15, 16}, {12, 20}, {5, 20}, {1, 16}, {5, 16}, {1, 16}, {1, 20}};
    static lv_point_precise_t bookmark[] = {{5, 2}, {17, 2}, {17, 20}, {11, 15}, {5, 20}, {5, 2}};
    static lv_point_precise_t textT[] = {{3, 3}, {19, 3}, {11, 3}, {11, 20}};
    static lv_point_precise_t bars[] = {{3, 4}, {19, 4}, {3, 10}, {19, 10}, {3, 16}, {19, 16}};
    static lv_point_precise_t check[] = {{3, 11}, {8, 16}, {19, 5}};
    static lv_point_precise_t star[] = {{10, 1}, {12, 7}, {19, 7}, {14, 11}, {16, 18},
                                        {10, 14}, {4, 18}, {6, 11}, {1, 7}, {8, 7}, {10, 1}};
    static lv_point_precise_t globeMeridian[] = {{10, 2}, {6, 6}, {6, 14}, {10, 18},
                                                 {14, 14}, {14, 6}, {10, 2}};
    static lv_point_precise_t globeEquator[] = {{2, 10}, {6, 7}, {14, 7}, {18, 10},
                                                {14, 13}, {6, 13}, {2, 10}};
    static lv_point_precise_t backspaceArrow[] = {{9, 10}, {14, 5}, {9, 10}, {14, 15}, {9, 10}, {19, 10}};
    static lv_point_precise_t closeCrossA[] = {{4, 4}, {20, 20}};
    static lv_point_precise_t closeCrossB[] = {{20, 4}, {4, 20}};

    if (name == "home") {
        vectorLine(parent, homeRoof, 3, x, y, w, h, color);
        vectorLine(parent, homeBody, 4, x, y, w, h, color);
        vectorLine(parent, homeDoor, 4, x, y, w, h, color);
    } else if (name == "calendar") {
        vectorOutline(parent, x + 2, y + 3, w - 4, h - 5, 2, color);
        vectorLine(parent, calendarHeader, 2, x, y, w, h, color);
        vectorLine(parent, calendarPins, 4, x, y, w, h, color, 1);
    } else if (name == "list") {
        vectorLine(parent, listRows, 6, x, y, w, h, color);
        for (int row = 0; row < 3; ++row) vectorDot(parent, x + 1, y + 2 + row * 7, 3, color);
    } else if (name == "wifi") {
        vectorLine(parent, wifiTop, 5, x, y, w, h, color);
        vectorLine(parent, wifiMid, 4, x, y, w, h, color);
        vectorDot(parent, x + w / 2 - 2, y + h - 5, 4, color);
    } else if (name == "bluetooth") {
        vectorLine(parent, bluetooth, 7, x, y, w, h, color);
    } else if (name == "image") {
        vectorOutline(parent, x + 1, y + 2, w - 2, h - 4, 2, color);
        vectorLine(parent, imageMountain, 5, x, y, w, h, color);
        vectorDot(parent, x + w - 7, y + 6, 3, color);
    } else if (name == "edit" || name == "tool") {
        vectorLine(parent, tool, 11, x, y, w, h, color);
    } else if (name == "settings") {
        vectorOutline(parent, x + 4, y + 4, w - 8, h - 8, (w - 8) / 2, color);
        static lv_point_precise_t spokeTop[] = {{10, 0}, {10, 4}};
        static lv_point_precise_t spokeBottom[] = {{10, 16}, {10, 20}};
        static lv_point_precise_t spokeLeft[] = {{0, 10}, {4, 10}};
        static lv_point_precise_t spokeRight[] = {{16, 10}, {20, 10}};
        static lv_point_precise_t spokeNW[] = {{3, 3}, {6, 6}};
        static lv_point_precise_t spokeNE[] = {{17, 3}, {14, 6}};
        static lv_point_precise_t spokeSW[] = {{3, 17}, {6, 14}};
        static lv_point_precise_t spokeSE[] = {{17, 17}, {14, 14}};
        vectorLine(parent, spokeTop, 2, x, y, w, h, color, 1);
        vectorLine(parent, spokeBottom, 2, x, y, w, h, color, 1);
        vectorLine(parent, spokeLeft, 2, x, y, w, h, color, 1);
        vectorLine(parent, spokeRight, 2, x, y, w, h, color, 1);
        vectorLine(parent, spokeNW, 2, x, y, w, h, color, 1);
        vectorLine(parent, spokeNE, 2, x, y, w, h, color, 1);
        vectorLine(parent, spokeSW, 2, x, y, w, h, color, 1);
        vectorLine(parent, spokeSE, 2, x, y, w, h, color, 1);
        vectorDot(parent, x + w / 2 - 2, y + h / 2 - 2, 4, color);
    } else if (name == "sd") {
        vectorOutline(parent, x + 3, y + 2, w - 6, h - 4, 2, color);
        static lv_point_precise_t sdSlots[] = {{7, 3}, {7, 8}, {11, 3}, {11, 8}, {15, 3}, {15, 8}};
        vectorLine(parent, sdSlots, 6, x, y, w, h, color, 1);
    } else if (name == "refresh" || name == "loop") {
        vectorLine(parent, name == "loop" ? loop : refresh, name == "loop" ? 14 : 11, x, y, w, h, color);
    } else if (name == "bell") {
        vectorLine(parent, bell, 8, x, y, w, h, color);
        vectorLine(parent, bellClapper, 2, x, y, w, h, color);
    } else if (name == "stop") {
        vectorOutline(parent, x + 3, y + 3, w - 6, h - 6, 2, color);
    } else if (name == "directory") {
        vectorOutline(parent, x + 2, y + 5, w - 4, h - 8, 2, color);
        static lv_point_precise_t folderTab[] = {{3, 5}, {3, 2}, {10, 2}, {12, 5}};
        vectorLine(parent, folderTab, 4, x, y, w, h, color);
    } else if (name == "upload") {
        vectorLine(parent, uploadArrow, 5, x, y, w, h, color);
        vectorLine(parent, uploadTray, 4, x, y, w, h, color);
    } else if (name == "bars") {
        vectorLine(parent, bars, 6, x, y, w, h, color);
    } else if (name == "check") {
        vectorLine(parent, check, 3, x, y, w, h, color);
    } else if (name == "bookmark") {
        vectorLine(parent, bookmark, 6, x, y, w, h, color);
    } else if (name == "text") {
        vectorLine(parent, textT, 4, x, y, w, h, color);
    } else if (name == "star") {
        vectorLine(parent, star, 11, x, y, w, h, color);
    } else if (name == "globe") {
        vectorOutline(parent, x + 1, y + 1, w - 2, h - 2, (w - 2) / 2, color);
        vectorLine(parent, globeMeridian, 7, x, y, w, h, color, 1);
        vectorLine(parent, globeEquator, 7, x, y, w, h, color, 1);
    } else if (name == "backspace") {
        static lv_point_precise_t body[] = {{7, 3}, {20, 3}, {20, 17}, {7, 17}, {2, 10}, {7, 3}};
        vectorLine(parent, body, 6, x, y, w, h, color);
        vectorLine(parent, backspaceArrow, 6, x, y, w, h, color, 1);
    } else if (name == "close") {
        vectorLine(parent, closeCrossA, 2, x, y, w, h, color);
        vectorLine(parent, closeCrossB, 2, x, y, w, h, color);
    } else {
        return false;
    }
    return true;
}

std::string two(int v)
{
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%02d", v);
    return buf;
}

std::tm nowTm()
{
    std::time_t now = std::time(nullptr);
    std::tm out {};
    localtime_r(&now, &out);
    return out;
}

std::string dateText()
{
    auto t = nowTm();
    char buf[24];
    if (g_language == SystemLanguage::Vietnamese) {
        static const char* days[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
        std::snprintf(buf, sizeof(buf), "%02d/%02d %s", t.tm_mon + 1, t.tm_mday,
                      days[std::clamp(t.tm_wday, 0, 6)]);
    } else {
        static const char* days[] = {"日", "一", "二", "三", "四", "五", "六"};
        std::snprintf(buf, sizeof(buf), "%02d/%02d周%s", t.tm_mon + 1, t.tm_mday,
                      days[std::clamp(t.tm_wday, 0, 6)]);
    }
    return buf;
}

std::string clockText()
{
    auto t = nowTm();
    return two(t.tm_hour) + ":" + two(t.tm_min);
}

std::string fullDateText()
{
    auto t = nowTm();
    char buf[64];
    if (g_language == SystemLanguage::Vietnamese) {
        std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    } else {
        std::snprintf(buf, sizeof(buf), "%04d年%d月%d日", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }
    return buf;
}

std::string secText(std::uint32_t sec)
{
    if (g_language == SystemLanguage::Vietnamese) {
        if (sec >= 3600) return std::to_string(sec / 3600) + " giờ " + std::to_string((sec % 3600) / 60) + " phút";
        return std::to_string(sec / 60) + " phút";
    }
    if (sec >= 3600) return std::to_string(sec / 3600) + " 小时 " + std::to_string((sec % 3600) / 60) + " 分";
    return std::to_string(sec / 60) + " 分钟";
}

int currentBookIndex(const AppState& state)
{
    if (state.selectedBook >= 0 && state.selectedBook < static_cast<int>(state.books.size())) return state.selectedBook;
    return state.books.empty() ? -1 : 0;
}

std::vector<int> filteredBooks(const AppState& state)
{
    std::vector<int> indexes;
    for (int i = 0; i < static_cast<int>(state.books.size()); ++i) {
        const bool fav = state.books[i].favorite;
        if (state.bookshelfFilter == 2 && !fav) continue;
        if (state.bookshelfTagFilter == 1 && state.books[i].tag != "待读") continue;
        if (state.bookshelfTagFilter == 2 && state.books[i].tag != "进行中") continue;
        if (state.bookshelfTagFilter == 3 && state.books[i].tag != "已完成") continue;
        indexes.push_back(i);
    }
    if (state.bookshelfFilter == 0) {
        std::sort(indexes.begin(), indexes.end(), [&](int a, int b) {
            return state.books[a].lastOpenedEpoch > state.books[b].lastOpenedEpoch;
        });
    }
    return indexes;
}

const char* bookTagFilterName(int filter)
{
    static const char* names[] = {"全部标签", "待读", "进行中", "已完成"};
    return names[std::clamp(filter, 0, 3)];
}

void bar(lv_obj_t* parent, int x, int y, int w, int h, int value, const Palette& p)
{
    lv_obj_t* b = lv_bar_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, p.line, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(b, h / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, p.active, LV_PART_INDICATOR);
    lv_obj_set_style_radius(b, h / 2, LV_PART_INDICATOR);
    lv_bar_set_range(b, 0, 100);
    lv_bar_set_value(b, std::clamp(value, 0, 100), LV_ANIM_OFF);
}

void pillText(lv_obj_t* parent, const std::string& text, int x, int y, int w, int h, bool active, const Palette& p)
{
    obj(parent, x, y, w, h, active ? p.active : p.card, h / 2, p.line);
    label(parent, text, x, y + (h - 18) / 2, w, 22, active ? p.activeText : p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
}

void pillIcon(lv_obj_t* parent, const std::string& icon, int x, int y, int w, int h, bool active, const Palette& p)
{
    obj(parent, x, y, w, h, active ? p.active : p.card, h / 2, p.line);
    vectorIcon(parent, icon, x + (w - 22) / 2, y + (h - 22) / 2, 22, 22,
               active ? p.activeText : p.text);
}

void iconLabel(lv_obj_t* parent, const std::string& icon, const std::string& text, int x, int y, int w, int h, bool active, const Palette& p)
{
    /* Keep the nav as one continuous Apple-like bar.  Only the selected
     * segment gets a visible pill; inactive segments use the bar surface so
     * dark mode does not turn into a row of white capsules. */
    obj(parent, x, y, w, h, active ? p.active : p.card2, h / 2, active ? p.line : p.card2,
        active ? 1 : 0);
    if (!vectorIcon(parent, icon, x + (w - 24) / 2, y + 10, 24, 24, active ? p.activeText : p.text)) {
        label(parent, icon, x, y + 12, w, 22, active ? p.activeText : p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
    }
    // Keep the caption inside the 54px segment; the previous baseline fell
    // below the capsule on the 960px portrait viewport.
    label(parent, text, x, y + 34, w, 20, active ? p.activeText : p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
}

void drawBookCard(lv_obj_t* parent, const AppState& state, int bookIndex, int x, int y, int w, int h, const Palette& p)
{
    const BookInfo* book = bookIndex >= 0 && bookIndex < static_cast<int>(state.books.size()) ? &state.books[bookIndex] : nullptr;
    paperCard(parent, x, y, w, h, p, 14);
    const std::string title = book ? book->title : "电子书";
    const std::string author = book ? book->author : "Author";
    const bool hasCover = book && book->path.find("ming") != std::string::npos && w >= 100;
    if (hasCover) {
        coverImage(parent, &papers3_cover_small, x + (w - 92) / 2, y + 10);
    }
    /* A missing cover is still a designed cover: center its title and author
     * inside the same paper area instead of leaving a blank rectangle. */
    if (!hasCover) {
        label(parent, title, x + 16, y + 42, w - 32, 34, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
        label(parent, author, x + 16, y + 82, w - 32, 22, p.muted, fontSmall(), LV_TEXT_ALIGN_CENTER);
    }
    label(parent, title, x + 8, y + h - 58, w - 16, 22, p.text, fontCn());
    const int progress = book ? book->progressPercent : 0;
    const std::string tag = book && !book->tag.empty() ? " · " + book->tag : "";
    label(parent, std::to_string(progress) + "% - " + (book ? book->type : "EPUB") + tag,
          x + 8, y + h - 34, w - 16, 22, p.muted, fontSmall());
    bar(parent, x + 8, y + h - 9, w - 16, 4, progress, p);
    const bool favorite = book && book->favorite;
    obj(parent, x + w - 38, y + 10, 34, 34, favorite ? p.active : p.card, 17, p.line);
    vectorIcon(parent, "star", x + w - 32, y + 16, 22, 22, favorite ? p.activeText : p.text);
}

void drawRecentCard(lv_obj_t* parent, const AppState& state, int bookIndex, int x, int y, const Palette& p)
{
    drawBookCard(parent, state, bookIndex, x, y, 112, 176, p);
}

void drawReadingHero(lv_obj_t* parent, const AppState& state, int x, int y, int w, int h, const Palette& p)
{
    const int idx = currentBookIndex(state);
    const BookInfo* book = idx >= 0 ? &state.books[idx] : nullptr;
    paperCard(parent, x, y, w, h, p, 22);
    paperInset(parent, x + 18, y + 18, 144, h - 36, p, 8);
    const bool hasCover = book && book->path.find("ming") != std::string::npos;
    if (hasCover) {
        coverImage(parent, &papers3_cover_hero, x + 30, y + 30);
    } else {
        label(parent, book ? book->title : "明朝那些事儿", x + 26, y + 58, 128, 48, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
    }
    label(parent, "继续阅读", x + 172, y + 34, 220, 24, p.muted, fontCn());
    label(parent, book ? book->title : "明朝那些事儿", x + 180, y + 70, w - 206, 42, p.text, fontCn());
    const int progress = book ? book->progressPercent : 22;
    const int page = state.readerPageCount > 0 ? state.readerPage + 1 : 56;
    const int count = state.readerPageCount > 0 ? state.readerPageCount : 252;
    label(parent, std::to_string(page) + " / " + std::to_string(count) + " 页 - " + std::to_string(progress) + "%",
          x + 180, y + 118, w - 206, 24, p.muted, fontCn());
    bar(parent, x + 180, y + 145, w - 206, 7, progress, p);
}

void drawStatusBar(lv_obj_t* root, AppState& state, const Palette& p, bool reader = false)
{
    if (!state.system.statusBar || (reader && !state.readerChromeVisible)) return;
    obj(root, 0, 0, W, STATUS_H, p.card2, 0, p.line, 0);
    label(root, dateText() + "  " + clockText(), 24, 15, 210, 22, p.text, fontSmall());
    int right = W - 24;
    label(root, std::to_string(state.status.batteryPercent) + "%", right - 54, 15, 54, 22, p.muted, fontLatin20(), LV_TEXT_ALIGN_RIGHT);
    right -= 68;
    if (state.status.bluetoothConnected) {
        vectorIcon(root, "bluetooth", right - 24, 14, 24, 24, p.text);
        right -= 28;
    }
    if (state.status.wifiEnabled && state.status.wifiConnected) {
        vectorIcon(root, "wifi", right - 28, 14, 28, 24, p.text);
    }
}

void renderNav(lv_obj_t* root, const AppState& state, const Palette& p)
{
    paperCard(root, 24, NAV_Y, W - 48, NAV_H, p, 36);
    const Page pages[] = {Page::Home, Page::Bookshelf, Page::Tools, Page::Settings};
    const char* names[] = {"首页", "书架", "工具", "设置"};
    const char* icons[] = {"home", "list", "tool", "settings"};
    for (int i = 0; i < 4; ++i) {
        const bool active = state.page == pages[i];
        const int x = 30 + i * 120;
        iconLabel(root, icons[i], names[i], x, NAV_Y + 9, 108, 54, active, p);
    }
}

void drawHomePage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    lv_obj_t* clock = paperCard(root, 24, 70, W - 48, 170, p, 26);
    paperInset(clock, 24, 22, 150, 34, p, 17);
    label(clock, dateText().substr(0, 5), 38, 29, 64, 22, p.text, fontCn());
    label(clock, "已同步", 104, 29, 64, 22, p.muted, fontCn(), LV_TEXT_ALIGN_RIGHT);
    label(clock, clockText(), 24, 64, 240, 70, p.text, fontNum());
    label(clock, fullDateText(), W - 234, 112, 170, 24, p.text, fontCn(), LV_TEXT_ALIGN_RIGHT);
    label(clock, std::to_string(nowTm().tm_year + 1900), W - 120, 30, 60, 24, p.muted, fontLatin20(), LV_TEXT_ALIGN_RIGHT);

    drawReadingHero(root, state, 24, 258, W - 48, 150, p);
    ui.add({24, 258, W - 48, 150}, UiAction::BookOpen, currentBookIndex(state));

    paperCard(root, 24, 426, W - 48, 224, p, 22);
    label(root, "最近阅读", 40, 446, 170, 28, p.text, fontCn());
    label(root, std::to_string(std::min(4, static_cast<int>(state.books.size()))) + " 本", W - 100, 446, 60, 24, p.muted, fontCn(), LV_TEXT_ALIGN_RIGHT);
    auto indexes = filteredBooks(state);
    for (int i = 0; i < 4 && i < static_cast<int>(indexes.size()); ++i) {
        const int x = 40 + i * 120;
        drawRecentCard(root, state, indexes[i], x, 466, p);
        ui.add({x, 466, 112, 176}, UiAction::BookOpen, indexes[i]);
    }

    paperCard(root, 24, 666, W - 48, 126, p, 22);
    const char* statNames[] = {"累计阅读", "本月阅读", "今日阅读"};
    const std::uint32_t stats[] = {state.totalReadingSeconds, state.monthlyReadingSeconds, state.dailyReadingSeconds};
    for (int i = 0; i < 3; ++i) {
        const int x = 40 + i * 160;
        paperInset(root, x, 686, 144, 84, p, 14);
        label(root, statNames[i], x + 12, 702, 120, 22, p.muted, fontCn());
        label(root, secText(stats[i]), x + 12, 732, 122, 26, p.text, fontCn());
    }
}

void drawBooksPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    paperCard(root, 24, 68, W - 48, 48, p, 24);
    magnifier(root, 44, 80, p);
    label(root, state.searchText.empty() ? "搜索书名、作者或格式" : state.searchText, 82, 82, 390, 24, p.muted, fontCn());
    ui.add({24, 68, W - 48, 48}, UiAction::Search);

    const char* tabs[] = {"最近阅读", "全部", "收藏"};
    for (int i = 0; i < 3; ++i) {
        const int x = 24 + i * 164;
        pillText(root, tabs[i], x, 132, 154, 46, state.bookshelfFilter == i, p);
        ui.add({x, 132, 154, 46}, UiAction::BooksFilter, i);
    }

    pillText(root, std::string("标签 · ") + bookTagFilterName(state.bookshelfTagFilter), 344, 180, 172, 30,
             state.bookshelfTagFilter != 0, p);
    ui.add({344, 176, 172, 38}, UiAction::BooksTagFilter);

    auto indexes = filteredBooks(state);
    constexpr int perPage = 12;
    const int pageCount = std::max(1, (static_cast<int>(indexes.size()) + perPage - 1) / perPage);
    state.bookshelfPage = std::clamp(state.bookshelfPage, 0, pageCount - 1);
    const int start = state.bookshelfPage * perPage;
    for (int i = 0; i < perPage && start + i < static_cast<int>(indexes.size()); ++i) {
        const int col = i % 4;
        const int row = i / 4;
        const int x = 24 + col * 126;
        const int y = 202 + row * 190;
        drawBookCard(root, state, indexes[start + i], x, y, 112, 176, p);
        ui.add({x, y, 112, 176}, UiAction::BookOpen, indexes[start + i]);
        ui.add({x + 4, y + 136, 104, 36}, UiAction::BookTag, indexes[start + i]);
        ui.add({x + 78, y + 8, 34, 34}, UiAction::BookFavorite, indexes[start + i]);
    }
    pillText(root, "上一页", 38, 798, 110, 44, state.bookshelfPage > 0, p);
    pillText(root, std::to_string(state.bookshelfPage + 1) + " / " + std::to_string(pageCount) + " - " + std::to_string(indexes.size()) + " 本",
             190, 808, 160, 24, false, p);
    pillText(root, "下一页", 392, 798, 110, 44, state.bookshelfPage + 1 < pageCount, p);
    ui.add({38, 798, 110, 44}, UiAction::BooksPrevious);
    ui.add({392, 798, 110, 44}, UiAction::BooksNext);
}

void drawListPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p, bool settings)
{
    label(root, settings ? "设置" : "工具", 24, 72, 220, 44, p.text, fontTitle());
    label(root, settings ? "系统" : "9 个", W - 120, 84, 94, 24, p.muted, fontCn(), LV_TEXT_ALIGN_RIGHT);
    const char* settingNames[] = {"Wi‑Fi", "蓝牙", "显示", "字体", "壁纸", "存储", "固件升级", "通用"};
    const char* settingValues[] = {"网络连接", "设备连接", "主题、休眠", "系统字体", "待机壁纸", "SD 卡", "/ota/firmware.bin", "语言、时间、重置"};
    const char* toolNames[] = {"日历", "番茄钟", "倒计时", "待办", "电子相册", "字体测试", "文件管理", "文件传输", "固件自检"};
    const char* toolValues[] = {"年月日查看", "专注与休息循环", "多个计时器", "添加和完成事项", "浏览壁纸与图片", "多语言字体预览", "浏览、导入、重命名", "Wi‑Fi 二维码上传", "模拟功能状态"};
    const int count = settings ? 8 : 9;
    const int rowH = settings ? 68 : 64;
    paperCard(root, 24, 128, W - 48, count * rowH, p, 16);
    const char* settingIcons[] = {"wifi", "bluetooth", "image", "edit",
                                  "image", "sd", "refresh", "settings"};
    const char* toolIcons[] = {"calendar", "bell", "stop", "check", "image", "text",
                               "directory", "upload", "bars"};
    for (int i = 0; i < count; ++i) {
        const int y = 128 + i * rowH;
        if (i > 0) line(root, 54, y, W - 108, p.line);
        const std::string iconName = settings ? settingIcons[i] : toolIcons[i];
        if (!vectorIcon(root, iconName, 46, y + 20, 24, 24, p.text)) {
            label(root, iconName, 44, y + 22, 28, 24, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
        }
        label(root, settings ? settingNames[i] : toolNames[i], 86, y + 14, 160, 26, p.text, fontCn());
        label(root, settings ? settingValues[i] : toolValues[i], 238, y + 16, 230, 24, p.muted, fontCn(), LV_TEXT_ALIGN_RIGHT);
        chevron(root, W - 62, y + 27, p);
        ui.add({24, y, W - 48, rowH}, settings ? UiAction::SettingsItem : UiAction::ToolsItem, i);
    }
}

void choiceRow(lv_obj_t* root, UiRenderer& ui, int i, const std::string& title, const std::string& value, const Palette& p)
{
    const int y = 132 + i * 68;
    paperCard(root, 24, y, W - 48, 58, p, 14);
    label(root, title, 46, y + 18, 170, 24, p.text, fontCn());
    label(root, value, 226, y + 18, 250, 24, p.muted, fontCn(), LV_TEXT_ALIGN_RIGHT);
    ui.add({24, y, W - 48, 58}, UiAction::SettingChoice, i);
}

std::string timerText(std::uint32_t seconds)
{
    const auto minutes = seconds / 60;
    const auto secs = seconds % 60;
    char value[16] {};
    std::snprintf(value, sizeof(value), "%02u:%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(secs));
    return value;
}

int daysInMonth(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
    return days[std::clamp(month, 1, 12) - 1];
}

int firstWeekday(int year, int month)
{
    std::tm value {};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = 1;
    std::mktime(&value);
    return value.tm_wday;
}

void drawCalendarPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    paperCard(root, 24, 132, W - 48, 520, p, 20);
    label(root, std::to_string(state.calendar.year) + " 年 " + std::to_string(state.calendar.month) + " 月",
          42, 154, 260, 30, p.text, fontTitle());
    obj(root, 342, 146, 64, 42, p.card, 22, p.line);
    obj(root, 414, 146, 64, 42, p.card, 22, p.line);
    directionalChevron(root, 370, 160, true, p);
    directionalChevron(root, 442, 160, false, p);
    ui.add({342, 146, 64, 42}, UiAction::CalendarPrevious);
    ui.add({414, 146, 64, 42}, UiAction::CalendarNext);

    const char* weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    for (int col = 0; col < 7; ++col) {
        label(root, weekdays[col], 34 + col * 68, 214, 56, 24, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
    }
    const int offset = firstWeekday(state.calendar.year, state.calendar.month);
    const int total = daysInMonth(state.calendar.year, state.calendar.month);
    for (int day = 1; day <= total; ++day) {
        const int cell = offset + day - 1;
        const int col = cell % 7;
        const int row = cell / 7;
        const int x = 34 + col * 68;
        const int y = 250 + row * 54;
        pillText(root, std::to_string(day), x, y, 56, 42, state.calendar.selectedDay == day, p);
        ui.add({x, y, 56, 42}, UiAction::CalendarDay, day);
    }
    label(root, "已选择 " + std::to_string(state.calendar.year) + "/" +
                    std::to_string(state.calendar.month) + "/" + std::to_string(state.calendar.selectedDay),
          42, 594, W - 84, 24, p.muted, fontCn());
}

void drawPomodoroPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    paperCard(root, 24, 132, W - 48, 330, p, 22);
    label(root, state.pomodoro.focusMode ? "专注" : "休息", 44, 156, 160, 28, p.text, fontCn());
    label(root, "第 " + std::to_string(state.pomodoro.cycles) + " 个循环", W - 210, 160, 164, 22, p.muted, fontCn(), LV_TEXT_ALIGN_RIGHT);
    label(root, timerText(state.pomodoro.remainingSeconds), 44, 206, W - 88, 76, p.text, fontNum(), LV_TEXT_ALIGN_CENTER);
    pillText(root, state.pomodoro.focusMode ? "切换到休息" : "切换到专注", 44, 314, 204, 48, false, p);
    pillText(root, state.pomodoro.running ? "暂停" : "开始", 264, 314, 112, 48, true, p);
    pillText(root, "重置", 392, 314, 86, 48, false, p);
    ui.add({44, 314, 204, 48}, UiAction::PomodoroMode);
    ui.add({264, 314, 112, 48}, UiAction::PomodoroStartPause);
    ui.add({392, 314, 86, 48}, UiAction::PomodoroReset);
    label(root, "专注 " + std::to_string(state.pomodoro.focusMinutes) + " 分钟 · 休息 " +
                    std::to_string(state.pomodoro.breakMinutes) + " 分钟",
          44, 408, W - 88, 24, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
}

void drawTimersPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    pillText(root, "+ 添加", 386, 72, 116, 44, false, p);
    ui.add({386, 72, 116, 44}, UiAction::TimerAdd);
    const int count = std::min(4, static_cast<int>(state.timers.size()));
    for (int i = 0; i < count; ++i) {
        const auto& timer = state.timers[i];
        const int y = 132 + i * 62;
        const bool selected = i == state.selectedTimer;
        paperCard(root, 24, y, W - 48, 52, p, 14);
        label(root, timer.name, 42, y + 14, 150, 24, p.text, fontCn());
        label(root, timerText(timer.remainingSeconds), 204, y + 14, 110, 24, p.muted, fontLatin20(), LV_TEXT_ALIGN_CENTER);
        pillText(root, timer.running ? "暂停" : "开始", 326, y + 7, 76, 38, selected, p);
        pillText(root, "重置", 410, y + 7, 72, 38, false, p);
        ui.add({24, y, 300, 52}, UiAction::TimerSelect, i);
        ui.add({326, y + 7, 76, 38}, UiAction::TimerStartPause);
        ui.add({410, y + 7, 72, 38}, UiAction::TimerReset);
    }
    if (count == 0) label(root, "还没有计时器", 24, 160, W - 48, 28, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
    if (state.selectedTimer >= 0 && state.selectedTimer < count) {
        const int y = 132 + count * 62 + 24;
        const auto& timer = state.timers[state.selectedTimer];
        paperCard(root, 24, y, W - 48, 160, p, 18);
        label(root, "调整“" + timer.name + "”", 44, y + 20, 210, 24, p.text, fontCn());
        label(root, timerText(timer.durationSeconds), 44, y + 58, 140, 32, p.text, fontLatin24());
        pillText(root, "− 1 分钟", 204, y + 52, 104, 44, false, p);
        pillText(root, "+ 1 分钟", 318, y + 52, 104, 44, false, p);
        pillText(root, "重命名", 204, y + 104, 104, 38, false, p);
        pillText(root, "删除", 318, y + 104, 104, 38, false, p);
        ui.add({204, y + 52, 104, 44}, UiAction::TimerMinus);
        ui.add({318, y + 52, 104, 44}, UiAction::TimerPlus);
        ui.add({204, y + 104, 104, 38}, UiAction::TimerRename);
        ui.add({318, y + 104, 104, 38}, UiAction::TimerDelete);
    }
}

void drawTodoPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    pillText(root, "+ 添加", 386, 72, 116, 44, false, p);
    ui.add({386, 72, 116, 44}, UiAction::TodoAdd);
    const int count = std::min(6, static_cast<int>(state.todos.size()));
    for (int i = 0; i < count; ++i) {
        const auto& item = state.todos[i];
        const int y = 132 + i * 66;
        paperCard(root, 24, y, W - 48, 56, p, 14);
        const bool active = item.done;
        pillText(root, active ? "✓" : "○", 38, y + 9, 38, 38, active, p);
        label(root, item.text.empty() ? "未命名事项" : item.text, 88, y + 16, 300, 24,
              item.done ? p.muted : p.text, fontCn());
        pillText(root, "删除", 414, y + 9, 72, 38, false, p);
        ui.add({24, y, 380, 56}, UiAction::TodoToggle, i);
        ui.add({414, y + 9, 72, 38}, UiAction::TodoDelete, i);
    }
    if (count == 0) {
        paperCard(root, 24, 148, W - 48, 126, p, 18);
        label(root, "还没有待办事项", 42, 184, W - 84, 28, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
        label(root, "点击右上角添加第一条", 42, 220, W - 84, 24, p.muted, fontSmall(), LV_TEXT_ALIGN_CENTER);
    }
}

void drawAlbumPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    paperCard(root, 24, 132, W - 48, 370, p, 20);
    if (state.albumIndex >= 0 && state.albumIndex < static_cast<int>(state.files.size())) {
        const auto& entry = state.files[state.albumIndex];
        paperInset(root, 48, 156, W - 96, 248, p, 16);
        vectorIcon(root, "image", 244, 224, 52, 52, p.muted);
        label(root, entry.name, 48, 420, W - 96, 28, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
        label(root, entry.path, 48, 454, W - 96, 20, p.muted, fontSmall(), LV_TEXT_ALIGN_CENTER);
    } else {
        paperInset(root, 48, 156, W - 96, 248, p, 16);
        vectorIcon(root, "image", 244, 224, 52, 52, p.muted);
        label(root, "选择一张壁纸预览", 48, 300, W - 96, 28, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
    }
    const int count = std::min(6, static_cast<int>(state.files.size()));
    for (int i = 0; i < count; ++i) {
        const int col = i % 3;
        const int row = i / 3;
        const int x = 36 + col * 160;
        const int y = 530 + row * 72;
        pillText(root, state.files[i].name, x, y, 148, 54, i == state.albumIndex, p);
        ui.add({x, y, 148, 54}, UiAction::AlbumSelect, i);
    }
    if (state.files.empty()) label(root, "/sdcard/wallpapers 中暂无图片", 24, 540, W - 48, 28, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
}

void drawFileManagerPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    label(root, state.currentFolder, 24, 118, W - 48, 22, p.muted, fontSmall());
    pillText(root, "上一级", 388, 72, 114, 44, false, p);
    ui.add({388, 72, 114, 44}, UiAction::FileUp);
    const int maxRows = 9;
    for (int i = 0; i < maxRows && i < static_cast<int>(state.files.size()); ++i) {
        const auto& entry = state.files[i];
        const int y = 154 + i * 64;
        const bool selected = state.selectedFile == i;
        paperCard(root, 24, y, W - 48, 54, p, 14);
        fileOutline(root, 42, y + 14, entry.directory, p);
        label(root, entry.name, 72, y + 10, 210, 22, selected ? p.text : p.text, fontCn());
        label(root, entry.directory ? "目录" : std::to_string(entry.size / 1024) + " KB", 72, y + 32, 130, 18, p.muted, fontSmall());
        pillText(root, entry.directory ? "打开" : "阅读", 290, y + 8, 62, 38, selected, p);
        pillText(root, "改名", 358, y + 8, 62, 38, false, p);
        pillText(root, "删", 426, y + 8, 62, 38, false, p);
        ui.add({24, y, 258, 54}, UiAction::FileSelect, i);
        ui.add({290, y + 8, 62, 38}, UiAction::FileOpen);
        ui.add({358, y + 8, 62, 38}, UiAction::FileRename);
        ui.add({426, y + 8, 62, 38}, UiAction::FileDelete);
    }
    if (state.files.empty()) label(root, "当前目录为空", 24, 210, W - 48, 28, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
}

void drawSubPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const std::vector<WifiNetwork>& networks, const Palette& p)
{
    backArrow(root, 26, 80, p);
    label(root, pageName(state.page), 70, 76, 360, 34, p.text, fontCn());
    ui.add({12, 58, 90, 70}, UiAction::Back);

    if (state.page == Page::Wifi) {
        choiceRow(root, ui, 0, "Wi‑Fi", state.status.wifiEnabled ? "已开启" : "已关闭", p);
        ui.add({24, 132, W - 48, 58}, UiAction::WifiToggle);
        choiceRow(root, ui, 1, "扫描网络", "点击刷新附近网络", p);
        ui.add({24, 200, W - 48, 58}, UiAction::WifiRefresh);
        const int networkCount = std::min(6, static_cast<int>(networks.size()));
        for (int i = 0; i < networkCount; ++i) {
            choiceRow(root, ui, i + 2, networks[i].ssid, std::to_string(networks[i].rssi) + " dBm", p);
            ui.add({24, 132 + (i + 2) * 68, W - 48, 58}, UiAction::WifiNetwork, i);
        }
        const int passwordRow = networkCount + 2;
        choiceRow(root, ui, passwordRow, "密码", state.wifiPassword.empty() ? "点击输入" : "已输入", p);
        ui.add({24, 132 + passwordRow * 68, W - 48, 58}, UiAction::WifiPassword);
        const int connectY = 132 + (passwordRow + 1) * 68 + 8;
        pillText(root, state.status.wifiConnected ? "断开连接" : "连接", 150, connectY, 240, 54, true, p);
        ui.add({150, connectY, 240, 54}, UiAction::WifiConnect);
    } else if (state.page == Page::Bluetooth) {
        choiceRow(root, ui, 0, "蓝牙", state.status.bluetoothEnabled ? "打开" : "关闭", p);
        ui.add({24, 132, W - 48, 58}, UiAction::BluetoothToggle);
        choiceRow(root, ui, 1, "扫描设备", "点击扫描", p);
        ui.add({24, 200, W - 48, 58}, UiAction::BluetoothScan);
        for (int i = 0; i < std::min(6, static_cast<int>(state.bluetoothDevices.size())); ++i) {
            choiceRow(root, ui, i + 2, state.bluetoothDevices[i].name, state.bluetoothDevices[i].connected ? "已连接" : std::to_string(state.bluetoothDevices[i].rssi), p);
            ui.add({24, 132 + (i + 2) * 68, W - 48, 58}, UiAction::BluetoothDevice, i);
        }
    } else if (state.page == Page::Display) {
        choiceRow(root, ui, 0, "白色主题", state.theme == Theme::Light ? "当前" : "", p);
        choiceRow(root, ui, 1, "黑色主题", state.theme == Theme::Dark ? "当前" : "", p);
        choiceRow(root, ui, 2, "竖屏", state.system.orientation == Orientation::Portrait ? "当前" : "", p);
        choiceRow(root, ui, 3, "横屏", "本机不支持", p);
        choiceRow(root, ui, 4, "状态栏", state.system.statusBar ? "显示" : "隐藏", p);
        choiceRow(root, ui, 5, "休眠时间 · 5 分钟", state.system.sleepMinutes == 5 ? "当前" : "", p);
        choiceRow(root, ui, 6, "休眠时间 · 15 分钟", state.system.sleepMinutes == 15 ? "当前" : "", p);
        choiceRow(root, ui, 7, "关机时间 · 2 小时", state.system.powerOffHours == 2 ? "当前" : "", p);
        choiceRow(root, ui, 8, "EPUB 自动旋转", state.reader.autoRotate ? "开启" : "关闭", p);
    } else if (state.page == Page::Fonts) {
        const char* names[] = {"系统字体", "内置粗体", "内置斜体", "从 SD 卡读取"};
        const char* ids[] = {"system", "builtin-bold", "builtin-italic"};
        for (int i = 0; i < 4; ++i) {
            std::string value;
            if (i < 3) value = state.system.systemFontId == ids[i] ? "当前" : ids[i];
            else value = state.system.systemFontId.rfind("/sdcard/fonts/", 0) == 0 ? state.system.systemFontId : "/sdcard/fonts/*.vlw";
            choiceRow(root, ui, i, names[i], value, p);
        }
    } else if (state.page == Page::General) {
        choiceRow(root, ui, 0, "系统语言", state.language == SystemLanguage::Chinese ? "中文" : "", p);
        choiceRow(root, ui, 1, "Ngôn ngữ", state.language == SystemLanguage::Vietnamese ? "Tiếng Việt" : "", p);
        choiceRow(root, ui, 2, "网络校时", state.system.ntpEnabled ? "开启" : "关闭", p);
        choiceRow(root, ui, 3, "自动重连", state.system.autoReconnect ? "开启" : "关闭", p);
        choiceRow(root, ui, 4, "Wi‑Fi 低功耗", state.system.lowPower ? "开启" : "关闭", p);
        choiceRow(root, ui, 5, "时区", "UTC+7", p);
        choiceRow(root, ui, 6, "时区", "UTC+8", p);
        choiceRow(root, ui, 7, "时区", "UTC+9", p);
    } else if (state.page == Page::Wallpapers) {
        choiceRow(root, ui, 0, "纸感晨雾", state.system.wallpaperId == "paper-morning" ? "当前" : "", p);
        choiceRow(root, ui, 1, "墨色山影", state.system.wallpaperId == "ink-mountain" ? "当前" : "", p);
        choiceRow(root, ui, 2, "极简时钟", state.system.wallpaperId == "minimal-clock" ? "当前" : "", p);
        choiceRow(root, ui, 3, "从 SD 卡读取", "/sdcard/wallpapers", p);
        choiceRow(root, ui, 4, "锁屏随机封面", state.system.randomLockWallpaper ? "开启" : "关闭", p);
    } else if (state.page == Page::FileTransfer) {
        paperCard(root, 24, 140, W - 48, 230, p, 22);
        label(root, "文件传输", 48, 164, 200, 32, p.text, fontCn());
        label(root, state.transferUrl.empty() ? "开启后生成 Wi‑Fi 上传页面" : state.transferUrl, 48, 210, W - 96, 48, p.muted, fontCn());
        pillText(root, state.transferUrl.empty() ? "开启上传服务" : "关闭上传服务", 96, 286, W - 192, 56, true, p);
        ui.add({96, 286, W - 192, 56}, UiAction::TransferToggle);
        if (state.transferUrl.empty()) {
            label(root, "设备会创建临时 Wi‑Fi 热点", 48, 394, W - 96, 24, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
        } else {
            label(root, "扫描二维码加入热点", 48, 394, W - 96, 24, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
            label(root, "网页上传 / FTP 端口 21", 48, 426, W - 96, 22, p.muted, fontSmall(), LV_TEXT_ALIGN_CENTER);
            label(root, "连接后打开 " + state.transferUrl, 48, 684, W - 96, 24, p.muted, fontSmall(), LV_TEXT_ALIGN_CENTER);
        }
    } else if (state.page == Page::Storage) {
        paperCard(root, 24, 140, W - 48, 220, p, 22);
        label(root, "SD 卡", 48, 164, 160, 28, p.text, fontCn());
        label(root, state.status.sdMounted ? "已挂载，可读写" : "未检测到 SD 卡", 48, 208, W - 96, 30, p.text, fontCn());
        label(root, std::to_string(state.books.size()) + " 本图书", 48, 250, 160, 24, p.muted, fontCn());
        pillText(root, "刷新存储", 48, 296, 192, 48, false, p);
        pillText(root, "打开文件管理", 264, 296, 214, 48, true, p);
        ui.add({48, 296, 192, 48}, UiAction::StorageRefresh);
        ui.add({264, 296, 214, 48}, UiAction::StorageOpenManager);
    } else if (state.page == Page::Firmware) {
        paperCard(root, 24, 140, W - 48, 220, p, 22);
        label(root, "固件升级", 48, 164, 180, 28, p.text, fontCn());
        label(root, state.otaStatus.empty() ? "将 firmware.bin 放入 /sdcard/ota" : state.otaStatus,
              48, 212, W - 96, 52, p.muted, fontCn());
        label(root, "当前版本 · Paper S3 LVGL", 48, 270, W - 96, 24, p.muted, fontSmall());
        pillText(root, "安装并重启", 136, 306, 268, 50, true, p);
        ui.add({136, 306, 268, 50}, UiAction::FirmwareInstall);
    } else if (state.page == Page::Calendar) {
        drawCalendarPage(root, state, ui, p);
    } else if (state.page == Page::Pomodoro) {
        drawPomodoroPage(root, state, ui, p);
    } else if (state.page == Page::Timers) {
        drawTimersPage(root, state, ui, p);
    } else if (state.page == Page::Todos) {
        drawTodoPage(root, state, ui, p);
    } else if (state.page == Page::Album) {
        drawAlbumPage(root, state, ui, p);
    } else if (state.page == Page::FontTest) {
        paperCard(root, 24, 140, W - 48, 420, p, 22);
        label(root, "字体测试", 48, 166, 180, 28, p.text, fontCn());
        label(root, "中文：明朝那些事儿", 48, 226, W - 96, 34, p.text, fontCn());
        label(root, "Tiếng Việt: Những ngày yên tĩnh", 48, 282, W - 96, 34, p.text, fontCn());
        label(root, "English: The Small Machine", 48, 338, W - 96, 34, p.text, fontLatin24());
        label(root, "한국어 · 日本語", 48, 394, W - 96, 34, p.text, fontCn());
        label(root, "系统字体：" + state.system.systemFontId, 48, 478, W - 96, 24, p.muted, fontSmall());
    } else if (state.page == Page::FileManager) {
        drawFileManagerPage(root, state, ui, p);
    } else if (state.page == Page::Diagnostics) {
        paperCard(root, 24, 140, W - 48, 440, p, 22);
        label(root, "设备自检", 48, 166, 180, 28, p.text, fontCn());
        label(root, state.diagnosticsText.empty() ? "点击下方按钮开始检测" : state.diagnosticsText,
              48, 218, W - 96, 260, p.text, fontCn());
        pillText(root, "开始检测", 136, 512, 268, 50, true, p);
        ui.add({136, 512, 268, 50}, UiAction::DiagnosticsRun);
    } else {
        paperCard(root, 24, 140, W - 48, 220, p, 22);
        label(root, "功能正在准备", 48, 172, W - 96, 30, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
        label(root, "该页面没有可配置项", 48, 222, W - 96, 24, p.muted, fontCn(), LV_TEXT_ALIGN_CENTER);
    }
    if (!state.status.message.empty()) label(root, state.status.message, 24, 808, W - 48, 34, p.muted, fontSmall(), LV_TEXT_ALIGN_CENTER);
}

void drawReaderLandscapePage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    constexpr int rw = 960;
    constexpr int rh = 540;
    obj(root, 0, 0, rw, rh, state.theme == Theme::Dark ? lv_color_hex(0x111111) : lv_color_hex(0xfbfbf4), 0, p.bg, 0);
    const int top = state.readerChromeVisible ? 66 : 18;
    const int bottom = state.readerChromeVisible ? 128 : 18;
    label(root, state.readerText.empty() ? "这是 EPUB 阅读页面" : state.readerText,
          56, top, rw - 112, rh - top - bottom, p.text, readerFont(state));
    ui.add({0, state.readerChromeVisible ? 66 : 0, rw, state.readerChromeVisible ? rh - 194 : rh}, UiAction::ReaderToggleChrome);
    if (!state.readerChromeVisible) return;
    paperCard(root, 0, 0, rw, 58, p, 0);
    backArrow(root, 24, 16, p);
    ui.add({0, 0, 84, 58}, UiAction::ReaderBack);
    const UiAction topActions[] = {UiAction::ReaderAutoPage, UiAction::ReaderTurnPicker, UiAction::ReaderBookmark};
    const char* topIcons[] = {"bell", "loop", "bookmark"};
    for (int i = 0; i < 3; ++i) {
        const int x = rw - 194 + i * 58;
        obj(root, x, 8, 44, 40, p.card2, 12, p.line);
        vectorIcon(root, topIcons[i], x + 10, 16, 24, 24, p.text);
        ui.add({x, 0, 50, 58}, topActions[i]);
    }
    paperCard(root, 0, rh - 132, rw, 132, p, 0);
    if (state.readerPanel == ReaderPanel::Contents) {
        const int count = std::min(4, static_cast<int>(state.readerChapterTitles.size()));
        for (int i = 0; i < count; ++i) {
            pillText(root, state.readerChapterTitles[i], 30 + i * 230, rh - 112, 214, 46,
                     state.readerPage >= state.readerChapterPages[i] && (i + 1 == count || state.readerPage < state.readerChapterPages[i + 1]), p);
            ui.add({30 + i * 230, rh - 112, 214, 46}, UiAction::ReaderChapter, i);
        }
    } else if (state.readerPanel == ReaderPanel::Progress) {
        label(root, "阅读进度", 34, rh - 118, 150, 20, p.text, fontCn());
        obj(root, 190, rh - 108, 650, 6, p.line, 3, p.line, 0);
        const int knob = 190 + (state.readerPageCount > 1 ? 650 * state.readerPage / (state.readerPageCount - 1) : 0);
        obj(root, knob - 7, rh - 114, 14, 18, p.text, 9, p.text, 0);
        ui.add({190, rh - 122, 650, 34}, UiAction::ReaderPageSlider);
    } else if (state.readerPanel == ReaderPanel::Typography) {
        label(root, "字体", 30, rh - 116, 56, 20, p.muted, fontSmall());
        pillText(root, state.reader.fontName, 86, rh - 124, 146, 40, false, p);
        ui.add({86, rh - 124, 146, 40}, UiAction::ReaderFontPicker);
        label(root, "字号", 254, rh - 116, 50, 20, p.muted, fontSmall());
        obj(root, 314, rh - 108, 180, 6, p.line, 3, p.line, 0);
        obj(root, 314 + (state.reader.fontSize - 14) * 180 / 10 - 7, rh - 114, 14, 18, p.text, 9, p.text, 0);
        ui.add({314, rh - 122, 180, 34}, UiAction::ReaderFontSize);
        label(root, "边距", 520, rh - 116, 50, 20, p.muted, fontSmall());
        obj(root, 580, rh - 108, 150, 6, p.line, 3, p.line, 0);
        obj(root, 580 + state.reader.marginLevel * 50 - 7, rh - 114, 14, 18, p.text, 9, p.text, 0);
        ui.add({580, rh - 122, 150, 34}, UiAction::ReaderMargin);
        label(root, "行距", 756, rh - 116, 50, 20, p.muted, fontSmall());
        obj(root, 814, rh - 108, 110, 6, p.line, 3, p.line, 0);
        obj(root, 814 + state.reader.lineHeightLevel * 36 - 7, rh - 114, 14, 18, p.text, 9, p.text, 0);
        ui.add({814, rh - 122, 110, 34}, UiAction::ReaderLineHeight);
    } else {
        const char* icons[] = {"list", "settings", "edit"};
        const UiAction actions[] = {UiAction::ReaderContents, UiAction::ReaderProgress, UiAction::ReaderTypography};
        for (int i = 0; i < 3; ++i) {
            const int x = 278 + i * 142;
            obj(root, x, rh - 104, 118, 48, p.card, 24, p.line);
            vectorIcon(root, icons[i], x + 47, rh - 92, 24, 24, p.text);
            ui.add({x, rh - 104, 118, 48}, actions[i]);
        }
    }
}

void drawReaderPage(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    obj(root, 0, 0, W, H, state.theme == Theme::Dark ? lv_color_hex(0x111111) : lv_color_hex(0xfbfbf4), 0, p.bg, 0);
    const int top = state.readerChromeVisible ? 104 : 24;
    const int bottom = state.readerChromeVisible ? 224 : 24;
    label(root, state.readerText.empty() ? "这是 LVGL 阅读页面。点击屏幕中心显示工具栏，真机版会使用 EPUB 自带导航生成章节列表。" : state.readerText,
          54, top, W - 108, H - top - bottom, p.text, readerFont(state));
    // The whole reading surface is interactive. In tap mode the left/right
    // thirds turn pages and the centre toggles the chrome; toolbar targets
    // added below take precedence when the chrome is visible.
    ui.add({0, state.readerChromeVisible ? STATUS_H + 56 : 0, W,
            state.readerChromeVisible ? H - STATUS_H - 240 : H}, UiAction::ReaderToggleChrome);
    if (!state.readerChromeVisible) return;

    paperCard(root, 0, STATUS_H, W, 56, p, 0);
    backArrow(root, 24, STATUS_H + 14, p);
    ui.add({0, STATUS_H, 82, 56}, UiAction::ReaderBack);
    const char* icons[] = {"bell", "loop", "bookmark"};
    const UiAction actions[] = {UiAction::ReaderAutoPage, UiAction::ReaderTurnPicker, UiAction::ReaderBookmark};
    for (int i = 0; i < 3; ++i) {
        const int x = W - 190 + i * 58;
        obj(root, x, STATUS_H + 8, 44, 40, i == 1 && state.readerPanel == ReaderPanel::PageTurnPicker ? p.active : p.card2, 12, p.line);
        const lv_color_t iconColor = i == 1 && state.readerPanel == ReaderPanel::PageTurnPicker ? p.activeText : p.text;
        if (!vectorIcon(root, icons[i], x + 10, STATUS_H + 16, 24, 24, iconColor)) {
            label(root, icons[i], x, STATUS_H + 17, 44, 24, iconColor, fontCn(), LV_TEXT_ALIGN_CENTER);
        }
        ui.add({x, STATUS_H + 8, 44, 40}, actions[i]);
    }

    paperCard(root, 0, H - 184, W, 184, p, 24);
    if (state.readerPanel == ReaderPanel::Progress) {
        label(root, "阅读进度", 28, H - 164, 160, 26, p.text, fontCn());
        bar(root, 28, H - 114, W - 56, 8, state.readerPageCount > 1 ? state.readerPage * 100 / (state.readerPageCount - 1) : 0, p);
        label(root, std::to_string(state.readerPage + 1) + " / " + std::to_string(std::max(1, state.readerPageCount)) + " 页",
              28, H - 92, 180, 22, p.muted, fontCn());
        ui.add({28, H - 130, W - 56, 46}, UiAction::ReaderPageSlider, -1);
    } else if (state.readerPanel == ReaderPanel::Typography) {
        label(root, "字体", 28, H - 164, 90, 26, p.text, fontCn());
        label(root, state.reader.fontName, 112, H - 164, 180, 26, p.muted, fontCn());
        ui.add({28, H - 172, 260, 44}, UiAction::ReaderFontPicker);
        label(root, "字号", 28, H - 120, 60, 22, p.muted, fontCn());
        bar(root, 96, H - 112, 360, 6, (state.reader.fontSize - 14) * 100 / 10, p);
        ui.add({96, H - 130, 360, 42}, UiAction::ReaderFontSize);
        label(root, "边距", 28, H - 84, 60, 22, p.muted, fontCn());
        bar(root, 96, H - 76, 360, 6, state.reader.marginLevel * 25, p);
        ui.add({96, H - 94, 360, 42}, UiAction::ReaderMargin);
        label(root, "行距", 28, H - 48, 60, 22, p.muted, fontCn());
        bar(root, 96, H - 40, 360, 6, state.reader.lineHeightLevel * 25, p);
        ui.add({96, H - 58, 360, 42}, UiAction::ReaderLineHeight);
    } else if (state.readerPanel == ReaderPanel::Contents) {
        label(root, "章节", 28, H - 164, 90, 26, p.text, fontCn());
        for (int i = 0; i < std::min(4, static_cast<int>(state.readerChapterTitles.size())); ++i) {
            label(root, state.readerChapterTitles[i], 28, H - 126 + i * 28, 380, 24, p.text, fontCn());
            ui.add({24, H - 132 + i * 28, W - 48, 28}, UiAction::ReaderChapter, i);
        }
    } else if (state.readerPanel == ReaderPanel::FontPicker) {
        label(root, "选择字体", 28, H - 164, 160, 26, p.text, fontCn());
        const char* fonts[] = {"System", "Regular", "Bold", "Italic"};
        for (int i = 0; i < 4; ++i) {
            pillText(root, fonts[i], 28 + (i % 2) * 244, H - 124 + (i / 2) * 52, 224, 42, state.reader.fontName == fonts[i], p);
            ui.add({28 + (i % 2) * 244, H - 124 + (i / 2) * 52, 224, 42}, UiAction::ReaderFontChoice, i);
        }
    } else if (state.readerPanel == ReaderPanel::PageTurnPicker) {
        label(root, "翻页方式", 28, H - 164, 160, 26, p.text, fontCn());
        const char* modes[] = {"滑动", "点击", "仿真翻页", "翻转感应"};
        const char* ids[] = {"swipe", "tap", "simulation", "tilt"};
        for (int i = 0; i < 4; ++i) {
            pillText(root, modes[i], 28 + (i % 2) * 244, H - 124 + (i / 2) * 52, 224, 42, state.reader.pageTurnMode == ids[i], p);
            ui.add({28 + (i % 2) * 244, H - 124 + (i / 2) * 52, 224, 42}, UiAction::ReaderTurnMode, i);
        }
    } else {
        const char* icons2[] = {"list", "settings", "edit"};
        const UiAction actions2[] = {UiAction::ReaderContents, UiAction::ReaderProgress, UiAction::ReaderTypography};
        for (int i = 0; i < 3; ++i) {
            const int x = 44 + i * 160;
            obj(root, x, H - 74, 132, 48, p.card, 24, p.line);
            if (!vectorIcon(root, icons2[i], x + 54, H - 62, 24, 24, p.text)) {
                label(root, icons2[i], x, H - 64, 132, 22, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
            }
            ui.add({x, H - 74, 132, 48}, actions2[i]);
        }
    }
}

void drawWallpaper(lv_obj_t* root, const AppState& state, const Palette& p)
{
    // Keep the built-in wallpapers vector-only so they render predictably on
    // the 16-level e-ink panel. Custom SD images are selected in Settings and
    // can be added later without changing the lock-screen layout.
    if (state.system.wallpaperId == "ink-mountain") {
        const lv_color_t ink = lv_color_hex(0x171817);
        obj(root, 0, 0, W, H, ink, 0, ink, 0);
        obj(root, 52, 98, 436, 290, lv_color_hex(0x202320), 34, lv_color_hex(0x2b2e2b), 1);
        static lv_point_precise_t ridgeA[] = {{0, 610}, {92, 490}, {176, 570}, {280, 420}, {410, 540}, {540, 450}};
        static lv_point_precise_t ridgeB[] = {{0, 730}, {128, 590}, {250, 700}, {372, 560}, {540, 680}};
        vectorLine(root, ridgeA, 6, 0, 0, W, H, lv_color_hex(0x777a76), 3);
        vectorLine(root, ridgeB, 5, 0, 0, W, H, lv_color_hex(0xb4b7b1), 2);
        return;
    }
    if (state.system.wallpaperId == "minimal-clock") {
        const lv_color_t paper = lv_color_hex(0xf1f2ee);
        obj(root, 0, 0, W, H, paper, 0, paper, 0);
        line(root, 42, 246, W - 84, lv_color_hex(0xd5d8d3));
        line(root, 42, 708, W - 84, lv_color_hex(0xd5d8d3));
        vectorDot(root, W / 2 - 4, 438, 8, lv_color_hex(0x969b95));
        return;
    }
    obj(root, 0, 0, W, H, lv_color_hex(0xe6e9e5), 0, lv_color_hex(0xe6e9e5), 0);
    obj(root, 36, 72, W - 72, 260, lv_color_hex(0xf3f4f0), 38, lv_color_hex(0xd9ddd8), 1);
    obj(root, 92, 546, W - 184, 250, lv_color_hex(0xdde2dc), 120, lv_color_hex(0xd0d6d0), 1);
}

void renderOverlay(lv_obj_t* root, AppState& state, UiRenderer& ui, const Palette& p)
{
    if (state.overlay == SystemOverlay::None) return;
    if (state.overlay == SystemOverlay::LockScreen) {
        drawWallpaper(root, state, p);
        const bool darkWallpaper = state.system.wallpaperId == "ink-mountain";
        const lv_color_t lockText = darkWallpaper ? lv_color_hex(0xffffff) : lv_color_hex(0x171817);
        const lv_color_t lockMuted = darkWallpaper ? lv_color_hex(0xdfdfdc) : lv_color_hex(0x555a55);
        label(root, clockText(), 44, 120, W - 88, 80, lockText, fontNum());
        label(root, fullDateText(), 48, 210, W - 96, 32, lockMuted, fontCn());
        obj(root, 70, 720, W - 140, 6, lockText, 3, lockText, 0);
    } else {
        paperCard(root, 0, 0, W, H, p, 0);
        label(root, "控制中心", 36, 72, 220, 34, p.text, fontCn());
        // Register the dismiss target first. Specific controls are added
        // afterwards so hitTest's reverse order gives them priority.
        ui.add({0, 0, W, H}, UiAction::OverlayClose);
        pillText(root, state.status.wifiEnabled ? "Wi‑Fi 开" : "Wi‑Fi 关", 44, 142, 210, 82, state.status.wifiEnabled, p);
        pillText(root, state.status.bluetoothEnabled ? "蓝牙开" : "蓝牙关", 286, 142, 210, 82, state.status.bluetoothEnabled, p);
        ui.add({44, 142, 210, 82}, UiAction::ControlWifi);
        ui.add({286, 142, 210, 82}, UiAction::ControlBluetooth);
    }
}

void drawKeyboardLayer(lv_obj_t* root, UiRenderer& ui, InputMethod& ime, const Palette& p)
{
    obj(root, 0, 560, W, 400, p.card, 28, p.line);
    label(root, ime.composition().empty() ? "输入内容" : ime.composition(), 28, 580, W - 110, 34, p.text, fontCn());
    obj(root, W - 70, 578, 44, 36, p.card2, 18, p.line);
    vectorIcon(root, "close", W - 60, 585, 22, 22, p.text);
    ui.clear_ = {W - 70, 578, 44, 36};

    const auto candidates = ime.candidates();
    ui.candidateCount_ = std::min(5, static_cast<int>(candidates.size()));
    for (int i = 0; i < ui.candidateCount_; ++i) {
        const int x = 26 + i * 98;
        obj(root, x, 626, 88, 36, p.card2, 18, p.line);
        label(root, candidates[i], x, 634, 88, 22, p.text, fontCn(), LV_TEXT_ALIGN_CENTER);
        ui.candidateRects_[i] = {x, 626, 88, 36};
    }

    const char* rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    int keyIndex = 0;
    for (int r = 0; r < 3; ++r) {
        const int count = static_cast<int>(std::strlen(rows[r]));
        const int keyW = 44;
        const int gap = 6;
        const int startX = (W - count * keyW - (count - 1) * gap) / 2;
        const int y = 680 + r * 52;
        for (int c = 0; c < count; ++c) {
            const int x = startX + c * (keyW + gap);
            obj(root, x, y, keyW, 42, p.card2, 10, p.line);
            std::string s(1, rows[r][c]);
            label(root, s, x, y + 10, keyW, 22, p.text, fontLatin20(), LV_TEXT_ALIGN_CENTER);
            ui.keyRects_[keyIndex++] = {x, y, keyW, 42};
        }
    }
    ui.keyCount_ = keyIndex;
    ui.globe_ = {22, 844, 70, 44};
    ui.symbols_ = {102, 844, 70, 44};
    ui.space_ = {182, 844, 176, 44};
    ui.backspace_ = {368, 844, 70, 44};
    ui.enter_ = {448, 844, 70, 44};
    pillIcon(root, "globe", ui.globe_.x, ui.globe_.y, ui.globe_.w, ui.globe_.h, false, p);
    pillText(root, "符", ui.symbols_.x, ui.symbols_.y, ui.symbols_.w, ui.symbols_.h, false, p);
    pillText(root, "空格", ui.space_.x, ui.space_.y, ui.space_.w, ui.space_.h, false, p);
    pillIcon(root, "backspace", ui.backspace_.x, ui.backspace_.y, ui.backspace_.w, ui.backspace_.h, false, p);
    pillText(root, "完成", ui.enter_.x, ui.enter_.y, ui.enter_.w, ui.enter_.h, true, p);
}

}  // namespace

void UiRenderer::draw(AppState& state, PaperS3Hal& hal, InputMethod& ime, const std::vector<WifiNetwork>& networks)
{
    currentTheme_ = state.theme;
    g_language = state.language;
    resetHitTargets();
    keyCount_ = 0;
    candidateCount_ = 0;

    const bool landscapeReader = state.page == Page::Reader && state.system.orientation == Orientation::Landscape;
    initLvgl(hal, landscapeReader ? 960 : W, landscapeReader ? 540 : H);
    if (state.needsFullRefresh) {
        hal.setQualityRefresh();
        state.needsFullRefresh = false;
    } else {
        hal.setTextRefresh();
    }

    const Palette p = palette(state.theme);
    lv_obj_t* root = lv_screen_active();
    lv_obj_clean(root);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, landscapeReader ? 960 : W, landscapeReader ? 540 : H);
    lv_obj_set_style_bg_color(root, p.bg, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    if (state.page == Page::Reader) {
        if (landscapeReader) drawReaderLandscapePage(root, state, *this, p);
        else drawReaderPage(root, state, *this, p);
        if (!landscapeReader) drawStatusBar(root, state, p, true);
        if (!landscapeReader && state.system.statusBar && state.readerChromeVisible) {
            add({0, 0, W / 2, STATUS_H}, UiAction::StatusLock);
            add({W / 2, 0, W / 2, STATUS_H}, UiAction::StatusControl);
        }
    } else {
        drawStatusBar(root, state, p);
        if (state.system.statusBar) {
            add({0, 0, W / 2, STATUS_H}, UiAction::StatusLock);
            add({W / 2, 0, W / 2, STATUS_H}, UiAction::StatusControl);
        }
        switch (state.page) {
            case Page::Home: drawHomePage(root, state, *this, p); break;
            case Page::Bookshelf: drawBooksPage(root, state, *this, p); break;
            case Page::Tools: drawListPage(root, state, *this, p, false); break;
            case Page::Settings: drawListPage(root, state, *this, p, true); break;
            default: drawSubPage(root, state, *this, networks, p); break;
        }
        if (state.page == Page::Home || state.page == Page::Bookshelf || state.page == Page::Tools || state.page == Page::Settings) {
            renderNav(root, state, p);
            for (int i = 0; i < 4; ++i) add({30 + i * 120, NAV_Y + 9, 108, 54}, UiAction::Nav, i);
        }
    }

    renderOverlay(root, state, *this, p);
    if (ime.isOpen()) drawKeyboardLayer(root, *this, ime, p);

    lv_timer_handler();
    lv_refr_now(g_display);
    // M5GFX already contains a compact QR encoder. Draw the Wi-Fi payload
    // after LVGL has flushed so the QR stays crisp on the e-paper surface and
    // is not covered by the page background. The next full LVGL redraw clears
    // this area before drawing it again.
    if (state.page == Page::FileTransfer && !state.transferUrl.empty()) {
        const std::string payload = "WIFI:T:WPA;S:" + state.transferSsid + ";P:" + state.transferPassword + ";;";
        hal.display().qrcode(payload.c_str(), 178, 480, 184, 4, true);
    }
    if (state.page == Page::Album && state.albumIndex >= 0 && state.albumIndex < static_cast<int>(state.files.size())) {
        const auto& image = state.files[state.albumIndex];
        std::string lower = image.path;
        for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        const auto hasSuffix = [&](const char* suffix) {
            const std::string value = suffix;
            return lower.size() >= value.size() && lower.compare(lower.size() - value.size(), value.size(), value) == 0;
        };
        if (hasSuffix(".jpg") || hasSuffix(".jpeg")) {
            hal.display().drawJpgFile(image.path.c_str(), 48, 156, 444, 248);
        } else if (hasSuffix(".png")) {
            hal.display().drawPngFile(image.path.c_str(), 48, 156, 444, 248);
        }
    }
}

UiEvent UiRenderer::hitTest(int x, int y) const
{
    for (auto it = targets_.rbegin(); it != targets_.rend(); ++it) {
        if (it->rect.contains(x, y)) {
            int value = 0;
            switch (it->action) {
                case UiAction::ReaderPageSlider:
                    value = it->rect.w > 1 ? std::clamp((x - it->rect.x) * 100 / it->rect.w, 0, 100) : 0;
                    break;
                case UiAction::ReaderFontSize:
                case UiAction::ReaderMargin:
                case UiAction::ReaderLineHeight:
                    value = it->rect.w > 1 ? std::clamp((x - it->rect.x) * 100 / it->rect.w, 0, 100) : 0;
                    break;
                default:
                    break;
            }
            return {it->action, it->index, value};
        }
    }
    return {};
}

int UiRenderer::keyboardKeyIndex(int x, int y) const
{
    for (int i = 0; i < keyCount_; ++i) if (keyRects_[i].contains(x, y)) return i;
    return -1;
}

int UiRenderer::candidateIndex(int x, int y) const
{
    for (int i = 0; i < candidateCount_; ++i) if (candidateRects_[i].contains(x, y)) return i;
    return -1;
}

bool UiRenderer::keyboardBackspaceHit(int x, int y) const { return backspace_.contains(x, y); }
bool UiRenderer::keyboardEnterHit(int x, int y) const { return enter_.contains(x, y); }
bool UiRenderer::keyboardClearHit(int x, int y) const { return clear_.contains(x, y); }
bool UiRenderer::keyboardSpaceHit(int x, int y) const { return space_.contains(x, y); }
bool UiRenderer::keyboardGlobeHit(int x, int y) const { return globe_.contains(x, y); }
bool UiRenderer::keyboardSymbolsHit(int x, int y) const { return symbols_.contains(x, y); }

void UiRenderer::add(const Rect& rect, UiAction action, int index)
{
    targets_.push_back({rect, action, index});
}

void UiRenderer::resetHitTargets()
{
    targets_.clear();
}

}  // namespace papers3
