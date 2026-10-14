#include "PanelWidgets.h"

#include "../core/Theme.h"

#include <QPainter>
#include <QPaintEvent>

// ============================================================================
// TitlePanel —— 标题条
// ============================================================================

TitlePanel::TitlePanel(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void TitlePanel::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    update();
}

void TitlePanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 标题栏本体
    Theme::drawPanelHeader(&p, QRectF(rect()), m_title);

    // 底部再压一条暗色分隔线，强化与内容区的界限
    p.setPen(QPen(Theme::background(), 1.2));
    p.drawLine(QPointF(0, height() - 1), QPointF(width(), height() - 1));
}

// ============================================================================
// StatPanel —— 键值文本区
// ============================================================================

StatPanel::StatPanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void StatPanel::setLines(const QVector<Line> &lines)
{
    m_lines = lines;
    update();
}

void StatPanel::setSimpleLines(const QStringList &texts, int fontSize)
{
    m_lines.clear();
    for (const QString &t : texts) {
        Line l;
        l.text = t;
        l.color = Theme::text();
        l.fontSize = fontSize;
        l.bold = true;
        m_lines.append(l);
    }
    update();
}

void StatPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 背景透明（由父级 host 提供面板底色），这里只画文字
    if (m_lines.isEmpty())
        return;

    const qreal lineH = 22.0;
    const qreal blockH = lineH * m_lines.size();
    // 垂直居中
    const qreal top = (height() - blockH) * m_centerRatio + (m_centerRatio - 0.5) * 0.0;
    const qreal startY = qBound(0.0, top, qMax(0.0, height() - blockH));

    for (int i = 0; i < m_lines.size(); ++i) {
        const Line &l = m_lines.at(i);
        p.setFont(Theme::valueFont(l.fontSize, l.bold));
        p.setPen(l.color);
        p.drawText(QRectF(0, startY + lineH * i, width(), lineH),
                   Qt::AlignLeft | Qt::AlignVCenter, l.text);
    }
}

// ============================================================================
// DataTablePanel —— 数据表格
// ============================================================================

DataTablePanel::DataTablePanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void DataTablePanel::setColumns(const QVector<Column> &columns)
{
    m_columns = columns;
    update();
}

void DataTablePanel::setRows(const QVector<QStringList> &rows)
{
    m_rows = rows;
    update();
}

void DataTablePanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.fillRect(rect(), m_bodyColor);

    if (m_columns.isEmpty())
        return;

    // ---- 表头 ----
    const QRectF headerRect(0, 0, width(), m_headerHeight);
    Theme::drawPanelHeader(&p, headerRect, QString());
    // 清掉右上角装饰（表格表头不需要）
    p.fillRect(QRectF(width() - 60, 0, 60, m_headerHeight),
               Theme::panelHeader().darker(120));

    p.setFont(Theme::valueFont(12));
    qreal x = 0;
    for (const Column &col : m_columns) {
        p.setPen(Theme::accentCyan());
        p.drawText(QRectF(x, 0, col.width, m_headerHeight),
                   Qt::AlignCenter, col.title);
        x += col.width;
    }

    // ---- 数据行 ----
    const int visible = int((height() - m_headerHeight) / m_rowHeight);
    const int count = qMin(visible, m_rows.size());
    for (int r = 0; r < count; ++r) {
        const qreal y = m_headerHeight + m_rowHeight * r;
        const QRectF rowRect(0, y, width(), m_rowHeight);

        // 斑马纹
        if (m_zebra && r % 2 == 0)
            p.fillRect(rowRect, QColor(0x2C, 0x30, 0x54));

        p.setFont(Theme::axisFont(12));
        const QStringList &cells = m_rows.at(r);
        qreal cx = 0;
        for (int c = 0; c < m_columns.size(); ++c) {
            const QString text = c < cells.size() ? cells.at(c) : QString();
            p.setPen(Theme::text());
            p.drawText(QRectF(cx, y, m_columns.at(c).width, m_rowHeight),
                       Qt::AlignCenter, text);
            cx += m_columns.at(c).width;
        }
    }

    // ---- 网格线（最后统一画，保证线条连续不被单元格覆盖）----
    p.setPen(QPen(Theme::panelBorder(), 1.0));
    // 竖线
    x = 0;
    for (const Column &col : m_columns) {
        p.drawLine(QPointF(x, 0), QPointF(x, height()));
        x += col.width;
    }
    p.drawLine(QPointF(x, 0), QPointF(x, height()));
    // 横线
    p.drawLine(QPointF(0, m_headerHeight), QPointF(width(), m_headerHeight));
    for (int r = 0; r <= visible; ++r) {
        const qreal y = m_headerHeight + m_rowHeight * r;
        if (y > height())
            break;
        p.drawLine(QPointF(0, y), QPointF(width(), y));
    }
}