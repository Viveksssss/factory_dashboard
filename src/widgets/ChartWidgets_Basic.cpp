#include "ChartWidgets.h"

#include "../src/core/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QtMath>

namespace {

/// 屏幕坐标系下的极坐标取点：0° = 12 点方向，顺时针为正
inline QPointF polarPoint(const QPointF &c, qreal r, qreal deg)
{
    const qreal rad = qDegreesToRadians(deg);
    return QPointF(c.x() + std::sin(rad) * r, c.y() - std::cos(rad) * r);
}

} // namespace

// ============================================================================
// PieChart —— 饼图
// ============================================================================

PieChart::PieChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void PieChart::setData(const QVector<Chart::PieSlice> &slices, bool showPercent)
{
    m_slices = slices;
    m_showPercent = showPercent;
    update();
}

void PieChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_slices.isEmpty())
        return;

    double total = 0.0;
    for (const Chart::PieSlice &s : m_slices)
        total += s.value;
    if (total <= 0.0)
        return;

    // 半径取短边的 0.37：留出四周空间放百分比标签
    const qreal radius = qMin(width(), height()) * 0.37;
    if (radius <= 4.0)
        return;
    // 圆心略高于几何中心，给下方留出标签位置
    const QPointF center(width() / 2.0, height() * 0.42);
    const QRectF box(center.x() - radius, center.y() - radius, radius * 2, radius * 2);

    // 从 12 点方向开始、顺时针依次铺扇区。
    //
    // 角度转换说明（这里是最容易出错的地方）：
    //   自定义约定 deg'：0° = 12 点方向，顺时针为正；
    //   QPainterPath::arcTo 约定 a：0° = 3 点方向，逆时针为正。
    //   两者关系为 a = 90 - deg'，且 deg' 增大（顺时针）对应 a 减小，
    //   所以扫描角取负 —— 写成 -spanDeg。
    //   之前的实现把「起点 90°」和「+90° 换算」叠加了两次，
    //   导致整个饼图被额外旋转 90°、扇区之间互相重叠并留下缺口。
    qreal startDeg = 0.0;

    for (const Chart::PieSlice &slice : m_slices) {
        const qreal spanDeg = slice.value / total * 360.0;

        if (spanDeg >= 359.9) {
            // 只有一个扇区（或某扇区占满整圆）：arcTo 起止点重合会退化成空路径，
            // 直接画整圆更稳妥。
            p.setPen(QPen(Theme::panelBody(), 1.2));
            p.setBrush(slice.color);
            p.drawEllipse(box);
            startDeg += spanDeg;
            continue;
        }

        QPainterPath path;
        path.moveTo(center);                                   // 先落到圆心
        path.arcTo(box, 90.0 - startDeg, -spanDeg);             // 半径线 + 外弧
        path.closeSubpath();                                    // 回到圆心，闭合成扇区

        p.setPen(QPen(Theme::panelBody(), 1.2));   // 扇区间用底色描边做分隔
        p.setBrush(slice.color);
        p.drawPath(path);

        // 标签：扇区中线上，第一行名称（可选）、第二行百分比（与设计稿一致）
        if (m_showPercent) {
            const QPointF pos = polarPoint(center, radius * 0.62, startDeg + spanDeg / 2.0);
            QString label;
            if (m_showNames)
                label = slice.label + QLatin1Char('\n');
            label += QString::number(qRound(slice.value / total * 100.0)) + QLatin1Char('%');
            p.setFont(Theme::valueFont(qMax(9, qRound(radius * 0.17))));

            // 依据扇区底色亮度自动选择黑/白字，保证对比度
            const QColor textColor = slice.color.lightness() > 150
                                         ? QColor(0x20, 0x24, 0x30)
                                         : QColor(0xFF, 0xFF, 0xFF);
            p.setPen(textColor);
            p.drawText(QRectF(pos.x() - radius * 0.62, pos.y() - radius * 0.34,
                              radius * 1.24, radius * 0.68),
                       Qt::AlignCenter, label);
        }

        startDeg += spanDeg;
    }
}

// ============================================================================
// ProgressRing —— 进度圆环
// ============================================================================

