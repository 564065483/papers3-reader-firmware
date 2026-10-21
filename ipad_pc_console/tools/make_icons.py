"""生成主屏幕图标：python tools/make_icons.py"""
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "pcconsole" / "web"


def make(size: int) -> Image.Image:
    s = size / 512
    img = Image.new("RGB", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            t = (x + y) / (2 * size)
            px[x, y] = (int(70 + 40 * t), int(110 - 40 * t), int(255 - 30 * t))
    d = ImageDraw.Draw(img)
    # 显示器
    d.rounded_rectangle([96 * s, 120 * s, 416 * s, 330 * s], radius=26 * s, fill=(255, 255, 255))
    d.rounded_rectangle([120 * s, 144 * s, 392 * s, 306 * s], radius=12 * s, fill=(28, 34, 52))
    d.rectangle([236 * s, 330 * s, 276 * s, 372 * s], fill=(255, 255, 255))
    d.rounded_rectangle([176 * s, 366 * s, 336 * s, 392 * s], radius=12 * s, fill=(255, 255, 255))
    # 屏幕内的四个按钮
    colors = [(91, 140, 255), (34, 197, 94), (249, 115, 22), (236, 72, 153)]
    for i, c in enumerate(colors):
        x0 = 142 * s + i * 62 * s
        d.rounded_rectangle([x0, 196 * s, x0 + 48 * s, 254 * s], radius=10 * s, fill=c)
    return img


for size in (180, 512):
    make(size).save(OUT / f"icon-{size}.png", optimize=True)
    print("saved", size)
