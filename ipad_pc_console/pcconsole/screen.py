"""屏幕截图与画面串流（拉取式：iPad 每取完一帧再请求下一帧，自动适应网速）。"""
from __future__ import annotations

import io

from PIL import Image, ImageDraw

from .backend import ctl
from .common import IS_MAC, IS_WIN

try:
    from PIL import ImageGrab
except ImportError:  # 部分 Linux 构建没有 ImageGrab
    ImageGrab = None


def list_monitors() -> list[dict]:
    if IS_WIN or IS_MAC:
        return ctl.monitors()
    try:
        w, h = ImageGrab.grab().size
    except Exception:
        w, h = 1920, 1080
    return [{"name": "Display", "left": 0, "top": 0, "width": w, "height": h, "primary": True}]


def _cursor_shape(x: float, y: float, s: float) -> list[tuple[float, float]]:
    pts = [(0, 0), (0, 17), (4.5, 13), (7.5, 20), (10, 19), (7, 12), (13, 12)]
    return [(x + px * s, y + py * s) for px, py in pts]


def _placeholder(mon: dict, message: str) -> Image.Image:
    img = Image.new("RGB", (max(320, mon["width"] // 2), max(180, mon["height"] // 2)), (32, 36, 44))
    ImageDraw.Draw(img).text((24, 24), message, fill=(220, 220, 220))
    return img


def _grab(mon: dict) -> Image.Image:
    box = (mon["left"], mon["top"], mon["left"] + mon["width"], mon["top"] + mon["height"])
    if IS_WIN:
        return ImageGrab.grab(bbox=box, all_screens=True)
    if IS_MAC:
        try:
            return ctl.capture_display(mon)
        except PermissionError:
            raise
        except Exception:
            return ImageGrab.grab(bbox=box)  # 退回系统 screencapture（较慢）
    return ImageGrab.grab()


def capture(index: int = 0, max_width: int = 1600, quality: int = 70,
            draw_cursor: bool = True) -> tuple[bytes, dict]:
    mons = list_monitors()
    mon = mons[index] if 0 <= index < len(mons) else mons[0]
    try:
        img = _grab(mon)
    except Exception as exc:
        img = _placeholder(mon, f"Screen capture unavailable: {exc}")

    if max_width and img.width > max_width:
        img = img.resize((max_width, max(1, round(img.height * max_width / img.width))), Image.BILINEAR)
    # 图像像素与屏幕坐标的比例（Retina 屏幕坐标单位是“点”，像素是它的 2 倍）
    scale = img.width / mon["width"]

    if draw_cursor and (IS_WIN or IS_MAC):
        try:
            cx, cy = ctl.cursor_pos()
            if mon["left"] <= cx < mon["left"] + mon["width"] and mon["top"] <= cy < mon["top"] + mon["height"]:
                x, y = (cx - mon["left"]) * scale, (cy - mon["top"]) * scale
                s = max(1.0, scale * 1.4)
                ImageDraw.Draw(img).polygon(_cursor_shape(x, y, s), fill=(255, 255, 255), outline=(0, 0, 0))
        except Exception:
            pass

    if img.mode != "RGB":
        img = img.convert("RGB")
    buf = io.BytesIO()
    img.save(buf, "JPEG", quality=max(20, min(95, quality)))
    return buf.getvalue(), mon
