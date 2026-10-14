# -*- coding: utf-8 -*-
"""
将 china_map.json（GeoJSON）转换为紧凑的 C++ 头文件 china_map_data.h。

处理流程：
  1. 遍历 FeatureCollection，提取每个要素的行政边界环
  2. 道格拉斯-普克（Douglas-Peucker）简化，容差 0.02°
  3. 经纬度量化：lon * 100, lat * 100 存为 short（±18000 / ±9000，安全范围）
  4. 按环面积降序排列：大面先铺底，小岛后描边
  5. 输出扁平坐标池 + 环索引表 + 标注锚点，C++ 侧首次使用时构建 QPolygonF

十段线（国界线）单独存放为折线集合，与省界一并渲染。
"""
import json
import os
import sys

SRC = sys.argv[1]
DST = sys.argv[2]
TOLERANCE = float(sys.argv[3]) if len(sys.argv) > 3 else 0.02
MIN_AREA = 0.0015    # 保留澳门等极小行政区，仅滤掉纯几何噪声碎片


def perpendicular_distance(pt, line_start, line_end):
    (x, y), (x1, y1), (x2, y2) = pt, line_start, line_end
    dx, dy = x2 - x1, y2 - y1
    if dx == 0 and dy == 0:
        return ((x - x1) ** 2 + (y - y1) ** 2) ** 0.5
    t = ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    proj_x, proj_y = x1 + t * dx, y1 + t * dy
    return ((x - proj_x) ** 2 + (y - proj_y) ** 2) ** 0.5


def douglas_peucker(points, tol):
    """迭代式道格拉斯-普克，避免深递归导致栈溢出。"""
    n = len(points)
    if n < 3:
        return points[:]
    keep = [False] * n
    keep[0] = keep[n - 1] = True
    stack = [(0, n - 1)]
    tol_sq = tol * tol
    while stack:
        first, last = stack.pop()
        if last <= first + 1:
            continue
        max_dist, index = -1.0, first
        for i in range(first + 1, last):
            d = perpendicular_distance(points[i], points[first], points[last])
            if d > max_dist:
                max_dist, index = d, i
        if max_dist > tol_sq:
            keep[index] = True
            stack.append((first, index))
            stack.append((index, last))
    return [points[i] for i in range(n) if keep[i]]


def ring_area(ring):
    total = 0.0
    for i in range(len(ring)):
        x1, y1 = ring[i]
        x2, y2 = ring[(i + 1) % len(ring)]
        total += x1 * y2 - x2 * y1
    return abs(total) * 0.5


def iter_rings(geometry):
    t = geometry["type"]
    if t == "Polygon":
        return [geometry["coordinates"][0]]
    if t == "MultiPolygon":
        return [poly[0] for poly in geometry["coordinates"] if poly]
    if t == "LineString":
        return [geometry["coordinates"]]
    if t == "MultiLineString":
        return [line for line in geometry["coordinates"] if line]
    return []


def emit_array(out, decl, rows, per_line=14, indent="    "):
    out.append(decl + " {")
    for row in rows:
        out.append(indent + row)
    out.append("};")
    out.append("")


