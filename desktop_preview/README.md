# Paper S3 固件桌面预览

这个程序直接编译并调用固件里的 `main/ui/ui_renderer.cpp`，不是另做一套网页。窗口内部始终按开发板竖屏坐标 `540 × 960` 绘制，鼠标点击会映射成真实触摸坐标。

## 运行

双击：

- `打开固件预览.bat`：86% 缩放，适合大多数电脑屏幕。
- `打开高清预览.bat`：100% 原尺寸，适合检查文字、间距和图标细节。

也可以拖动窗口边角缩放。预览截图会保存到 `screenshots/last-screen.png`，首页示例保存为 `screenshots/firmware-home.png`。

## 编译

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
python -m platformio run -d .
```

## 持续预览

双击 `持续预览.bat`。它会先编译并打开 540 × 960 的高清预览，然后监听固件 UI 源文件；保存代码后会自动重新编译并刷新窗口。关闭持续预览的控制台即可停止监听。

持续预览调用的仍然是固件里的 `main/ui/ui_renderer.cpp`，所以这里看到的坐标、字体、图标和交互热区就是 LVGL 桌面版当前实现的内容。

桌面预览复用真实固件的 LVGL 绘制代码和交互热区；它不模拟墨水屏刷新时间、残影、真实触摸误差、电池、Wi‑Fi、蓝牙、SD 卡和 IMU 的硬件行为。
