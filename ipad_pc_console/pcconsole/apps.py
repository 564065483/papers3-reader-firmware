"""已安装应用列表与启动。

Windows：优先使用 PowerShell 的 Get-StartApps，它和“开始”菜单完全一致，
包含传统桌面程序与 Microsoft Store 应用；失败时扫描开始菜单快捷方式。
macOS：扫描“应用程序”文件夹。
"""
from __future__ import annotations

import json
import os
import subprocess
import threading
import time
from pathlib import Path

from . import icons, winctl
from .backend import ctl
from .common import CREATE_NO_WINDOW, IS_MAC

_cache: list[dict] = []
_cache_time = 0.0
_cache_file: Path | None = None
_refresh_lock = threading.Lock()
STALE_AFTER = 600  # 秒：超过后在后台刷新，期间仍立即返回旧列表
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


def set_cache_file(path: Path) -> None:
    """应用列表保存到磁盘：服务重启后第一次打开“应用”页也能立即显示。"""
    global _cache, _cache_time, _cache_file
    _cache_file = path
    try:
        data = json.loads(path.read_text("utf-8"))
        if isinstance(data, list) and data:
            _cache, _cache_time = data, path.stat().st_mtime
    except (OSError, ValueError):
        pass


def _scan() -> list[dict]:
    ctl._require()
    if IS_MAC:
        apps = ctl.list_apps()
    else:
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
    return result


def refresh(max_age: float = 0) -> list[dict]:
    """重新扫描应用（同一时间只运行一次），写入缓存并在后台预生成图标。

    max_age > 0 时，如果等锁期间别的线程刚刚扫描完（不超过 max_age 秒），直接复用其结果。"""
    global _cache, _cache_time
    with _refresh_lock:
        if max_age and _cache and time.time() - _cache_time < max_age:
            return _cache
        result = _scan()
        _cache, _cache_time = result, time.time()
        if _cache_file is not None:
            try:
                _cache_file.write_text(json.dumps(result, ensure_ascii=False), "utf-8")
            except OSError:
                pass
    icons.prewarm([a["target"] for a in result])
    return result


def list_apps(force: bool = False) -> list[dict]:
    if force:
        return refresh()
    if not _cache:
        return refresh(max_age=60)  # 启动时的后台扫描若正在进行，等它完成即可，不重复扫描
    if time.time() - _cache_time > STALE_AFTER and not _refresh_lock.locked():
        threading.Thread(target=refresh, daemon=True).start()
    return _cache


def launch(target: str, args: str = "", cwd: str = "") -> None:
    """启动程序 / 打开文件 / AppsFolder 应用 / URL / ms-settings: 等协议。"""
    if IS_MAC:
        ctl.launch(target, args, cwd)
        return
    winctl._require()
    target = os.path.expandvars(target.strip().strip('"'))
    if not target:
        raise ValueError("没有填写要打开的目标")
    if target.lower().startswith("shell:"):
        subprocess.Popen(["explorer.exe", target], creationflags=CREATE_NO_WINDOW)
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
        subprocess.Popen(target, shell=True, creationflags=CREATE_NO_WINDOW)
