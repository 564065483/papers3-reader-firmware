@echo off
chcp 65001 >nul
cd /d "%~dp0"
".venv\Scripts\python.exe" run.py --remove-autostart
echo 正在停止后台运行的控制台服务...
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='pythonw.exe'\" | Where-Object { $_.CommandLine -like '*run.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"
pause
