"""按钮动作执行器。

控制台上的每个按钮由若干“步骤”组成，按顺序执行，可组合成宏：
    {"type": "launch",  "value": "notepad.exe", "args": ""}   启动程序 / 应用（Mac 可写 Safari）
    {"type": "open",    "value": "D:\\资料"}                    打开文件、文件夹
    {"type": "url",     "value": "https://www.bilibili.com"}   用默认浏览器打开网址
    {"type": "hotkey",  "value": "ctrl+shift+esc"}             组合键（Mac 用 cmd / option / ctrl）
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

from . import apps, audio
from .backend import ctl
from .common import CREATE_NO_WINDOW, NotSupported


def media_action(name: str) -> None:
    if name == "volup":
        audio.step(+4)
    elif name == "voldown":
        audio.step(-4)
    elif name == "mute":
        audio.set_mute(None)
    elif name in ("playpause", "next", "prev", "stop"):
        ctl.media_key(name)
    else:
        raise ValueError(f"未知媒体操作：{name}")


def run_background_command(cmd: str) -> None:
    ctl._require()
    subprocess.Popen(cmd, shell=True, cwd=os.path.expanduser("~"), creationflags=CREATE_NO_WINDOW)


def _run_sync(kind: str, value: str, step: dict) -> None:
    if kind == "launch":
        apps.launch(value, step.get("args", ""), step.get("cwd", ""))
    elif kind in ("open", "url"):
        apps.launch(value)
    elif kind in ("hotkey", "key"):
        ctl.hotkey(value)
    elif kind == "text":
        ctl.type_text(value)
    elif kind == "paste":
        ctl.set_clipboard(value)
        ctl.hotkey(ctl.PASTE_COMBO)
    elif kind == "command":
        run_background_command(value)
    elif kind == "system":
        ctl.system_action(value, int(step.get("delay", 0) or 0))
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
            # pycaw 的 COM 对象属于主线程，Windows 上音量操作留在主线程执行
            if audio.MAIN_THREAD:
                media_action(value)
            else:
                await loop.run_in_executor(None, media_action, value)
            continue
        await loop.run_in_executor(None, _run_sync, kind, value, step)
        # 连续步骤之间留一点间隔，给目标程序响应时间
        if len(steps) > 1:
            await asyncio.sleep(0.05)


__all__ = ["run_actions", "media_action", "NotSupported"]
