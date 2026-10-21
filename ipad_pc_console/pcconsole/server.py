"""HTTP + WebSocket 服务：为 iPad 网页端提供全部接口。"""
from __future__ import annotations

import asyncio
import html
import io
import ipaddress
import json
import os
import re
import secrets
import socket
import time
import traceback
from pathlib import Path
from urllib.parse import quote

import psutil
from aiohttp import WSMsgType, web

from . import __version__, actions, apps, audio, discovery, files, icons, screen, sysinfo
from .backend import ctl
from .common import CREATE_NO_WINDOW, IS_MAC, IS_WIN, OS_TYPE, NotSupported
from .config import Config

WEB_DIR = Path(__file__).parent / "web"
LOCAL_ADDRS = {"127.0.0.1", "::1"}
PUBLIC_API = {"/api/login", "/api/ping"}

CFG: web.AppKey = web.AppKey("cfg", Config)
DISCOVERY: web.AppKey = web.AppKey("discovery", asyncio.Task)


# ---------------------------------------------------------------------------
# 工具函数
# ---------------------------------------------------------------------------
def lan_addresses() -> list[str]:
    """本机局域网 IPv4 地址，最可能的那个排在最前。"""
    preferred = None
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("10.255.255.255", 1))
            preferred = s.getsockname()[0]
    except OSError:
        pass
    found = []
    for addrs in psutil.net_if_addrs().values():
        for a in addrs:
            if a.family != socket.AF_INET:
                continue
            ip = ipaddress.ip_address(a.address)
            if ip.is_private and not ip.is_loopback and not ip.is_link_local:
                found.append(a.address)
    ordered = ([preferred] if preferred in found else []) + [ip for ip in found if ip != preferred]
    return ordered or ["127.0.0.1"]


def pair_urls(cfg: Config) -> list[str]:
    return [f"http://{ip}:{cfg['port']}/?token={cfg['token']}" for ip in lan_addresses()]


def _err(status: int, message: str) -> web.Response:
    return web.json_response({"error": message}, status=status)


async def _json(request: web.Request) -> dict:
    try:
        data = await request.json()
    except (ValueError, UnicodeDecodeError):
        raise ValueError("请求内容不是有效的 JSON")
    if not isinstance(data, dict):
        raise ValueError("请求内容格式错误")
    return data


def _blocking(func, *args):
    return asyncio.get_running_loop().run_in_executor(None, func, *args)


_stats_cache: dict = {"t": 0.0, "data": None}
_stats_lock = asyncio.Lock()


async def cached_stats() -> dict:
    async with _stats_lock:
        if time.time() - _stats_cache["t"] > 1.5 or _stats_cache["data"] is None:
            _stats_cache["data"] = await _blocking(sysinfo.stats)
            _stats_cache["t"] = time.time()
        return _stats_cache["data"]


# ---------------------------------------------------------------------------
# 中间件
# ---------------------------------------------------------------------------
@web.middleware
async def error_middleware(request: web.Request, handler):
    try:
        return await handler(request)
    except web.HTTPException:
        raise
    except NotSupported as exc:
        return _err(501, str(exc))
    except FileNotFoundError as exc:
        return _err(404, str(exc) or "找不到文件")
    except FileExistsError as exc:
        return _err(409, str(exc) or "文件已存在")
    except PermissionError as exc:
        return _err(403, str(exc) or "没有权限（可尝试以管理员身份运行服务端）")
    except psutil.NoSuchProcess:
        return _err(404, "进程已不存在")
    except psutil.AccessDenied:
        return _err(403, "没有权限结束该进程（可尝试以管理员身份运行服务端）")
    except (ValueError, KeyError, TypeError) as exc:
        return _err(400, str(exc))
    except Exception as exc:  # noqa: BLE001
        traceback.print_exc()
        return _err(500, f"{type(exc).__name__}: {exc}")


_CORS = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Headers": "X-Token, Content-Type",
    "Access-Control-Allow-Methods": "GET, POST, PUT, OPTIONS",
    "Access-Control-Expose-Headers": "X-Mon-Left, X-Mon-Top, X-Mon-Width, X-Mon-Height",
    "Access-Control-Max-Age": "600",
}


