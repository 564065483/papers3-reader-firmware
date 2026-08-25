#include "app_state.h"

namespace papers3 {

const char* pageName(Page page)
{
    switch (page) {
        case Page::Home: return "首页";
        case Page::Bookshelf: return "书架";
        case Page::Tools: return "工具";
        case Page::Settings: return "设置";
        case Page::Reader: return "阅读";
        case Page::Wifi: return "Wi-Fi";
        case Page::FileTransfer: return "文件传输";
        case Page::Bluetooth: return "蓝牙";
        case Page::Display: return "显示";
        case Page::Fonts: return "字体";
        case Page::Wallpapers: return "壁纸";
        case Page::Storage: return "存储";
        case Page::Firmware: return "固件升级";
        case Page::General: return "通用";
        case Page::Calendar: return "日历";
        case Page::Pomodoro: return "番茄钟";
        case Page::Timers: return "倒计时";
        case Page::Todos: return "待办";
        case Page::Album: return "电子相册";
        case Page::FontTest: return "字体测试";
        case Page::FileManager: return "文件管理";
        case Page::Diagnostics: return "固件自检";
    }
    return "";
}

}  // namespace papers3
