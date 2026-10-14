# -*- coding: utf-8 -*-
"""
离屏预渲染：用 Python/PIL 按 1920x1080 设计坐标复刻一张大屏效果图。

用途：
  本机无 Qt，无法编译运行。为了验证「布局坐标 + 配色 + 图表算法」是否正确，
  这里用与 C++ 完全相同的坐标表、配色和绘制算法渲染一张静态图，
  从而在交付前发现明显的错位、遮挡或配色偏离。

  注意：这是**验证工具**，不是产品代码。它与 C++ 实现共享同一份坐标常量，
  两边不一致时以 C++ 为准并回头修正本脚本。
"""
import math
import os
import re

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")

# ---------------------------------------------------------------- 主题配色
BG = (0x04, 0x07, 0x26)
PANEL_HEADER = (0x30, 0x30, 0x56)
PANEL_BODY = (0x26, 0x29, 0x4A)
PANEL_BORDER = (0x1A, 0x1D, 0x3E)
TEXT_SUB = (0x9A, 0xA3, 0xC8)
TEXT = (0xD8, 0xDE, 0xF0)
ACCENT_CYAN = (0x00, 0xB0, 0xB5)
ACCENT_BLUE = (0x00, 0x71, 0xC0)
WARN_YELLOW = (0xFF, 0xC0, 0x00)
DANGER_RED = (0xFE, 0x00, 0x00)
OK_GREEN = (0x00, 0xAF, 0x50)
GRID = (0x4A, 0x50, 0x7A)

W, H = 1920, 1080
HERE_FONTS = [
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/msyhbd.ttc",
    "C:/Windows/Fonts/simhei.ttf",
]
FONT_CACHE = {}


def font(size, bold=False):
    key = (size, bold)
    if key in FONT_CACHE:
        return FONT_CACHE[key]
    cands = ["C:/Windows/Fonts/msyhbd.ttc" if bold else "C:/Windows/Fonts/msyh.ttc",
             "C:/Windows/Fonts/simhei.ttf"]
    for c in cands:
        if os.path.exists(c):
            try:
                f = ImageFont.truetype(c, size)
                FONT_CACHE[key] = f
                return f
            except Exception:
                pass
    f = ImageFont.load_default()
    FONT_CACHE[key] = f
    return f


def lighten(c, f=1.15):
    return tuple(min(255, int(v * f)) for v in c)


def darken(c, f=0.82):
    return tuple(int(v * f) for v in c)


class _Ctx:
    """给绘图函数一个访问底层图像的入口，等价于 Qt 里的 QPainter。

    透明转发常用的 ImageDraw 方法，这样绘图函数的代码
    可以直接照着 C++ 里的 QPainter 写法来写。
    """

    def __init__(self, img):
        self._image = img
        self.im = img
        self._d = ImageDraw.Draw(img)

    def __getattr__(self, name):
        return getattr(self._d, name)


def vgrad(d, box, top, bottom):
    x0, y0, x1, y1 = [int(round(v)) for v in box]
    w, h = max(1, x1 - x0), max(1, y1 - y0)
    grad = Image.new("RGB", (1, h))
    gp = grad.load()
    for i in range(h):
        t = i / max(1, h - 1)
        gp[0, i] = tuple(int(top[c] * (1 - t) + bottom[c] * t) for c in range(3))
    d.im.paste(grad.resize((w, h)), (x0, y0))


def text_c(d, xy, s, size, color=TEXT, bold=False, anchor="la"):
    d.text(xy, s, font=font(size, bold), fill=color, anchor=anchor)


def text_center(d, box, s, size, color=TEXT, bold=False):
    x0, y0, x1, y1 = box
    d.text(((x0 + x1) / 2, (y0 + y1) / 2), s, font=font(size, bold),
           fill=color, anchor="mm")


# ---------------------------------------------------------------- 面板绘制
def panel(d, x, y, w, h, title, header_h=41):
    """面板：底色 + 标题栏 + 四角装饰"""
    d.rectangle([x, y, x + w, y + h], fill=PANEL_BODY)
    if title:
        d.rectangle([x, y, x + w, y + header_h], fill=PANEL_HEADER)
        d.line([x, y + header_h, x + w, y + header_h], fill=(0x3A, 0x3E, 0x68))
        text_center(d, (x, y, x + w, y + header_h), title,
                    max(11, int(header_h * 0.52)), TEXT, bold=True)
        # 右上角装饰方块
        sq = max(3, header_h * 0.16)
        for i in range(3):
            s = sq * (1 - i * 0.22)
            cx = x + w - sq * 1.6 * (i + 1) - sq * 0.2
            cy = y + header_h / 2 - s / 2
            d.rectangle([cx, cy, cx + s, cy + s],
                        fill=tuple(int(v * 0.55) for v in ACCENT_CYAN))
    # 四角角标
    c = ACCENT_CYAN
    L = min(14, min(w, h) * 0.18)
    for (px, py, dx, dy) in [(x, y, 1, 1), (x + w, y, -1, 1),
                (x, y + h, 1, -1), (x + w, y + h, -1, -1)]:
        d.line([px, py, px + dx * L, py], fill=c, width=2)
        d.line([px, py, px, py + dy * L], fill=c, width=2)


