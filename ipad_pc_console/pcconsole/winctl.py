"""Windows 原生控制层：鼠标、键盘、窗口、剪贴板、电源与显示器。

全部通过 ctypes 调用 Win32 API 实现，不依赖第三方库。
非 Windows 平台导入本模块不会报错，但调用控制函数会抛出 NotSupported，
方便在其它系统上调试界面和接口。
"""
from __future__ import annotations

import ctypes
import subprocess
import sys
import time

IS_WIN = sys.platform == "win32"

# 让子进程不弹出黑色控制台窗口（pythonw 运行时尤其重要）
CREATE_NO_WINDOW = 0x08000000 if IS_WIN else 0


class NotSupported(RuntimeError):
    pass


def _require() -> None:
    if not IS_WIN:
        raise NotSupported("此功能仅在 Windows 电脑上可用")


# ---------------------------------------------------------------------------
# 虚拟键码表
# ---------------------------------------------------------------------------
VK: dict[str, int] = {
    "backspace": 0x08, "tab": 0x09, "enter": 0x0D, "return": 0x0D,
    "shift": 0x10, "ctrl": 0x11, "control": 0x11, "alt": 0x12, "menu": 0x12,
    "pause": 0x13, "capslock": 0x14, "esc": 0x1B, "escape": 0x1B,
    "space": 0x20, "pageup": 0x21, "pgup": 0x21, "pagedown": 0x22, "pgdn": 0x22,
    "end": 0x23, "home": 0x24, "left": 0x25, "up": 0x26, "right": 0x27, "down": 0x28,
    "printscreen": 0x2C, "prtsc": 0x2C, "insert": 0x2D, "ins": 0x2D,
    "delete": 0x2E, "del": 0x2E,
    "win": 0x5B, "lwin": 0x5B, "rwin": 0x5C, "apps": 0x5D, "contextmenu": 0x5D,
    "sleep": 0x5F,
    "num0": 0x60, "num1": 0x61, "num2": 0x62, "num3": 0x63, "num4": 0x64,
    "num5": 0x65, "num6": 0x66, "num7": 0x67, "num8": 0x68, "num9": 0x69,
    "multiply": 0x6A, "add": 0x6B, "subtract": 0x6D, "decimal": 0x6E, "divide": 0x6F,
    "numlock": 0x90, "scrolllock": 0x91,
    "lshift": 0xA0, "rshift": 0xA1, "lctrl": 0xA2, "rctrl": 0xA3, "lalt": 0xA4, "ralt": 0xA5,
    "browserback": 0xA6, "browserforward": 0xA7, "browserrefresh": 0xA8,
    "browserstop": 0xA9, "browsersearch": 0xAA, "browserfavorites": 0xAB, "browserhome": 0xAC,
    "volumemute": 0xAD, "volumedown": 0xAE, "volumeup": 0xAF,
    "medianext": 0xB0, "mediaprev": 0xB1, "mediastop": 0xB2, "playpause": 0xB3,
    "launchmail": 0xB4, "launchmedia": 0xB5, "launchapp1": 0xB6, "launchapp2": 0xB7,
    ";": 0xBA, "=": 0xBB, ",": 0xBC, "-": 0xBD, ".": 0xBE, "/": 0xBF, "`": 0xC0,
    "[": 0xDB, "\\": 0xDC, "]": 0xDD, "'": 0xDE,
}
for _c in "abcdefghijklmnopqrstuvwxyz":
    VK[_c] = ord(_c.upper())
for _d in "0123456789":
    VK[_d] = ord(_d)
for _i in range(1, 25):
    VK[f"f{_i}"] = 0x6F + _i

# 需要带 EXTENDEDKEY 标志的按键（方向键、编辑键、右侧修饰键等）
_EXTENDED = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2C, 0x2D, 0x2E,
             0x5B, 0x5C, 0x5D, 0x6F, 0x90, 0xA3, 0xA5}

_MODIFIERS = {"ctrl", "control", "shift", "alt", "win", "lwin", "rwin",
              "lctrl", "rctrl", "lshift", "rshift", "lalt", "ralt"}


def parse_key(name: str) -> int:
    key = name.strip().lower()
    if key in VK:
        return VK[key]
    if key.startswith("0x"):
        return int(key, 16)
    raise ValueError(f"无法识别的按键：{name}")


