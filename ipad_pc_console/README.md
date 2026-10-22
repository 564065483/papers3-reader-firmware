# iPad 电脑控制台（iPad → Windows / Mac）

把闲置的 iPad 变成电脑的触控控制台：点一下按钮就能打开软件、文件、网址，执行快捷键和宏；还能当触控板/键盘、远程看屏幕并直接点触操作、调音量、管理窗口和进程、传文件、互传剪贴板、远程关机……**同时支持 Windows 和 Mac，一台 iPad 可以随时切换控制家里的多台电脑。**

**iPad 端不需要安装任何 App**：每台电脑上运行一个小服务，iPad 用 Safari 打开网页并“添加到主屏幕”，就和原生 App 一样全屏使用。在 iPad Pro（M1）上效果最好，其它 iPad / iPhone / 安卓平板同样可用。

界面采用 iPadOS 风格的**毛玻璃材质 + 弹簧动画**：按压回弹、页面切换、滑动指示器、底部弹出面板、灵动岛式提示；图标为一套 SF Symbols 风格的线性图标，应用列表与文件显示电脑上的真实图标和图片缩略图；支持深色 / 浅色 / 跟随系统。

## 功能一览

| 页面 | 功能 |
| --- | --- |
| 控制台 | 快捷指令风格的彩色按钮面板；多页面；每个按钮可由多个步骤组成宏：打开应用、文件或文件夹、网址、快捷键、输入文字、粘贴文字、后台命令、媒体、系统操作、等待。iPad 上直接编辑（70 多个矢量图标或 emoji、12 种系统色、排序），实时预览 |
| 触控板 | 单指移动、轻点左键、双指轻点右键、双指滚动（带惯性）、长按拖拽、三指手势（上 = 任务视图 / 调度中心，下 = 显示桌面，左右 = 切换应用）、左/中/右键、拖拽锁定 |
| 键盘 | 实时输入，支持 iPad 中文输入法；支持外接键盘（控制 Mac 时 ⌘ 就是 ⌘，控制 Windows 时 ⌘C/⌘V 映射为 Ctrl）；粘滞修饰键、方向键、F1–F12；整段文字逐字输入或粘贴 |
| 远程屏幕 | 实时画面（多显示器、Retina、画质/帧率可调）；点哪里点哪里、长按右键、拖动、双指滚动、双指缩放；沉浸式全屏；截图 |
| 媒体 | 播放/暂停、上一首/下一首、控制中心风格的音量滑条、笔记本屏幕亮度（Windows）/ 亮度键（Mac）、视频网站常用快捷键 |
| 应用 | 列出电脑上所有应用（Windows 开始菜单含 Store 应用；Mac“应用程序”文件夹），显示真实图标，搜索并一键打开，📌 固定到控制台；运行中的窗口/应用：切换、最小化/隐藏、最大化/全屏、关闭/退出 |
| 文件 | 浏览所有磁盘；图片缩略图；在电脑上打开、在资源管理器 / 访达中显示；在 iPad 上预览或下载；从 iPad 上传；重命名、新建文件夹、删除到回收站 / 废纸篓；固定到控制台 |
| 监控 | CPU（每个核心）、内存、网络速率、磁盘、电池、运行时间曲线；进程列表搜索排序、结束 / 强制结束 |
| 系统 | 锁屏、睡眠、休眠、注销、定时关机/重启与取消、关闭显示器；显示桌面、任务管理器 / 活动监视器、虚拟桌面、聚焦搜索等；剪贴板双向互传；远程命令行（CMD / PowerShell / zsh / bash，支持 cd） |
| 设置 | 切换 / 添加电脑、重命名电脑、手感、主题、按钮大小、导入/导出按钮配置、修改 PIN、注销所有设备 |

## 快速开始

### Windows

1. 安装 Python 3.9+：<https://www.python.org/downloads/>，安装时勾选 *Add python.exe to PATH*。
2. 把 `ipad_pc_console` 文件夹复制到电脑任意位置，双击 **`启动控制台.bat`**。首次运行会自动安装依赖（约 1 分钟），然后显示 PIN 码和二维码，并打开配对页。
3. Windows 弹出防火墙提示时勾选 **专用网络** 并允许。若 iPad 打不开页面，右键以管理员身份运行 **`允许防火墙.bat`**（放行 TCP 8765 与局域网发现用的 UDP 8766），并确认当前 Wi‑Fi 是“专用网络”。

