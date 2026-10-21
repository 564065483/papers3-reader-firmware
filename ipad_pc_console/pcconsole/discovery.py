"""局域网自动发现：每台运行控制台的电脑定时广播自己，iPad 端据此列出“附近的电脑”。

使用 UDP 端口 8766 广播，消息内容只有电脑名称、系统类型和服务端口，不含 PIN 或令牌。
"""
from __future__ import annotations

import asyncio
import ipaddress
import json
import socket
import time

import psutil

from .common import IS_MAC, OS_TYPE

PORT = 8766
INTERVAL = 5
_peers: dict[str, dict] = {}


def _broadcast_addresses() -> list[str]:
    out = {"255.255.255.255"}
    for addrs in psutil.net_if_addrs().values():
        for a in addrs:
            if a.family != socket.AF_INET or not a.netmask:
                continue
            try:
                net = ipaddress.IPv4Network(f"{a.address}/{a.netmask}", strict=False)
            except ValueError:
                continue
            if net.is_private and not net.is_loopback and not net.is_link_local and net.prefixlen < 31:
                out.add(str(net.broadcast_address))
    return sorted(out)


class _Listener(asyncio.DatagramProtocol):
    def __init__(self, device_id: str):
        self.device_id = device_id

    def datagram_received(self, data: bytes, addr) -> None:
        try:
            msg = json.loads(data.decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            return
        if msg.get("app") != "pcconsole" or msg.get("id") == self.device_id:
            return
        _peers[msg["id"]] = {"id": msg["id"], "name": str(msg.get("name", ""))[:64],
                             "os": msg.get("os", ""), "port": int(msg.get("port", 8765)),
                             "address": addr[0], "seen": time.time()}


def peers() -> list[dict]:
    now = time.time()
    return [p for p in _peers.values() if now - p["seen"] < INTERVAL * 4]


async def run(device_id: str, name: str, port: int) -> None:
    loop = asyncio.get_running_loop()
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if IS_MAC and hasattr(socket, "SO_REUSEPORT"):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        sock.bind(("0.0.0.0", PORT))
        await loop.create_datagram_endpoint(lambda: _Listener(device_id), sock=sock)
    except OSError as exc:
        print(f"[发现] 无法监听 UDP {PORT}（{exc}），将只广播不接收")

    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sender.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sender.setblocking(False)
    payload = json.dumps({"app": "pcconsole", "v": 1, "id": device_id, "name": name,
                          "os": OS_TYPE, "port": port}).encode("utf-8")
    targets: list[str] = []
    tick = 0
    while True:
        if tick % 12 == 0:  # 网络环境可能变化，每分钟刷新一次广播地址
            targets = _broadcast_addresses()
        for ip in targets:
            try:
                sender.sendto(payload, (ip, PORT))
            except OSError:
                pass
        tick += 1
        await asyncio.sleep(INTERVAL)
