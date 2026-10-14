#include "Theme.h"

namespace Theme {

void drawPanelHeader(QPainter *p, const QRectF &rect, const QString &title)
{
    // 标题栏底色：中间略亮的横向渐变，模拟原稿的金属质感
    QLinearGradient grad(rect.left(), 0, rect.right(), 0);
    grad.setColorAt(0.0, panelHeader().darker(130));
    grad.setColorAt(0.5, panelHeader());
    grad.setColorAt(1.0, panelHeader().darker(130));
    p->fillRect(rect, grad);

    // 底部高亮分隔线
    QPen line(panelBorderLite(), 1.0);
    p->setPen(line);
    p->drawLine(QPointF(rect.left(), rect.bottom()),
                QPointF(rect.right(), rect.bottom()));

    // 标题居中绘制
    QFont f = titleFont(qMax(11, qRound(rect.height() * 0.52)));
    p->setFont(f);
    // 文字带一层深色阴影，先画偏移的暗色副本再画亮色本体
    p->setPen(QColor(0, 0, 0, 160));
    p->drawText(rect.translated(0, 1), Qt::AlignCenter, title);
    p->setPen(text());
    p->drawText(rect, Qt::AlignCenter, title);

    // 右上角装饰：三个递减的小方块
    const qreal sq = qMax(3.0, rect.height() * 0.16);
    const qreal gap = sq * 1.6;
    QColor deco = accentCyan();
    deco.setAlpha(140);
    for (int i = 0; i < 3; ++i) {
        const qreal s = sq * (1.0 - i * 0.22);
        const QRectF r(rect.right() - gap * (i + 1) - sq * 0.2,
                       rect.center().y() - s / 2.0, s, s);
        p->fillRect(r, deco);
    }
}

void drawPanelFrame(QPainter *p, const QRectF &rect, const QColor &accent)
{
    // 外框
    p->setPen(QPen(accent, 1.0));
    p->setBrush(Qt::NoBrush);
    p->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));

    // 四角装饰角标：短横线 + 短竖线构成 L 形
    const qreal len = qMin(14.0, qMin(rect.width(), rect.height()) * 0.18);
    const qreal lw = 1.6;
    QColor corner = accentCyan();
    corner.setAlpha(190);

    // 左上
    p->setPen(QPen(corner, lw, Qt::SolidLine, Qt::SquareCap));
    p->drawLine(rect.topLeft(), QPointF(rect.left() + len, rect.top()));
    p->drawLine(rect.topLeft(), QPointF(rect.left(), rect.top() + len));
    // 右上
    p->drawLine(rect.topRight(), QPointF(rect.right() - len, rect.top()));
    p->drawLine(rect.topRight(), QPointF(rect.right(), rect.top() + len));
    // 左下
    p->drawLine(rect.bottomLeft(), QPointF(rect.left() + len, rect.bottom()));
    p->drawLine(rect.bottomLeft(), QPointF(rect.left(), rect.bottom() - len));
    // 右下
    p->drawLine(rect.bottomRight(), QPointF(rect.right() - len, rect.bottom()));
    p->drawLine(rect.bottomRight(), QPointF(rect.right(), rect.bottom() - len));
}

void drawVerticalGradient(QPainter *p, const QRectF &rect,
                          const QColor &top, const QColor &bottom)
{
    QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
    grad.setColorAt(0.0, top);
    grad.setColorAt(1.0, bottom);
    p->fillRect(rect, grad);
}

void drawHorizontalGradient(QPainter *p, const QRectF &rect, const QList<QColor> &colors)
{
    if (colors.isEmpty())
        return;
    QLinearGradient grad(rect.topLeft(), rect.topRight());
    const int n = colors.size();
    for (int i = 0; i < n; ++i)
        grad.setColorAt(n == 1 ? 0.0 : double(i) / (n - 1), colors.at(i));
    p->fillRect(rect, grad);
}

void drawArrowCurve(QPainter *p, const QPointF &start, const QPointF &end,
                    const QPointF &control, const QColor &color, qreal width,
                    bool arrow)
{
    // 构造二次贝塞尔曲线
    QPainterPath path(start);
    path.quadTo(control, end);

    // 曲线主体
    p->setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);

    if (!arrow)
        return;

    // 箭头：取曲线末端（t=0.92 处）附近两点求切线方向，
    // 直接用端点控制点连线求切线在端点处是近似的，误差较大，
    // 所以取两个采样点做差分。
    const QPointF p1 = path.pointAtPercent(0.88);
    const QPointF p2 = path.pointAtPercent(0.94);
    QPointF dir = p2 - p1;
    const qreal len = std::hypot(dir.x(), dir.y());
    if (len < 1e-6)
        return;
    dir /= len;

    const qreal size = 6.0 + width * 1.6;
    const qreal angle = qRadiansToDegrees(std::atan2(dir.y(), dir.x()));

    p->save();
    p->translate(end);
    p->rotate(angle);
    QPolygonF tri;
    tri << QPointF(0, 0)
        << QPointF(-size, size * 0.42)
        << QPointF(-size, -size * 0.42);
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPolygon(tri);
    p->restore();
}

void drawTextCentered(QPainter *p, const QRectF &rect, const QString &text,
                      const QFont &font, const QColor &color)
{
    p->setFont(font);
    p->setPen(color);
    p->drawText(rect, Qt::AlignCenter, text);
}

} // namespace Theme