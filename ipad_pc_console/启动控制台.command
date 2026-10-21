#!/bin/bash
# macOS：双击运行。首次运行会创建虚拟环境并安装依赖。
cd "$(dirname "$0")" || exit 1
if ! command -v python3 >/dev/null 2>&1; then
  echo "未找到 python3。请先安装：https://www.python.org/downloads/macos/ （或执行 xcode-select --install）"
  read -r -p "按回车键退出"; exit 1
fi
if [ ! -x ".venv/bin/python" ]; then
  echo "首次运行，正在创建虚拟环境并安装依赖，请稍候..."
  python3 -m venv .venv || exit 1
  .venv/bin/python -m pip install --upgrade pip >/dev/null
  if ! .venv/bin/python -m pip install -r requirements.txt; then
    echo "依赖安装失败，请检查网络（国内网络可加 -i https://pypi.tuna.tsinghua.edu.cn/simple）"
    read -r -p "按回车键退出"; exit 1
  fi
fi
echo "提示：首次使用请在 系统设置 → 隐私与安全性 中为“终端”打开【辅助功能】和【屏幕录制】，然后重新运行。"
exec .venv/bin/python run.py "$@"
