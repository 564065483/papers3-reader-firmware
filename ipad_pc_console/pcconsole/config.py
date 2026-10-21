"""配置文件读写：PIN、访问令牌、端口和控制台按钮布局。"""
from __future__ import annotations

import copy
import json
import os
import secrets
import socket
import sys
import threading
import uuid
from pathlib import Path

from .common import OS_TYPE


def default_config_path() -> Path:
    if sys.platform == "win32":
        base = Path(os.environ.get("APPDATA", Path.home()))
    else:
        base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return base / "iPadPCConsole" / "config.json"


def _btn(label: str, icon: str, color: str, *steps: tuple) -> dict:
    actions = []
    for step in steps:
        action = {"type": step[0], "value": step[1]}
        if len(step) > 2:
            action["args"] = step[2]
        actions.append(action)
    return {"id": uuid.uuid4().hex[:10], "label": label, "icon": icon, "color": color,
            "actions": actions}


def default_pages(os_type: str = OS_TYPE) -> list[dict]:
    if os_type == "mac":
        return _mac_pages()
    return [
        {"id": "home", "name": "常用", "buttons": [
            _btn("文件资源管理器", "sf:folder", "blue", ("launch", "explorer.exe")),
            _btn("浏览器", "sf:globe", "cyan", ("url", "https://www.bing.com")),
            _btn("记事本", "sf:notes", "yellow", ("launch", "notepad.exe")),
            _btn("计算器", "sf:calculator", "orange", ("launch", "calc.exe")),
            _btn("命令提示符", "sf:terminal", "gray", ("launch", "cmd.exe")),
            _btn("PowerShell", "sf:code", "indigo", ("launch", "powershell.exe")),
            _btn("下载", "sf:download", "green", ("open", r"%USERPROFILE%\Downloads")),
            _btn("文档", "sf:doc", "teal", ("open", r"%USERPROFILE%\Documents")),
            _btn("桌面文件夹", "sf:display", "purple", ("open", r"%USERPROFILE%\Desktop")),
            _btn("Windows 设置", "sf:gear", "gray", ("open", "ms-settings:")),
            _btn("任务管理器", "sf:chart", "red", ("hotkey", "ctrl+shift+esc")),
            _btn("截图", "sf:scissors", "pink", ("hotkey", "win+shift+s")),
            _btn("显示桌面", "sf:macwindow", "blue", ("hotkey", "win+d")),
            _btn("任务视图", "sf:taskview", "indigo", ("hotkey", "win+tab")),
            _btn("锁屏", "sf:lock", "red", ("system", "lock")),
        ]},
        {"id": "keys", "name": "快捷键", "buttons": _shortcut_buttons({
            "copy": "ctrl+c", "paste": "ctrl+v", "cut": "ctrl+x", "undo": "ctrl+z", "redo": "ctrl+y",
            "all": "ctrl+a", "save": "ctrl+s", "find": "ctrl+f", "newtab": "ctrl+t", "closetab": "ctrl+w",
            "reopen": "ctrl+shift+t", "refresh": "f5", "switch": "alt+tab", "quit": "alt+f4",
            "full": "f11", "left": "ctrl+win+left", "right": "ctrl+win+right",
        })},
        {"id": "macro", "name": "宏示例", "buttons": [
            _btn("记事本问候", "sf:sparkles", "yellow", ("launch", "notepad.exe"), ("wait", "1200"),
                 ("paste", "你好，这段文字来自 iPad 控制台！\n")),
            _btn("上班模式", "sf:briefcase", "green", ("url", "https://outlook.live.com"),
                 ("wait", "500"), ("open", r"%USERPROFILE%\Documents")),
            _btn("刷新 DNS", "sf:wifi", "gray", ("command", "ipconfig /flushdns")),
            _btn("静音并锁屏", "sf:moon", "red", ("media", "mute"), ("system", "lock")),
        ]},
    ]


def _shortcut_buttons(keys: dict) -> list[dict]:
    spec = [("复制", "copy", "blue", "copy"), ("粘贴", "clipboard", "blue", "paste"),
            ("剪切", "scissors", "blue", "cut"), ("撤销", "undo", "orange", "undo"),
            ("重做", "redo", "orange", "redo"), ("全选", "selection", "teal", "all"),
            ("保存", "save", "green", "save"), ("查找", "search", "cyan", "find"),
            ("新标签页", "plus", "purple", "newtab"), ("关闭标签页", "xmark", "purple", "closetab"),
            ("恢复标签页", "undo", "purple", "reopen"), ("刷新", "refresh", "teal", "refresh"),
            ("切换窗口", "shuffle", "indigo", "switch"), ("关闭窗口", "xcircle", "red", "quit"),
            ("全屏", "fullscreen", "gray", "full"), ("左侧桌面", "arrow.left", "indigo", "left"),
            ("右侧桌面", "arrow.right", "indigo", "right")]
    return [_btn(label, f"sf:{icon}", color, ("hotkey", keys[k])) for label, icon, color, k in spec if k in keys]


