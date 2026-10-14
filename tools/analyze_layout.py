# -*- coding: utf-8 -*-
"""
分析参考图的面板网格布局，输出各栏/各区块的相对比例，
供 DashboardWidget 中的坐标定义使用（换算到 1920x1080 设计分辨率）。

思路：面板区域 = 非背景像素。栏与栏之间存在一条「几乎全是背景」的竖缝，
        据此定位三栏的精确边界；行方向同理。
"""
import numpy as np
from PIL import Image

BG = (4, 7, 38)
img = Image.open("a3e0c4a3698816bd6112211aaf6e891b.png").convert("RGB")
a = np.asarray(img).astype(int)
H, W, _ = a.shape
print("image %dx%d  ratio=%.3f" % (W, H, W / H))

# 非背景掩膜：面板区域
notbg = (np.abs(a - np.array(BG)).max(axis=2) > 8)
col_cov = notbg.sum(axis=0).astype(float) / H * 100   # 该列被面板占据的百分比
row_cov = notbg.sum(axis=1).astype(float) / W * 100

# 真正的竖缝：该列被面板占据的比例很低
seam_thr = 20.0
is_seam = col_cov < seam_thr

# 压缩连续区间
def segments(mask1d):
    out, start = [], None
    for i, v in enumerate(mask1d):
        if v and start is None:
            start = i
        elif not v and start is not None:
            out.append((start, i - 1))
            start = None
    if start is not None:
        out.append((start, len(mask1d) - 1))
    return out

seams = segments(is_seam)
# 过滤掉边缘的窄条
seams = [s for s in seams if s[1] - s[0] >= 4]
print("\n--- vertical seams (background separators) ---")
for s, e in seams:
    print("  x %4d..%4d  w=%3d" % (s, e, e - s + 1))

bounds = [0] + [e + 1 for _, e in seams] + [W]
print("\n--- columns ---")
cols = []
for i in range(len(bounds) - 1):
    w = bounds[i + 1] - bounds[i]
    if w < 30:
        continue
    cols.append((bounds[i], w))
    print("  col%d x=%4d..%4d w=%4d  (%.2f%%)"
          % (len(cols) - 1, bounds[i], bounds[i + 1] - 1, w, w / W * 100))

# 行分隔：只在三个栏的主体范围内统计（排除地图造成的低覆盖）
print("\n--- row seams (computed on panel areas only) ---")
# 用三个栏的 x 区间分别统计行覆盖，再合并
x_ranges = [(c[0], c[0] + c[1]) for c in cols]
combined = np.zeros(H)
for x0, x1 in x_ranges:
    sub = notbg[:, x0:x1]
    combined += sub.sum(axis=1) / max(1, (x1 - x0))
row_comb = combined / max(1, len(x_ranges)) * 100
is_seam_row = row_comb < 25.0
row_seams = segments(is_seam_row)
row_seams = [r for r in row_seams if r[1] - r[0] >= 3]
for s, e in row_seams:
    print("  y %4d..%4d  h=%3d  (cov %.1f%%)" % (s, e, e - s + 1, row_comb[s:e+1].max()))

rbounds = [0] + [e + 1 for _, e in row_seams] + [H]
print("\n--- rows ---")
rows = []
for i in range(len(rbounds) - 1):
    h = rbounds[i + 1] - rbounds[i]
    if h < 25:
        continue
    rows.append((rbounds[i], h))
    print("  row%d y=%4d..%4d h=%4d  (%.2f%%)"
          % (len(rows) - 1, rbounds[i], rbounds[i + 1] - 1, h, h / H * 100))

DW, DH = 1920, 1080
print("\n=== scaled to %dx%d ===" % (DW, DH))
for i, (x, w) in enumerate(cols):
    print("  col%d  x=%7.1f  w=%7.1f" % (i, x / W * DW, w / W * DW))
for i, (y, h) in enumerate(rows):
    print("  row%d  y=%7.1f  h=%7.1f" % (i, y / H * DH, h / H * DH))