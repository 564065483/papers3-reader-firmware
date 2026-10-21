"""macOS 控制层：鼠标、键盘、应用、剪贴板、电源、音量、显示器。

依赖 pyobjc（pyobjc-framework-Quartz，会自动带上 AppKit），
并需要在“系统设置 → 隐私与安全性”中为运行服务的程序（终端或 Python）开启：
  · 辅助功能 —— 用于模拟鼠标键盘
  · 屏幕录制 —— 用于远程屏幕与窗口标题
函数签名与 winctl 保持一致，上层代码无需区分平台。
"""
from __future__ import annotations

import os
import shlex
import subprocess
import threading
import time
from pathlib import Path

from .common import IS_MAC, NotSupported, run_quiet

OS = "mac"
PASTE_COMBO = "cmd+v"

Quartz = None
AppKit = None
_IMPORT_ERROR = ""
if IS_MAC:
    try:
        import AppKit  # noqa: F811
        import Quartz  # noqa: F811
    except ImportError as exc:  # pragma: no cover - 仅在缺少依赖时
        _IMPORT_ERROR = str(exc)


def _require() -> None:
    if not IS_MAC:
        raise NotSupported("此功能仅在 Mac 上可用")
    if Quartz is None:
        raise NotSupported(f"缺少 pyobjc，请执行 pip install pyobjc-framework-Quartz（{_IMPORT_ERROR}）")


def enable_dpi_awareness() -> None:  # 与 winctl 接口保持一致
    pass


def check_permissions(prompt: bool = True) -> dict:
    """检查（并请求）辅助功能、屏幕录制权限。"""
    result = {"accessibility": None, "screen": None}
    if Quartz is None:
        return result
    try:
        from ApplicationServices import AXIsProcessTrustedWithOptions, kAXTrustedCheckOptionPrompt
        result["accessibility"] = bool(AXIsProcessTrustedWithOptions({kAXTrustedCheckOptionPrompt: prompt}))
    except Exception:
        pass
    try:
        ok = bool(Quartz.CGPreflightScreenCaptureAccess())
        if not ok and prompt:
            Quartz.CGRequestScreenCaptureAccess()
        result["screen"] = ok
    except Exception:
        pass
    return result


# ---------------------------------------------------------------------------
# 键码表（ANSI 键盘布局）
# ---------------------------------------------------------------------------
KEYCODES: dict[str, int] = {
    "a": 0, "s": 1, "d": 2, "f": 3, "h": 4, "g": 5, "z": 6, "x": 7, "c": 8, "v": 9, "b": 11,
    "q": 12, "w": 13, "e": 14, "r": 15, "y": 16, "t": 17, "1": 18, "2": 19, "3": 20, "4": 21,
    "6": 22, "5": 23, "=": 24, "9": 25, "7": 26, "-": 27, "8": 28, "0": 29, "]": 30, "o": 31,
    "u": 32, "[": 33, "i": 34, "p": 35, "enter": 36, "return": 36, "l": 37, "j": 38, "'": 39,
    "k": 40, ";": 41, "\\": 42, ",": 43, "/": 44, "n": 45, "m": 46, ".": 47, "tab": 48,
    "space": 49, "`": 50, "backspace": 51, "esc": 53, "escape": 53,
    "cmd": 55, "command": 55, "win": 55, "meta": 55, "super": 55, "lwin": 55, "rwin": 54,
    "shift": 56, "lshift": 56, "capslock": 57, "alt": 58, "option": 58, "opt": 58, "lalt": 58,
    "ctrl": 59, "control": 59, "lctrl": 59, "rshift": 60, "ralt": 61, "rctrl": 62, "fn": 63,
    "decimal": 65, "multiply": 67, "add": 69, "divide": 75, "subtract": 78,
    "num0": 82, "num1": 83, "num2": 84, "num3": 85, "num4": 86, "num5": 87, "num6": 88,
    "num7": 89, "num8": 91, "num9": 92,
    "f1": 122, "f2": 120, "f3": 99, "f4": 118, "f5": 96, "f6": 97, "f7": 98, "f8": 100,
    "f9": 101, "f10": 109, "f11": 103, "f12": 111, "f13": 105, "f14": 107, "f15": 113,
    "f16": 106, "f17": 64, "f18": 79, "f19": 80, "f20": 90,
    "insert": 114, "ins": 114, "help": 114, "home": 115, "pageup": 116, "pgup": 116,
    "delete": 117, "del": 117, "end": 119, "pagedown": 121, "pgdn": 121,
    "left": 123, "right": 124, "down": 125, "up": 126,
}
_MOD_FLAGS = {55: 0x100000, 54: 0x100000, 56: 0x20000, 60: 0x20000,
              58: 0x80000, 61: 0x80000, 59: 0x40000, 62: 0x40000, 63: 0x800000}
