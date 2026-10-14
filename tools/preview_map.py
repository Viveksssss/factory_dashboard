# -*- coding: utf-8 -*-
"""
离线校验脚本：用 Python 渲染 china_map_data.h 中的坐标，
在不依赖 Qt 的情况下验证「数据简化 + 投影 + 绘制」链路是否正确。

用途：修改简化容差后快速回归，避免把有明显缺面的地图编进固件。
"""
import math
import os
import re
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = os.path.join(HERE, "..", "src", "data", "china_map_data.h")
OUT = os.path.join(HERE, "..", "_map_preview.png")

C_BG = (4, 7, 38)
C_LAND = (38, 41, 74)
C_EDGE = (26, 29, 62)
C_ACCENT = (0, 176, 181)
C_TEXT = (0, 200, 205)

CITIES = [("北京", 116.40, 39.90), ("成都", 104.07, 30.57),
          ("武汉", 114.30, 30.59), ("上海", 121.47, 31.23),
          ("广州", 113.26, 23.13), ("厦门", 118.09, 24.48)]


def load():
    src = open(HEADER, encoding="utf-8").read()

    def pairs(name):
        m = re.search(name + r"\[\d+\](?:\[\d+\])? = \{(.*?)\n\};", src, re.S)
        return [(int(a) / 100.0, int(b) / 100.0)
                for a, b in re.findall(r"\{(-?\d+),(-?\d+)\}", m.group(1))]

    def spans(name, count):
        m = re.search(name + r"\[" + str(count) + r"\] = \{(.*?)\n\};", src, re.S)
        return [(int(a), int(b))
                for a, b in re.findall(r"\{(-?\d+),(-?\d+)\}", m.group(1))]

    pool = pairs("kPointPool")
    ring_spans = spans("kRingSpans", re.search(r"kRingSpans\[(\d+)\]", src).group(1))
    border = pairs("kBorderPool")
    border_spans = spans("kBorderSpans", re.search(r"kBorderSpans\[(\d+)\]", src).group(1))
    return pool, ring_spans, border, border_spans


def main():
    pool, ring_spans, border, border_spans = load()
    print("pool=%d rings=%d border=%d/%d" %
          (len(pool), len(ring_spans), len(border), len(border_spans)))

    W, H, PAD = 1280, 940, 24
    lons = [p[0] for p in pool]
    lats = [p[1] for p in pool]
    lo0, lo1, la0, la1 = min(lons), max(lons), min(lats), max(lats)
    mid_lat = (la0 + la1) / 2.0
    kx = math.cos(math.radians(mid_lat))     # 经度按纬度收缩

    scale = min((W - 2 * PAD) / ((lo1 - lo0) * kx), (H - 2 * PAD) / (la1 - la0))

    def proj(lon, lat):
        x = PAD + (lon - lo0) * kx * scale
        y = H - PAD - (lat - la0) * scale
        return x, y

    img = Image.new("RGB", (W, H), C_BG)
    d = ImageDraw.Draw(img)

    for s, c in ring_spans:
        ring = [proj(*pool[i]) for i in range(s, s + c)]
        if len(ring) >= 3:
            d.polygon(ring, fill=C_LAND, outline=C_EDGE)

    for s, c in border_spans:
        d.line([proj(*border[i]) for i in range(s, s + c)], fill=C_ACCENT, width=2)

    for name, lon, lat in CITIES:
        x, y = proj(lon, lat)
        d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=C_ACCENT)
        d.text((x + 9, y - 6), name, fill=C_TEXT)

    img.save(OUT)
    print("saved", os.path.abspath(OUT))


if __name__ == "__main__":
    main()