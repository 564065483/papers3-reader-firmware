"""配置文件读写：PIN、访问令牌、端口和控制台按钮布局。"""
from __future__ import annotations

import copy
import json
import os
import secrets
import sys
import threading
import uuid
from pathlib import Path


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


def default_pages() -> list[dict]:
    return [
        {"id": "home", "name": "常用", "buttons": [
            _btn("此电脑", "💻", "blue", ("launch", "explorer.exe")),
            _btn("浏览器", "🌐", "cyan", ("url", "https://www.bing.com")),
            _btn("记事本", "📝", "yellow", ("launch", "notepad.exe")),
            _btn("计算器", "🧮", "orange", ("launch", "calc.exe")),
            _btn("命令行", "⌨️", "gray", ("launch", "cmd.exe")),
            _btn("PowerShell", "🟦", "indigo", ("launch", "powershell.exe")),
            _btn("下载", "📥", "green", ("open", r"%USERPROFILE%\Downloads")),
            _btn("文档", "📂", "teal", ("open", r"%USERPROFILE%\Documents")),
            _btn("桌面文件夹", "🗂️", "purple", ("open", r"%USERPROFILE%\Desktop")),
            _btn("Windows 设置", "⚙️", "gray", ("open", "ms-settings:")),
            _btn("任务管理器", "📊", "red", ("hotkey", "ctrl+shift+esc")),
            _btn("截图", "✂️", "pink", ("hotkey", "win+shift+s")),
            _btn("显示桌面", "🖥️", "blue", ("hotkey", "win+d")),
            _btn("任务视图", "🪟", "indigo", ("hotkey", "win+tab")),
            _btn("锁屏", "🔒", "red", ("system", "lock")),
        ]},
        {"id": "keys", "name": "快捷键", "buttons": [
            _btn("复制", "📋", "blue", ("hotkey", "ctrl+c")),
            _btn("粘贴", "📌", "blue", ("hotkey", "ctrl+v")),
            _btn("剪切", "✂️", "blue", ("hotkey", "ctrl+x")),
            _btn("撤销", "↩️", "orange", ("hotkey", "ctrl+z")),
            _btn("重做", "↪️", "orange", ("hotkey", "ctrl+y")),
            _btn("全选", "🔲", "teal", ("hotkey", "ctrl+a")),
            _btn("保存", "💾", "green", ("hotkey", "ctrl+s")),
            _btn("查找", "🔍", "cyan", ("hotkey", "ctrl+f")),
            _btn("新标签页", "➕", "purple", ("hotkey", "ctrl+t")),
            _btn("关闭标签页", "✖️", "purple", ("hotkey", "ctrl+w")),
            _btn("恢复标签页", "♻️", "purple", ("hotkey", "ctrl+shift+t")),
            _btn("刷新", "🔄", "teal", ("hotkey", "f5")),
            _btn("切换窗口", "🔀", "indigo", ("hotkey", "alt+tab")),
            _btn("关闭窗口", "❌", "red", ("hotkey", "alt+f4")),
            _btn("最大化", "⬆️", "gray", ("hotkey", "win+up")),
            _btn("全屏 F11", "⛶", "gray", ("hotkey", "f11")),
            _btn("左虚拟桌面", "⬅️", "indigo", ("hotkey", "ctrl+win+left")),
            _btn("右虚拟桌面", "➡️", "indigo", ("hotkey", "ctrl+win+right")),
        ]},
        {"id": "macro", "name": "宏示例", "buttons": [
            _btn("记事本问候", "👋", "yellow", ("launch", "notepad.exe"), ("wait", "1200"),
                 ("paste", "你好，这段文字来自 iPad 控制台！\n")),
            _btn("上班模式", "💼", "green", ("url", "https://outlook.live.com"),
                 ("wait", "500"), ("open", r"%USERPROFILE%\Documents")),
            _btn("刷新 DNS", "🧹", "gray", ("command", "ipconfig /flushdns")),
            _btn("静音并锁屏", "🤫", "red", ("media", "mute"), ("system", "lock")),
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
