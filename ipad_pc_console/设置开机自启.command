#!/bin/bash
# macOS：登录后在后台自动运行控制台服务（LaunchAgent）
cd "$(dirname "$0")" || exit 1
if [ ! -x ".venv/bin/python" ]; then
  echo "请先双击运行一次 启动控制台.command 完成安装。"; read -r -p "按回车键退出"; exit 1
fi
.venv/bin/python run.py --install-autostart
echo "注意：后台运行时需要在 系统设置 → 隐私与安全性 → 辅助功能 / 屏幕录制 中允许 .venv 里的 Python。"
read -r -p "按回车键退出"