def parse_combo(combo: str) -> list[int]:
    """把 "ctrl+shift+esc" 解析为虚拟键码列表。单独的 "+" 键写作 "plus"。"""
    parts = [p for p in combo.replace(" ", "").split("+") if p]
    return [VK["="] if p.lower() == "plus" else parse_key(p) for p in parts]


# ---------------------------------------------------------------------------
# Win32 结构与函数声明
# ---------------------------------------------------------------------------
if IS_WIN:
    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    powrprof = ctypes.WinDLL("powrprof")
    try:
        dwmapi = ctypes.WinDLL("dwmapi")
    except OSError:  # 极老系统
        dwmapi = None

    ULONG_PTR = wintypes.WPARAM

    class MOUSEINPUT(ctypes.Structure):
        _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG),
                    ("mouseData", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
                    ("time", wintypes.DWORD), ("dwExtraInfo", ULONG_PTR)]

    class KEYBDINPUT(ctypes.Structure):
        _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD),
                    ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD),
                    ("dwExtraInfo", ULONG_PTR)]

    class HARDWAREINPUT(ctypes.Structure):
        _fields_ = [("uMsg", wintypes.DWORD), ("wParamL", wintypes.WORD),
                    ("wParamH", wintypes.WORD)]

    class _INPUTUNION(ctypes.Union):
        _fields_ = [("mi", MOUSEINPUT), ("ki", KEYBDINPUT), ("hi", HARDWAREINPUT)]

    class INPUT(ctypes.Structure):
        _anonymous_ = ("u",)
        _fields_ = [("type", wintypes.DWORD), ("u", _INPUTUNION)]

    class MONITORINFOEXW(ctypes.Structure):
        _fields_ = [("cbSize", wintypes.DWORD), ("rcMonitor", wintypes.RECT),
                    ("rcWork", wintypes.RECT), ("dwFlags", wintypes.DWORD),
                    ("szDevice", wintypes.WCHAR * 32)]

    user32.SendInput.argtypes = (wintypes.UINT, ctypes.POINTER(INPUT), ctypes.c_int)
    user32.SendInput.restype = wintypes.UINT
    user32.MapVirtualKeyW.argtypes = (wintypes.UINT, wintypes.UINT)
    user32.MapVirtualKeyW.restype = wintypes.UINT
    user32.SetCursorPos.argtypes = (ctypes.c_int, ctypes.c_int)
    user32.GetCursorPos.argtypes = (ctypes.POINTER(wintypes.POINT),)

    WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    MONITORENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HMONITOR, wintypes.HDC,
                                         ctypes.POINTER(wintypes.RECT), wintypes.LPARAM)
    user32.EnumWindows.argtypes = (WNDENUMPROC, wintypes.LPARAM)
    user32.EnumDisplayMonitors.argtypes = (wintypes.HDC, ctypes.c_void_p,
                                           MONITORENUMPROC, wintypes.LPARAM)
    user32.GetMonitorInfoW.argtypes = (wintypes.HMONITOR, ctypes.POINTER(MONITORINFOEXW))
    user32.IsWindowVisible.argtypes = (wintypes.HWND,)
    user32.IsIconic.argtypes = (wintypes.HWND,)
    user32.IsZoomed.argtypes = (wintypes.HWND,)
    user32.GetWindow.argtypes = (wintypes.HWND, wintypes.UINT)
    user32.GetWindow.restype = wintypes.HWND
    user32.GetWindowLongW.argtypes = (wintypes.HWND, ctypes.c_int)
    user32.GetWindowLongW.restype = wintypes.LONG
    user32.GetWindowTextLengthW.argtypes = (wintypes.HWND,)
    user32.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
    user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user32.GetForegroundWindow.restype = wintypes.HWND
    user32.SetForegroundWindow.argtypes = (wintypes.HWND,)
    user32.ShowWindow.argtypes = (wintypes.HWND, ctypes.c_int)
    user32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
    user32.OpenClipboard.argtypes = (wintypes.HWND,)
    user32.GetClipboardData.argtypes = (wintypes.UINT,)
    user32.GetClipboardData.restype = ctypes.c_void_p
    user32.SetClipboardData.argtypes = (wintypes.UINT, ctypes.c_void_p)
    user32.SetClipboardData.restype = ctypes.c_void_p
    kernel32.GlobalAlloc.argtypes = (wintypes.UINT, ctypes.c_size_t)
    kernel32.GlobalAlloc.restype = ctypes.c_void_p
    kernel32.GlobalLock.argtypes = (ctypes.c_void_p,)
    kernel32.GlobalLock.restype = ctypes.c_void_p
    kernel32.GlobalUnlock.argtypes = (ctypes.c_void_p,)
    kernel32.GlobalFree.argtypes = (ctypes.c_void_p,)
    powrprof.SetSuspendState.argtypes = (wintypes.BOOLEAN, wintypes.BOOLEAN, wintypes.BOOLEAN)
    if dwmapi is not None:
        dwmapi.DwmGetWindowAttribute.argtypes = (wintypes.HWND, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD)

