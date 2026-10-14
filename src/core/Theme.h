/**
 * @file Theme.h
 * @brief 大屏视觉主题：配色、字体、留白与通用绘制助手
 *
 * 设计说明
 * --------
 * 本项目的所有颜色均从参考设计稿逐像素采样得到，而非凭经验估计，
 * 以保证复刻结果与原稿在色相、明度上高度一致。
 *
 * 配色体系为「深蓝底 + 青/蓝双主色 + 黄红点缀」：
 *   - 底色    #040726 深空蓝，作为大屏背景，压暗以突出数据层
 *   - 面板头   #303056 略亮于底色，形成层级
 *   - 面板体   #26294A 卡片主体
 *   - 主强调   #00B0B5 青绿 / #0071C0 蓝，图表双主色
 *   - 点缀     #FFC000 警戒黄、#FE0000 危险红、#00AF50 完成绿
 *
 * 所有绘制函数均为无状态静态函数，接收 QPainter* 而非持有状态，
 * 这样控件只需组合调用，便于拆分与复用。
 */
#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QtMath>

#include <cmath>

namespace Theme {

// ===================== 基础色板 =====================

/// 大屏最底层背景（深空蓝）
inline QColor background()      { return QColor(0x04, 0x07, 0x26); }
/// 面板标题栏底色
inline QColor panelHeader()     { return QColor(0x30, 0x30, 0x56); }
/// 面板主体底色
inline QColor panelBody()       { return QColor(0x26, 0x29, 0x4A); }
/// 面板描边 / 分隔线
inline QColor panelBorder()     { return QColor(0x1A, 0x1D, 0x3E); }
/// 面板描边（更亮一档，用于需要强调边界的面板）
inline QColor panelBorderLite() { return QColor(0x3A, 0x3E, 0x68); }
/// 次级文字 / 坐标轴标签
inline QColor textSub()         { return QColor(0x9A, 0xA3, 0xC8); }
/// 常规正文
inline QColor text()            { return QColor(0xD8, 0xDE, 0xF0); }
/// 高亮数值文字
inline QColor textStrong()      { return QColor(0xFF, 0xFF, 0xFF); }

// ===================== 数据主色 =====================

/// 主强调色 1 —— 青绿（柱状图主色、进度环、地图飞线）
inline QColor accentCyan()      { return QColor(0x00, 0xB0, 0xB5); }
/// 主强调色 2 —— 蓝（柱状图次色、折线图主色）
inline QColor accentBlue()      { return QColor(0x00, 0x71, 0xC0); }
/// 折线图第三色（浅蓝，用于面积图叠加）
inline QColor accentLightBlue() { return QColor(0x23, 0x6F, 0xA1); }
/// 折线图第四色（深青）
inline QColor accentDeepCyan()  { return QColor(0x0F, 0x79, 0x87); }

/// 警戒黄（延期、超标柱体）
inline QColor warnYellow()      { return QColor(0xFF, 0xC0, 0x00); }
/// 危险红（不合格、告警）
inline QColor dangerRed()       { return QColor(0xFE, 0x00, 0x00); }
/// 完成绿（正常交付）
inline QColor okGreen()         { return QColor(0x00, 0xAF, 0x50); }
/// 亮蓝（仪表盘高亮刻度）
inline QColor brightBlue()      { return QColor(0x41, 0xB0, 0xFF); }
/// 品红（仪表盘高值区）
inline QColor hotPink()         { return QColor(0xFF, 0x2E, 0x63); }

/// 图表默认调色板顺序（青 -> 蓝 -> 黄 -> 红 -> 绿）
inline QVector<QColor> palette()
{
    return { accentCyan(), accentBlue(), warnYellow(), dangerRed(), okGreen() };
}

// ===================== 字号 =====================

/// 统一字体族：大屏环境优先使用微软雅黑
inline QString family() { return QStringLiteral("Microsoft YaHei"); }

/// 面板大标题（居中，带轻微加粗）
inline QFont titleFont(int px)
{
    QFont f(family(), px, QFont::Bold);
    f.setHintingPreference(QFont::PreferFullHinting);
    return f;
}

/// 面板小标题（左对齐）
inline QFont subTitleFont(int px)
{
    QFont f(family(), px, QFont::Normal);
    return f;
}

/// 数据标签
inline QFont valueFont(int px, bool bold = true)
{
    QFont f(family(), px, bold ? QFont::Bold : QFont::Normal);
    return f;
}

/// 坐标轴刻度
inline QFont axisFont(int px)
{
    QFont f(family(), px, QFont::Normal);
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

// ===================== 通用绘制助手 =====================

/**
 * @brief 绘制面板标题栏
 *
 * 标题栏底色为横向渐变（左深右深，中间略亮），
 * 底部一条高亮分隔线，右上角绘制三个装饰小方块，模拟原稿的科技感细节。
 */
void drawPanelHeader(QPainter *p, const QRectF &rect, const QString &title);

/**
 * @brief 绘制面板外框（四边描边 + 四角装饰角标）
 * @param accent 角标与高亮边使用的颜色
 */
void drawPanelFrame(QPainter *p, const QRectF &rect, const QColor &accent = panelBorderLite());

/**
 * @brief 绘制渐变填充矩形（上下渐变）
 */
void drawVerticalGradient(QPainter *p, const QRectF &rect,
                          const QColor &top, const QColor &bottom);

/**
 * @brief 绘制线性渐变条（水平方向），用于图例色条
 */
void drawHorizontalGradient(QPainter *p, const QRectF &rect, const QList<QColor> &colors);

/**
 * @brief 在两点之间绘制一条带箭头的贝塞尔弧线
 *
 * 用于地图飞线：control 为弧顶偏移点，箭头沿曲线末端切线方向绘制。
 */
void drawArrowCurve(QPainter *p, const QPointF &start, const QPointF &end,
                    const QPointF &control, const QColor &color, qreal width = 1.6,
                    bool arrow = true);

/**
 * @brief 圆角矩形路径（Qt6 推荐用 QPainterPath 而非 QPainter::drawRoundedRect）
 */
inline QPainterPath roundedPath(const QRectF &rect, qreal radius)
{
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    return path;
}

/**
 * @brief 居中绘制文本
 */
void drawTextCentered(QPainter *p, const QRectF &rect, const QString &text,
                      const QFont &font, const QColor &color);

/**
 * @brief 计算「漂亮」的坐标轴刻度上界
 *
 * 图表纵轴通常直接用数据最大值会导致柱顶贴边、
 * 或最高点正好落在网格线上。这里按 1/2/2.5/5/10 的步进向上取整，
 * 保证至少留出一格余量，并让刻度值为整数。
 */
inline qreal niceAxisMax(qreal rawMax)
{
    if (rawMax <= 0.0)
        return 10.0;
    const int steps = 5;                     // 期望分成 5 段
    const qreal rough = rawMax * 1.15/ steps; // 留 15% 余量
    const qreal mag = std::pow(10.0, std::floor(std::log10(rough)));
    const qreal norm = rough / mag;
    qreal nice = 10.0;
    if (norm <= 1.0)      nice = 1.0;
    else if (norm <= 2.0) nice = 2.0;
    else if (norm <= 2.5) nice = 2.5;
    else if (norm <= 5.0) nice = 5.0;
    return nice * mag * steps;
}

} // namespace Theme