# ---------------------------------------------------------------- 图表绘制
def nice_axis_max(v):
    if v <= 0:
        return 10.0
    steps = 5
    rough = v * 1.15 / steps
    mag = 10 ** math.floor(math.log10(rough))
    norm = rough / mag
    nice = 1.0 if norm <= 1 else 2.0 if norm <= 2 else 2.5 if norm <= 2.5 else 5.0
    return nice * mag * steps


def grouped_bar(d, box, cats, series, cfg):
    """分组柱状图：series = [(values, color_or_perBarColorList, label)]

    与 C++ 版 GroupedBarChart 行为一致：
      单系列 + 逐柱配色 => 每类一根柱子；否则每类 barCount 根。
    """
    x0, y0, x1, y1 = box
    ml, mr, mt, mb = (cfg.get(k, v) for k, v in
                      (("ml", 34), ("mr", 10), ("mt", 20), ("mb", 22)))
    px0, py0, px1, py1 = x0 + ml, y0 + mt, x1 - mr, y1 - mb
    pw, ph = px1 - px0, py1 - py0
    if ph <= 0:
        return
    vmax = cfg.get("ymax") or nice_axis_max(
        max(max(s[0]) for s in series))
    ticks = cfg.get("yticks", 5)
    # 网格 + Y 刻度
    for i in range(ticks + 1):
        r = i / ticks
        yy = py1 - ph * r
        d.line([px0, yy, px1, yy], fill=GRID, width=1)
        text_c(d, (px0 - 4, yy), str(int(round(vmax * r))), 10, TEXT_SUB,
               anchor="rm")
    gc = len(cats)
    slot = pw / gc
    single = len(series) == 1
    per_bar = (single and isinstance(series[0][1], list)
               and len(series[0][1]) == gc)
    bc = 1 if single else len(series)
    bw = slot * 0.78 / bc
    for gi, cat in enumerate(cats):
        gl = px0 + slot * gi + slot * 0.11
        for si in range(bc):
            vals, colspec, _ = series[si]
            v = vals[gi]
            col = colspec[gi] if per_bar else colspec
            bh = ph * v / vmax
            bx = gl + bw * si
            box_ = [bx, py1 - bh, bx + bw * 0.92, py1]
            vgrad(d, box_, lighten(col, 1.18), darken(col, 0.87))
            d.line([box_[0], box_[1], box_[2], box_[1]], fill=lighten(col, 1.5))
            if cfg.get("show", True):
                text_center(d, (bx - 6, py1 - bh - 17, bx + bw + 6, py1 - bh - 2),
                            str(int(round(v))), cfg.get("vfs", 11), TEXT)
        text_center(d, (px0 + slot * gi, py1 + 3, px0 + slot * (gi + 1), py1 + 19),
                    cat, cfg.get("xfs", 11), TEXT_SUB)
    d.line([px0, py1, px1, py1], fill=GRID, width=2)


def stacked_bar(d, box, cats, rows, cfg):
    """堆叠柱状图：rows = [(values, colors)]"""
    x0, y0, x1, y1 = box
    ml, mr, mt, mb = (cfg.get(k, v) for k, v in
                      (("ml", 30), ("mr", 10), ("mt", 10), ("mb", 20)))
    px0, py0, px1, py1 = x0 + ml, y0 + mt, x1 - mr, y1 - mb
    pw, ph = px1 - px0, py1 - py0
    vmax = cfg.get("ymax") or nice_axis_max(max(sum(r[0]) for r in rows))
    ticks = cfg.get("yticks", 4)
    for i in range(ticks + 1):
        r = i / ticks
        yy = py1 - ph * r
        d.line([px0, yy, px1, yy], fill=GRID, width=1)
        text_c(d, (px0 - 4, yy), str(int(round(vmax * r))), 10, TEXT_SUB,
               anchor="rm")
    slot = pw / len(cats)
    bw = slot * 0.62
    for i, (parts, cols) in enumerate(rows):
        bx = px0 + slot * i + (slot - bw) / 2
        acc = 0.0
        for j, v in enumerate(parts):
            if v <= 0:
                continue
            bh = ph * v / vmax
            yt = py1 - acc - bh
            col = cols[j]
            d.rectangle([bx, yt, bx + bw, yt + bh - 1],
                        fill=lighten(col, 1.1))
            d.line([bx, yt, bx + bw, yt], fill=lighten(col, 1.35), width=1)
            acc += bh
        # 标签抽稀
        fm = font(10)
        tw = d.textlength(str(cats[i]), font=fm)
        if slot >= tw + 2:
            text_center(d, (px0 + slot * i, py1 + 3, px0 + slot * (i + 1), py1 + 18),
                        str(cats[i]), 10, TEXT_SUB)
    d.line([px0, py1, px1, py1], fill=GRID, width=2)


