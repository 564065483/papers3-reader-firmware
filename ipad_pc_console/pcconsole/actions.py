"""按钮动作执行器。

控制台上的每个按钮由若干“步骤”组成，按顺序执行，可组合成宏：
    {"type": "launch",  "value": "notepad.exe", "args": ""}   启动程序 / 应用
    {"type": "open",    "value": "D:\\资料"}                    打开文件、文件夹
    {"type": "url",     "value": "https://www.bilibili.com"}   用默认浏览器打开网址
    {"type": "hotkey",  "value": "ctrl+shift+esc"}             组合键
    {"type": "text",    "value": "你好"}                       逐字输入文本
    {"type": "paste",   "value": "你好"}                       写入剪贴板并粘贴（适合长文本）
    {"type": "command", "value": "ipconfig /flushdns"}         后台执行命令
    {"type": "media",   "value": "playpause"}                  媒体 / 音量
    {"type": "system",  "value": "lock"}                       系统操作
    {"type": "wait",    "value": "500"}                        等待毫秒
"""
from __future__ import annotations

import asyncio
import os
import subprocess

from . import apps, audio, winctl

MEDIA = {
    "playpause": "playpause", "next": "medianext", "prev": "mediaprev", "stop": "mediastop",
    "volup": None, "voldown": None, "mute": None,
}

SYSTEM = {
    "lock": "锁定电脑", "sleep": "睡眠", "hibernate": "休眠",
    "shutdown": "关机", "restart": "重启", "logoff": "注销",
    "cancel_shutdown": "取消关机/重启", "monitor_off": "关闭显示器",
    "show_desktop": "显示桌面", "task_view": "任务视图", "task_manager": "任务管理器",
    "explorer": "文件资源管理器", "settings": "Windows 设置", "run_dialog": "运行对话框",
    "snip": "截图工具", "action_center": "通知中心", "emoji_panel": "表情面板",
    "switch_window": "切换到上一个窗口", "close_window": "关闭当前窗口",
    "minimize_all": "最小化全部窗口", "desktop_left": "左侧虚拟桌面",
    "desktop_right": "右侧虚拟桌面", "new_desktop": "新建虚拟桌面",
}

_SYSTEM_HOTKEYS = {
    "show_desktop": "win+d", "task_view": "win+tab", "task_manager": "ctrl+shift+esc",
    "explorer": "win+e", "settings": "win+i", "run_dialog": "win+r", "snip": "win+shift+s",
    "action_center": "win+n", "emoji_panel": "win+.", "switch_window": "alt+tab",
    "close_window": "alt+f4", "minimize_all": "win+m", "desktop_left": "ctrl+win+left",
    "desktop_right": "ctrl+win+right", "new_desktop": "ctrl+win+d",
}


def system_action(name: str, delay: int = 0) -> None:
    if name in _SYSTEM_HOTKEYS:
        winctl.hotkey(_SYSTEM_HOTKEYS[name])
    elif name == "lock":
        winctl.lock()
    elif name == "sleep":
        winctl.suspend(False)
    elif name == "hibernate":
        winctl.suspend(True)
    elif name == "shutdown":
        winctl.shutdown_cmd("/s", "/t", str(max(0, delay)))
    elif name == "restart":
        winctl.shutdown_cmd("/r", "/t", str(max(0, delay)))
    elif name == "logoff":
        winctl.shutdown_cmd("/l")
    elif name == "cancel_shutdown":
        winctl.shutdown_cmd("/a")
    elif name == "monitor_off":
        winctl.monitor_off()
    else:
        raise ValueError(f"未知系统操作：{name}")


def media_action(name: str) -> None:
    if name == "volup":
        audio.step(+4)
    elif name == "voldown":
        audio.step(-4)
    elif name == "mute":
        audio.set_mute(None)
    elif name in MEDIA:
        winctl.hotkey(MEDIA[name])
    else:
        raise ValueError(f"未知媒体操作：{name}")


def run_background_command(cmd: str) -> None:
    winctl._require()
    subprocess.Popen(cmd, shell=True, cwd=os.path.expanduser("~"),
                     creationflags=winctl.CREATE_NO_WINDOW)


def _run_sync(kind: str, value: str, step: dict) -> None:
    if kind == "launch":
        apps.launch(value, step.get("args", ""), step.get("cwd", ""))
    elif kind in ("open", "url"):
        apps.launch(value)
    elif kind in ("hotkey", "key"):
        winctl.hotkey(value)
    elif kind == "text":
        winctl.type_text(value)
    elif kind == "paste":
        winctl.set_clipboard(value)
        winctl.hotkey("ctrl+v")
    elif kind == "command":
        run_background_command(value)
    elif kind == "media":
        media_action(value)
    elif kind == "system":
        system_action(value, int(step.get("delay", 0) or 0))
    else:
        raise ValueError(f"未知动作类型：{kind}")


async def run_actions(steps: list[dict]) -> None:
    loop = asyncio.get_running_loop()
    for step in steps:
        kind = str(step.get("type", "")).strip()
        value = str(step.get("value", ""))
        if kind == "wait":
            await asyncio.sleep(max(0, min(60000, int(float(value or 0)))) / 1000)
            continue
        if kind == "media":
            # pycaw 的 COM 对象在主线程创建，音量操作留在主线程执行
            media_action(value)
            continue
        await loop.run_in_executor(None, _run_sync, kind, value, step)
        # 连续步骤之间留一点间隔，给目标程序响应时间
        if len(steps) > 1:
            await asyncio.sleep(0.05)
