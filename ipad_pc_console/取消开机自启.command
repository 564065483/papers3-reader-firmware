#!/bin/bash
cd "$(dirname "$0")" || exit 1
.venv/bin/python run.py --remove-autostart
read -r -p "按回车键退出"
