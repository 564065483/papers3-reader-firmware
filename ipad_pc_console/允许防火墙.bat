@echo off
chcp 65001 >nul
:: 需要管理员权限：在局域网（专用网络）上放行控制台端口
net session >nul 2>&1
if errorlevel 1 (
  powershell -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
  exit /b
)
netsh advfirewall firewall delete rule name="iPad PC Console" >nul 2>&1
netsh advfirewall firewall add rule name="iPad PC Console" dir=in action=allow protocol=TCP localport=8765 profile=private,domain
echo.
echo 已允许端口 8765（专用/域网络）。若你的 Wi-Fi 被识别为“公用网络”，请在 Windows 设置中改为“专用网络”。
pause