INPUT_MOUSE, INPUT_KEYBOARD = 0, 1
KEYEVENTF_EXTENDEDKEY, KEYEVENTF_KEYUP, KEYEVENTF_UNICODE = 0x1, 0x2, 0x4
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_WHEEL, MOUSEEVENTF_HWHEEL = 0x0800, 0x1000
_BUTTON_FLAGS = {
    "left": (0x0002, 0x0004, 0),
    "right": (0x0008, 0x0010, 0),
    "middle": (0x0020, 0x0040, 0),
    "x1": (0x0080, 0x0100, 1),
    "x2": (0x0080, 0x0100, 2),
}


def enable_dpi_awareness() -> None:
    """使坐标以物理像素计算，保证截屏、点击位置在高分屏上对得上。"""
    if not IS_WIN:
        return
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            user32.SetProcessDPIAware()
        except Exception:
            pass


def _send(inputs: list) -> None:
    if not inputs:
        return
    arr = (INPUT * len(inputs))(*inputs)
    user32.SendInput(len(inputs), arr, ctypes.sizeof(INPUT))


def _mouse_input(flags: int, dx: int = 0, dy: int = 0, data: int = 0):
    return INPUT(type=INPUT_MOUSE, mi=MOUSEINPUT(dx, dy, data & 0xFFFFFFFF, flags, 0, 0))


def _key_input(vk: int, up: bool):
    flags = KEYEVENTF_KEYUP if up else 0
    if vk in _EXTENDED:
        flags |= KEYEVENTF_EXTENDEDKEY
    scan = user32.MapVirtualKeyW(vk, 0)
    return INPUT(type=INPUT_KEYBOARD, ki=KEYBDINPUT(vk, scan, flags, 0, 0))


def _unicode_input(code_unit: int, up: bool):
    flags = KEYEVENTF_UNICODE | (KEYEVENTF_KEYUP if up else 0)
    return INPUT(type=INPUT_KEYBOARD, ki=KEYBDINPUT(0, code_unit, flags, 0, 0))


# ---------------------------------------------------------------------------
# 鼠标
# ---------------------------------------------------------------------------
def mouse_move(dx: int, dy: int) -> None:
    _require()
    _send([_mouse_input(MOUSEEVENTF_MOVE, int(dx), int(dy))])


def mouse_set_pos(x: int, y: int) -> None:
    _require()
    user32.SetCursorPos(int(x), int(y))


def cursor_pos() -> tuple[int, int]:
    _require()
    pt = wintypes.POINT()
    user32.GetCursorPos(ctypes.byref(pt))
    return pt.x, pt.y


def mouse_button(button: str = "left", action: str = "click") -> None:
    """action: click / down / up / double"""
    _require()
    if button not in _BUTTON_FLAGS:
        raise ValueError(f"未知鼠标按键：{button}")
    down, up, xdata = _BUTTON_FLAGS[button]
    if action == "down":
        _send([_mouse_input(down, data=xdata)])
    elif action == "up":
        _send([_mouse_input(up, data=xdata)])
    else:
        times = 2 if action == "double" else 1
        seq = []
        for _ in range(times):
            seq += [_mouse_input(down, data=xdata), _mouse_input(up, data=xdata)]
        _send(seq)


