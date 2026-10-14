#include "ChinaMapWidget.h"

#include "../src/core/Theme.h"
#include "../data/china_map_data.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QtGlobal>
#include <QtMath>

namespace {

/// 陆地填充色
QColor landColor()          { return QColor(0x2A, 0x2D, 0x52); }
/// 省界描边色
QColor provinceBorder()     { return QColor(0x1C, 0x1F, 0x40); }
/// 国界线颜色
QColor borderLine()         { return QColor(0x00, 0x8C, 0x92); }
/// 飞线主色
QColor flightColor()        { return QColor(0x00, 0xC8, 0xCE); }
/// 城市光晕色
QColor cityGlow()           { return QColor(0x4F, 0xE8, 0xF0); }

} // namespace

ChinaMapWidget::ChinaMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(200, 150);
    buildMap();
    // 60fps 驱动动画：16ms 一帧足够，120 帧为一个完整循环（2 秒）
    m_timerId = startTimer(16);
}

ChinaMapWidget::~ChinaMapWidget()
{
    if (m_timerId >= 0)
        killTimer(m_timerId);
}

QPointF ChinaMapWidget::project(double lon, double lat) const
{
    // 等经纬度圆柱投影 + 纬度按 cos(latMid) 收缩
    // 用 qDegreesToRadians 而非 M_PI：MSVC 下M_PI 宏默认不定义，
    // 直接写 M_PI 会在 Windows 上编译失败。
    static const double kLatMid = (MapData::kMinLat + MapData::kMaxLat) / 2.0;
    static const double kCos = std::cos(qDegreesToRadians(kLatMid));

    const double x = (lon - MapData::kMinLon) * kCos
                   / ((MapData::kMaxLon - MapData::kMinLon) * kCos);
    const double y = (MapData::kMaxLat - lat) / (MapData::kMaxLat - MapData::kMinLat);
    return QPointF(x, y);
}

void ChinaMapWidget::buildMap()
{
    m_provinces.clear();
    m_borders.clear();

    const int ringCount = MapData::kRingCount;
    m_provinces.reserve(ringCount);
    for (int i = 0; i < ringCount; ++i) {
        const MapData::RingSpan &span = MapData::kRingSpans[i];
        if (span.count < 3)
            continue;
        // 越界保护：坐标表由脚本生成，若生成脚本有bug，
        // 这里宁可少画一个环也不能越界读导致崩溃。
        const int endIdx = int(span.start) + int(span.count);
        if (span.start < 0 || endIdx > int(MapData::kPointPoolRows)) {
            qWarning("ChinaMapWidget: ring %d out of range (%d..%d)",
                     i, int(span.start), endIdx);
            continue;
        }
        QPolygonF poly;
        poly.reserve(span.count);
        for (short k = 0; k < span.count; ++k) {
            const int idx = int(span.start) + k;
            poly.append(project(MapData::kPointPool[idx][0] / 100.0,
                                MapData::kPointPool[idx][1] / 100.0));
        }
        m_provinces.append(poly);
    }

    for (int i = 0; i < MapData::kBorderCount; ++i) {
        const MapData::RingSpan &span = MapData::kBorderSpans[i];
        if (span.count < 2)
            continue;
        const int endIdx = int(span.start) + int(span.count);
        if (span.start < 0 || endIdx > int(MapData::kBorderPoolRows)) {
            qWarning("ChinaMapWidget: border %d out of range (%d..%d)",
                     i, int(span.start), endIdx);
            continue;
        }
        QPolygonF line;
        line.reserve(span.count);
        for (short k = 0; k < span.count; ++k) {
            const int idx = int(span.start) + k;
            line.append(project(MapData::kBorderPool[idx][0] / 100.0,
                                MapData::kBorderPool[idx][1] / 100.0));
        }
        m_borders.append(line);
    }

    // 预置城市坐标（与数据文件中的锚点表一致，此处直接用经纬度）
    static const MapCity kDefaultCities[] = {
        { QStringLiteral("北京"), 116.40, 39.90, true },
        { QStringLiteral("成都"), 104.07, 30.57, false },
        { QStringLiteral("武汉"), 114.30, 30.59, false },
        { QStringLiteral("上海"), 121.47, 31.23, true },
        { QStringLiteral("广州"), 113.26, 23.13, true },
        { QStringLiteral("厦门"), 118.09, 24.48, false },
    };
    for (const MapCity &c : kDefaultCities) {
        m_cities.append(c);
        m_cityPos.insert(c.name, project(c.lon, c.lat));
    }

    // 默认飞线：以北京为枢纽发往各基地
    static const char *kTargets[] = { "成都", "武汉", "上海", "广州", "厦门" };
    for (const char *t : kTargets) {
        FlightRoute r;
        r.from = QStringLiteral("北京");
        r.to = QString::fromUtf8(t);
        r.color = flightColor();
        r.volume = 50;
        m_routes.append(r);
    }
}