@web.middleware
async def cors_middleware(request: web.Request, handler):
    """允许 iPad 上的同一个网页同时控制多台电脑（跨域访问其它电脑的接口）。
    鉴权依靠 X-Token 请求头而非 Cookie，因此开放跨域不会带来 CSRF 风险。"""
    if request.method == "OPTIONS":
        return web.Response(status=204, headers=_CORS)
    try:
        response = await handler(request)
    except web.HTTPException as exc:
        exc.headers.update(_CORS)
        raise
    if request.path.startswith("/api/"):
        response.headers.update(_CORS)
    return response


def _token_of(request: web.Request) -> str:
    return (request.headers.get("X-Token") or request.query.get("token")
            or request.cookies.get("pc_token") or "")


@web.middleware
async def auth_middleware(request: web.Request, handler):
    path = request.path
    protected = (path.startswith("/api/") and path not in PUBLIC_API) or path == "/ws"
    if protected and not secrets.compare_digest(_token_of(request), request.app[CFG]["token"]):
        return _err(401, "未授权，请重新输入 PIN")
    if path.startswith("/pair") and request.remote not in LOCAL_ADDRS:
        return _err(403, "配对页面只能在电脑本机打开")
    return await handler(request)


async def _no_cache(_request: web.Request, response: web.StreamResponse) -> None:
    response.headers.setdefault("Cache-Control", "no-cache")


# ---------------------------------------------------------------------------
# 登录 / 基本信息 / 配置
# ---------------------------------------------------------------------------
_login_fails: dict[str, tuple[int, float]] = {}


async def api_login(request: web.Request) -> web.Response:
    cfg = request.app[CFG]
    ip = request.remote or "?"
    fails, until = _login_fails.get(ip, (0, 0.0))
    if until > time.time():
        return _err(429, f"尝试次数过多，请 {int(until - time.time()) + 1} 秒后再试")
    data = await _json(request)
    if secrets.compare_digest(str(data.get("pin", "")).strip(), str(cfg["pin"])):
        _login_fails.pop(ip, None)
        return web.json_response({"token": cfg["token"]})
    fails += 1
    # 连续输错 5 次锁定 60 秒，防止暴力猜测
    _login_fails[ip] = (0, time.time() + 60) if fails >= 5 else (fails, 0.0)
    return _err(403, "PIN 不正确")


async def api_ping(request: web.Request) -> web.Response:
    cfg = request.app[CFG]
    return web.json_response({"ok": True, "version": __version__, "name": cfg["name"],
                              "os_type": OS_TYPE, "device_id": cfg["device_id"]})


async def api_info(request: web.Request) -> web.Response:
    info = await _blocking(sysinfo.basic_info)
    cfg = request.app[CFG]
    info.update(version=__version__, windows=IS_WIN, os_type=OS_TYPE, controllable=IS_WIN or IS_MAC,
                precise_volume=audio.available(), addresses=lan_addresses(),
                port=cfg["port"], device_id=cfg["device_id"], name=cfg["name"])
    return web.json_response(info)


async def api_pages_get(request: web.Request) -> web.Response:
    return web.json_response({"pages": request.app[CFG]["pages"]})


def _validate_pages(pages) -> list[dict]:
    if not isinstance(pages, list):
        raise ValueError("pages 必须是列表")
    for page in pages:
        if not isinstance(page, dict) or not isinstance(page.get("buttons"), list):
            raise ValueError("页面格式错误")
        page.setdefault("id", secrets.token_hex(5))
        page["name"] = str(page.get("name") or "未命名")
        for btn in page["buttons"]:
            if not isinstance(btn, dict) or not isinstance(btn.get("actions"), list):
                raise ValueError("按钮格式错误")
            btn.setdefault("id", secrets.token_hex(5))
    return pages


async def api_pages_put(request: web.Request) -> web.Response:
    data = await _json(request)
    pages = _validate_pages(data.get("pages"))
    request.app[CFG].set("pages", pages)
    return web.json_response({"pages": pages})


async def api_pages_reset(request: web.Request) -> web.Response:
    return web.json_response({"pages": request.app[CFG].reset_pages()})


async def api_pin(request: web.Request) -> web.Response:
    data = await _json(request)
    pin = str(data.get("pin", "")).strip()
    if not re.fullmatch(r"\d{4,12}", pin):
        raise ValueError("PIN 需为 4–12 位数字")
    request.app[CFG].set("pin", pin)
    return web.json_response({"ok": True})


