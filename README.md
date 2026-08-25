# PaperS3 Reader Firmware

面向 M5Stack PaperS3（ESP32-S3、4.7 英寸 960×540、16 级灰度墨水屏）的 ESP-IDF 固件工程。默认使用设备真实的 540×960 竖屏坐标，不是网页演示壳。

## 已写入固件的功能

### 系统与界面

- 540×960 竖屏主页、书架、工具和设置，多层页面使用统一触控动作路由。
- 白色/黑色主题、状态栏、中文/越南语系统界面。
- 左侧状态栏进入待机时钟，底部上滑解锁；右侧状态栏打开 Wi-Fi/蓝牙控制中心。
- 字体选择入口、VLW 文件上传和 `/sdcard/fonts` 目录管理已接通；内置 UI 字体保证中文/越南语不缺字，VLW 解码需在真机用实际字体文件确认内存占用。
- 三套内置矢量锁屏壁纸（纸感晨雾、墨色山影、极简时钟）；JPG/PNG 可通过手机页上传到 `/sdcard/wallpapers`，自定义图片解码需在真机做最后验证。
- 空闲休眠、定时关机、电量显示、NTP 校时和设置持久化。

### 图书与阅读

- FAT32 SD 卡挂载并自动创建 `/sdcard/books`、`fonts`、`wallpapers`、`ota`。
- TXT UTF-8 分页。
- EPUB ZIP、`container.xml`、OPF manifest/spine、XHTML 正文和真实章节起始页解析。
- 书架搜索、最近/全部/收藏、每页 12 本、翻页按钮和收藏持久化。
- 阅读全屏、中心触控显示/隐藏工具栏、点击/滑动/仿真/IMU 倾斜翻页。
- 章节跳转、进度拖动、书签、自动翻页和长按切换间隔。
- 字号、边距、行距实时重分页；阅读字体样式切换。
- 每本书进度、书签、收藏、阅读秒数，以及累计/本月/今日阅读时间保存到 NVS。

### 网络、文件和升级

- Wi-Fi 真实扫描、选择网络、密码输入、连接、断开、凭据保存和自动重连。
- BLE 真实扫描、RSSI、连接和断开状态同步。
- 文件传输热点 `PaperS3-Transfer`，手机访问 `http://192.168.4.1`。
- 设备屏幕会生成 Wi‑Fi 配置二维码（SSID、密码），手机扫码加入热点后访问管理页地址。
- 手机管理页上传 EPUB/TXT/JPG/PNG/VLW/BIN；按类型保存到对应 SD 目录；图书支持列出和删除。
- 文件管理器浏览目录、打开图书、重命名和二次确认删除。
- 双 OTA 分区、升级包校验、从 `/sdcard/ota/firmware.bin` 写入备用分区并切换启动分区。

### 工具

- 按真实月份天数和星期排列的日历。
- 番茄钟、多个倒计时、新增/重命名/调时/删除、到时蜂鸣和重启后恢复。
- 中/英/拼音软键盘，候选词、UTF-8 删除、空格、清空和完成键。
- 字体测试、存储状态、固件自检。

## 必须等真机验证的项目

以下代码已经接入真实驱动或协议，但仅凭编译不能证明具体这块硬件和外设一定正常：

- PaperS3 触摸坐标、SD 引脚、电量计、蜂鸣器、IMU 方向阈值。
- 墨水屏局刷残影、刷新耗时、休眠唤醒与实际续航。
- 不同路由器下的 Wi-Fi 连接，以及 BLE 外设配对兼容性。
- 自定义 JPG/PNG 壁纸解码和不同 VLW 字体文件的内存占用。
- EPUB 出版物的特殊 CSS、图片、脚注、竖排和 DRM；当前目标是可重排正文阅读。
- BLE 已能扫描和建立连接；键盘/翻页器的 HID 报告映射还要拿实际设备报告确认后补对应键值。
- 当前固件固定使用 540×960 竖屏；设置中的横屏项会提示不可用，避免旋转后裁剪 UI。

## 工具链与构建

- ESP-IDF v5.3.3
- M5GFX 0.2.15
- M5Unified 0.2.10

```powershell
python .\fetch_repos.py
idf.py set-target esp32s3
idf.py build
```

### 在 macOS 上继续开发

安装并激活 ESP-IDF v5.3.3 后，在终端执行：

```bash
git clone <仓库地址>
cd papers3-reader-fw
python3 fetch_repos.py
idf.py set-target esp32s3
idf.py build
```

连接开发板后可用 `ls /dev/cu.*` 查找串口，再完整烧录：

```bash
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

`build/`、PlatformIO 缓存和可重新下载的组件不会提交到 Git；`fetch_repos.py` 会按 `repos.json` 下载固定版本的 M5GFX 与 M5Unified，ESP-IDF 会根据 `dependencies.lock` 恢复托管组件。

当前应用镜像为 `build/papers3_reader_fw.bin`。项目使用 16 MB Flash 的双 OTA 分区，首次烧录或分区表变化后必须完整烧录：

```powershell
idf.py -p COM端口 flash monitor
```

不能只把应用 BIN 写到旧的 factory 地址；本工程应用分区从 `0x20000` 开始。

## 真机首轮验收顺序

1. 屏幕方向、全屏刷新、触摸四角和状态栏左右入口。
2. SD 卡挂载、目录创建、TXT/EPUB 打开与章节跳转。
3. Wi-Fi 扫描连接、NTP、重启自动连接。
4. 文件传输热点、屏幕 Wi‑Fi 二维码、手机上传/删除。
5. 蓝牙扫描连接，再抓取翻页器/HID 的真实输入报告。
6. 进度、收藏、书签和阅读时间重启恢复。
7. 壁纸、VLW 字体、休眠唤醒、关机和 OTA 回滚。
8. 最后根据残影与速度调整局刷/全刷策略。