void ChinaMapWidget::setCities(const QVector<MapCity> &cities)
{
    m_cities = cities;
    m_cityPos.clear();
    for (const MapCity &c : m_cities)
        m_cityPos.insert(c.name, project(c.lon, c.lat));
    update();
}

void ChinaMapWidget::setRoutes(const QVector<FlightRoute> &routes)
{
    m_routes = routes;
    update();
}

void ChinaMapWidget::setAnimated(bool animated)
{
    if (m_animated == animated)
        return;
    m_animated = animated;
    update();
}

void ChinaMapWidget::setProvinceBordersVisible(bool visible)
{
    if (m_provinceBorders == visible)
        return;
    m_provinceBorders = visible;
    update();
}

void ChinaMapWidget::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    if (!m_animated)
        return;
    ++m_frame;

    // 本控件被渲染到ScaledCanvas 的离屏容器中，
    // 自身的 update() 不会触发屏幕重绘，
    // 必须通过回调让外层把离屏缓存标记为失效。
    if (m_onFrame)
        m_onFrame();

    emit frameRendered(m_frame);
}

void ChinaMapWidget::drawCities(QPainter *p, qreal phase, qreal invScale)
{
    // 注意：此处处于世界变换（已 scale），所有线宽与半径都需乘以 invScale
    // 抵消缩放，才能得到期望的屏幕像素尺寸。
    for (const MapCity &city : m_cities) {
        const QPointF np = m_cityPos.value(city.name, QPointF(-1, -1));
        if (np.x() < 0)
            continue;

        const qreal baseR = (city.major ? 5.0 : 3.6) * invScale;

        // 扩散波纹：每半周期发出一圈，共两层，循环
        for (int layer = 0; layer < 2; ++layer) {
            const qreal t = std::fmod(phase + layer * 0.5, 1.0);
            const qreal rr = baseR * (1.0 + t * 3.4);
            QColor c = cityGlow();
            c.setAlpha(int(120 * (1.0 - t)));
            p->setPen(QPen(c, 1.2 * invScale));
            p->setBrush(Qt::NoBrush);
            p->drawEllipse(np, rr, rr);
        }

        // 实心节点：外层光晕 + 内层高亮核
        QColor halo = cityGlow();
        halo.setAlpha(70);
        p->setPen(Qt::NoPen);
        p->setBrush(halo);
        p->drawEllipse(np, baseR * 1.9, baseR * 1.9);

        p->setPen(QPen(QColor(0xBF, 0xFF, 0xFF), 1.0 * invScale));
        p->setBrush(cityGlow());
        p->drawEllipse(np, baseR, baseR);
    }
}