def mouse_scroll(dy: int = 0, dx: int = 0) -> None:
    """dy > 0 向上滚动，dx > 0 向右滚动；单位为滚轮刻度（120 = 一格）。"""
    _require()
    seq = []
    if dy:
        seq.append(_mouse_input(MOUSEEVENTF_WHEEL, data=int(dy)))
    if dx:
        seq.append(_mouse_input(MOUSEEVENTF_HWHEEL, data=int(dx)))
    _send(seq)


# ---------------------------------------------------------------------------
# 键盘
# ---------------------------------------------------------------------------
def key_down(name: str) -> None:
    _require()
    _send([_key_input(parse_key(name), False)])


def key_up(name: str) -> None:
    _require()
    _send([_key_input(parse_key(name), True)])


def hotkey(combo: str) -> None:
    """按下组合键，例如 "ctrl+c"、"win+shift+s"、"f5"。"""
    _require()
    codes = parse_combo(combo)
    if not codes:
        return
    _send([_key_input(c, False) for c in codes] + [_key_input(c, True) for c in reversed(codes)])


def type_text(text: str) -> None:
    """以 Unicode 方式逐字输入，支持中文、emoji，不受当前输入法影响。"""
    _require()
    seq = []
    for ch in text.replace("\r\n", "\n"):
        if ch == "\n":
            seq += [_key_input(0x0D, False), _key_input(0x0D, True)]
            continue
        if ch == "\t":
            seq += [_key_input(0x09, False), _key_input(0x09, True)]
            continue
        data = ch.encode("utf-16-le")
        for i in range(0, len(data), 2):
            unit = int.from_bytes(data[i:i + 2], "little")
            seq += [_unicode_input(unit, False), _unicode_input(unit, True)]
    # 分批发送，避免一次注入过多导致部分程序丢字
    for i in range(0, len(seq), 200):
        _send(seq[i:i + 200])
        if i + 200 < len(seq):
            time.sleep(0.01)


def is_modifier(name: str) -> bool:
    return name.strip().lower() in _MODIFIERS


# ---------------------------------------------------------------------------
# 窗口
# ---------------------------------------------------------------------------
GW_OWNER = 4
GWL_EXSTYLE = -20
WS_EX_TOOLWINDOW = 0x00000080
DWMWA_CLOAKED = 14
SW_MAXIMIZE, SW_MINIMIZE, SW_RESTORE = 3, 6, 9
WM_CLOSE = 0x0010


def _is_cloaked(hwnd) -> bool:
    if dwmapi is None:
        return False
    val = ctypes.c_int(0)
    try:
        dwmapi.DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, ctypes.byref(val), ctypes.sizeof(val))
    except Exception:
        return False
    return bool(val.value)


def list_windows() -> list[dict]:
    """列出任务栏上可见的顶层窗口。"""
    _require()
    fg = user32.GetForegroundWindow() or 0
    out: list[dict] = []

    def callback(hwnd, _lparam):
        if not user32.IsWindowVisible(hwnd) or user32.GetWindow(hwnd, GW_OWNER):
            return True
        if user32.GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW:
            return True
        length = user32.GetWindowTextLengthW(hwnd)
        if length == 0 or _is_cloaked(hwnd):
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        out.append({
            "hwnd": int(hwnd or 0),
            "title": buf.value,
            "pid": int(pid.value),
            "minimized": bool(user32.IsIconic(hwnd)),
            "maximized": bool(user32.IsZoomed(hwnd)),
            "foreground": int(hwnd or 0) == int(fg),
        })
        return True

    user32.EnumWindows(WNDENUMPROC(callback), 0)
    return out


def window_op(hwnd: int, op: str) -> None:
    _require()
    h = wintypes.HWND(hwnd)
    if op == "activate":
        if user32.IsIconic(h):
            user32.ShowWindow(h, SW_RESTORE)
        # Windows 限制后台进程抢前台；先模拟一次 Alt 键即可解除限制
        _send([_key_input(0x12, False), _key_input(0x12, True)])
        user32.SetForegroundWindow(h)
    elif op == "minimize":
        user32.ShowWindow(h, SW_MINIMIZE)
    elif op == "maximize":
        user32.ShowWindow(h, SW_MAXIMIZE)
    elif op == "restore":
        user32.ShowWindow(h, SW_RESTORE)
    elif op == "close":
        user32.PostMessageW(h, WM_CLOSE, 0, 0)
    else:
        raise ValueError(f"未知窗口操作：{op}")


