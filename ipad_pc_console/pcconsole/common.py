"""各平台共用的常量与工具。"""
from __future__ import annotations

import subprocess
import sys

IS_WIN = sys.platform == "win32"
IS_MAC = sys.platform == "darwin"
OS_TYPE = "windows" if IS_WIN else "mac" if IS_MAC else "linux"

# 让 Windows 子进程不弹出黑色控制台窗口（pythonw 运行时尤其重要）
CREATE_NO_WINDOW = 0x08000000 if IS_WIN else 0


class NotSupported(RuntimeError):
    pass


def run_quiet(args, timeout: float = 15, input_text: str | None = None) -> str:
    """执行命令并返回 UTF-8 输出；失败时抛出 RuntimeError。"""
    proc = subprocess.run(args, capture_output=True, timeout=timeout,
                          input=input_text.encode("utf-8") if input_text is not None else None,
                          creationflags=CREATE_NO_WINDOW)
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode("utf-8", "replace").strip() or f"{args[0]} 执行失败")
    return proc.stdout.decode("utf-8", "replace").strip()