void ChinaMapWidget::drawRoutes(QPainter *p, qreal phase, qreal invScale)
{
    for (int i = 0; i < m_routes.size(); ++i) {
        const FlightRoute &route = m_routes.at(i);
        const QPointF a = m_cityPos.value(route.from, QPointF(-1, -1));
        const QPointF b = m_cityPos.value(route.to, QPointF(-1, -1));
        if (a.x() < 0 || b.x() < 0)
            continue;

        // 弧顶控制点：两端中点向上抬高 curvature * |距离|
        const QPointF mid((a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0);
        const qreal dist = std::hypot(b.x() - a.x(), b.y() - a.y());
        const QPointF ctrl(mid.x(), mid.y() - dist * route.curvature);

        QPainterPath path;
        path.moveTo(a);
        path.quadTo(ctrl, b);

        // 外发光：一条更粗、更透明的底线，营造霓虹感
        p->setBrush(Qt::NoBrush);
        QColor glow = route.color;
        glow.setAlpha(60);
        p->setPen(QPen(glow, 4.0 * invScale, Qt::SolidLine, Qt::RoundCap));
        p->drawPath(path);

        // 线条本体
        p->setPen(QPen(route.color, 1.6 * invScale, Qt::SolidLine, Qt::RoundCap));
        p->drawPath(path);

        // 沿曲线流动的光点
        if (route.volume > 0 && m_animated) {
            // 每条线错开相位，形成「同时有多批货在途」的观感
            const qreal offset = qreal(i) / m_routes.size();
            for (int dot = 0; dot < 3; ++dot) {
                const qreal t = std::fmod(phase + dot / 3.0 + offset, 1.0);
                const QPointF pt = path.pointAtPercent(t);
                // 首尾淡出，避免光点在端点处突然出现/消失
                QColor c(0xD8, 0xFF, 0xFF);
                c.setAlpha(int(230 * std::sin(qDegreesToRadians(180.0 * t))));
                p->setPen(Qt::NoPen);
                p->setBrush(c);
                p->drawEllipse(pt, 2.4 * invScale, 2.4 * invScale);
            }
        }

        // 终点箭头
        const QPointF tipA = path.pointAtPercent(0.93);
        const QPointF tipB = path.pointAtPercent(0.985);
        QPointF dir = tipB - tipA;
        const qreal len = std::hypot(dir.x(), dir.y());
        if (len > 1e-9) {
            dir /= len;
            const qreal ang = qRadiansToDegrees(std::atan2(dir.y(), dir.x()));
            const qreal size = 5.0 * invScale;
            p->save();
            p->translate(b);
            p->rotate(ang);
            QPolygonF tri;
            tri << QPointF(0, 0)
                << QPointF(-size, size * 0.4)
                << QPointF(-size, -size * 0.4);
            p->setPen(Qt::NoPen);
            p->setBrush(route.color);
            p->drawPolygon(tri);
            p->restore();
        }
    }
}

void ChinaMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    p.fillRect(rect(), Theme::background());

    if (m_provinces.isEmpty())
        return;

    // ---- 地图区域：铺满控件并保持等比 ----
    const qreal margin = 8.0;
    const QRectF target(margin, margin,
                        width() - margin * 2,
                        height() - margin * 2);

    // 世界变换：归一化 (0..1) -> 目标矩形
    // （y 轴翻转已在 project() 中完成，此处不再处理）
    QTransform toScreen;
    toScreen.translate(target.topLeft().x(), target.topLeft().y());
    toScreen.scale(target.width(), target.height());

    // 缩放倒数：世界坐标下的 1 单位等于 invScale 个屏幕像素。
    // 线宽、圆半径乘以它即可抵消缩放，得到期望的物理尺寸。
    const qreal invScale = 1.0 / qMax(target.width(), target.height());

    p.save();
    p.setWorldTransform(toScreen, false);

    // ---- 省界填充 ----
    p.setBrush(landColor());
    p.setPen(m_provinceBorders
                 ? QPen(provinceBorder(), 0.9 * invScale)
                 : Qt::NoPen);
    for (const QPolygonF &poly : m_provinces)
        p.drawPolygon(poly);

    // ---- 国界线 / 十段线 ----
    if (!m_borders.isEmpty()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(borderLine(), 1.1 * invScale, Qt::SolidLine,
                      Qt::RoundCap, Qt::RoundJoin));
        for (const QPolygonF &line : m_borders)
            p.drawPolyline(line);
    }

    // ---- 飞线与城市节点 ----
    const qreal ph = phase();
    drawRoutes(&p, ph, invScale);
    drawCities(&p, ph, invScale);

    p.restore();

    // ---- 城市名称标签（用屏幕坐标绘制，字号不参与缩放）----
    p.setFont(Theme::valueFont(qMax(9, qRound(qMin(width(), height()) * 0.024))));
    for (const MapCity &city : m_cities) {
        const QPointF np = m_cityPos.value(city.name, QPointF(-1, -1));
        if (np.x() < 0)
            continue;
        const QPointF sp = toScreen.map(np);
        p.setPen(cityGlow().lighter(125));
        p.drawText(QRectF(sp.x() + 9, sp.y() - 9, 90, 18),
                   Qt::AlignLeft | Qt::AlignVCenter, city.name);
    }
}
