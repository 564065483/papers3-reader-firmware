"""性能监控与进程管理（基于 psutil，跨平台）。"""
from __future__ import annotations

import getpass
import os
import platform
import socket
import time

import psutil

_last_net = None
_procs: dict[int, psutil.Process] = {}
_cpu_count = psutil.cpu_count() or 1

psutil.cpu_percent(interval=None)
psutil.cpu_percent(interval=None, percpu=True)


def basic_info() -> dict:
    return {
        "hostname": socket.gethostname(),
        "user": getpass.getuser(),
        "os": f"{platform.system()} {platform.release()} ({platform.version()})",
        "cpu": platform.processor() or platform.machine(),
        "cores": _cpu_count,
        "memory": psutil.virtual_memory().total,
        "boot_time": psutil.boot_time(),
    }


def _disks() -> list[dict]:
    out = []
    for part in psutil.disk_partitions(all=False):
        if "cdrom" in part.opts or not part.fstype:
            continue
        try:
            usage = psutil.disk_usage(part.mountpoint)
        except (PermissionError, OSError):
            continue
        out.append({"mount": part.mountpoint, "fs": part.fstype,
                    "total": usage.total, "used": usage.used, "percent": usage.percent})
    return out


def _net_rate() -> dict:
    global _last_net
    now = time.time()
    io = psutil.net_io_counters()
    rate = {"up": 0.0, "down": 0.0, "sent": io.bytes_sent, "recv": io.bytes_recv}
    if _last_net:
        t0, sent0, recv0 = _last_net
        dt = max(0.001, now - t0)
        rate["up"] = max(0.0, (io.bytes_sent - sent0) / dt)
        rate["down"] = max(0.0, (io.bytes_recv - recv0) / dt)
    _last_net = (now, io.bytes_sent, io.bytes_recv)
    return rate


def stats() -> dict:
    mem = psutil.virtual_memory()
    swap = psutil.swap_memory()
    freq = None
    try:
        f = psutil.cpu_freq()
        freq = round(f.current) if f else None
    except Exception:
        pass
    battery = None
    try:
        b = psutil.sensors_battery()
        if b:
            battery = {"percent": round(b.percent), "plugged": b.power_plugged,
                       "secs": b.secsleft if b.secsleft and b.secsleft > 0 else None}
    except Exception:
        pass
    return {
        "time": time.time(),
        "cpu": psutil.cpu_percent(interval=None),
        "cpu_per": psutil.cpu_percent(interval=None, percpu=True),
        "cpu_freq": freq,
        "mem": {"total": mem.total, "used": mem.total - mem.available, "percent": mem.percent},
        "swap": {"total": swap.total, "used": swap.used, "percent": swap.percent},
        "disks": _disks(),
        "net": _net_rate(),
        "battery": battery,
        "uptime": time.time() - psutil.boot_time(),
        "procs": len(psutil.pids()),
    }


def processes() -> list[dict]:
    """进程列表；CPU 占用是相对上一次调用的平均值，已按核心数归一化。"""
    alive = set()
    out = []
    for p in psutil.process_iter(["pid", "name", "username", "memory_info"]):
        pid = p.info["pid"]
        alive.add(pid)
        proc = _procs.setdefault(pid, p)
        try:
            cpu = proc.cpu_percent(interval=None) / _cpu_count
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            cpu = 0.0
        mem = p.info["memory_info"].rss if p.info["memory_info"] else 0
        user = p.info["username"] or ""
        out.append({"pid": pid, "name": p.info["name"] or "?", "cpu": round(cpu, 1),
                    "mem": mem, "user": user.split("\\")[-1]})
    for pid in list(_procs):
        if pid not in alive:
            del _procs[pid]
    return out


def kill(pid: int, force: bool = False) -> None:
    if pid in (0, 4, os.getpid()):
        raise PermissionError("不能结束该进程")
    proc = psutil.Process(pid)
    if force:
        proc.kill()
    else:
        proc.terminate()


def process_names() -> dict[int, str]:
    out = {}
    for p in psutil.process_iter(["pid", "name"]):
        out[p.info["pid"]] = p.info["name"] or ""
    return out


_brief: dict[int, tuple[str, str]] = {}


def proc_brief(pids: list[int]) -> dict[int, tuple[str, str]]:
    """只查询给定进程的名称与程序路径（带缓存），比遍历全部进程快得多。"""
    out = {}
    for pid in pids:
        if pid not in _brief:
            try:
                p = psutil.Process(pid)
                name = p.name()
                try:
                    exe = p.exe()
                except (psutil.Error, OSError):
                    exe = ""
                _brief[pid] = (name, exe)
            except (psutil.Error, OSError):
                _brief[pid] = ("", "")
        out[pid] = _brief[pid]
    for pid in [p for p in _brief if p not in out]:
        del _brief[pid]
    return out
