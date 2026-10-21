"""按当前操作系统选择控制层：Windows → winctl，macOS → macctl。

其它系统（如 Linux）使用 winctl，所有控制函数会提示“不支持”，便于调试界面。
"""
from .common import IS_MAC

if IS_MAC:
    from . import macctl as ctl
else:
    from . import winctl as ctl

__all__ = ["ctl"]
