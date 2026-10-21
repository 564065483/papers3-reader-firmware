# iPad 电脑控制台（iPad → Windows）

把闲置的 iPad 变成 Windows 电脑的触控控制台：点一下按钮就能打开软件、文件、网址，执行快捷键和宏；还能当触控板/键盘、远程看屏幕并直接点触操作、调音量、管理窗口和进程、传文件、互传剪贴板、远程关机……

**iPad 端不需要安装任何 App**：电脑上运行一个小服务，iPad 用 Safari 打开网页并“添加到主屏幕”，就和原生 App 一样全屏使用。在 iPad Pro（M1）上效果最好，其它 iPad / iPhone / 安卓平板同样可用。

## 功能一览

| 页面 | 功能 |
| --- | --- |
| 🎛️ 控制台 | Stream Deck 式的大按钮面板；多页面；每个按钮可由多个步骤组成宏：启动程序 / Store 应用、打开文件或文件夹、打开网址、快捷键、输入文字、粘贴文字、后台命令、媒体、系统操作、等待。iPad 上直接编辑（图标、颜色、排序），配置保存在电脑上 |
| 🖱️ 触控板 | 单指移动、轻点左键、双指轻点右键、双指滚动（带惯性）、长按拖拽、三指手势（上=任务视图，下=显示桌面，左右=切换窗口）、左/中/右键、拖拽锁定、灵敏度与自然滚动 |
| ⌨️ 键盘 | 实时输入，支持 iPad 中文输入法；支持外接键盘（妙控键盘 ⌘C/⌘V 映射为 Ctrl）；Ctrl/Shift/Alt/Win 粘滞键、方向键、F1–F12；整段文字逐字输入或粘贴 |
| 🖥️ 远程屏幕 | 实时画面（多显示器、画质/帧率可调）；点哪里点哪里、长按右键、拖动、双指滚动、双指缩放；沉浸式全屏；截图 |
| 🎵 媒体 | 播放/暂停、上一首/下一首、精确音量滑块与静音、笔记本屏幕亮度、视频网站常用快捷键 |
| 🚀 应用 | 列出“开始”菜单中所有应用（含 Microsoft Store 应用），搜索并一键启动，📌 固定到控制台；正在运行的窗口：切换、最小化、最大化、关闭 |
| 📁 文件 | 浏览所有磁盘；在电脑上打开、在资源管理器中定位；在 iPad 上预览（图片/视频/PDF）或下载；从 iPad 上传到电脑；重命名、新建文件夹、删除到回收站；固定到控制台 |
| 📈 监控 | CPU（每个核心）、内存、网络速率、磁盘、电池、运行时间曲线；进程列表搜索排序、结束/强制结束 |
| ⚙️ 系统 | 锁定、睡眠、休眠、注销、定时关机/重启与取消、关闭显示器；显示桌面、任务管理器、虚拟桌面等；剪贴板双向互传；远程命令行（CMD / PowerShell，支持 cd） |
| 🔧 设置 | 手感、主题（深色/浅色/跟随系统）、按钮大小、导入/导出按钮配置、修改 PIN、注销所有设备 |

## 快速开始

1. **电脑上安装 Python 3.9+**：<https://www.python.org/downloads/>，安装时勾选 *Add python.exe to PATH*。
2. 把 `ipad_pc_console` 文件夹复制到电脑任意位置，双击 **`启动控制台.bat`**。
   首次运行会自动创建虚拟环境并安装依赖（约 1 分钟），然后显示 PIN 码和二维码，并在电脑浏览器里打开配对页。
3. Windows 弹出防火墙提示时，勾选 **专用网络** 并点“允许”。若 iPad 打不开页面，右键以管理员身份运行 **`允许防火墙.bat`**，并确认电脑当前 Wi‑Fi 是“专用网络”。
4. 确保 iPad 与电脑在**同一个 Wi‑Fi / 局域网**，用 iPad 相机扫配对页的二维码即可直接登录；或者在 Safari 输入窗口上显示的地址（如 `http://192.168.1.23:8765`）后输入 PIN。
5. Safari 中点 **分享 → 添加到主屏幕**，以后从主屏幕图标打开就是全屏 App。

