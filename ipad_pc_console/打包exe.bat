@echo off
chcp 65001 >nul
cd /d "%~dp0"
:: 打包成单个 exe（无需在目标电脑安装 Python）
".venv\Scripts\python.exe" -m pip install pyinstaller || goto :eof
".venv\Scripts\python.exe" -m PyInstaller --noconfirm --onefile --name iPadPCConsole --icon NONE --add-data "pcconsole\web;pcconsole\web" --hidden-import pycaw.pycaw --hidden-import comtypes.stream run.py
echo 生成文件：dist\iPadPCConsole.exe
pause
