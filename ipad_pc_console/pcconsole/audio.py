"""系统主音量控制。

安装了 pycaw 时可以读取/设置精确音量；没有安装时退化为模拟音量键（每次约 2%）。
"""
from __future__ import annotations

from . import winctl

_endpoint = None
_error: str | None = None


def _get_endpoint():
    global _endpoint, _error
    if _endpoint is not None:
        return _endpoint
    if not winctl.IS_WIN:
        _error = "非 Windows 系统"
        return None
    try:
        from pycaw.pycaw import AudioUtilities
        speakers = AudioUtilities.GetSpeakers()
        endpoint = getattr(speakers, "EndpointVolume", None)
        if endpoint is None:  # 旧版 pycaw
            from ctypes import POINTER, cast
            from comtypes import CLSCTX_ALL
            from pycaw.pycaw import IAudioEndpointVolume
            iface = speakers.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
            endpoint = cast(iface, POINTER(IAudioEndpointVolume))
        _endpoint = endpoint
    except Exception as exc:  # 未安装或没有音频设备
        _error = str(exc)
    return _endpoint


def available() -> bool:
    return _get_endpoint() is not None


def get_state() -> dict:
    ep = _get_endpoint()
    if ep is None:
        return {"precise": False, "level": None, "muted": None, "reason": _error}
    try:
        return {"precise": True,
                "level": round(ep.GetMasterVolumeLevelScalar() * 100),
                "muted": bool(ep.GetMute())}
    except Exception as exc:
        _reset()
        return {"precise": False, "level": None, "muted": None, "reason": str(exc)}


def _reset() -> None:
    # 默认播放设备切换（插拔耳机等）后旧的接口会失效，下次重新获取
    global _endpoint
    _endpoint = None


def set_level(level: float) -> dict:
    ep = _get_endpoint()
    if ep is None:
        raise winctl.NotSupported("精确音量需要安装 pycaw（pip install pycaw）")
    try:
        ep.SetMasterVolumeLevelScalar(max(0.0, min(1.0, level / 100)), None)
        if level > 0 and ep.GetMute():
            ep.SetMute(0, None)
    except Exception:
        _reset()
        raise
    return get_state()


def set_mute(muted: bool | None = None) -> dict:
    ep = _get_endpoint()
    if ep is None:
        winctl.hotkey("volumemute")
        return get_state()
    try:
        target = (not ep.GetMute()) if muted is None else muted
        ep.SetMute(1 if target else 0, None)
    except Exception:
        _reset()
        raise
    return get_state()


def step(delta: int) -> dict:
    ep = _get_endpoint()
    if ep is None:
        key = "volumeup" if delta > 0 else "volumedown"
        for _ in range(max(1, abs(delta) // 2)):
            winctl.hotkey(key)
        return get_state()
    state = get_state()
    return set_level((state["level"] or 0) + delta)
