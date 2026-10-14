#include "ChartWidgets.h"

#include "../src/core/Theme.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

// ============================================================================
// GroupedBarChart —— 分组柱状图
// ============================================================================

GroupedBarChart::GroupedBarChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void GroupedBarChart::setData(const QStringList &categoryLabels,
                              const QVector<Chart::Series> &series,
                              const Chart::Config &config)
{
    m_categories = categoryLabels;
    m_series = series;
    m_config = config;
    update();
}

void GroupedBarChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_categories.isEmpty() || m_series.isEmpty())
        return;

    const Chart::Config &cfg = m_config;

    // 计算 Y 轴范围
    double vmax = cfg.yMax;
    if (vmax <= 0.0) {
        for (const auto &s : m_series)
            for (double v : s.values)
                vmax = qMax(vmax, v);
        vmax = Theme::niceAxisMax(vmax);
    }
    const double vmin = cfg.yMin;

    // 绘图区
    const int ml = cfg.marginLeft, mr = cfg.marginRight;
    const int mt = cfg.marginTop, mb = cfg.marginBottom;
    const QRectF plot(ml, mt, width() - ml - mr, height() - mt - mb);
    if (plot.height() <= 0)
        return;

    // ---- Y 轴网格线与刻度 ----
    if (cfg.gridLines) {
        p.setFont(Theme::axisFont(cfg.axisFontSize));
        for (int i = 0; i <= cfg.yTicks; ++i) {
            const double ratio = double(i) / cfg.yTicks;
            const qreal y = plot.bottom() - plot.height() * ratio;
            const double value = vmin + (vmax - vmin) * ratio;

            p.setPen(QPen(cfg.gridColor, 1.0, Qt::SolidLine));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

            p.setPen(Theme::textSub());
            const QString label = qFuzzyCompare(value, qRound(value))
                                      ? QString::number(qRound(value))
                                      : QString::number(value, 'f', 0);
            p.drawText(QRectF(0, y - 9, plot.left() - 4, 18),
                       Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }

    // ---- 柱子 ----
    const int groupCount = m_categories.size();
    const double groupW = plot.width() / groupCount;

    // 单系列 + 逐柱配色 => 每个类别一根柱子（用于按状态着色的场景）
    const bool singleSeries = (m_series.size() == 1);
    const bool usePerBarColor =
        singleSeries && m_series.first().perBarColors.size() == groupCount;

    const int barCount = singleSeries ? 1 : m_series.size();
    // 组内留 22% 间隙，柱间留 8% 间隙
    const double barW = groupW * 0.78 / barCount;

    for (int g = 0; g < groupCount; ++g) {
        const qreal groupLeft = plot.left() + groupW * g + groupW * 0.11;

        for (int s = 0; s < barCount; ++s) {
            const Chart::Series &series = m_series.at(s);
            if (g >= series.values.size())
                continue;
            const double v = series.values.at(g);
            const QColor color = usePerBarColor
                                     ? series.perBarColors.at(g)
                                     : series.color;

            // 值 -> 像素高度
            const qreal h = plot.height() * (v - vmin) / (vmax - vmin);
            const qreal x = groupLeft + barW * s;
            const QRectF bar(x, plot.bottom() - h, barW * 0.92, h);

            if (h > 0.5) {
                // 柱子垂直渐变：顶部取系列色，底部压暗
                QLinearGradient g2(bar.topLeft(), bar.bottomLeft());
                QColor top = color.lighter(118);
                QColor bottom = color.darker(126);
                g2.setColorAt(0.0, top);
                g2.setColorAt(1.0, bottom);
                p.fillRect(bar, g2);

                // 柱顶高亮线，强化立体感
                p.setPen(QPen(color.lighter(150), 1.0));
                p.drawLine(bar.topLeft(), bar.topRight());
            }

            // 柱顶数值标签
            if (cfg.showValues) {
                p.setFont(Theme::valueFont(cfg.valueFontSize));
                // 用最亮的文字色：柱顶数值需要一眼能读，
                // Theme::text() 偏灰，压暗后几乎看不清
                p.setPen(Theme::textStrong());
                // 标签框只比柱体宽一点点：留白太多会让相邻柱的数值互相压在一起
                // （窄柱密集排列时尤其明显，比如「零件达成率」4 组 × 2 柱）
                p.drawText(QRectF(x - 2, bar.top() - 16, barW + 4, 15),
                           Qt::AlignCenter, QString::number(qRound(v)));
            }
        }

        // X 轴类别标签
        p.setFont(Theme::axisFont(cfg.xLabelFontSize));
        p.setPen(Theme::textSub());
        p.drawText(QRectF(plot.left() + groupW * g, plot.bottom() + 3, groupW, 16),
                   Qt::AlignCenter, m_categories.at(g));
    }

    // ---- X 轴基线 ----
    p.setPen(QPen(cfg.gridColor, 1.2));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
}
