#include "ChartWidgets.h"

#include "../src/core/Theme.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QtMath>

namespace {

/**
 * @brief 用 Catmull-Rom 样条把折线转成平滑曲线路径
 *
 * 为什么不用 QPainterPath::quadTo / cubicTo 手工连线？
 * 因为控制点一旦手调，数据点变多或变少时曲线形状就会走样。
 * Catmull-Rom 的性质是：曲线必定穿过所有数据点，且邻段切线连续，
 * 完全由数据决定，无需针对每张图调参。
 *
 * 实现：对每段 P1->P2，取控制点
 *   C1 = P1 + (P2 - P0) / 6 * tension
 *   C2 = P2 - (P3 - P1) / 6 * tension
 * 其中 tension=1 为标准 Catmull-Rom。
 * 端点处 P0/P3 用自身或镜像点代替，保证首尾段不产生折角。
 *
 * @param points 数据点序列（已映射为屏幕坐标）
 * @param tension 张力系数，0 退化为直线，1 为标准 Catmull-Rom
 */
QPainterPath buildSmoothPath(const QVector<QPointF> &points, qreal tension = 1.0)
{
    QPainterPath path;
    if (points.isEmpty())
        return path;

    path.moveTo(points.first());
    if (points.size() == 1)
        return path;

    if (points.size() == 2) {
        // 两点无法构成样条，直接画直线
        path.lineTo(points.at(1));
        return path;
    }

    const int n = points.size();
    for (int i = 0; i < n - 1; ++i) {
        const QPointF p0 = (i == 0)     ? points.at(0)
                           : points.at(i - 1);
        const QPointF p1 = points.at(i);
        const QPointF p2 = points.at(i + 1);
        const QPointF p3 = (i + 2 < n) ? points.at(i + 2)
                           : points.at(n - 1);

        const QPointF c1 = p1 + (p2 - p0) * (tension / 6.0);
        const QPointF c2 = p2 - (p3 - p1) * (tension / 6.0);
        path.cubicTo(c1, c2, p2);
    }
    return path;
}

/// 数据点描边空心圆
void drawDataPoint(QPainter *p, const QPointF &pt, const QColor &color, qreal r = 2.6)
{
    p->setBrush(color);
    p->setPen(QPen(QColor(0xFF, 0xFF, 0xFF), 1.1));
    p->drawEllipse(pt, r, r);
}

} // namespace

// ============================================================================
// LineChart —— 折线 / 面积图
// ============================================================================

LineChart::LineChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void LineChart::setData(const QStringList &xLabels,
                        const QVector<Chart::Series> &series,
                        const Chart::Config &config)
{
    m_xLabels = xLabels;
    m_series = series;
    m_config = config;
    update();
}

void LineChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_series.isEmpty())
        return;

    const Chart::Config &cfg = m_config;

    // ---- 计算 Y 轴范围 ----
    double vmax = cfg.yMax;
    double vmin = cfg.yMin;
    if (vmax <= 0.0) {
        for (const auto &s : m_series)
            for (double v : s.values)
                vmax = qMax(vmax, v);
        vmax = Theme::niceAxisMax(vmax);
    }
    if (vmin >= vmax) {
        vmin = 0.0;
    }

    const int ml = cfg.marginLeft, mr = cfg.marginRight;
    const int mt = cfg.marginTop, mb = cfg.marginBottom;
    const QRectF plot(ml, mt, width() - ml - mr, height() - mt - mb);
    if (plot.width() <= 0 || plot.height() <= 0)
        return;

    // ---- 网格线 + Y 轴刻度 + X 轴刻度 ----
    p.setFont(Theme::axisFont(cfg.axisFontSize));
    for (int i = 0; i <= cfg.yTicks; ++i) {
        const double ratio = double(i) / cfg.yTicks;
        const qreal y = plot.bottom() - plot.height() * ratio;
        const double value = vmin + (vmax - vmin) * ratio;

        if (cfg.gridLines) {
            p.setPen(QPen(cfg.gridColor, 1.0, Qt::SolidLine));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }

        p.setPen(Theme::textSub());
        p.drawText(QRectF(0, y - 9, plot.left() - 4, 18),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(qRound(value)));
    }

    // X 轴：网格竖线 + 标签
    const int n = m_xLabels.size();
    if (n > 0) {
        const double slotW = plot.width() / n;
        for (int i = 0; i < n; ++i) {
            const qreal x = plot.left() + slotW * (i + 0.5);
            if (cfg.gridLines) {
                p.setPen(QPen(cfg.gridColor, 0.7, Qt::DotLine));
                p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            }
            p.setFont(Theme::axisFont(cfg.xLabelFontSize));
            p.setPen(Theme::textSub());
            // 数据点密集时按可用宽度抽稀标签，避免糊成一团
            const QFontMetrics fm(p.font());
            const double step = qMax(1.0, std::ceil((fm.horizontalAdvance(QStringLiteral("000")) + 6) / slotW));
            if (std::fmod(i, step) < 0.001) {
                p.drawText(QRectF(x - slotW / 2, plot.bottom() + 3, slotW, 15),
                           Qt::AlignCenter, m_xLabels.at(i));
            }
        }
    }

    // ---- 逐系列绘制 ----
    const double slotW = n > 0 ? plot.width() / n : plot.width();

    for (const Chart::Series &series : m_series) {
        if (series.values.isEmpty())
            continue;

        // 值 -> 屏幕点
        QVector<QPointF> pts;
        pts.reserve(series.values.size());
        for (int i = 0; i < series.values.size(); ++i) {
            const qreal x = (n > 1) ? plot.left() + slotW * (i + 0.5)
                                    : plot.center().x();
            const double ratio = (series.values.at(i) - vmin) / (vmax - vmin);
            const qreal y = plot.bottom() - plot.height() * ratio;
            pts.append(QPointF(x, y));
        }

        // ---- 面积填充：自折线向下的透明度渐变 ----
        if (series.filled) {
            QPainterPath area = buildSmoothPath(pts);
            // 闭合成区域：沿折线正向走，再从末点回到底边，再回起点
            area.lineTo(pts.last().x(), plot.bottom());
            area.lineTo(pts.first().x(), plot.bottom());
            area.closeSubpath();

            QColor c = series.color;
            QLinearGradient grad(plot.topLeft(), plot.bottomLeft());
            c.setAlpha(150);
            grad.setColorAt(0.0, c);
            c.setAlpha(30);
            grad.setColorAt(1.0, c);
            p.setPen(Qt::NoPen);
            p.setBrush(grad);
            p.drawPath(area);
        }

        // ---- 折线本体 ----
        QPainterPath line = buildSmoothPath(pts);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(series.color, 2.0, Qt::SolidLine, Qt::RoundCap,
                      Qt::RoundJoin));
        p.drawPath(line);

        // ---- 数据点 ----
        for (const QPointF &pt : pts)
            drawDataPoint(&p, pt, series.color);
    }

    // ---- X 轴基线 ----
    p.setPen(QPen(cfg.gridColor, 1.2));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
}