# ---------------------------------------------------------------------------
# 剪贴板
# ---------------------------------------------------------------------------
CF_UNICODETEXT = 13
GMEM_MOVEABLE = 0x0002


def _open_clipboard() -> None:
    for _ in range(10):
        if user32.OpenClipboard(None):
            return
        time.sleep(0.05)
    raise RuntimeError("剪贴板被其它程序占用，请稍后再试")


def get_clipboard() -> str:
    _require()
    _open_clipboard()
    try:
        handle = user32.GetClipboardData(CF_UNICODETEXT)
        if not handle:
            return ""
        ptr = kernel32.GlobalLock(handle)
        try:
            return ctypes.wstring_at(ptr) if ptr else ""
        finally:
            kernel32.GlobalUnlock(handle)
    finally:
        user32.CloseClipboard()


def set_clipboard(text: str) -> None:
    _require()
    data = text.encode("utf-16-le") + b"\x00\x00"
    handle = kernel32.GlobalAlloc(GMEM_MOVEABLE, len(data))
    if not handle:
        raise MemoryError("无法分配剪贴板内存")
    ptr = kernel32.GlobalLock(handle)
    ctypes.memmove(ptr, data, len(data))
    kernel32.GlobalUnlock(handle)
    _open_clipboard()
    try:
        user32.EmptyClipboard()
        if not user32.SetClipboardData(CF_UNICODETEXT, handle):
            kernel32.GlobalFree(handle)
            raise RuntimeError("写入剪贴板失败")
    finally:
        user32.CloseClipboard()


# ---------------------------------------------------------------------------
# 显示器
# ---------------------------------------------------------------------------
def monitors() -> list[dict]:
    """返回所有显示器的虚拟桌面坐标，主显示器排在最前。"""
    _require()
    out: list[dict] = []

    def callback(hmon, _hdc, _rect, _lparam):
        info = MONITORINFOEXW()
        info.cbSize = ctypes.sizeof(MONITORINFOEXW)
        user32.GetMonitorInfoW(hmon, ctypes.byref(info))
        r = info.rcMonitor
        out.append({
            "name": info.szDevice.replace("\\\\.\\", ""),
            "left": r.left, "top": r.top,
            "width": r.right - r.left, "height": r.bottom - r.top,
            "primary": bool(info.dwFlags & 1),
        })
        return True

    user32.EnumDisplayMonitors(None, None, MONITORENUMPROC(callback), 0)
    out.sort(key=lambda m: (not m["primary"], m["left"], m["top"]))
    return out


def monitor_off() -> None:
    _require()
    HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER = 0xFFFF, 0x0112, 0xF170
    user32.PostMessageW(wintypes.HWND(HWND_BROADCAST), WM_SYSCOMMAND, SC_MONITORPOWER, 2)


def _powershell(script: str, timeout: float = 15) -> str:
    proc = subprocess.run(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
         "-Command", "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;" + script],
        capture_output=True, timeout=timeout, creationflags=CREATE_NO_WINDOW,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode("utf-8", "replace").strip() or "PowerShell 执行失败")
    return proc.stdout.decode("utf-8", "replace").strip()


def get_brightness() -> int | None:
    """笔记本内置屏亮度（外接显示器通常不支持，返回 None）。"""
    _require()
    try:
        out = _powershell("(Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightness"
                          " -ErrorAction Stop | Select-Object -First 1).CurrentBrightness", 8)
        return int(out) if out else None
    except Exception:
        return None


def set_brightness(level: int) -> None:
    _require()
    level = max(0, min(100, int(level)))
    _powershell("Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightnessMethods"
                f" -ErrorAction Stop | Invoke-CimMethod -MethodName WmiSetBrightness"
                f" -Arguments @{{Timeout=1;Brightness={level}}} | Out-Null", 8)


# ---------------------------------------------------------------------------
# 电源
# ---------------------------------------------------------------------------
def lock() -> None:
    _require()
    user32.LockWorkStation()


def suspend(hibernate: bool = False) -> None:
    _require()
    powrprof.SetSuspendState(bool(hibernate), False, False)


def shutdown_cmd(*args: str) -> None:
    _require()
    subprocess.Popen(["shutdown", *args], creationflags=CREATE_NO_WINDOW)
