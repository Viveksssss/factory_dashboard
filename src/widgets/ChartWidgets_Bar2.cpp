#include "ChartWidgets.h"

#include "../src/core/Theme.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

// ============================================================================
// StackedBarChart —— 堆叠柱状图
// ============================================================================

StackedBarChart::StackedBarChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void StackedBarChart::setData(const QStringList &categoryLabels,
                              const QVector<Chart::StackRow> &rows,
                              const Chart::Config &config)
{
    m_categories = categoryLabels;
    m_rows = rows;
    m_config = config;
    update();
}

void StackedBarChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_rows.isEmpty())
        return;

    const Chart::Config &cfg = m_config;

    // Y 轴上界：取每行各分段之和的最大值
    double vmax = cfg.yMax;
    if (vmax <= 0.0) {
        for (const auto &row : m_rows) {
            double sum = 0.0;
            for (double v : row.parts)
                sum += v;
            vmax = qMax(vmax, sum);
        }
        vmax = Theme::niceAxisMax(vmax);
    }

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
            const double value = vmax * ratio;

            p.setPen(QPen(cfg.gridColor, 1.0, Qt::SolidLine));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

            p.setPen(Theme::textSub());
            p.drawText(QRectF(0, y - 9, plot.left() - 4, 18),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(qRound(value)));
        }
    }

    // ---- 堆叠柱体 ----
    const int n = m_rows.size();
    const double slotW = plot.width() / n;
    // 柱宽占槽位 62%，密集数据下留白更均匀
    const double barW = slotW * 0.62;

    for (int i = 0; i < n; ++i) {
        const Chart::StackRow &row = m_rows.at(i);
        const qreal x = plot.left() + slotW * i + (slotW - barW) / 2.0;

        qreal acc = 0.0;   // 已堆叠的累计高度（像素）
        for (int j = 0; j < row.parts.size(); ++j) {
            const double v = row.parts.at(j);
            if (v <= 0.0)
                continue;

            const qreal h = plot.height() * v / vmax;
            const qreal yTop = plot.bottom() - acc - h;
            const QRectF seg(x, yTop, barW, h - 1.0);  // 段间留 1px 间隙
            const QColor c = j < row.colors.size() ? row.colors.at(j)
                                                   : Theme::accentCyan();

            // 每个分段顶部稍亮，形成分段质感
            QLinearGradient g(seg.topLeft(), seg.bottomLeft());
            g.setColorAt(0.0, c.lighter(115));
            g.setColorAt(1.0, c.darker(115));
            p.fillRect(seg, g);

            p.setPen(QPen(c.lighter(140), 0.8));
            p.drawLine(seg.topLeft(), seg.topRight());

            acc += h;
        }

        // X 轴标签：数据点密集时只显示部分，避免文字重叠
        p.setFont(Theme::axisFont(cfg.xLabelFontSize));
        p.setPen(Theme::textSub());
        // 估算标签宽度，动态决定是否绘制
        const QFontMetrics fm(p.font());
        if (slotW >= fm.horizontalAdvance(m_categories.value(i)) + 2)
            p.drawText(QRectF(plot.left() + slotW * i, plot.bottom() + 3, slotW, 15),
                       Qt::AlignCenter, m_categories.value(i));
    }

    p.setPen(QPen(cfg.gridColor, 1.2));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
}

// ============================================================================
// HorizontalBarChart —— 横向条形图
// ============================================================================

HorizontalBarChart::HorizontalBarChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void HorizontalBarChart::setData(const QVector<Chart::BarRow> &rows,
                                 const Chart::Config &config)
{
    m_rows = rows;
    m_config = config;
    m_maxValue = 0.0;
    for (const auto &r : rows)
        m_maxValue = qMax(m_maxValue, r.value);
    if (m_maxValue <= 0.0)
        m_maxValue = 1.0;
    update();
}

void HorizontalBarChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_rows.isEmpty())
        return;

    const Chart::Config &cfg = m_config;
    const int labelW = cfg.marginLeft;
    const int valueW = cfg.marginRight;

    // 条形区（去掉左侧类别名与右侧数值栏）
    const QRectF plot(labelW, cfg.marginTop,
                      width() - labelW - valueW,
                      height() - cfg.marginTop - cfg.marginBottom);
    if (plot.width() <= 0 || plot.height() <= 0)
        return;

    // 每行可能有多条（如「设变」下的两条不同颜色的条），
    // 因此先按 label 分组，行高 = 该行条数 * 单条高
    QVector<QVector<int>> groups;   // 每组包含的 rows 下标
    for (int i = 0; i < m_rows.size(); ++i) {
        if (groups.isEmpty() || m_rows.at(i).label != m_rows.at(groups.last().first()).label) {
            groups.append(QVector<int>());
        }
        groups.last().append(i);
    }

    // 组间留白
    const double groupGap = plot.height() / groups.size() * 0.28;
    const double groupH = (plot.height() - groupGap * (groups.size() - 1)) / groups.size();
    const double barH = groupH / qMax(1, groups.first().size());

    QFontMetrics fm(Theme::axisFont(cfg.axisFontSize + 2));

    for (int g = 0; g < groups.size(); ++g) {
        const QVector<int> &idxs = groups.at(g);
        const qreal groupTop = plot.top() + groupH * g + groupGap * g;

        for (int k = 0; k < idxs.size(); ++k) {
            const Chart::BarRow &row = m_rows.at(idxs.at(k));
            const qreal w = plot.width() * row.value / m_maxValue;
            const QRectF bar(plot.left(), groupTop + barH * k, w, barH * 0.82);

            QLinearGradient grad(bar.topLeft(), bar.bottomLeft());
            grad.setColorAt(0.0, row.color.lighter(112));
            grad.setColorAt(1.0, row.color.darker(118));
            p.fillRect(bar, grad);

            p.setPen(QPen(row.color.lighter(140), 1.0));
            p.drawLine(bar.topLeft(), bar.topRight());

            // 数值标签（放在条形右侧）
            p.setFont(Theme::valueFont(cfg.valueFontSize));
            p.setPen(Theme::textStrong());
            const QString vt = row.valueText.isEmpty()
                                   ? QString::number(qRound(row.value))
                                   : row.valueText;
            // 标签宽度按字体实测宽度取，并至少留出四位数字的空间。
            // 不能直接沿用 cfg.marginRight —— 它的默认值只有 10，
            // 减去 4px 内边距后只剩 6px，两位数会被硬裁成一个字符
            // （45 显示成 "4"、18 显示成 "1"）。
            const QFontMetrics vfm(Theme::valueFont(cfg.valueFontSize));
            const qreal vw = qMax(qreal(valueW) - 4.0,
                                  qreal(vfm.horizontalAdvance(QStringLiteral("0000"))) + 2.0);
            p.drawText(QRectF(bar.right() + 3, bar.top() - 2, vw, bar.height() + 4),
                       Qt::AlignLeft | Qt::AlignVCenter, vt);
        }

        // 左侧类别名，垂直居中于整组
        const QString label = m_rows.at(idxs.first()).label;
        p.setFont(Theme::axisFont(cfg.axisFontSize + 2));
        p.setPen(Theme::textSub());
        p.drawText(QRectF(0, groupTop, labelW - 6, groupH),
                   Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // 左侧轴线
    p.setPen(QPen(cfg.gridColor, 1.2));
    p.drawLine(QPointF(plot.left(), plot.top() - 2),
               QPointF(plot.left(), plot.bottom() + 2));
}
