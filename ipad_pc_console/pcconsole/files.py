"""电脑文件浏览、传输与管理。"""
from __future__ import annotations

import ctypes
import os
import shutil
import string
import subprocess
from pathlib import Path

from . import winctl
from .backend import ctl
from .common import IS_MAC, IS_WIN, NotSupported


def quick_places() -> list[dict]:
    home = Path.home()
    places = [("主目录", "house", home)]
    for label, icon, name in (("桌面", "display", "Desktop"), ("下载", "download", "Downloads"),
                              ("文档", "doc", "Documents"), ("图片", "photo", "Pictures"),
                              ("视频", "film", "Movies" if IS_MAC else "Videos"), ("音乐", "music", "Music")):
        path = home / name
        onedrive = home / "OneDrive" / name
        if not path.is_dir() and onedrive.is_dir():
            path = onedrive
        if path.is_dir():
            places.append((label, icon, path))
    return [{"label": l, "icon": i, "path": str(p)} for l, i, p in places]


def drives() -> list[dict]:
    out = []
    if IS_WIN:
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        for i, letter in enumerate(string.ascii_uppercase):
            if mask & (1 << i):
                root = f"{letter}:\\"
                item = {"name": root, "path": root, "dir": True}
                try:
                    usage = shutil.disk_usage(root)
                    item.update(total=usage.total, free=usage.free)
                except OSError:
                    pass
                out.append(item)
    else:
        roots = [Path("/")]
        if IS_MAC and Path("/Volumes").is_dir():
            roots += [p for p in Path("/Volumes").iterdir() if p.is_dir() and not p.is_symlink()]
        for root in roots:
            item = {"name": "Macintosh HD" if IS_MAC and str(root) == "/" else (root.name or "/"),
                    "path": str(root), "dir": True}
            try:
                usage = shutil.disk_usage(root)
                item.update(total=usage.total, free=usage.free)
            except OSError:
                pass
            out.append(item)
    return out


def list_dir(path: str, show_hidden: bool = False) -> dict:
    if not path:
        return {"path": "", "parent": None, "entries": drives()}
    p = Path(os.path.expandvars(path)).expanduser()
    if not p.is_dir():
        raise FileNotFoundError(f"文件夹不存在：{path}")
    entries = []
    with os.scandir(p) as it:
        for e in it:
            if not show_hidden and (e.name.startswith(".") or _is_hidden(e)):
                continue
            try:
                st = e.stat()
                is_dir = e.is_dir()
            except OSError:
                continue
            entries.append({"name": e.name, "path": e.path, "dir": is_dir,
                            "size": 0 if is_dir else st.st_size, "mtime": st.st_mtime})
    entries.sort(key=lambda x: (not x["dir"], x["name"].lower()))
    parent = str(p.parent) if p.parent != p else ""
    return {"path": str(p), "parent": parent, "entries": entries}


def _is_hidden(entry: os.DirEntry) -> bool:
    if not IS_WIN:
        return False
    try:
        attrs = entry.stat(follow_symlinks=False).st_file_attributes  # type: ignore[attr-defined]
        return bool(attrs & 0x2 or attrs & 0x4)  # HIDDEN | SYSTEM
    except (OSError, AttributeError):
        return False


def unique_path(folder: Path, name: str) -> Path:
    name = os.path.basename(name.replace("\\", "/")) or "upload.bin"
    target = folder / name
    stem, suffix = target.stem, target.suffix
    n = 1
    while target.exists():
        target = folder / f"{stem} ({n}){suffix}"
        n += 1
    return target


def reveal(path: str) -> None:
    if IS_MAC:
        ctl.reveal(path)
        return
    winctl._require()
    subprocess.Popen(["explorer.exe", f"/select,{path}"])


def rename(path: str, new_name: str) -> str:
    if not new_name or any(c in new_name for c in '\\/:*?"<>|'):
        raise ValueError("文件名不合法")
    src = Path(path)
    dst = src.with_name(new_name)
    if dst.exists():
        raise FileExistsError("同名文件已存在")
    src.rename(dst)
    return str(dst)


def mkdir(parent: str, name: str) -> str:
    if not name or any(c in name for c in '\\/:*?"<>|'):
        raise ValueError("文件夹名不合法")
    target = Path(parent) / name
    target.mkdir()
    return str(target)


def delete_to_recycle_bin(path: str) -> None:
    """删除到回收站 / 废纸篓，可在电脑上还原。"""
    if IS_MAC:
        ctl.move_to_trash(path)
        return
    if not IS_WIN:
        raise NotSupported("删除到回收站仅支持 Windows 和 Mac")
    from ctypes import wintypes

    class SHFILEOPSTRUCTW(ctypes.Structure):
        _fields_ = [("hwnd", wintypes.HWND), ("wFunc", wintypes.UINT),
                    ("pFrom", ctypes.c_void_p), ("pTo", ctypes.c_void_p),
                    ("fFlags", ctypes.c_ushort), ("fAnyOperationsAborted", wintypes.BOOL),
                    ("hNameMappings", ctypes.c_void_p), ("lpszProgressTitle", wintypes.LPCWSTR)]

    FO_DELETE, FOF_SILENT, FOF_NOCONFIRMATION, FOF_ALLOWUNDO, FOF_NOERRORUI = 3, 4, 0x10, 0x40, 0x400
    # pFrom 需要以两个 NUL 结尾
    buf = ctypes.create_unicode_buffer(os.path.abspath(path) + "\0")
    op = SHFILEOPSTRUCTW(None, FO_DELETE, ctypes.addressof(buf), None,
                         FOF_SILENT | FOF_NOCONFIRMATION | FOF_ALLOWUNDO | FOF_NOERRORUI,
                         False, None, None)
    result = ctypes.windll.shell32.SHFileOperationW(ctypes.byref(op))
    if result != 0 or op.fAnyOperationsAborted:
        raise OSError(f"删除失败（错误码 {result}）")