async def api_name(request: web.Request) -> web.Response:
    data = await _json(request)
    name = str(data.get("name", "")).strip()[:40]
    if not name:
        raise ValueError("名称不能为空")
    request.app[CFG].set("name", name)
    return web.json_response({"name": name})


async def api_peers(_request: web.Request) -> web.Response:
    return web.json_response({"peers": discovery.peers()})


async def api_icon(request: web.Request) -> web.Response:
    path = request.query.get("path", "")
    size = int(request.query.get("size", 96))
    data = await _blocking(icons.icon_png, path, size)
    return web.Response(body=data, content_type="image/png",
                        headers={"Cache-Control": "private, max-age=86400"})


async def api_token_reset(request: web.Request) -> web.Response:
    cfg = request.app[CFG]
    cfg.set("token", secrets.token_hex(24))
    return web.json_response({"token": cfg["token"]})


# ---------------------------------------------------------------------------
# 动作 / 系统 / 媒体
# ---------------------------------------------------------------------------
async def api_run(request: web.Request) -> web.Response:
    data = await _json(request)
    steps = data.get("actions")
    if not isinstance(steps, list) or not steps:
        raise ValueError("没有要执行的动作")
    await actions.run_actions(steps)
    return web.json_response({"ok": True})


async def _audio(func, *args):
    # Windows 的 pycaw 需要在主线程调用；Mac 的 AppleScript 较慢，放到线程池
    return func(*args) if audio.MAIN_THREAD else await _blocking(func, *args)


async def api_volume_get(_request: web.Request) -> web.Response:
    return web.json_response(await _audio(audio.get_state))


async def api_volume_post(request: web.Request) -> web.Response:
    data = await _json(request)
    if "level" in data:
        state = await _audio(audio.set_level, float(data["level"]))
    elif "mute" in data:
        state = await _audio(audio.set_mute, None if data["mute"] is None else bool(data["mute"]))
    elif "step" in data:
        state = await _audio(audio.step, int(data["step"]))
    else:
        raise ValueError("缺少参数")
    return web.json_response(state)


async def api_brightness_get(_request: web.Request) -> web.Response:
    return web.json_response({"level": await _blocking(ctl.get_brightness)})


async def api_brightness_post(request: web.Request) -> web.Response:
    data = await _json(request)
    try:
        await _blocking(ctl.set_brightness, int(data["level"]))
    except RuntimeError:
        raise NotSupported("这台电脑的显示器不支持软件调节亮度（通常仅笔记本内屏支持）")
    return web.json_response({"level": int(data["level"])})


async def api_clipboard_get(_request: web.Request) -> web.Response:
    return web.json_response({"text": await _blocking(ctl.get_clipboard)})


async def api_clipboard_post(request: web.Request) -> web.Response:
    data = await _json(request)
    await _blocking(ctl.set_clipboard, str(data.get("text", "")))
    if data.get("paste"):
        await asyncio.sleep(0.05)
        await _blocking(ctl.hotkey, ctl.PASTE_COMBO)
    return web.json_response({"ok": True})


async def api_command(request: web.Request) -> web.Response:
    data = await _json(request)
    cmd = str(data.get("cmd", "")).strip()
    shell = data.get("shell", "cmd")
    cwd = str(data.get("cwd") or Path.home())
    timeout = max(1, min(600, int(data.get("timeout", 60))))
    if not cmd:
        raise ValueError("命令为空")
    if not os.path.isdir(cwd):
        cwd = str(Path.home())

    # 处理 cd，让网页终端可以切换工作目录
    m = re.fullmatch(r"cd(?:\s+/d)?\s*(.*)", cmd, re.IGNORECASE)
    if m:
        target = m.group(1).strip().strip('"')
        if not target:
            return web.json_response({"code": 0, "stdout": cwd, "stderr": "", "cwd": cwd})
        new = Path(cwd, os.path.expandvars(target)).resolve()
        if not new.is_dir():
            return web.json_response({"code": 1, "stdout": "", "stderr": f"系统找不到指定的路径：{target}",
                                      "cwd": cwd})
        return web.json_response({"code": 0, "stdout": "", "stderr": "", "cwd": str(new)})

    kw = {"stdout": asyncio.subprocess.PIPE, "stderr": asyncio.subprocess.PIPE, "cwd": cwd,
          "stdin": asyncio.subprocess.DEVNULL}
    encoding = "utf-8"
    if IS_WIN:
        kw["creationflags"] = CREATE_NO_WINDOW
        if shell == "powershell":
            proc = await asyncio.create_subprocess_exec(
                "powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;" + cmd, **kw)
        else:
            encoding = "oem"
            proc = await asyncio.create_subprocess_shell(cmd, **kw)
    elif IS_MAC and shell in ("zsh", "bash"):
        proc = await asyncio.create_subprocess_exec(f"/bin/{shell}", "-lc", cmd, **kw)
    else:
        proc = await asyncio.create_subprocess_shell(cmd, **kw)
    try:
        out, err = await asyncio.wait_for(proc.communicate(), timeout)
    except asyncio.TimeoutError:
        proc.kill()
        return web.json_response({"code": -1, "stdout": "", "stderr": f"命令超过 {timeout} 秒未结束，已终止",
                                  "cwd": cwd})
    return web.json_response({"code": proc.returncode, "cwd": cwd,
                              "stdout": out.decode(encoding, "replace")[-200_000:],
                              "stderr": err.decode(encoding, "replace")[-50_000:]})