### Mac

1. 若没有 Python 3，在“终端”执行 `xcode-select --install`，或从 <https://www.python.org/downloads/macos/> 安装。
2. 把 `ipad_pc_console` 文件夹复制到 Mac，双击 **`启动控制台.command`**（若提示无法打开：右键 → 打开）。首次运行会自动安装依赖。
3. 按提示在 **系统设置 → 隐私与安全性** 中为“终端”打开：
   - **辅助功能**：用于模拟鼠标键盘（必需）；
   - **屏幕录制**：用于远程屏幕和窗口标题。
   授权后关闭终端窗口，再次双击 `启动控制台.command`。
4. 首次关机/重启时 macOS 可能询问是否允许控制“System Events”，选择允许。

### iPad

1. 确保 iPad 与电脑在**同一个 Wi‑Fi / 局域网**，用 iPad 相机扫电脑端配对页的二维码即可直接登录；或在 Safari 输入电脑窗口上显示的地址（如 `http://192.168.1.23:8765`）后输入 PIN。
2. **只显示控制台、隐藏网址栏**：
   - 推荐：Safari 中点 **分享 → 添加到主屏幕**，以后从主屏幕图标打开就是全屏 App（登录状态会自动带过去，无需再输 PIN）；
   - 临时：点右上角的全屏按钮；或在 设置 → 全屏显示 中打开“轻点自动全屏”。Safari 全屏时弹出键盘可能会退出全屏，长期使用请用主屏幕方式。

### 控制多台电脑（Windows + Mac）

在每台电脑上都运行服务端。iPad 上点右上角的**电脑名称胶囊**（或 设置 → 切换或添加电脑）：

- **附近的电脑**：同一局域网中运行着服务端的电脑会被自动发现，点“添加”并输入那台电脑的 PIN 即可；
- **手动添加**：输入 IP 地址（如 `192.168.1.30`）和 PIN；
- 之后点列表里的任意一台即可切换，每台电脑有自己的按钮布局。

建议把常开的那台电脑的网页“添加到主屏幕”，其余电脑都通过切换列表访问。

### 开机自动启动

先成功运行一次启动脚本，再双击 **`设置开机自启.bat`**（Windows）或 **`设置开机自启.command`**（Mac）：以后登录时服务会在后台自动运行。需要再次查看 PIN/二维码时，在电脑浏览器打开 <http://127.0.0.1:8765/pair>。取消请运行对应的“取消开机自启”脚本。

> Mac 后台运行时，权限需要授予 `.venv/bin/python` 实际指向的 Python 程序（在“辅助功能”列表中点 ＋ 添加，或在首次弹窗时允许）。

### 打包成 exe（可选，Windows）

运行 `打包exe.bat` 会生成 `dist\iPadPCConsole.exe`，复制到其它 Windows 电脑无需安装 Python 即可运行。

## 使用技巧