def smooth_points(pts, tension=1.0, samples=16):
    """Catmull-Rom 插值成密集点列"""
    if len(pts) < 3:
        return pts
    out = []
    n = len(pts)
    for i in range(n - 1):
        p0 = pts[max(i - 1, 0)]
        p1 = pts[i]
        p2 = pts[i + 1]
        p3 = pts[min(i + 2, n - 1)]
        for s in range(samples):
            t = s / samples
            t2, t3 = t * t, t * t * t
            x = 0.5 * ((2 * p1[0]) + (-p0[0] + p2[0]) * t +
                       (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2 +
                       (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3)
            y = 0.5 * ((2 * p1[1]) + (-p0[1] + p2[1]) * t +
                       (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2 +
                       (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3)
            out.append((x, y))
    out.append(pts[-1])
    return out


def line_chart(d, box, xlabels, series, cfg):
    """折线/面积图：series = [(values, color, filled)]"""
    x0, y0, x1, y1 = box
    ml, mr, mt, mb = (cfg.get(k, v) for k, v in
                      (("ml", 30), ("mr", 10), ("mt", 14), ("mb", 20)))
    px0, py0, px1, py1 = x0 + ml, y0 + mt, x1 - mr, y1 - mb
    pw, ph = px1 - px0, py1 - py0
    allv = [v for s in series for v in s[0]]
    vmax = cfg.get("ymax") or nice_axis_max(max(allv))
    vmin = cfg.get("ymin", 0)
    ticks = cfg.get("yticks", 5)
    for i in range(ticks + 1):
        r = i / ticks
        yy = py1 - ph * r
        d.line([px0, yy, px1, yy], fill=GRID, width=1)
        text_c(d, (px0 - 4, yy), str(int(round(vmin + (vmax - vmin) * r))),
               cfg.get("afs", 10), TEXT_SUB, anchor="rm")
    n = len(xlabels)
    slot = pw / n if n else pw
    # X 网格与标签（抽稀）
    step = max(1, int(math.ceil(26 / max(slot, 1))))
    for i in range(n):
        xx = px0 + slot * (i + 0.5)
        d.line([xx, py0, xx, py1], fill=GRID, width=1)
        if i % step == 0:
            text_center(d, (xx - slot / 2, py1 + 3, xx + slot / 2, py1 + 18),
                        str(xlabels[i]), cfg.get("xfs", 10), TEXT_SUB)
    for vals, col, filled in series:
        pts = []
        for i, v in enumerate(vals):
            xx = px0 + slot * (i + 0.5)
            yy = py1 - ph * (v - vmin) / (vmax - vmin)
            pts.append((xx, yy))
        if filled:
            # 面积填充（半透明层）
            poly = pts + [(pts[-1][0], py1), (pts[0][0], py1)]
            ov = Image.new("RGBA", (int(pw) + 2, int(ph) + 2), (0, 0, 0, 0))
            od = ImageDraw.Draw(ov)
            sp = [((p[0] - px0 + 1), (p[1] - py0 + 1)) for p in smooth_points(poly)]
            od.polygon(sp, fill=col + (150,))
            ov = ov.filter(__import__("PIL.ImageFilter", fromlist=["GaussianBlur"]).GaussianBlur(0.4))
            d._image.paste(ov, (int(px0) - 1, int(py0) - 1), ov)
        d.line(smooth_points(pts), fill=col, width=2, joint="curve")
        for p in pts:
            d.ellipse([p[0] - 2.6, p[1] - 2.6, p[0] + 2.6, p[1] + 2.6],
                      fill=col, outline=(255, 255, 255), width=1)
    d.line([px0, py1, px1, py1], fill=GRID, width=2)


def hbar_chart(d, box, rows, cfg):
    """横向条形图：rows = [(label, value, color)]，同 label 归为一组"""
    x0, y0, x1, y1 = box
    lw, vw = cfg.get("ml", 46), cfg.get("mr", 26)
    px0, py0, px1, py1 = x0 + lw, y0 + cfg.get("mt", 10), \
        x1 - vw, y1 - cfg.get("mb", 8)
    pw, ph = px1 - px0, py1 - py0
    vmax = max([r[1] for r in rows]) or 1
    groups = []
    for r in rows:
        if not groups or groups[-1][0][0] != r[0]:
            groups.append(([r],))
        else:
            groups[-1][0].append(r)
    ngap = ph / max(1, len(groups)) * 0.28
    gh = (ph - ngap * (len(groups) - 1)) / max(1, len(groups))
    for gi, (g,) in enumerate(groups):
        gt = py0 + gh * gi + ngap * gi
        bh = gh / max(1, len(g))
        for ki, (lab, val, col) in enumerate(g):
            w = pw * val / vmax
            bx0, by0 = px0, gt + bh * ki
            vgrad(d, [bx0, by0, bx0 + w, by0 + bh * 0.82],
                  lighten(col, 1.12), darken(col, 0.88))
            d.line([bx0, by0, bx0 + w, by0], fill=lighten(col, 1.4))
            text_c(d, (bx0 + w + 3, by0 + bh * 0.82 / 2), str(int(round(val))),
                   11, TEXT, anchor="lm")
        text_c(d, (px0 - 6, gt + gh / 2), g[0][0], 12, TEXT_SUB, anchor="rm")
    d.line([px0, py0 - 2, px0, py1 + 2], fill=GRID, width=2)


def pie_chart(d, box, slices, show_pct=True):
    """饼图：slices = [(label, value, color)]，从12 点顺时针"""
    x0, y0, x1, y1 = box
    r = min(x1 - x0, y1 - y0) * 0.42
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2 - r * 0.06
    total = sum(s[1] for s in slices) or 1
    # 用 pieslice 逐段绘制，起始角从 -90（12点）开始
    ang = -90.0
    for lab, val, col in slices:
        sweep = 360.0 * val / total
        d.pieslice([cx - r, cy - r, cx + r, cy + r], ang, ang + sweep,
                   fill=col, outline=PANEL_BODY, width=2)
        if show_pct:
            mid = math.radians(ang + sweep / 2)
            lx = cx + math.sin(mid) * r * 0.62
            ly = cy - math.cos(mid) * r * 0.62
            tc = (0x22, 0x22, 0x22) if sum(col) / 3 > 140 else (255, 255, 255)
            text_center(d, (lx - 24, ly - 9, lx + 24, ly + 9),
                        "%d%%" % round(100 * val / total), 11, tc)
        ang += sweep


def progress_ring(d, box, pct, caption=""):
    x0, y0, x1, y1 = box
    r = min(x1 - x0, y1 - y0) / 2 - 3
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    box2 = [cx - r, cy - r, cx + r, cy + r]
    lw = max(4, r * 0.22)
    d.arc(box2, 0, 360, fill=lighten(PANEL_HEADER, 1.15), width=int(lw))
    col = ACCENT_CYAN if pct >= 60 else (WARN_YELLOW if pct >= 30 else DANGER_RED)
    if pct > 0:
        d.arc(box2, -90, -90 + 360 * pct / 100, fill=col, width=int(lw))
    text_center(d, (cx - r, cy - r * 0.62, cx + r, cy + r * 0.16),
                "%d%%" % round(pct), max(10, int(r * 0.5)), (255, 255, 255),
                bold=True)


def seven_seg(d, box, s, color=(0xE4, 0xF2, 0xFF), off=(0xA6, 0xD4, 0xE6)):
    """七段数码管"""
    x0, y0, x1, y1 = box
    cnt = len(s)
    if cnt == 0:
        return
    gap = (x1 - x0) / cnt * 0.24
    cw = (x1 - x0 - gap * (cnt - 1)) / cnt
    hh = min(y1 - y0, cw * 1.62)
    top = y0 + (y1 - y0 - hh) / 2
    t = max(1.8, cw * 0.155)
    inset = cw * 0.09
    vi = max(t, hh * 0.055)
    SEG = {
        0: 0b0111111, 1: 0b0000110, 2: 0b1011011, 3: 0b1001111, 4: 0b1100110,
        5: 0b1101101, 6: 0b1111101, 7: 0b0000111, 8: 0b1111111, 9: 0b1101111,
    }

    def seg(ax, ay, bx, by, on):
        c = color if on else tuple(int(v * 0.30) for v in off)
        d.line([ax, ay, bx, by], fill=c, width=max(1, int(t)))

    for i, ch in enumerate(s):
        if not ch.isdigit():
            continue
        x = x0 + i * (cw + gap)
        mx, my = x + cw / 2, top + hh / 2
        m = SEG[int(ch)]
        seg(x + inset, top, x + cw - inset, top, m & 1)          # a
        seg(x + cw, top + vi, x + cw, my - vi, m & 2)            # b
        seg(x + cw, my + vi, x + cw, top + hh - vi, m & 4)       # c
        seg(x + inset, top + hh, x + cw - inset, top + hh, m & 8)  # d
        seg(x, my + vi, x, top + hh - vi, m & 16)               # e
        seg(x, top + vi, x, my - vi, m & 32)                    # f
        seg(x + inset, my, x + cw - inset, my, m & 64)          # g


def gauge(d, box, value, vmin, vmax, ticks, unit="", valtext=""):
    """270° 环形刻度仪表盘"""
    x0, y0, x1, y1 = box
    r = min(x1 - x0, y1 - y0) * 0.44
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    start, span = 135.0, 270.0
    lw = max(3, r * 0.11)
    # 外圈渐变环（分段近似）
    segs = 72
    zone = [ACCENT_CYAN, ACCENT_CYAN, ACCENT_BLUE, (0x41, 0xB0, 0xFF),
            (0xFF, 0x2E, 0x63)]

    def zc(t):
        t = min(max(t, 0), 1) * (len(zone) - 1)
        i = int(t)
        if i >= len(zone) - 1:
            return zone[-1]
        f = t - i
        return tuple(int(zone[i][c] * (1 - f) + zone[i + 1][c] * f) for c in range(3))

    for i in range(segs):
        a0 = start + span * i / segs
        d.arc([cx - r - 2, cy - r - 2, cx + r + 2, cy + r + 2],
              a0, a0 + span / segs + 0.6, fill=zc(i / (segs - 1)),
              width=int(lw))
    # 刻度标签
    lr = r + max(4, r * 0.10)
    n = len(ticks)
    for i, t in enumerate(ticks):
        deg = start + span * i / max(1, n - 1)
        rad = math.radians(deg)
        text_center(d, (cx + math.cos(rad) * lr - 26, cy + math.sin(rad) * lr - 8,
                        cx + math.cos(rad) * lr + 26, cy + math.sin(rad) * lr + 8),
                    t, max(7, int(r * 0.15)), TEXT_SUB)
    # 指针
    ratio = min(max((value - vmin) / (vmax - vmin), 0), 1)
    deg = start + span * ratio
    rad = math.radians(deg)
    tip = (cx + math.cos(rad) * (r - lw * 1.2), cy + math.sin(rad) * (r - lw * 1.2))
    tail = (cx - math.cos(rad) * r * 0.16, cy - math.sin(rad) * r * 0.16)
    d.line([tail, tip], fill=(255, 255, 255), width=max(2, int(r * 0.06)))
    d.ellipse([cx - r * 0.055, cy - r * 0.055, cx + r * 0.055, cy + r * 0.055],
              fill=ACCENT_CYAN)
    if unit:
        text_center(d, (cx - r, cy + r * 0.30, cx + r, cy + r * 0.52),
                    unit, max(7, int(r * 0.15)), TEXT_SUB)
    if valtext:
        text_center(d, (cx - r, cy + r * 0.52, cx + r, cy + r * 0.86),
                    valtext, max(10, int(r * 0.26)), (255, 255, 255), bold=True)


def semicircle_dial(d, box, value, vmin, vmax, tick_count, caption=""):
    """180° 半圆刻度盘"""
    x0, y0, x1, y1 = box
    r = min((x1 - x0) * 0.52, (y1 - y0) * 0.86)
    cx, cy = (x0 + x1) / 2, y0 + (y1 - y0) * 0.88
    aw = max(3, r * 0.075)
    segs = 72
    for i in range(segs):
        t = i / (segs - 1)
        f = t
        col = tuple(int(ACCENT_CYAN[c] * (1 - f) + (0x41, 0xB0, 0xFF)[c] * f)
                    for c in range(3))
        d.arc([cx - r, cy - r, cx + r, cy + r], 180 + 180 * t,
              180 + 180 * (t + 1.0 / segs) + 0.6, fill=col, width=int(aw))
    # 细刻度
    minor = 5
    total = (tick_count - 1) * minor
    for i in range(total + 1):
        t = i / total
        rad = math.radians(180 + 180 * t)
        major = (i % minor) == 0
        ro = r + aw * 0.9
        ri = ro - (r * 0.085 if major else r * 0.042)
        d.line([cx - math.cos(rad) * ri, cy - math.sin(rad) * ri,
                cx - math.cos(rad) * ro, cy - math.sin(rad) * ro],
               fill=ACCENT_CYAN if major else ACCENT_BLUE,
               width=2 if major else 1)
    # 主刻度数值
    for i in range(tick_count):
        t = i / (tick_count - 1)
        rad = math.radians(180 + 180 * t)
        rl = r + aw * 0.9 + r * 0.13
        text_center(d, (cx - math.cos(rad) * rl - 20, cy - math.sin(rad) * rl - 9,
                        cx - math.cos(rad) * rl + 20, cy - math.sin(rad) * rl + 9),
                    str(int(round(vmin + (vmax - vmin) * t))),
                    max(7, int(r * 0.13)), ACCENT_CYAN)
    # 当前值圆点
    ratio = min(max((value - vmin) / (vmax - vmin), 0), 1)
    rad = math.radians(180 + 180 * ratio)
    rd = r + aw * 0.9 - r * 0.045
    dx, dy = cx - math.cos(rad) * rd, cy - math.sin(rad) * rd
    d.ellipse([dx - r * 0.085, dy - r * 0.085, dx + r * 0.085, dy + r * 0.085],
              fill=lighten(ACCENT_CYAN, 1.0))
    d.ellipse([dx - r * 0.045, dy - r * 0.045, dx + r * 0.045, dy + r * 0.045],
              fill=ACCENT_CYAN)
    # 中心大字
    text_center(d, (cx - r, cy - r * 0.72, cx + r, cy - r * 0.30),
                "%d%%" % round(value), max(16, int(r * 0.34)), ACCENT_CYAN,
                bold=True)
    if caption:
        text_center(d, (cx - r, cy - r * 0.30, cx + r, cy - r * 0.06),
                    caption, max(8, int(r * 0.16)), lighten(ACCENT_CYAN, 1.3))


def data_table(d, box, headers, widths, rows):
    """数据表格"""
    x0, y0, x1, y1 = box
    d.rectangle([x0, y0, x1, y1], fill=PANEL_BODY)
    hh, rh = 28, 26
    d.rectangle([x0, y0, x1, y0 + hh], fill=PANEL_HEADER)
    x = x0
    for h, w in zip(headers, widths):
        text_center(d, (x, y0, x + w, y0 + hh), h, 12, ACCENT_CYAN, bold=True)
        x += w
    visible = int((y1 - y0 - hh) / rh)
    for r in range(min(visible, len(rows))):
        ry = y0 + hh + rh * r
        if r % 2 == 0:
            d.rectangle([x0, ry, x1, ry + rh], fill=(0x2C, 0x30, 0x54))
        x = x0
        for ci, w in enumerate(widths):
            text_center(d, (x, ry, x + w, ry + rh), str(rows[r][ci]), 12, TEXT)
            x += w
    # 网格线
    x = x0
    for w in widths:
        d.line([x, y0, x, y1], fill=PANEL_BORDER)
        x += w
    d.line([x, y0, x, y1], fill=PANEL_BORDER)
    d.line([x0, y0 + hh, x1, y0 + hh], fill=PANEL_BORDER)
    for r in range(visible + 1):
        yy = y0 + hh + rh * r
        if yy > y1:
            break
        d.line([x0, yy, x1, yy], fill=PANEL_BORDER)


# ---------------------------------------------------------------- 中国地图
def load_map():
    src = open(os.path.join(ROOT, "src", "data", "china_map_data.h"),
               encoding="utf-8").read()

    def pairs(name):
        m = re.search(name + r"\[\d+\](?:\[\d+\])? = \{(.*?)\n\};", src, re.S)
        return [(int(a) / 100.0, int(b) / 100.0)
                for a, b in re.findall(r"\{(-?\d+),(-?\d+)\}", m.group(1))]

    def spans(name):
        m = re.search(name + r"\[\d+\] = \{(.*?)\n\};", src, re.S)
        return [(int(a), int(b))
                for a, b in re.findall(r"\{(-?\d+),(-?\d+)\}", m.group(1))]

    return (pairs("kPointPool"), spans("kRingSpans"),
            pairs("kBorderPool"), spans("kBorderSpans"))


def draw_map(d, box):
    pool, rspans, bpool, bspans = load_map()
    x0, y0, x1, y1 = box
    d.rectangle([x0, y0, x1, y1], fill=BG)
    lons = [p[0] for p in pool]
    lats = [p[1] for p in pool]
    lo0, lo1, la0, la1 = min(lons), max(lons), min(lats), max(lats)
    mid = (la0 + la1) / 2
    kx = math.cos(math.radians(mid))
    pad = 8
    sc = min((x1 - x0 - 2 * pad) / ((lo1 - lo0) * kx), (y1 - y0 - 2 * pad) / (la1 - la0))

    def proj(lon, lat):
        return (x0 + pad + (lon - lo0) * kx * sc, y1 - pad - (lat - la0) * sc)

    for s, c in rspans:
        ring = [proj(*pool[i]) for i in range(s, s + c)]
        if len(ring) >= 3:
            d.polygon(ring, fill=(0x2A, 0x2D, 0x52), outline=(0x1C, 0x1F, 0x40))
    for s, c in bspans:
        d.line([proj(*bpool[i]) for i in range(s, s + c)],
               fill=(0x00, 0x8C, 0x92), width=2)
    cities = [("北京", 116.40, 39.90, True), ("成都", 104.07, 30.57, False),
              ("武汉", 114.30, 30.59, False), ("上海", 121.47, 31.23, True),
              ("广州", 113.26, 23.13, True), ("厦门", 118.09, 24.48, False)]
    pos = {}
    for name, lon, lat, major in cities:
        p = proj(lon, lat)
        pos[name] = p
    glow = (0x4F, 0xE8, 0xF0)
    bj = pos["北京"]
    for name, *_ in cities:
        if name == "北京":
            continue
        tp = pos[name]
        mid_p = ((bj[0] + tp[0]) / 2, (bj[1] + tp[1]) / 2)
        dist = math.hypot(tp[0] - bj[0], tp[1] - bj[1])
        ctrl = (mid_p[0], mid_p[1] - dist * 0.28)
        curve = []
        for i in range(41):
            t = i / 40
            bx = (1 - t) ** 2 * bj[0] + 2 * (1 - t) * t * ctrl[0] + t * t * tp[0]
            by = (1 - t) ** 2 * bj[1] + 2 * (1 - t) * t * ctrl[1] + t * t * tp[1]
            curve.append((bx, by))
        d.line(curve, fill=(0x00, 0xC8, 0xCE), width=2)
        # 流光点
        for k in range(3):
            tt = (0.18 + k / 3.0) % 1.0
            idx = int(tt * 40)
            px, py = curve[idx]
            d.ellipse([px - 2.4, py - 2.4, px + 2.4, py + 2.4], fill=glow)
    for name, lon, lat, major in cities:
        p = pos[name]
        rr = 5 if major else 3.6
        d.ellipse([p[0] - rr * 1.9, p[1] - rr * 1.9, p[0] + rr * 1.9, p[1] + rr * 1.9],
                  outline=glow, width=1)
        d.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr],
                  fill=glow, outline=(0xBF, 0xFF, 0xFF))
        text_c(d, (p[0] + 9, p[1] - 9), name, 12, lighten(glow, 1.1))


# ---------------------------------------------------------------- 主渲染
def main():
    img = Image.new("RGB", (W, H), BG)
    d = _Ctx(img)
    draw = d._d
    d.rectangle([0, 0, W, H], fill=BG)

    LX, LW = 0, 609
    CX, CW = 613, 826
    RX, RW = 1444, 476
    TH, STH = 41, 42

    # ================= 左栏 =================
    panel(d, LX, 0, LW, TH, "当月计划达成率", TH)
    panel(d, LX, 48, 170, 232, "模具达成率", STH)
    panel(d, 174, 48, 212, 232, "零件达成率", STH)
    panel(d, 392, 48, 218, 232, "零件数", STH)

    panel(d, LX, 90, 170, 190, "")
    progress_ring(d, (8, 102, 162, 256), 83)
    text_c(d, (0, 168), "计划: 100", 13, TEXT)
    text_c(d, (0, 194), "达成: 83", 13, TEXT)

    panel(d, 174, 90, 212, 190, "")
    grouped_bar(d, (174, 90, 386, 280),
                ["模仁", "零件", "辅件", "电极"],
                [([592, 562, 511, 304], ACCENT_CYAN, "计划"),
                 ([583, 285, 283, 304], ACCENT_BLUE, "达成")],
                dict(ml=34, mt=20, mb=22))

    panel(d, 392, 90, 218, 190, "")
    gauge(d, (398, 94, 604, 242), 4027, 0, 4416,
          ["0", "1104", "2208", "3312", "4416"], "零件数", "4027")
    seven_seg(d, (412, 242, 590, 276), "4027")

    panel(d, LX, 287, LW, TH, "每日工序达成数", TH)
    panel(d, LX, 328, LW, 204, "")
    import random
    rng = random.Random(7)
    rows = []
    days = list(range(1, 32))
    for day in days:
        f = 0.72 if (day % 7 in (5, 6)) else 1.0
        rows.append(([18 + rng.randint(0, 11), 14 + rng.randint(0, 9),
                      20 + rng.randint(0, 13)] * 1,
                     [ACCENT_CYAN, ACCENT_BLUE, WARN_YELLOW]))
    stacked_bar(d, (LX, 328, LX + LW, 532), days, rows,
                dict(ml=30, mt=10, mb=20, yticks=4))

    panel(d, LX, 531, LW, TH, "模具进度", TH)
    panel(d, LX, 572, LW, STH, "模具进度", STH)
    panel(d, 424, 572, 186, STH, "模具状态统计", STH)
    panel(d, 424, 882, 186, STH, "加工中模具数量", STH)

    panel(d, LX, 614, 420, 466, "")
    types = ["新模", "模仁", "零件", "电极"]
    prods = ["面板", "支架", "夹具", "冲头"]
    trows = []
    for i in range(1, 16):
        trows.append([str(i), "FK%d" % i, "V%d" % i, types[i % 4],
                      "已交付" if i % 6 == 0 else "加工中", prods[i % 4]])
    data_table(d, (0, 614, 420, 1080),
               ["序号", "模具编号", "版本号", "类型", "状态", "产品名称"],
               [54, 76, 64, 64, 72, 90], trows)

    panel(d, 424, 614, 186, 264, "")
    pie_chart(d, (424, 614, 610, 878),
              [("正常交付", 38, OK_GREEN), ("加工中", 31, ACCENT_CYAN),
               ("延期", 31, WARN_YELLOW)])

    panel(d, 424, 924, 186, 156, "")
    gauge(d, (428, 924, 606, 1080), 59, 0, 100,
          ["0", "25", "50", "75", "100"], "模具数", "059")
    seven_seg(d, (454, 1052, 580, 1078), "059")

    # ================= 中栏 =================
    # 顶部标题条
    d.rectangle([CX, 0, CX + CW, 88], fill=(0x0E, 0x12, 0x3A))
    for i in range(5):
        yy = 10 + i * 5
        ins = 8 + i * 7
        d.line([CX + ins, yy, CX + ins + 22, yy], fill=(0x00, 0x6E, 0x80))
        d.line([CX + CW - ins, yy, CX + CW - ins - 22, yy], fill=(0x00, 0x6E, 0x80))
    text_center(d, (CX, 18, CX + CW, 66), "可视化大屏电子看板系统", 31,
                (0x00, 0xDC, 0xFF), bold=True)

    draw_map(d, (CX, 88, CX + CW, 534))

    panel(d, CX, 534, CW, TH, "送检一次合格率", TH)
    panel(d, CX, 575, 413, STH, "零件合格率（15天）", STH)
    panel(d, 1028, 575, 411, STH, "电极合格率（15天）", STH)
    panel(d, CX, 617, CW, 217, "")
    l15 = list(range(1, 16))
    line_chart(d, (CX, 617, CX + 413, 834), l15,
               [([70, 45, 30, 22, 45, 48, 62, 70, 68, 75, 85, 78, 72, 78, 75],
                 ACCENT_CYAN, True)],
               dict(ml=30, mt=14, mb=20, ymax=120))
    line_chart(d, (1028, 617, 1439, 834), l15,
               [([62, 55, 40, 45, 55, 62, 52, 60, 68, 45, 75, 55, 62, 50, 68],
                 ACCENT_BLUE, True)],
               dict(ml=30, mt=14, mb=20, ymax=120))

    panel(d, CX, 834, 305, TH, "当天合格率", TH)
    panel(d, CX, 875, 305, 205, "")
    semicircle_dial(d, (CX, 875, CX + 305, 1080), 36, 0, 100, 11, "合格率")

    panel(d, 924, 834, 515, TH, "模具零件合格率", TH)
    panel(d, 924, 875, 515, 205, "")
    mpr = [77, 55, 70, 43, 69, 86]
    s = (mpr, [WARN_YELLOW if v >= 60 else DANGER_RED for v in mpr], "合格率")
    grouped_bar(d, (924, 875, 1439, 1080),
                ["CNC粗", "CNC精", "EDM", "WEDM", "铣床", "磨床"], [s],
                dict(ml=36, mt=20, mb=22, vfs=12))

    # ================= 右栏 =================
    panel(d, RX, 0, RW, TH, "年度产量汇总", TH)
    panel(d, RX, 41, 215, STH, "模具产量", STH)
    panel(d, 1661, 41, 259, STH, "每月模具产量趋势图", STH)
    panel(d, RX, 83, RW, 183, "")
    hbar_chart(d, (RX, 83, RX + 215, 266),
               [("设变", 45, ACCENT_BLUE), ("设变", 18, ACCENT_CYAN),
                ("修模", 3, ACCENT_BLUE), ("修模", 45, ACCENT_CYAN),
                ("新模", 19, ACCENT_BLUE), ("新模", 28, ACCENT_CYAN)],
               dict(ml=46, mt=10, mb=8))
    months = ["1月", "2月", "3月", "4月", "5月", "6月", "7月", "8月"]
    mm = [[75, 55, 95, 50, 68, 95, 40, 48],
          [20, 50, 63, 40, 22, 42, 33, 28],
          [30, 35, 82, 47, 22, 85, 28, 40]]
    line_chart(d, (1661, 83, 1920, 266), months,
               [(v, c, False) for v, c in
                zip(mm, [WARN_YELLOW, ACCENT_CYAN, ACCENT_BLUE])],
               dict(ml=28, mt=14, mb=20, afs=9, xfs=9))

    panel(d, RX, 266, 215, STH, "零件产量", STH)
    panel(d, 1661, 266, 259, STH, "每月零件产量趋势图", STH)
    panel(d, RX, 308, RW, 182, "")
    hbar_chart(d, (RX, 308, RX + 215, 490),
               [("其他", 7414, ACCENT_BLUE), ("其他", 866, ACCENT_CYAN),
                ("钢件", 6019, ACCENT_BLUE), ("钢件", 8217, ACCENT_CYAN),
                ("电极", 6885, ACCENT_BLUE), ("电极", 8313, ACCENT_CYAN)],
               dict(ml=46, mt=10, mb=8))
    pm = [[28, 20, 68, 47, 28, 68, 20, 40],
          [45, 30, 63, 47, 62, 42, 30, 38],
          [20, 20, 40, 20, 40, 20, 18, 25]]
    line_chart(d, (1661, 308, 1920, 490), months,
               [(v, c, False) for v, c in
                zip(pm, [WARN_YELLOW, ACCENT_CYAN, ACCENT_BLUE])],
               dict(ml=28, mt=14, mb=20, afs=9, xfs=9))

    panel(d, RX, 489, RW, TH, "品质管理", TH)
    panel(d, RX, 530, 181, STH, "品质占比", STH)
    panel(d, 1628, 530, 292, STH, "班组合格率", STH)
    panel(d, RX, 572, RW, 240, "")
    pie_chart(d, (RX, 572, RX + 181, 812),
              [("合格", 27, OK_GREEN), ("让步接收", 23, DANGER_RED),
               ("返工", 30, WARN_YELLOW), ("报废", 21, ACCENT_BLUE)])
    tr = [45, 64, 45, 59, 71, 70]
    ts = (tr, [WARN_YELLOW if v >= 60 else DANGER_RED for v in tr], "合格率")
    grouped_bar(d, (1628, 572, 1920, 812),
                ["CNC", "EDM", "线割", "磨床", "铣床", "外协"], [ts],
                dict(ml=32, mt=20, mb=22, vfs=11, xfs=9, afs=9))

    panel(d, RX, 812, RW, TH, "每日合格率统计", TH)
    panel(d, RX, 853, RW, 227, "")
    dr = [75, 45, 96, 88, 46, 83, 94, 94, 84, 77, 88, 93, 52, 57, 82, 70,
          56, 94, 78, 60, 80, 96, 75, 63, 90, 38, 78, 65, 92, 52, 94]
    line_chart(d, (RX, 853, RX + RW, 1080), list(range(1, 32)),
               [(dr, ACCENT_CYAN, False)],
               dict(ml=34, mt=14, mb=20, yticks=6, xfs=9))

    out = os.path.join(ROOT, "_preview_1920.png")
    img.save(out)
    print("saved", os.path.abspath(out), img.size)


if __name__ == "__main__":
    main()