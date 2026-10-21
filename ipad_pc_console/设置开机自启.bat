@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo 请先运行一次 启动控制台.bat 完成安装。
  pause
  exit /b 1
)
".venv\Scripts\python.exe" run.py --install-autostart
echo 下次登录 Windows 时会在后台自动启动（无窗口）。
echo 立即在后台启动一次...
start "" ".venv\Scripts\pythonw.exe" run.py --no-browser
pause