ProgressRing::ProgressRing(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void ProgressRing::setValue(double percent)
{
    percent = qBound(0.0, percent, 100.0);
    if (qFuzzyCompare(m_percent, percent))
        return;
    m_percent = percent;
    update();
}

void ProgressRing::setCaption(const QString &caption)
{
    if (m_caption == caption)
        return;
    m_caption = caption;
    update();
}

void ProgressRing::setThreshold(double warnBelow, double dangerBelow)
{
    m_warnBelow = warnBelow;
    m_dangerBelow = dangerBelow;
    update();
}

void ProgressRing::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal radius = qMin(width(), height()) / 2.0 - 3.0;
    if (radius <= 2.0)
        return;
    const QPointF center(width() / 2.0, height() / 2.0);
    const QRectF box(center.x() - radius, center.y() - radius, radius * 2, radius * 2);
    const qreal lineW = qMax(4.0, radius * 0.22);

    // 底环（轨道）
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::panelHeader().lighter(115), lineW, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(box, 0, 360 * 16);

    // 前景环：按百分比决定颜色
    QColor fg = Theme::accentCyan();
    if (m_percent < m_dangerBelow)
        fg = Theme::dangerRed();
    else if (m_percent < m_warnBelow)
        fg = Theme::warnYellow();

    if (m_percent > 0.0) {
        // 起始角 -90（12 点），顺时针
        p.setPen(QPen(fg, lineW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(box, 90 * 16, int(-m_percent / 100.0 * 360.0 * 16));
    }

    // 中心百分比数字
    const QFont f = Theme::valueFont(qMax(10, qRound(radius * 0.5)));
    p.setFont(f);
    p.setPen(Theme::textStrong());
    const QRectF textRect(center.x() - radius, center.y() - radius * 0.62,
                          radius * 2, radius * 0.78);
    p.drawText(textRect, Qt::AlignCenter,
               QString::number(qRound(m_percent)) + "%");

    // 中心下方说明文字
    if (!m_caption.isEmpty()) {
        p.setFont(Theme::axisFont(qMax(7, qRound(radius * 0.24))));
        p.setPen(Theme::textSub());
        p.drawText(QRectF(center.x() - radius, center.y() + radius * 0.16,
                          radius * 2, radius * 0.4),
                   Qt::AlignCenter, m_caption);
    }
}

// ============================================================================
// SevenSegDisplay —— 七段数码管
// ============================================================================

SevenSegDisplay::SevenSegDisplay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void SevenSegDisplay::setNumber(const QString &text)
{
    QString filtered;
    for (const QChar &c : text) {
        if (c.isDigit())
            filtered.append(c);
    }
    if (m_text == filtered)
        return;
    m_text = filtered;
    update();
}

void SevenSegDisplay::setDecimalPoint(bool on)
{
    if (m_decimal == on)
        return;
    m_decimal = on;
    update();
}

void SevenSegDisplay::drawSegment(QPainter *p, const QPointF &a, const QPointF &b,
                                  qreal width, bool on, const QColor &color)
{
    // 七段数码管的每段是六边形（两端带斜切角），而不是简单的粗线段。
    // 斜切角是数码管造型的关键：没有它就会显得像「加粗的线」，
    // 而不像真实显示器件。这里用 6 个顶点拼出六边形。
    const qreal t = width;
    QPointF dir = b - a;
    const qreal len = std::hypot(dir.x(), dir.y());
    if (len < 1e-6)
        return;
    dir /= len;
    // 法向量
    const QPointF n(-dir.y(), dir.x());
    // 斜切长度：约为厚度的 40%，这样拐角处能自然对接
    const qreal cut = qMin(t * 0.40, len * 0.30);

    QPolygonF hex;
    hex << a + n * (t / 2.0)
        << a + dir * cut + n * (t / 2.0)
        << b - dir * cut + n * (t / 2.0)
        << b + n * (t / 2.0)
        << b - n * (t / 2.0)
        << b - dir * cut - n * (t / 2.0)
        << a + dir * cut - n * (t / 2.0)
        << a - n * (t / 2.0);

    QColor c = color;
    if (!on) {
        // 未点亮的段近似不显示（alpha 16）。
        // 设计稿里熄灭段基本看不见；alpha 一高，7 段全部显形，
        // 每个数字就都糊成一个「日」字，完全读不出来。
        c.setAlpha(16);
    }
    p->setPen(Qt::NoPen);
    p->setBrush(c);
    p->drawPolygon(hex);
}

void SevenSegDisplay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_text.isEmpty())
        return;

    // ---- 尺寸推算 ----
    // 真实数码管是「高约为宽的 1.6 倍」的竖长造型，
    // 所以不能直接把 width()/n 当字宽——那样得到的是宽扁的字。
    // 这里先按理想宽高比算，宽度不够时再整体等比收缩。
    const int count = m_text.size();
    const qreal aspect = 0.62;      // 单管 宽 / 高
    const qreal gapRatio = 0.30;    // 管间距 / 管宽

    qreal h = height();
    qreal cellW = h * aspect;
    qreal gap = cellW * gapRatio;
    const qreal need = count * cellW + (count - 1) * gap;
    if (need > width()) {
        const qreal k = width() / need;
        h *= k;
        cellW *= k;
        gap *= k;
    }
    const qreal groupW = count * cellW + (count - 1) * gap;
    const qreal left = (width() - groupW) / 2.0;
    const qreal top = (height() - h) / 2.0;
    const qreal t = qMax(1.8, cellW * 0.135);      // 线段厚度
    const qreal inset = cellW * 0.10;              // 横向端点内缩

    // 段位布局：
    //        aaaa
    //       f    b
    //       f    b
    //        gggg
    //       e    c
    //       e    c
    //        dddd
    // 位序：a=0 b=1 c=2 d=3 e=4 f=5 g=6
    static const quint8 kDigits[10] = {
        0b0111111, // 0: a b c d e f
        0b0000110, // 1: b c
        0b1011011, // 2: a b d e g
        0b1001111, // 3: a b c d g
        0b1100110, // 4: b c f g
        0b1101101, // 5: a c d f g
        0b1111101, // 6: a c d e f g
        0b0000111, // 7: a b c
        0b1111111, // 8: 全亮
        0b1101111, // 9: a b c d f g
    };

    const QColor on(0xE4, 0xF2, 0xFF);

    for (int i = 0; i < count; ++i) {
        const qreal x = left + i * (cellW + gap);
        const qreal w = cellW;
        const qreal midY = top + h / 2.0;

        // 竖段端点纵向内缩量：必须按「半高」的比例取，绝不能沿用线宽 t。
        // 竖段可用长度只有 h/2，而 t ≈ cellW * 0.135；
        // 当控件比较扁时 t 会接近甚至超过 h/2，两端各内缩 t 之后
        // 整根竖段只剩几个像素，数码管就退化成「三条横杠」。
        const qreal vInset = h * 0.06;

        // 非数字字符（digitValue() == -1）会越界访问 kDigits，必须挡掉
        const int digit = m_text.at(i).digitValue();
        const quint8 mask = (digit >= 0 && digit <= 9) ? kDigits[digit] : quint8(0);

        // 三个横段
        drawSegment(&p, QPointF(x + inset, top),
                    QPointF(x + w - inset, top), t, mask & (1 << 0), on);          // a
        drawSegment(&p, QPointF(x + inset, midY),
                    QPointF(x + w - inset, midY), t, mask & (1 << 6), on);        // g
        drawSegment(&p, QPointF(x + inset, top + h),
                    QPointF(x + w - inset, top + h), t, mask & (1 << 3), on);      // d

        // 四个竖段
        drawSegment(&p, QPointF(x, top + vInset),
                    QPointF(x, midY - vInset), t, mask & (1 << 5), on);            // f 上
        drawSegment(&p, QPointF(x, midY + vInset),
                    QPointF(x, top + h - vInset), t, mask & (1 << 4), on);        // e 下
        drawSegment(&p, QPointF(x + w, top + vInset),
                    QPointF(x + w, midY - vInset), t, mask & (1 << 1), on);        // b 上
        drawSegment(&p, QPointF(x + w, midY + vInset),
                    QPointF(x + w, top + h - vInset), t, mask & (1 << 2), on);    // c 下

        // 小数点：绘制在最后一个数码管右下角
        if (m_decimal && i == count - 1) {
            p.setPen(Qt::NoPen);
            p.setBrush(on);
            p.drawEllipse(QPointF(x + w + t * 0.6, top + h), t * 0.5, t * 0.5);
        }
    }
}