# ---------------------------------------------------------------------------
# 监控 / 进程 / 窗口 / 应用
# ---------------------------------------------------------------------------
async def api_stats(_request: web.Request) -> web.Response:
    return web.json_response(await cached_stats())


async def api_processes(_request: web.Request) -> web.Response:
    return web.json_response({"processes": await _blocking(sysinfo.processes)})


async def api_process_kill(request: web.Request) -> web.Response:
    data = await _json(request)
    await _blocking(sysinfo.kill, int(data["pid"]), bool(data.get("force")))
    return web.json_response({"ok": True})


async def api_windows_get(_request: web.Request) -> web.Response:
    wins = await _blocking(ctl.list_windows)
    names = await _blocking(sysinfo.process_names)
    for w in wins:
        w.setdefault("process", names.get(w["pid"], ""))
        if not w.get("icon_path"):
            try:
                w["icon_path"] = psutil.Process(w["pid"]).exe()
            except (psutil.Error, OSError):
                w["icon_path"] = ""
    return web.json_response({"windows": wins})


async def api_windows_post(request: web.Request) -> web.Response:
    data = await _json(request)
    await _blocking(ctl.window_op, int(data["hwnd"]), str(data["op"]))
    return web.json_response({"ok": True})


async def api_apps(request: web.Request) -> web.Response:
    refresh = request.query.get("refresh") == "1"
    return web.json_response({"apps": await _blocking(apps.list_apps, refresh)})


# ---------------------------------------------------------------------------
# 文件
# ---------------------------------------------------------------------------
async def api_files(request: web.Request) -> web.Response:
    path = request.query.get("path", "")
    hidden = request.query.get("hidden") == "1"
    return web.json_response(await _blocking(files.list_dir, path, hidden))


async def api_places(_request: web.Request) -> web.Response:
    return web.json_response({"places": files.quick_places(), "drives": await _blocking(files.drives)})


async def api_download(request: web.Request) -> web.StreamResponse:
    path = Path(request.query.get("path", ""))
    if not path.is_file():
        raise FileNotFoundError("文件不存在")
    disposition = "inline" if request.query.get("inline") == "1" else "attachment"
    resp = web.FileResponse(path)
    resp.headers["Content-Disposition"] = f"{disposition}; filename*=UTF-8''{quote(path.name)}"
    return resp


async def api_upload(request: web.Request) -> web.Response:
    folder = Path(request.query.get("dir", "") or Path.home() / "Downloads")
    if not folder.is_dir():
        raise FileNotFoundError("目标文件夹不存在")
    reader = await request.multipart()
    saved = []
    async for part in reader:
        if not part.filename:
            continue
        target = files.unique_path(folder, part.filename)
        with open(target, "wb") as fh:
            while chunk := await part.read_chunk(1 << 20):
                fh.write(chunk)
        saved.append(str(target))
    return web.json_response({"saved": saved})


async def api_file_op(request: web.Request) -> web.Response:
    data = await _json(request)
    op, path = data.get("op"), str(data.get("path", ""))
    result = None
    if op == "open":
        await _blocking(apps.launch, path)
    elif op == "reveal":
        await _blocking(files.reveal, path)
    elif op == "rename":
        result = await _blocking(files.rename, path, str(data.get("name", "")).strip())
    elif op == "mkdir":
        result = await _blocking(files.mkdir, path, str(data.get("name", "")).strip())
    elif op == "delete":
        await _blocking(files.delete_to_recycle_bin, path)
    else:
        raise ValueError(f"未知文件操作：{op}")
    return web.json_response({"ok": True, "path": result})