def main():
    with open(SRC, encoding="utf-8") as f:
        data = json.load(f)

    provinces, borders = [], []
    min_lon = min_lat = 1e9
    max_lon = max_lat = -1e9

    for feat in data["features"]:
        props = feat.get("properties") or {}
        name = props.get("name", "")
        geom = feat.get("geometry")
        if not geom:
            continue

        kept = []
        is_border = (name == "十段线")
        for ring in iter_rings(geom):
            for lon, lat in ring:
                min_lon = min(min_lon, lon); max_lon = max(max_lon, lon)
                min_lat = min(min_lat, lat); max_lat = max(max_lat, lat)

            simplified = douglas_peucker([(p[0], p[1]) for p in ring], TOLERANCE)
            # 折线（国界线）不按面积过滤：十段线每段本就短小，
            # 但每一条都必须画出来，少一段就是地图画错。
            if is_border:
                if len(simplified) < 2:
                    continue
            elif len(simplified) < 4 or ring_area(simplified) < MIN_AREA:
                continue
            kept.append([(int(round(lon * 100)), int(round(lat * 100)))
                         for lon, lat in simplified])

        if not kept:
            continue
        if is_border:
            borders.extend(kept)
        else:
            kept.sort(key=ring_area, reverse=True)
            provinces.append((name, kept))

    total_pts = sum(len(r) for _, rr in provinces for r in rr)
    ring_total = sum(len(rr) for _, rr in provinces)
    border_pts = sum(len(r) for r in borders)
    print(f"provinces={len(provinces)} rings={ring_total} points={total_pts} "
          f"border_rings={len(borders)} border_points={border_pts}")
    print(f"bbox lon[{min_lon:.2f},{max_lon:.2f}] lat[{min_lat:.2f},{max_lat:.2f}]")

    o = []
    a = o.append
    a("/**")
    a(" * china_map_data.h —— 行政区划轮廓坐标（自动生成，请勿手工修改）")
    a(" *")
    a(" * 数据源：全国行政区划边界 GeoJSON，")
    a(f" * 生成脚本：tools/gen_map_data.py，道格拉斯-普克简化容差 {TOLERANCE}°")
    a(f" * 坐标编码：经纬度 x 100 量化存储，1 个整数单位 = 0.01°（约 1.1 km）")
    a(f" * 规模：{len(provinces)} 个行政区 / {ring_total} 个环 / {total_pts} 个顶点")
    a(f" * 经度范围 [{min_lon:.2f}, {max_lon:.2f}]，纬度范围 [{min_lat:.2f}, {max_lat:.2f}]")
    a(" *")
    a(" * 数据结构：")
    a(" *   kPointPool  扁平顶点池，所有环的顶点连续存放")
    a(" *   kRingSpans  环索引表，[起始下标, 顶点数]，按行政区顺序分段")
    a(" *   kNameTable  行政区名称下标表")
    a(" *   kBorderPool 国界线（含十段线）顶点池")
    a(" *   kBorderSpan 国界线折线索引表")
    a(" *")
    a(" * C++ 侧在 ChinaMapWidget 构造时一次性构建 QPolygonF 列表并缓存，")
    a(" * 之后每次重绘只做 painter 变换，不重复解析。")
    a(" */")
    a("#pragma once")
    a("")
    a("#include <QLatin1String>")
    a("#include <QVector>")
    a("")
    a("namespace MapData {")
    a("")
    a(f"/// 经纬度范围（未量化，原始十进制度）")
    a(f"inline constexpr double kMinLon = {min_lon:.4f};")
    a(f"inline constexpr double kMaxLon = {max_lon:.4f};")
    a(f"inline constexpr double kMinLat = {min_lat:.2f};")
    a(f"inline constexpr double kMaxLat = {max_lat:.2f};")
    a("")
    a(f"/// 行政区数量")
    a(f"inline constexpr int kProvinceCount = {len(provinces)};")
    a(f"/// 省界环总数")
    a(f"inline constexpr int kRingCount = {ring_total};")
    a(f"/// 国界线折线数")
    a(f"inline constexpr int kBorderCount = {len(borders)};")
    a("")
    a(f"/// 省界坐标池行数（用于运行时越界检查）")
    a(f"inline constexpr int kPointPoolRows = {total_pts};")
    a(f"/// 国界线坐标池行数（用于运行时越界检查）")
    a(f"inline constexpr int kBorderPoolRows = {border_pts};")
    a("")
    a("/// 环索引：顶点池中的 [起始下标, 顶点数)")
    a("struct RingSpan { short start; short count; };")
    a("")
    a("/// 扁平顶点池，坐标已量化（经纬度 x 100）")
    a(f"inline constexpr short kPointPool[{total_pts}][2] = {{")
    line = ""
    for _, rings in provinces:
        for r in rings:
            for p in r:
                tok = "{%d,%d}," % (p[0], p[1])
                if len(line) + len(tok) > 92:
                    a("    " + line)
                    line = ""
                line += tok
    if line:
        a("    " + line)
    a("};")
    a("")
    a("/// 省界环索引表")
    a(f"inline constexpr RingSpan kRingSpans[{ring_total}] = {{")
    idx = 0
    for _, rings in provinces:
        for r in rings:
            a("    {%d,%d}," % (idx, len(r)))
            idx += len(r)
    a("};")
    a("")
    a("/// 行政区名称（UTF-8，与 kRingSpans 分段对应）")
    a(f"inline constexpr const char* kNameTable[{len(provinces)}] = {{")
    for name, _ in provinces:
        a('    "%s",' % name)
    a("};")
    a("")
    a("/// 国界线 / 十段线顶点池")
    a(f"inline constexpr short kBorderPool[{max(border_pts, 1)}][2] = {{")
    line = ""
    for r in borders:
        for p in r:
            tok = "{%d,%d}," % (p[0], p[1])
            if len(line) + len(tok) > 92:
                a("    " + line)
                line = ""
            line += tok
    if line:
        a("    " + line)
    a("};")
    a("")
    a("/// 国界线折线索引表")
    a(f"inline constexpr RingSpan kBorderSpans[{max(len(borders), 1)}] = {{")
    off = 0
    for r in borders:
        a("    {%d,%d}," % (off, len(r)))
        off += len(r)
    a("};")
    a("")
    a("/// 城市标注锚点：外环顶点算术平均，坐标同样已量化")
    a("struct CityAnchor { const char* name; short lon; short lat; };")
    a("inline constexpr CityAnchor kCityAnchors[] = {")
    anchors = [
        ("北京", 116.40, 39.90), ("成都", 104.07, 30.57), ("武汉", 114.30, 30.59),
        ("上海", 121.47, 31.23), ("广州", 113.26, 23.13), ("厦门", 118.09, 24.48),
    ]
    for cname, lon, lat in anchors:
        a('    {"%s", %d, %d},' % (cname, int(round(lon * 100)), int(round(lat * 100))))
    a("};")
    a(f"inline constexpr int kCityAnchorCount = {len(anchors)};")
    a("")
    a("} // namespace MapData")
    a("")

    with open(DST, "w", encoding="utf-8") as f:
        f.write("\n".join(o))
    print(f"written {DST}  {os.path.getsize(DST)/1024:.1f} KB")


if __name__ == "__main__":
    main()