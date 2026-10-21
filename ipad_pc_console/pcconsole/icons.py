"""应用图标、文件图标与图片缩略图（PNG，带内存缓存）。"""
from __future__ import annotations

import io
import os
from collections import OrderedDict

from PIL import Image, ImageOps

from .backend import ctl
from .common import IS_MAC, IS_WIN, NotSupported

_IMAGE_EXT = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".tif", ".tiff"}
_cache: "OrderedDict[tuple, bytes]" = OrderedDict()
_MAX = 600


def _thumbnail(path: str, size: int) -> bytes:
    with Image.open(path) as img:
        img = ImageOps.exif_transpose(img)
        img.thumbnail((size, size))
        if img.mode not in ("RGB", "RGBA"):
            img = img.convert("RGBA")
        out = io.BytesIO()
        img.save(out, "PNG")
        return out.getvalue()


def icon_png(path: str, size: int = 96) -> bytes:
    size = max(16, min(256, size))
    try:
        mtime = os.path.getmtime(path)
    except OSError:
        mtime = 0
    key = (path, size, mtime)
    if key in _cache:
        _cache.move_to_end(key)
        return _cache[key]
    ext = os.path.splitext(path)[1].lower()
    if ext in _IMAGE_EXT and os.path.isfile(path):
        data = _thumbnail(path, size)
    elif IS_MAC:
        data = ctl.app_icon(path, size)
    elif IS_WIN:
        app_like = path.lower().startswith("shell:") or ext in (".exe", ".lnk", ".url", ".appref-ms") \
            or os.path.isdir(path)
        data = ctl.shell_image(path, size, icon_only=app_like)
    else:
        raise NotSupported("当前系统不支持读取图标")
    _cache[key] = data
    if len(_cache) > _MAX:
        _cache.popitem(last=False)
    return data