# NX_KEYTYPE_* 系统按键（媒体键、音量键）
_SPECIAL = {"volumeup": 0, "volumedown": 1, "volumemute": 7, "playpause": 16,
            "medianext": 17, "mediaprev": 18, "mediastop": 16,
            "brightnessup": 2, "brightnessdown": 3}


def _code(name: str) -> int:
    key = name.strip().lower()
    if key in KEYCODES:
        return KEYCODES[key]
    raise ValueError(f"无法识别的按键：{name}")


def is_modifier(name: str) -> bool:
    try:
        return _code(name) in _MOD_FLAGS
    except ValueError:
        return False


# ---------------------------------------------------------------------------
# 键盘
# ---------------------------------------------------------------------------
_held_flags = 0  # 通过 key_down 按住的修饰键


def _post(event) -> None:
    Quartz.CGEventPost(Quartz.kCGHIDEventTap, event)


def _key(code: int, down: bool, flags: int) -> None:
    ev = Quartz.CGEventCreateKeyboardEvent(None, code, down)
    Quartz.CGEventSetFlags(ev, flags)
    _post(ev)


def _system_key(code: int) -> None:
    for down in (True, False):
        flags = 0xA00 if down else 0xB00
        data1 = (code << 16) | ((0xA if down else 0xB) << 8)
        ev = AppKit.NSEvent.otherEventWithType_location_modifierFlags_timestamp_windowNumber_context_subtype_data1_data2_(
            14, (0, 0), flags, 0, 0, None, 8, data1, -1)
        _post(ev.CGEvent())


def hotkey(combo: str) -> None:
    _require()
    parts = [p.lower() for p in combo.replace(" ", "").split("+") if p]
    parts = ["=" if p == "plus" else p for p in parts]
    if not parts:
        return
    if len(parts) == 1 and parts[0] in _SPECIAL:
        _system_key(_SPECIAL[parts[0]])
        return
    codes = [_code(p) for p in parts]
    flags = _held_flags
    for c in codes[:-1]:
        flags |= _MOD_FLAGS.get(c, 0)
        _key(c, True, flags)
    _key(codes[-1], True, flags)
    _key(codes[-1], False, flags)
    for c in reversed(codes[:-1]):
        flags &= ~_MOD_FLAGS.get(c, 0)
        _key(c, False, flags | _held_flags)


def key_down(name: str) -> None:
    global _held_flags
    _require()
    code = _code(name)
    _held_flags |= _MOD_FLAGS.get(code, 0)
    _key(code, True, _held_flags)


def key_up(name: str) -> None:
    global _held_flags
    _require()
    code = _code(name)
    _held_flags &= ~_MOD_FLAGS.get(code, 0)
    _key(code, False, _held_flags)


def type_text(text: str) -> None:
    """以 Unicode 字符串直接输入，支持中文、emoji，不受当前输入法影响。"""
    _require()
    buf = ""

    def flush():
        nonlocal buf
        if not buf:
            return
        units = len(buf.encode("utf-16-le")) // 2
        for down in (True, False):
            ev = Quartz.CGEventCreateKeyboardEvent(None, 0, down)
            Quartz.CGEventSetFlags(ev, 0)
            Quartz.CGEventKeyboardSetUnicodeString(ev, units, buf)
            _post(ev)
        buf = ""
        time.sleep(0.004)

    for ch in text.replace("\r\n", "\n"):
        if ch in "\n\t":
            flush()
            code = KEYCODES["return" if ch == "\n" else "tab"]
            _key(code, True, 0)
            _key(code, False, 0)
        else:
            buf += ch
            if len(buf) >= 16:  # 单个事件最多携带约 20 个 UTF-16 字符
                flush()
    flush()


