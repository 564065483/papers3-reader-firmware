"""生成主屏幕图标：python tools/make_icons.py

以 2048px 绘制后缩小，得到平滑的边缘。iOS 会自动把图标裁成圆角，这里输出满版方形。
"""
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parent.parent / "pcconsole" / "web"
S = 2048


def gradient(size, c0, c1, diagonal=True):
    """左上 → 右下的线性渐变。"""
    vertical = Image.linear_gradient("L").resize((size, size))
    base = vertical
    if diagonal:
        horizontal = vertical.transpose(Image.Transpose.ROTATE_90).transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        base = ImageChops.add(vertical, horizontal, scale=2)
    return Image.composite(Image.new("RGB", (size, size), c1), Image.new("RGB", (size, size), c0), base)


def rounded_mask(size, box, radius):
    m = Image.new("L", (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle(box, radius=radius, fill=255)
    return m


def make() -> Image.Image:
    img = gradient(S, (24, 32, 74), (88, 44, 160))
    # 柔和的彩色光斑，营造玻璃背后的氛围
    glow = Image.new("RGB", (S, S), (0, 0, 0))
    g = ImageDraw.Draw(glow)
    g.ellipse([-300, -400, 1300, 1100], fill=(40, 110, 255))
    g.ellipse([1000, 1100, 2400, 2500], fill=(0, 190, 200))
    g.ellipse([1200, -200, 2300, 900], fill=(200, 70, 255))
    glow = glow.filter(ImageFilter.GaussianBlur(260))
    img = ImageChops.screen(img, glow)

    # 玻璃面板
    panel_box = (300, 330, 1748, 1718)
    shadow = Image.new("L", (S, S), 0)
    ImageDraw.Draw(shadow).rounded_rectangle((panel_box[0], panel_box[1] + 60, panel_box[2], panel_box[3] + 60), 300, fill=150)
    img = Image.composite(Image.new("RGB", (S, S), (10, 10, 30)), img, shadow.filter(ImageFilter.GaussianBlur(70)))
    blurred = img.filter(ImageFilter.GaussianBlur(60))
    frosted = Image.blend(blurred, Image.new("RGB", (S, S), (255, 255, 255)), 0.16)
    pmask = rounded_mask(S, panel_box, 300)
    img = Image.composite(frosted, img, pmask)
    # 面板高光描边
    edge = Image.new("L", (S, S), 0)
    ImageDraw.Draw(edge).rounded_rectangle(panel_box, 300, outline=170, width=10)
    top_fade = Image.linear_gradient("L").rotate(180).resize((S, S))
    img = Image.composite(Image.new("RGB", (S, S), (255, 255, 255)), img, ImageChops.multiply(edge, top_fade))

    # 四个“快捷指令”按钮
    colors = [((60, 150, 255), (10, 100, 255)), ((70, 230, 120), (30, 190, 80)),
              ((255, 180, 60), (255, 130, 10)), ((255, 90, 140), (235, 50, 100))]
    pad, gap = 120, 90
    inner = (panel_box[2] - panel_box[0] - 2 * pad - gap) // 2
    for i, (c0, c1) in enumerate(colors):
        x = panel_box[0] + pad + (i % 2) * (inner + gap)
        y = panel_box[1] + pad + (i // 2) * (inner + gap)
        box = (x, y, x + inner, y + inner)
        tile = gradient(S, c0, c1)
        m = rounded_mask(S, box, 120)
        sh = rounded_mask(S, (x, y + 30, x + inner, y + inner + 30), 120).filter(ImageFilter.GaussianBlur(40))
        img = Image.composite(Image.new("RGB", (S, S), (20, 10, 40)), img, sh.point(lambda v: v * 0.45))
        img = Image.composite(tile, img, m)
        # 顶部高光
        hl = Image.new("L", (S, S), 0)
        ImageDraw.Draw(hl).rounded_rectangle((x, y, x + inner, y + inner // 2), 120, fill=55)
        img = Image.composite(Image.new("RGB", (S, S), (255, 255, 255)), img, ImageChops.multiply(hl, m))
        # 白色符号
        d = ImageDraw.Draw(img)
        cx, cy, r = x + inner // 2, y + inner // 2, inner * 0.22
        w = int(inner * 0.075)
        if i == 0:   # 播放
            d.polygon([(cx - r * 0.7, cy - r), (cx - r * 0.7, cy + r), (cx + r, cy)], fill="white")
        elif i == 1:  # 文件夹
            d.rounded_rectangle((cx - r * 1.1, cy - r * 0.6, cx + r * 1.1, cy + r * 0.9), 30, fill="white")
            d.rounded_rectangle((cx - r * 1.1, cy - r * 0.9, cx - r * 0.1, cy - r * 0.3), 24, fill="white")
        elif i == 2:  # 闪电
            d.polygon([(cx + r * 0.2, cy - r * 1.2), (cx - r * 0.8, cy + r * 0.15), (cx - r * 0.05, cy + r * 0.15),
                       (cx - r * 0.25, cy + r * 1.2), (cx + r * 0.8, cy - r * 0.2), (cx + r * 0.05, cy - r * 0.2)], fill="white")
        else:  # 光标
            d.polygon([(cx - r * 0.6, cy - r), (cx - r * 0.6, cy + r * 0.85), (cx - r * 0.1, cy + r * 0.38),
                       (cx + r * 0.25, cy + r * 1.1), (cx + r * 0.55, cy + r * 0.95), (cx + r * 0.2, cy + r * 0.25),
                       (cx + r * 0.8, cy + r * 0.25)], fill="white")
            _ = w
    return img


if __name__ == "__main__":
    big = make()
    for size in (180, 512):
        big.resize((size, size), Image.LANCZOS).save(OUT / f"icon-{size}.png", optimize=True)
        print("saved", size)