# ---------------------------------------------------------------------------
# 屏幕
# ---------------------------------------------------------------------------
async def api_monitors(_request: web.Request) -> web.Response:
    return web.json_response({"monitors": await _blocking(screen.list_monitors)})


async def api_frame(request: web.Request) -> web.Response:
    q = request.query
    data, mon = await _blocking(screen.capture, int(q.get("mon", 0)), int(q.get("w", 1600)),
                                int(q.get("q", 70)), q.get("cursor", "1") == "1")
    headers = {"Cache-Control": "no-store", "X-Mon-Left": str(mon["left"]),
               "X-Mon-Top": str(mon["top"]), "X-Mon-Width": str(mon["width"]),
               "X-Mon-Height": str(mon["height"])}
    return web.Response(body=data, content_type="image/jpeg", headers=headers)


# ---------------------------------------------------------------------------
# WebSocket：低延迟鼠标键盘输入 + 状态推送
# ---------------------------------------------------------------------------
def handle_input(msg: dict) -> None:
    t = msg.get("t")
    if t == "m":
        ctl.mouse_move(int(msg.get("x", 0)), int(msg.get("y", 0)))
    elif t == "abs":
        ctl.mouse_set_pos(int(msg["x"]), int(msg["y"]))
    elif t == "b":
        ctl.mouse_button(msg.get("b", "left"), msg.get("a", "click"))
    elif t == "s":
        ctl.mouse_scroll(int(msg.get("y", 0)), int(msg.get("x", 0)))
    elif t == "k":
        ctl.hotkey(str(msg["k"]))
    elif t == "kd":
        ctl.key_down(str(msg["k"]))
    elif t == "ku":
        ctl.key_up(str(msg["k"]))
    elif t == "txt":
        ctl.type_text(str(msg.get("v", "")))
    elif t == "ping":
        pass
    else:
        raise ValueError(f"未知输入消息：{t}")


async def ws_handler(request: web.Request) -> web.WebSocketResponse:
    ws = web.WebSocketResponse(heartbeat=20, max_msg_size=4 * 1024 * 1024)
    await ws.prepare(request)
    want_stats = {"on": True}

    async def push_stats():
        while not ws.closed:
            if want_stats["on"]:
                try:
                    await ws.send_json({"t": "stats", "d": await cached_stats()})
                except (ConnectionError, RuntimeError):
                    break
            await asyncio.sleep(2)

    pusher = asyncio.create_task(push_stats())
    try:
        async for raw in ws:
            if raw.type != WSMsgType.TEXT:
                continue
            try:
                msg = json.loads(raw.data)
                if msg.get("t") == "stats":
                    want_stats["on"] = bool(msg.get("on", True))
                    continue
                if msg.get("t") == "txt" and len(str(msg.get("v", ""))) > 20:
                    await _blocking(handle_input, msg)
                else:
                    handle_input(msg)
            except Exception as exc:  # noqa: BLE001
                await ws.send_json({"t": "err", "msg": str(exc)})
    finally:
        pusher.cancel()
    return ws


# ---------------------------------------------------------------------------
# 页面与配对
# ---------------------------------------------------------------------------
async def index(_request: web.Request) -> web.FileResponse:
    return web.FileResponse(WEB_DIR / "index.html", headers={"Cache-Control": "no-cache"})


async def manifest(_request: web.Request) -> web.FileResponse:
    return web.FileResponse(WEB_DIR / "manifest.webmanifest",
                            headers={"Content-Type": "application/manifest+json"})


async def pair_qr(request: web.Request) -> web.Response:
    import qrcode
    url = pair_urls(request.app[CFG])[int(request.query.get("i", 0))]
    img = qrcode.make(url, box_size=8, border=2)
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return web.Response(body=buf.getvalue(), content_type="image/png")


