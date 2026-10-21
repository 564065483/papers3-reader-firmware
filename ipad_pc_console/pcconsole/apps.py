"""已安装应用列表与启动。

优先使用 PowerShell 的 Get-StartApps，它和“开始”菜单完全一致，
包含传统桌面程序与 Microsoft Store 应用；失败时扫描开始菜单快捷方式。
"""
from __future__ import annotations

import json
import os
import subprocess
import time
from pathlib import Path

from . import winctl

_cache: list[dict] = []
_cache_time = 0.0
_SKIP_WORDS = ("uninstall", "卸载", "readme", "help", "帮助", "documentation", "website")


def _skip(name: str) -> bool:
    low = name.lower()
    return any(w in low for w in _SKIP_WORDS)


def _from_start_apps() -> list[dict]:
    raw = winctl._powershell("Get-StartApps | Select-Object Name,AppID | ConvertTo-Json -Compress", 30)
    data = json.loads(raw) if raw else []
    if isinstance(data, dict):
        data = [data]
    return [{"name": d["Name"], "target": "shell:AppsFolder\\" + d["AppID"]}
            for d in data if d.get("Name") and d.get("AppID")]


def _from_shortcuts() -> list[dict]:
    roots = [Path(os.environ.get("ProgramData", r"C:\ProgramData")),
             Path(os.environ.get("APPDATA", ""))]
    out = []
    for root in roots:
        menu = root / "Microsoft" / "Windows" / "Start Menu" / "Programs"
        if not menu.is_dir():
            continue
        for path in menu.rglob("*"):
            if path.suffix.lower() in (".lnk", ".url", ".appref-ms"):
                out.append({"name": path.stem, "target": str(path)})
    return out


def list_apps(refresh: bool = False) -> list[dict]:
    global _cache, _cache_time
    if _cache and not refresh and time.time() - _cache_time < 600:
        return _cache
    winctl._require()
    try:
        apps = _from_start_apps()
    except Exception:
        apps = []
    if not apps:
        apps = _from_shortcuts()
    seen, result = set(), []
    for app in sorted(apps, key=lambda a: a["name"].lower()):
        key = app["name"].lower()
        if key in seen or _skip(app["name"]):
            continue
        seen.add(key)
        result.append(app)
    _cache, _cache_time = result, time.time()
    return result


def launch(target: str, args: str = "", cwd: str = "") -> None:
    """启动程序 / 打开文件 / AppsFolder 应用 / URL / ms-settings: 等协议。"""
    winctl._require()
    target = os.path.expandvars(target.strip().strip('"'))
    if not target:
        raise ValueError("没有填写要打开的目标")
    if target.lower().startswith("shell:"):
        subprocess.Popen(["explorer.exe", target], creationflags=winctl.CREATE_NO_WINDOW)
        return
    if args:
        work = os.path.expandvars(cwd) if cwd else (os.path.dirname(target) or None)
        if work and not os.path.isdir(work):
            work = None
        subprocess.Popen(f'"{target}" {os.path.expandvars(args)}', cwd=work)
        return
    try:
        os.startfile(target)  # type: ignore[attr-defined]
    except FileNotFoundError:
        # 例如 "notepad"、"calc" 这类在 PATH 中的命令
        subprocess.Popen(target, shell=True, creationflags=winctl.CREATE_NO_WINDOW)
