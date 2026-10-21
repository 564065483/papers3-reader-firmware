@echo off
chcp 65001 >nul
title iPad 电脑控制台
cd /d "%~dp0"

where python >nul 2>nul
if errorlevel 1 (
  echo 未找到 Python，请先从 https://www.python.org/downloads/ 安装 Python 3.9 或更高版本，
  echo 安装时勾选 "Add python.exe to PATH"。
  pause
  exit /b 1
)

if not exist ".venv\Scripts\python.exe" (
  echo 首次运行，正在创建虚拟环境并安装依赖，请稍候...
  python -m venv .venv || goto :fail
  ".venv\Scripts\python.exe" -m pip install --upgrade pip >nul
  ".venv\Scripts\python.exe" -m pip install -r requirements.txt || goto :fail
)

".venv\Scripts\python.exe" run.py %*
pause
exit /b 0

:fail
echo 依赖安装失败，请检查网络后重试（国内网络可在 pip 命令后加 -i https://pypi.tuna.tsinghua.edu.cn/simple）。
pause
exit /b 1