# ---------------------------------------------------------------------------
# 鼠标
# ---------------------------------------------------------------------------
_BUTTONS = {"left": (0, 1, 2, 6), "right": (1, 3, 4, 7), "middle": (2, 25, 26, 27)}
_down_buttons: set[str] = set()
_last_click = {"t": 0.0, "pos": (0.0, 0.0), "button": "", "count": 0}


def cursor_pos() -> tuple[float, float]:
    _require()
    loc = Quartz.CGEventGetLocation(Quartz.CGEventCreate(None))
    return loc.x, loc.y


def _desktop_bounds() -> tuple[float, float, float, float]:
    mons = monitors()
    return (min(m["left"] for m in mons), min(m["top"] for m in mons),
            max(m["left"] + m["width"] for m in mons) - 1, max(m["top"] + m["height"] for m in mons) - 1)


_bounds_cache = {"t": 0.0, "v": (0.0, 0.0, 1920.0, 1080.0)}


def _clamp(x: float, y: float) -> tuple[float, float]:
    if time.time() - _bounds_cache["t"] > 5:
        _bounds_cache["v"] = _desktop_bounds()
        _bounds_cache["t"] = time.time()
    x0, y0, x1, y1 = _bounds_cache["v"]
    return min(max(x, x0), x1), min(max(y, y0), y1)


def _move_event(x: float, y: float, dx: float = 0, dy: float = 0) -> None:
    if "left" in _down_buttons:
        etype, btn = Quartz.kCGEventLeftMouseDragged, 0
    elif "right" in _down_buttons:
        etype, btn = Quartz.kCGEventRightMouseDragged, 1
    elif "middle" in _down_buttons:
        etype, btn = Quartz.kCGEventOtherMouseDragged, 2
    else:
        etype, btn = Quartz.kCGEventMouseMoved, 0
    ev = Quartz.CGEventCreateMouseEvent(None, etype, (x, y), btn)
    Quartz.CGEventSetIntegerValueField(ev, Quartz.kCGMouseEventDeltaX, int(dx))
    Quartz.CGEventSetIntegerValueField(ev, Quartz.kCGMouseEventDeltaY, int(dy))
    _post(ev)


def mouse_move(dx: int, dy: int) -> None:
    _require()
    x, y = cursor_pos()
    nx, ny = _clamp(x + dx, y + dy)
    _move_event(nx, ny, dx, dy)


def mouse_set_pos(x: int, y: int) -> None:
    _require()
    x, y = _clamp(x, y)
    if not _down_buttons:
        Quartz.CGWarpMouseCursorPosition((x, y))
    _move_event(x, y)


def _click_state(button: str) -> int:
    """macOS 依靠事件里的点击次数识别双击，这里按时间和位置推算。"""
    now = time.time()
    pos = cursor_pos()
    last = _last_click
    if (button == last["button"] and now - last["t"] < 0.45
            and abs(pos[0] - last["pos"][0]) < 6 and abs(pos[1] - last["pos"][1]) < 6):
        last["count"] = min(3, last["count"] + 1)
    else:
        last["count"] = 1
    last.update(t=now, pos=pos, button=button)
    return last["count"]


def _button_event(button: str, down: bool, clicks: int) -> None:
    btn, down_t, up_t, _ = _BUTTONS[button]
    ev = Quartz.CGEventCreateMouseEvent(None, down_t if down else up_t, cursor_pos(), btn)
    Quartz.CGEventSetIntegerValueField(ev, Quartz.kCGMouseEventClickState, clicks)
    Quartz.CGEventSetFlags(ev, _held_flags)
    _post(ev)