def _mac_pages() -> list[dict]:
    return [
        {"id": "home", "name": "常用", "buttons": [
            _btn("访达", "sf:folder", "blue", ("launch", "Finder")),
            _btn("Safari", "sf:compass", "cyan", ("launch", "Safari")),
            _btn("备忘录", "sf:notes", "yellow", ("launch", "Notes")),
            _btn("计算器", "sf:calculator", "orange", ("launch", "Calculator")),
            _btn("终端", "sf:terminal", "gray", ("launch", "Terminal")),
            _btn("系统设置", "sf:gear", "gray", ("system", "settings")),
            _btn("活动监视器", "sf:chart", "red", ("system", "task_manager")),
            _btn("下载", "sf:download", "green", ("open", "~/Downloads")),
            _btn("文稿", "sf:doc", "teal", ("open", "~/Documents")),
            _btn("桌面", "sf:display", "purple", ("open", "~/Desktop")),
            _btn("截图", "sf:scissors", "pink", ("hotkey", "cmd+shift+5")),
            _btn("调度中心", "sf:taskview", "indigo", ("hotkey", "ctrl+up")),
            _btn("聚焦搜索", "sf:search", "blue", ("hotkey", "cmd+space")),
            _btn("显示桌面", "sf:macwindow", "blue", ("system", "show_desktop")),
            _btn("锁屏", "sf:lock", "red", ("system", "lock")),
        ]},
        {"id": "keys", "name": "快捷键", "buttons": _shortcut_buttons({
            "copy": "cmd+c", "paste": "cmd+v", "cut": "cmd+x", "undo": "cmd+z", "redo": "cmd+shift+z",
            "all": "cmd+a", "save": "cmd+s", "find": "cmd+f", "newtab": "cmd+t", "closetab": "cmd+w",
            "reopen": "cmd+shift+t", "refresh": "cmd+r", "switch": "cmd+tab", "quit": "cmd+q",
            "full": "ctrl+cmd+f", "left": "ctrl+left", "right": "ctrl+right",
        })},
        {"id": "macro", "name": "宏示例", "buttons": [
            _btn("备忘录问候", "sf:sparkles", "yellow", ("launch", "Notes"), ("wait", "1500"),
                 ("hotkey", "cmd+n"), ("wait", "600"), ("paste", "你好，这段文字来自 iPad 控制台！\n")),
            _btn("上班模式", "sf:briefcase", "green", ("url", "https://outlook.live.com"),
                 ("wait", "500"), ("open", "~/Documents")),
            _btn("关闭显示器", "sf:display.slash", "gray", ("system", "monitor_off")),
            _btn("静音并锁屏", "sf:moon", "red", ("media", "mute"), ("system", "lock")),
        ]},
    ]


class Config:
    def __init__(self, path: Path):
        self.path = path
        self._lock = threading.Lock()
        self.data: dict = {}
        self.load()

    def load(self) -> None:
        if self.path.exists():
            try:
                self.data = json.loads(self.path.read_text("utf-8"))
            except (OSError, ValueError):
                backup = self.path.with_suffix(".broken.json")
                self.path.replace(backup)
                print(f"[!] 配置文件损坏，已备份到 {backup}，将使用默认配置")
                self.data = {}
        changed = False
        defaults = {
            "port": 8765,
            "pin": f"{secrets.randbelow(1_000_000):06d}",
            "token": secrets.token_hex(24),
            "device_id": uuid.uuid4().hex,
            "name": socket.gethostname().removesuffix(".local"),
            "pages": default_pages(),
        }
        for key, value in defaults.items():
            if key not in self.data:
                self.data[key] = value
                changed = True
        if changed:
            self.save()

    def save(self) -> None:
        with self._lock:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            tmp = self.path.with_suffix(".tmp")
            tmp.write_text(json.dumps(self.data, ensure_ascii=False, indent=2), "utf-8")
            tmp.replace(self.path)

    def __getitem__(self, key):
        return self.data[key]

    def set(self, key, value) -> None:
        self.data[key] = value
        self.save()

    def reset_pages(self) -> list[dict]:
        self.set("pages", default_pages())
        return copy.deepcopy(self.data["pages"])
