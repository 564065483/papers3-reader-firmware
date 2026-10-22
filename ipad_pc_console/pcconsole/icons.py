"""应用图标、文件图标与图片缩略图（PNG）。

· 使用独立的小线程池生成，不会占用执行按钮动作的线程，点按钮不会被大量图标请求拖慢；
· 内存 + 磁盘两级缓存，服务重启后也能立即返回；
· 应用列表读取完成后在后台预先生成全部应用图标。
"""
from __future__ import annotations

import base64
import hashlib
import io
import os
import threading
from collections import OrderedDict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image, ImageOps

from .backend import ctl
from .common import IS_MAC, IS_WIN, NotSupported

_IMAGE_EXT = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".tif", ".tiff"}
_mem: "OrderedDict[tuple, bytes]" = OrderedDict()
_mem_lock = threading.Lock()
_MAX = 800
_disk_dir: Path | None = None

# 图标专用线程池：最多 3 个线程，与动作执行互不影响
executor = ThreadPoolExecutor(max_workers=3, thread_name_prefix="icon")


def set_cache_dir(path: Path) -> None:
    global _disk_dir
    try:
        path.mkdir(parents=True, exist_ok=True)
        _disk_dir = path
    except OSError:
        _disk_dir = None


def _thumbnail(path: str, size: int) -> bytes:
    with Image.open(path) as img:
        img = ImageOps.exif_transpose(img)
        img.thumbnail((size, size))
        if img.mode not in ("RGB", "RGBA"):
            img = img.convert("RGBA")
        out = io.BytesIO()
        img.save(out, "PNG")
        return out.getvalue()


def _generate(path: str, size: int) -> bytes:
    ext = os.path.splitext(path)[1].lower()
    if ext in _IMAGE_EXT and os.path.isfile(path):
        return _thumbnail(path, size)
    if IS_MAC:
        return ctl.app_icon(path, size)
    if IS_WIN:
        app_like = path.lower().startswith("shell:") or ext in (".exe", ".lnk", ".url", ".appref-ms") \
            or os.path.isdir(path)
        return ctl.shell_image(path, size, icon_only=app_like)
    raise NotSupported("当前系统不支持读取图标")


def icon_png(path: str, size: int = 96) -> bytes:
    size = max(16, min(256, int(size)))
    try:
        mtime = os.path.getmtime(path)
    except OSError:
        mtime = 0
    key = (path, size, mtime)
    with _mem_lock:
        if key in _mem:
            _mem.move_to_end(key)
            return _mem[key]
    disk = None
    if _disk_dir is not None:
        disk = _disk_dir / (hashlib.sha1(repr(key).encode("utf-8")).hexdigest() + ".png")
        if disk.is_file():
            data = disk.read_bytes()
            _remember(key, data)
            return data
    data = _generate(path, size)
    _remember(key, data)
    if disk is not None:
        try:
            disk.write_bytes(data)
        except OSError:
            pass
    return data


def _remember(key: tuple, data: bytes) -> None:
    with _mem_lock:
        _mem[key] = data
        if len(_mem) > _MAX:
            _mem.popitem(last=False)


def icon_data_uri(path: str, size: int = 96) -> str | None:
    try:
        return "data:image/png;base64," + base64.b64encode(icon_png(path, size)).decode("ascii")
    except Exception:
        return None


def prewarm(paths: list[str], size: int = 128) -> None:
    """后台逐个生成图标写入磁盘缓存（单线程，避免影响前台）。"""
    def work():
        for p in paths:
            try:
                icon_png(p, size)
            except Exception:
                pass
    executor.submit(work)