def mouse_button(button: str = "left", action: str = "click") -> None:
    _require()
    if button not in _BUTTONS:
        raise NotSupported(f"Mac 不支持鼠标按键：{button}")
    if action == "down":
        _button_event(button, True, _click_state(button))
        _down_buttons.add(button)
    elif action == "up":
        _button_event(button, False, _last_click["count"] or 1)
        _down_buttons.discard(button)
    elif action == "double":
        for n in (1, 2):
            _button_event(button, True, n)
            _button_event(button, False, n)
        _last_click.update(t=time.time(), count=2, button=button, pos=cursor_pos())
    else:
        n = _click_state(button)
        _button_event(button, True, n)
        _button_event(button, False, n)


def mouse_scroll(dy: int = 0, dx: int = 0) -> None:
    """单位与 Windows 一致（120 = 一格），这里换算成像素级平滑滚动。"""
    _require()
    py, px = round(dy / 3), round(dx / 3)
    if not py and not px:
        return
    ev = Quartz.CGEventCreateScrollWheelEvent(None, Quartz.kCGScrollEventUnitPixel, 2, py, px)
    Quartz.CGEventSetIntegerValueField(ev, Quartz.kCGScrollWheelEventIsContinuous, 1)
    _post(ev)


# ---------------------------------------------------------------------------
# 显示器
# ---------------------------------------------------------------------------
def monitors() -> list[dict]:
    """显示器位置（单位：点，与鼠标坐标一致），主显示器排在最前。"""
    _require()
    err, ids, count = Quartz.CGGetActiveDisplayList(16, None, None)
    main = Quartz.CGMainDisplayID()
    out = []
    for i, did in enumerate(ids[:count]):
        b = Quartz.CGDisplayBounds(did)
        out.append({"name": f"Display {i + 1}", "id": int(did),
                    "left": int(b.origin.x), "top": int(b.origin.y),
                    "width": int(b.size.width), "height": int(b.size.height),
                    "primary": did == main})
    out.sort(key=lambda m: (not m["primary"], m["left"], m["top"]))
    return out


def capture_display(mon: dict):
    """截取一个显示器（Retina 下为实际像素），返回 PIL 图像。"""
    from PIL import Image
    _require()
    cg = Quartz.CGDisplayCreateImage(mon["id"])
    if cg is None:
        raise PermissionError("无法截屏：请在“系统设置 → 隐私与安全性 → 屏幕录制”中允许运行本服务的程序")
    w, h = Quartz.CGImageGetWidth(cg), Quartz.CGImageGetHeight(cg)
    bpr = Quartz.CGImageGetBytesPerRow(cg)
    data = Quartz.CGDataProviderCopyData(Quartz.CGImageGetDataProvider(cg))
    return Image.frombuffer("RGBA", (w, h), bytes(data), "raw", "BGRA", bpr, 1).convert("RGB")


def monitor_off() -> None:
    _require()
    subprocess.Popen(["pmset", "displaysleepnow"])


def get_brightness():
    return None  # macOS 没有公开的亮度接口，使用亮度键代替


def set_brightness(level: int) -> None:
    raise NotSupported("Mac 请使用亮度键")


# ---------------------------------------------------------------------------
# 应用与窗口
# ---------------------------------------------------------------------------
_APP_DIRS = ["/Applications", "/Applications/Utilities", "/System/Applications",
             "/System/Applications/Utilities", str(Path.home() / "Applications")]


def list_apps() -> list[dict]:
    _require()
    seen, out = set(), []
    for folder in _APP_DIRS:
        p = Path(folder)
        if not p.is_dir():
            continue
        for app in list(p.glob("*.app")) + list(p.glob("*/*.app")):
            if app.stem.lower() in seen:
                continue
            seen.add(app.stem.lower())
            out.append({"name": _display_name(app), "target": str(app)})
    out.sort(key=lambda a: a["name"].lower())
    return out