async def pair_page(request: web.Request) -> web.Response:
    cfg = request.app[CFG]
    urls = pair_urls(cfg)
    firewall_tip = (f"若 iPad 打不开，请在 Windows 防火墙中允许端口 {cfg['port']}（运行“允许防火墙.bat”）。"
                    if IS_WIN else "若 iPad 打不开，请在“系统设置 → 网络 → 防火墙”中允许 Python 接收传入连接。")
    blocks = "".join(
        f'<div class="card"><img src="/pair/qr.png?i={i}" alt="QR"><code>{html.escape(u)}</code></div>'
        for i, u in enumerate(urls))
    page = f"""<!doctype html><html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>iPad 控制台配对</title>
<style>body{{font-family:system-ui,"Microsoft YaHei",sans-serif;background:#0f1218;color:#e8ebf1;
margin:0;padding:32px;text-align:center}}h1{{font-weight:600}}.pin{{font-size:48px;letter-spacing:10px;
font-weight:700;color:#7aa7ff}}.cards{{display:flex;gap:24px;justify-content:center;flex-wrap:wrap}}
.card{{background:#1a1f29;border-radius:16px;padding:20px;max-width:360px}}.card img{{width:280px;
background:#fff;border-radius:8px;padding:8px}}code{{display:block;word-break:break-all;margin-top:12px;
color:#9aa4b5;font-size:13px}}p{{color:#9aa4b5;line-height:1.8}}</style></head><body>
<h1>📱 用 iPad 连接这台电脑</h1>
<p>确保 iPad 与电脑连接同一个 Wi‑Fi / 局域网，用 iPad 相机扫描下方二维码即可直接登录。<br>
也可以在 iPad 的 Safari 中输入地址后用 PIN 码登录：</p>
<div class="pin">{html.escape(str(cfg['pin']))}</div>
<div class="cards">{blocks}</div>
<p>登录后点 Safari 的“分享 → 添加到主屏幕”，即可像 App 一样全屏使用。<br>
{firewall_tip}</p>
</body></html>"""
    return web.Response(text=page, content_type="text/html")


def create_app(cfg: Config) -> web.Application:
    app = web.Application(middlewares=[cors_middleware, error_middleware, auth_middleware],
                          client_max_size=64 * 1024 * 1024)
    app[CFG] = cfg
    app.on_response_prepare.append(_no_cache)

    async def start_discovery(_app):
        _app[DISCOVERY] = asyncio.create_task(discovery.run(cfg["device_id"], cfg["name"], cfg["port"]))

    async def stop_discovery(_app):
        _app[DISCOVERY].cancel()

    app.on_startup.append(start_discovery)
    app.on_cleanup.append(stop_discovery)
    r = app.router
    r.add_get("/", index)
    r.add_get("/manifest.webmanifest", manifest)
    r.add_get("/pair", pair_page)
    r.add_get("/pair/qr.png", pair_qr)
    r.add_get("/ws", ws_handler)
    r.add_post("/api/login", api_login)
    r.add_get("/api/ping", api_ping)
    r.add_get("/api/info", api_info)
    r.add_get("/api/pages", api_pages_get)
    r.add_put("/api/pages", api_pages_put)
    r.add_post("/api/pages/reset", api_pages_reset)
    r.add_post("/api/pin", api_pin)
    r.add_post("/api/token/reset", api_token_reset)
    r.add_post("/api/name", api_name)
    r.add_get("/api/peers", api_peers)
    r.add_get("/api/icon", api_icon)
    r.add_post("/api/run", api_run)
    r.add_get("/api/volume", api_volume_get)
    r.add_post("/api/volume", api_volume_post)
    r.add_get("/api/brightness", api_brightness_get)
    r.add_post("/api/brightness", api_brightness_post)
    r.add_get("/api/clipboard", api_clipboard_get)
    r.add_post("/api/clipboard", api_clipboard_post)
    r.add_post("/api/command", api_command)
    r.add_get("/api/stats", api_stats)
    r.add_get("/api/processes", api_processes)
    r.add_post("/api/processes/kill", api_process_kill)
    r.add_get("/api/windows", api_windows_get)
    r.add_post("/api/windows", api_windows_post)
    r.add_get("/api/apps", api_apps)
    r.add_get("/api/files", api_files)
    r.add_get("/api/files/places", api_places)
    r.add_get("/api/files/download", api_download)
    r.add_post("/api/files/upload", api_upload)
    r.add_post("/api/files/op", api_file_op)
    r.add_get("/api/screen/monitors", api_monitors)
    r.add_get("/api/screen/frame", api_frame)
    r.add_static("/static/", WEB_DIR, append_version=False)
    return app