- **添加按钮**：控制台页点“编辑”或长按任意按钮。“打开应用”可以填 `notepad.exe`、`chrome`、完整 exe 路径；Mac 上直接写应用名，如 `Safari`、`微信`，或 `/Applications/xxx.app`。也可以在“应用”页点 📌 一键固定。路径支持 `%USERPROFILE%`、`~` 等写法；`ms-settings:display`、`https://…` 等协议也能打开。
- **快速切换窗口**：控制台底部的程序坞显示电脑上正在运行的窗口，轻点即切换，长按可最小化、最大化或关闭；“应用 → 打开的窗口”以卡片形式列出全部窗口，轻点卡片即切换。
- **应用页“最近使用”**：最近打开过的应用显示在最上方。应用列表在服务启动时就在后台读取并缓存到磁盘，图标也会预先生成，打开应用页无需等待；安装了新软件后点刷新按钮即可。
- **宏**：一个按钮里可以加多个步骤，例如“打开微信 → 等待 1500ms → 快捷键 ctrl+f → 输入文字”。
- **快捷键写法**：Windows 如 `ctrl+shift+esc`、`win+shift+s`、`alt+tab`；Mac 如 `cmd+c`、`cmd+shift+4`、`ctrl+up`、`ctrl+cmd+f`。可用键名：ctrl shift alt/option win/cmd enter esc tab space backspace delete home end pageup pagedown up down left right f1–f24 以及字母、数字和 `; = , - . / [ ] \ '`；媒体键 playpause medianext mediaprev volumeup volumedown volumemute（Mac 另有 brightnessup / brightnessdown）。
- **管理员窗口（Windows）**：Windows 不允许普通程序操作“以管理员身份运行”的窗口（如任务管理器）。如需控制它们，请以管理员身份运行 `启动控制台.bat`。
- **Ctrl+Alt+Del** 属于系统安全组合键，任何软件都无法模拟。
- **精确音量**：Windows 依赖 `pycaw`（已包含在 requirements 中，安装失败时退化为音量键步进）；Mac 使用系统自带的 AppleScript。
- **屏幕亮度**：Windows 仅支持笔记本内屏等支持 WMI 调光的显示器；Mac 使用亮度键模拟（部分机型外接屏无效）。
- iPad 长时间使用建议接上电源，并在“设置 → 显示与亮度 → 自动锁定”中延长时间。

## 安全说明

- 服务只应在**家庭/办公室局域网**中使用，**不要**在路由器上做端口转发暴露到公网。
- 新设备登录需要 PIN 码；连续输错 5 次锁定 60 秒。登录后 iPad 保存访问令牌，设置页可一键“注销所有设备”。
- 接口通过 `X-Token` 请求头鉴权（不使用 Cookie），因此允许一个网页跨域控制多台电脑而不会带来 CSRF 风险。
- 局域网发现广播只包含电脑名称、系统类型和端口，不包含 PIN 或令牌。
- 配对页 `/pair` 只允许电脑本机访问。
- 配置文件：Windows 为 `%APPDATA%\iPadPCConsole\config.json`，Mac 为 `~/.config/iPadPCConsole/config.json`（PIN、令牌和所有按钮）。忘记 PIN 时可加参数 `--reset-pin` 启动，或直接查看该文件。

## 命令行参数

```
python run.py [--port 8765] [--host 0.0.0.0] [--config 路径] [--no-browser]
              [--reset-pin] [--install-autostart] [--remove-autostart]
```

## 技术结构

```
ipad_pc_console/
├─ run.py                 启动入口
├─ requirements.txt       aiohttp / psutil / Pillow / qrcode / pycaw(Win) / pyobjc(Mac)
├─ *.bat / *.command      Windows / Mac 一键脚本
├─ tools/make_icons.py    生成主屏幕图标
└─ pcconsole/
   ├─ main.py             命令行、配对信息、开机自启（启动文件夹 / LaunchAgent）
   ├─ server.py           HTTP 接口 + WebSocket（低延迟鼠标键盘）+ 跨域
   ├─ backend.py          按系统选择控制层
   ├─ winctl.py           Windows：ctypes 调用 Win32（SendInput、窗口、剪贴板、显示器、电源、图标提取）
   ├─ macctl.py           macOS：Quartz 事件、AppKit、AppleScript、pmset
   ├─ discovery.py        局域网自动发现（UDP 广播）
   ├─ actions.py          按钮动作 / 宏执行器
   ├─ apps.py             已安装应用列表与启动
   ├─ audio.py            系统音量
   ├─ icons.py            应用图标、文件图标与图片缩略图
   ├─ screen.py           截屏串流（自动适应网速）
   ├─ files.py            文件浏览、回收站 / 废纸篓删除
   ├─ sysinfo.py          性能与进程（psutil）
   ├─ config.py           配置与默认按钮（Windows / Mac 各一套）
   └─ web/                iPad 端 PWA（原生 ES Modules，无需构建）
      ├─ app.css          玻璃材质主题与弹簧动画
      └─ js/icons.js      线性图标库
```

服务端在其它系统（如 Linux）上也能启动以调试界面，此时键鼠等控制功能会提示“不支持”。