def _display_name(app: Path) -> str:
    try:
        name = AppKit.NSFileManager.defaultManager().displayNameAtPath_(str(app))
        return str(name).removesuffix(".app")
    except Exception:
        return app.stem


def launch(target: str, args: str = "", cwd: str = "") -> None:
    """打开 .app / 文件 / 文件夹 / 网址；也可以只写应用名称，如 “Safari”。"""
    _require()
    target = os.path.expanduser(os.path.expandvars(target.strip().strip('"')))
    if not target:
        raise ValueError("没有填写要打开的目标")
    extra = ["--args", *shlex.split(args)] if args else []
    if os.path.exists(target):
        cmd = ["open", "-a", target, *extra] if target.endswith(".app") and extra else ["open", target]
    elif "://" in target or (":" in target and not target.startswith("/")):
        cmd = ["open", target]
    else:
        cmd = ["open", "-a", target, *extra]
    run_quiet(cmd, 15)


def list_windows() -> list[dict]:
    """Mac 上以“正在运行的应用”为单位管理，hwnd 字段为进程 PID。"""
    _require()
    titles: dict[int, str] = {}
    try:
        opts = Quartz.kCGWindowListOptionOnScreenOnly | Quartz.kCGWindowListExcludeDesktopElements
        for w in Quartz.CGWindowListCopyWindowInfo(opts, Quartz.kCGNullWindowID) or []:
            if w.get("kCGWindowLayer", 1) == 0 and w.get("kCGWindowName"):
                titles.setdefault(int(w["kCGWindowOwnerPID"]), str(w["kCGWindowName"]))
    except Exception:
        pass
    out = []
    for app in AppKit.NSWorkspace.sharedWorkspace().runningApplications():
        if app.activationPolicy() != 0:  # 只要 Dock 上显示的普通应用
            continue
        pid = int(app.processIdentifier())
        name = str(app.localizedName() or "")
        title = titles.get(pid)
        bundle = app.bundleURL()
        out.append({"hwnd": pid, "pid": pid, "title": f"{name} — {title}" if title and title != name else name,
                    "icon_path": str(bundle.path()) if bundle else "",
                    "process": name, "minimized": bool(app.isHidden()), "maximized": False,
                    "foreground": bool(app.isActive())})
    return out


def window_op(hwnd: int, op: str) -> None:
    _require()
    app = AppKit.NSRunningApplication.runningApplicationWithProcessIdentifier_(int(hwnd))
    if app is None:
        raise FileNotFoundError("应用已退出")
    if op in ("activate", "restore"):
        app.unhide()
        app.activateWithOptions_(1 << 1)  # NSApplicationActivateIgnoringOtherApps
    elif op == "minimize":
        app.hide()
    elif op == "maximize":
        app.activateWithOptions_(1 << 1)
        time.sleep(0.25)
        hotkey("ctrl+cmd+f")  # 切换全屏
    elif op == "close":
        app.terminate()
    else:
        raise ValueError(f"未知窗口操作：{op}")


def app_icon(path: str, size: int = 96) -> bytes:
    """应用 / 文件在访达中的图标（PNG）。"""
    from io import BytesIO

    from PIL import Image
    _require()
    image = AppKit.NSWorkspace.sharedWorkspace().iconForFile_(path)
    image.setSize_((size, size))
    tiff = image.TIFFRepresentation()
    rep = AppKit.NSBitmapImageRep.imageRepWithData_(tiff)
    png = rep.representationUsingType_properties_(AppKit.NSBitmapImageFileTypePNG, None)
    img = Image.open(BytesIO(bytes(png)))
    img.thumbnail((size, size))
    out = BytesIO()
    img.save(out, "PNG")
    return out.getvalue()


# ---------------------------------------------------------------------------
# 剪贴板
# ---------------------------------------------------------------------------
_UTF8_ENV = {**os.environ, "LANG": "en_US.UTF-8", "LC_ALL": "en_US.UTF-8"}


def get_clipboard() -> str:
    _require()
    return subprocess.run(["pbpaste"], capture_output=True, env=_UTF8_ENV, timeout=5).stdout.decode("utf-8", "replace")


