# -*- coding: utf-8 -*-
"""
用「标题栏色」#303056 与「面板底色」#26294A 的行分布，精确定位每个面板的上下边界。

面板结构固定为：[标题栏色带][面板底色区]，
因此某一行若出现标题栏色，说明这里是某个面板的顶部。
"""
import numpy as np
from PIL import Image

HEADER = (48, 48, 86)
BODY = (38, 41, 74)

a = np.asarray(Image.open("a3e0c4a3698816bd6112211aaf6e891b.png").convert("RGB")).astype(int)
H, W, _ = a.shape

hdr = (np.abs(a - np.array(HEADER)).max(axis=2) <= 6)
bod = (np.abs(a - np.array(BODY)).max(axis=2) <= 6)

COLS = {
    "left":   (0, 719),
    "center": (723, 1698),
    "right":  (1703, 2265),
}


def segments(mask1d, minlen=2):
    out, start = [], None
    for i, v in enumerate(mask1d):
        if v and start is None:
            start = i
        elif not v and start is not None:
            out.append((start, i - 1))
            start = None
    if start is not None:
        out.append((start, len(mask1d) - 1))
    return [s for s in out if s[1] - s[0] + 1 >= minlen]


for name, (x0, x1) in COLS.items():
    w = x1 - x0
    print("\n=== %s : x %d..%d (w=%d) ===" % (name, x0, x1, w))
    hh = hdr[:, x0:x1].sum(axis=1) / w * 100
    print("  title-bar rows (>=8%% of column width):")
    for s, e in segments(hh >= 8.0, 3):
        print("    y %4d..%4d  h=%3d  (%.1f%% of H)" % (s, e, e - s + 1, (e - s + 1) / H * 1080 / 1.0))

# 左栏内部子面板（当月计划达成率 的 3 个子块）
print("\n=== left sub-panels (当月计划达成率区, y 0..600) ===")
sub_hdr = hdr[0:600, 0:719].sum(axis=1) / 719 * 100
sub_bod = bod[0:600, 0:719].sum(axis=1) / 719 * 100
for label, arr, thr in (("header", sub_hdr, 8.0), ("body", sub_bod, 30.0)):
    print("  %s rows:" % label)
    for s, e in segments(arr >= thr, 3):
        print("    y %4d..%4d h=%3d" % (s, e, e - s + 1))

# 左栏模具进度区内部（表格 vs 右侧两个小面板）
print("\n=== left sub-panels (模具进度区, y 600..1218) column split ===")
band = notbg = (np.abs(a - np.array((4, 7, 38))).max(axis=2) > 8)
col = band[700:1210, 0:719].sum(axis=0) / 510 * 100
for s, e in segments(col < 40.0, 2):
    print("    vertical seam x %4d..%4d" % (s, e))

# 中栏送检合格率区内部横向分割
print("\n=== center sub-panels (送检合格率区, y 600..1218) ===")
hh2 = hdr[600:1218, 723:1698].sum(axis=1) / 975 * 100
for s, e in segments(hh2 >= 8.0, 3):
    print("    header y %4d..%4d" % (s + 600, e + 600))
col2 = band[620:1200, 723:1698].sum(axis=0) / 580 * 100
for s, e in segments(col2 < 40.0, 2):
    print("    vertical seam x %4d..%4d" % (s + 723, e + 723))

# 右栏年度产量汇总区内部
print("\n=== right sub-panels (年度产量汇总, y 0..600) ===")
hh3 = hdr[0:600, 1703:2265].sum(axis=1) / 562 * 100
for s, e in segments(hh3 >= 8.0, 3):
    print("    header y %4d..%4d" % (s, e))
col3 = band[0:600, 1703:2265].sum(axis=0) / 600 * 100
for s, e in segments(col3 < 40.0, 2):
    print("    vertical seam x %4d..%4d" % (s + 1703, e + 1703))

# 右栏品质管理区内部
print("\n=== right sub-panels (品质管理, y 600..1218) ===")
hh4 = hdr[600:1218, 1703:2265].sum(axis=1) / 562 * 100
for s, e in segments(hh4 >= 8.0, 3):
    print("    header y %4d..%4d" % (s + 600, e + 600))
col4 = band[620:1200, 1703:2265].sum(axis=0) / 580 * 100
for s, e in segments(col4 < 40.0, 2):
    print("    vertical seam x %4d..%4d" % (s + 1703, e + 1703))