### 开机自动启动

先成功运行一次 `启动控制台.bat`，再双击 **`设置开机自启.bat`**：以后登录 Windows 时服务会在后台（无窗口）自动运行。需要再次查看 PIN/二维码时，在电脑浏览器打开 <http://127.0.0.1:8765/pair>。取消请运行 `取消开机自启.bat`。

### 打包成 exe（可选）

运行 `打包exe.bat` 会生成 `dist\iPadPCConsole.exe`，复制到其它电脑无需安装 Python 即可运行。

## 使用技巧

- **添加按钮**：控制台页点“✎ 编辑”或长按任意按钮。“启动程序”可以填 `notepad.exe`、`chrome`、完整 exe 路径，或从“应用”页点 📌 直接固定；路径支持 `%USERPROFILE%` 等环境变量；`ms-settings:display` 这类协议也能打开。
- **宏**：一个按钮里可以加多个步骤，例如“打开微信 → 等待 1500ms → 快捷键 ctrl+f → 输入文字”。
- **快捷键写法**：`ctrl+shift+esc`、`win+shift+s`、`alt+tab`、`f5`、`ctrl+win+right`。可用的键名：ctrl shift alt win enter esc tab space backspace delete home end pageup pagedown up down left right f1–f24 printscreen volumeup volumedown volumemute playpause medianext mediaprev 以及字母、数字和 `; = , - . / [ ] \ '`。
- **管理员窗口**：Windows 不允许普通程序操作“以管理员身份运行”的窗口（如任务管理器）。如需控制它们，请以管理员身份运行 `启动控制台.bat`。
- **Ctrl+Alt+Del** 属于系统安全组合键，任何软件都无法模拟，这是 Windows 的限制。
- **精确音量** 依赖 `pycaw`（已包含在 requirements 中）；若安装失败会自动退化为音量键步进。
- **屏幕亮度** 只对笔记本内屏等支持 WMI 调光的显示器有效，不支持时媒体页不会显示亮度卡片。
- iPad 长时间使用建议接上电源，并在“设置 → 显示与亮度 → 自动锁定”中延长时间。

## 安全说明

- 服务只应在**家庭/办公室局域网**中使用，**不要**在路由器上做端口转发暴露到公网。
- 新设备登录需要 PIN 码；连续输错 5 次锁定 60 秒。登录后 iPad 保存访问令牌，设置页可一键“注销所有设备”。
- 配对页 `/pair` 只允许电脑本机访问。
- 配置文件位于 `%APPDATA%\iPadPCConsole\config.json`（PIN、令牌和所有按钮）。忘记 PIN 时可运行 `启动控制台.bat --reset-pin`，或直接查看该文件。

## 命令行参数

```
python run.py [--port 8765] [--host 0.0.0.0] [--config 路径] [--no-browser]
              [--reset-pin] [--install-autostart] [--remove-autostart]
```

## 技术结构

```
ipad_pc_console/
├─ run.py                 启动入口
├─ requirements.txt       aiohttp / psutil / Pillow / qrcode / pycaw
├─ *.bat                  Windows 一键脚本
└─ pcconsole/
   ├─ main.py             命令行、配对信息、开机自启
   ├─ server.py           HTTP 接口 + WebSocket（低延迟鼠标键盘）
   ├─ winctl.py           ctypes 调用 Win32：SendInput 键鼠、窗口、剪贴板、显示器、电源
   ├─ actions.py          按钮动作 / 宏执行器
   ├─ apps.py             开始菜单应用（Get-StartApps）与启动
   ├─ audio.py            系统音量（pycaw）
   ├─ screen.py           截屏串流（Pillow ImageGrab，自动适应网速）
   ├─ files.py            文件浏览、回收站删除
   ├─ sysinfo.py          性能与进程（psutil）
   ├─ config.py           配置与默认按钮
   └─ web/                iPad 端 PWA（原生 ES Modules，无需构建）
```

服务端在非 Windows 系统上也能启动（用于调试界面），此时 Windows 专属功能会返回“仅在 Windows 上可用”。