def set_clipboard(text: str) -> None:
    _require()
    subprocess.run(["pbcopy"], input=text.encode("utf-8"), env=_UTF8_ENV, timeout=5, check=True)


# ---------------------------------------------------------------------------
# 音量（AppleScript，系统自带，无需额外依赖）
# ---------------------------------------------------------------------------
def _osascript(script: str, timeout: float = 10) -> str:
    return run_quiet(["osascript", "-e", script], timeout)


def volume_state() -> dict:
    _require()
    out = _osascript("get volume settings")  # output volume:50, input volume:..., output muted:false
    info = dict(part.strip().split(":", 1) for part in out.split(",") if ":" in part)
    level = info.get("output volume", "missing value")
    return {"precise": level != "missing value",
            "level": int(level) if level.isdigit() else None,
            "muted": info.get("output muted", "false").strip() == "true"}


def set_volume(level: float) -> None:
    _require()
    level = max(0, min(100, round(level)))
    _osascript(f"set volume output volume {level}" + (" without output muted" if level > 0 else ""))


def set_mute(muted: bool) -> None:
    _require()
    _osascript(f"set volume output muted {'true' if muted else 'false'}")


# ---------------------------------------------------------------------------
# 电源与系统
# ---------------------------------------------------------------------------
_power_timer: threading.Timer | None = None

SYSTEM_HOTKEYS = {
    "show_desktop": "f11", "task_view": "ctrl+up", "run_dialog": "cmd+space",
    "snip": "cmd+shift+5", "emoji_panel": "ctrl+cmd+space", "switch_window": "cmd+tab",
    "close_window": "cmd+w", "minimize_all": "cmd+alt+m", "desktop_left": "ctrl+left",
    "desktop_right": "ctrl+right", "lock": "ctrl+cmd+q", "app_windows": "ctrl+down",
    "quit_app": "cmd+q", "spotlight": "cmd+space", "force_quit": "cmd+alt+esc",
}
_SYSTEM_APPS = {"task_manager": "Activity Monitor", "explorer": "Finder"}


def _power_script(name: str) -> None:
    script = {"shutdown": "shut down", "restart": "restart", "logoff": "log out"}[name]
    _osascript(f'tell application "System Events" to {script}', 20)


def system_action(name: str, delay: int = 0) -> None:
    global _power_timer
    _require()
    if name in SYSTEM_HOTKEYS:
        hotkey(SYSTEM_HOTKEYS[name])
    elif name in _SYSTEM_APPS:
        run_quiet(["open", "-a", _SYSTEM_APPS[name]])
    elif name == "settings":
        run_quiet(["open", "-b", "com.apple.systempreferences"])
    elif name in ("sleep", "hibernate"):
        subprocess.Popen(["pmset", "sleepnow"])
    elif name in ("shutdown", "restart", "logoff"):
        if _power_timer:
            _power_timer.cancel()
        if delay > 0:
            _power_timer = threading.Timer(delay, _power_script, args=(name,))
            _power_timer.daemon = True
            _power_timer.start()
        else:
            _power_script(name)
    elif name == "cancel_shutdown":
        if _power_timer:
            _power_timer.cancel()
            _power_timer = None
    elif name == "monitor_off":
        monitor_off()
    elif name in ("action_center", "new_desktop"):
        raise NotSupported("Mac 上没有对应的系统快捷键")
    else:
        raise ValueError(f"未知系统操作：{name}")


def media_key(name: str) -> None:
    _require()
    _system_key({"playpause": 16, "next": 17, "prev": 18, "stop": 16}[name])


def reveal(path: str) -> None:
    _require()
    run_quiet(["open", "-R", path])


def move_to_trash(path: str) -> None:
    _require()
    url = AppKit.NSURL.fileURLWithPath_(os.path.abspath(path))
    ok, _, err = AppKit.NSFileManager.defaultManager().trashItemAtURL_resultingItemURL_error_(url, None, None)
    if not ok:
        raise OSError(str(err.localizedDescription()) if err else "移到废纸篓失败")
