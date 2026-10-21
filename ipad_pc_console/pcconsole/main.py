"""命令行入口：启动服务、显示配对信息、管理开机自启。"""
from __future__ import annotations

import argparse
import os
import socket
import sys
import webbrowser
from pathlib import Path

from . import __version__, winctl
from .config import Config, default_config_path

AUTOSTART_NAME = "iPadPCConsole.vbs"


def _startup_file() -> Path:
    return (Path(os.environ.get("APPDATA", "")) / "Microsoft" / "Windows" / "Start Menu"
            / "Programs" / "Startup" / AUTOSTART_NAME)


def _launch_command(extra: str) -> str:
    if getattr(sys, "frozen", False):  # PyInstaller 打包的 exe
        return f'"{sys.executable}" {extra}'
    exe = Path(sys.executable)
    pythonw = exe.with_name("pythonw.exe")
    script = Path(__file__).resolve().parent.parent / "run.py"
    return f'"{pythonw if pythonw.exists() else exe}" "{script}" {extra}'


def install_autostart(config_arg: str | None) -> None:
    if not winctl.IS_WIN:
        sys.exit("开机自启仅支持 Windows")
    extra = "--no-browser" + (f' --config "{config_arg}"' if config_arg else "")
    cmd = _launch_command(extra).replace('"', '""')
    target = _startup_file()
    target.parent.mkdir(parents=True, exist_ok=True)
    # VBS 用 UTF-16 保存才能正确处理中文路径；窗口样式 0 = 隐藏运行
    target.write_text(f'CreateObject("WScript.Shell").Run "{cmd}", 0, False\r\n', encoding="utf-16")
    print(f"已设置开机自启：{target}")


def remove_autostart() -> None:
    target = _startup_file()
    if target.exists():
        target.unlink()
        print("已取消开机自启")
    else:
        print("当前没有设置开机自启")


def _port_in_use(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(0.5)
        return s.connect_ex(("127.0.0.1", port)) == 0


def _redirect_output_if_windowless(cfg_path: Path) -> None:
    # pythonw 运行时没有控制台，print 会失败，改为写日志文件
    if sys.stdout is None or sys.stderr is None:
        log = open(cfg_path.with_name("server.log"), "a", encoding="utf-8", buffering=1)
        sys.stdout = sys.stdout or log
        sys.stderr = sys.stderr or log
    else:
        for stream in (sys.stdout, sys.stderr):
            try:
                stream.reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass


def _print_banner(cfg: Config) -> None:
    from .server import pair_urls
    urls = pair_urls(cfg)
    print("=" * 64)
    print(f"  iPad 电脑控制台 v{__version__} 已启动")
    print("=" * 64)
    print(f"  PIN 码：{cfg['pin']}")
    print("  在 iPad Safari 中打开（与电脑同一局域网）：")
    for url in urls:
        print(f"    http://{url.split('//', 1)[1].split('/', 1)[0]}")
    print(f"  配对二维码：电脑浏览器打开 http://127.0.0.1:{cfg['port']}/pair")
    try:
        import qrcode
        qr = qrcode.QRCode(border=1)
        qr.add_data(urls[0])
        qr.make()
        print("  或用 iPad 相机直接扫描：")
        qr.print_ascii(invert=True)
    except Exception:
        pass
    print("  按 Ctrl+C 停止服务")
    print("=" * 64, flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="把 iPad 变成 Windows 电脑的控制台")
    parser.add_argument("--port", type=int, help="监听端口（默认 8765，会写入配置）")
    parser.add_argument("--host", default="0.0.0.0", help="监听地址，默认所有网卡")
    parser.add_argument("--config", help="配置文件路径")
    parser.add_argument("--no-browser", action="store_true", help="启动时不打开配对页面")
    parser.add_argument("--install-autostart", action="store_true", help="设置开机自动启动")
    parser.add_argument("--remove-autostart", action="store_true", help="取消开机自动启动")
    parser.add_argument("--reset-pin", action="store_true", help="重新生成 PIN 和访问令牌")
    args = parser.parse_args()

    if args.install_autostart:
        install_autostart(args.config)
        return
    if args.remove_autostart:
        remove_autostart()
        return

    cfg_path = Path(args.config) if args.config else default_config_path()
    cfg_path.parent.mkdir(parents=True, exist_ok=True)
    _redirect_output_if_windowless(cfg_path)
    cfg = Config(cfg_path)
    if args.port:
        cfg.set("port", args.port)
    if args.reset_pin:
        import secrets
        cfg.set("pin", f"{secrets.randbelow(1_000_000):06d}")
        cfg.set("token", secrets.token_hex(24))

    pair_url = f"http://127.0.0.1:{cfg['port']}/pair"
    if _port_in_use(cfg["port"]):
        print(f"端口 {cfg['port']} 已被占用，服务可能已经在运行。")
        if not args.no_browser:
            webbrowser.open(pair_url)
        return

    winctl.enable_dpi_awareness()
    from aiohttp import web
    from .server import create_app

    app = create_app(cfg)
    print(f"配置文件：{cfg_path}")
    _print_banner(cfg)
    if not args.no_browser:
        async def _open(_app):
            webbrowser.open(pair_url)
        app.on_startup.append(_open)
    web.run_app(app, host=args.host, port=cfg["port"], print=None, access_log=None)


if __name__ == "__main__":
    main()
