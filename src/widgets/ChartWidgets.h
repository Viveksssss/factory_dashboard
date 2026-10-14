/**
 * @file ChartWidgets.h
 * @brief 自绘图表控件集合
 *
 * 设计取舍
 * --------
 * 没有引入 Qwt / QChart 等第三方图表库，原因有三：
 *   1. QChart 的样式定制能力弱，做不出设计稿里的渐变面积、
 *      定制刻度、双色分段弧等效果，改造成本高于自己写；
 *   2. 大屏需要 20+ 图表同屏刷新，自绘能精确控制绘制顺序与缓存策略；
 *   3. 零第三方依赖，交付时只需 Qt 6 本身，编译环境更简单。
 *
 * 所有控件都是 QWidget 子类，通过 setData() 灌数据、update() 触发重绘。
 * 控件内部只做「数据 -> 像素」的映射，不持有业务状态，
 * 因此可以任意复用组合。
 */
#pragma once

#include "ChartTypes.h"

#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

// ============================== 柱状图 ==============================

/**
 * @brief 分组柱状图（每组两根柱子，如「模仁/零件」的计划 vs 达成）
 *
 * 效果还原要点：
 *   - 柱子带垂直渐变（顶部亮、底部暗）
 *   - 柱顶显示数值标签
 *   - Y 轴网格线为浅色实线，X 轴标签居中于组下方
 */
class GroupedBarChart : public QWidget
{
    Q_OBJECT
public:
    explicit GroupedBarChart(QWidget *parent = nullptr);

    void setData(const QStringList &categoryLabels,
                 const QVector<Chart::Series> &series,
                 const Chart::Config &config = {});

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QStringList m_categories;
    QVector<Chart::Series> m_series;
    Chart::Config m_config;
};

// ============================== 堆叠柱状图 ==============================

/**
 * @brief 堆叠柱状图（每日工序达成数：每日堆叠 3 个工序的完成量）
 *
 * 每根柱子由多个分段自下而上堆叠，分段之间留 1px 间隙以增强层次感。
 */
class StackedBarChart : public QWidget
{
    Q_OBJECT
public:
    explicit StackedBarChart(QWidget *parent = nullptr);

    void setData(const QStringList &categoryLabels,
                 const QVector<Chart::StackRow> &rows,
                 const Chart::Config &config = {});

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QStringList m_categories;
    QVector<Chart::StackRow> m_rows;
    Chart::Config m_config;
};

// ============================== 横向条形图 ==============================

/**
 * @brief 横向条形图（年度产量汇总的模具/零件产量）
 *
 * 特点：左侧 Y 轴为类别名，右侧显示数值，条形有渐变填充。
 * 同一类别可有多条（如「设变」分蓝色/青色两条）。
 */
class HorizontalBarChart : public QWidget
{
    Q_OBJECT
public:
    explicit HorizontalBarChart(QWidget *parent = nullptr);

    void setData(const QVector<Chart::BarRow> &rows,
                 const Chart::Config &config = {});

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Chart::BarRow> m_rows;
    Chart::Config m_config;
    double m_maxValue = 0.0;
};

// ============================== 折线 / 面积图 ==============================

/**
 * @brief 多系列折线图，支持面积填充
 *
 * 用于「每月模具产量趋势图」（3~4 条折线，无填充）
 * 与「零件合格率（15天）」（单折线 + 渐变面积填充）。
 *
 * 效果还原要点：
 *   - 折线用 Catmull-Rom 平滑（转 cubic bezier），避免折线过于生硬
 *   - 数据点为空心圆点，白色描边
 *   - 面积填充为自上而下的透明度渐变
 */
class LineChart : public QWidget
{
    Q_OBJECT
public:
    explicit LineChart(QWidget *parent = nullptr);

    void setData(const QStringList &xLabels,
                 const QVector<Chart::Series> &series,
                 const Chart::Config &config = {});

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QStringList m_xLabels;
    QVector<Chart::Series> m_series;
    Chart::Config m_config;
};

// ============================== 饼图 ==============================

/**
 * @brief 饼图（模具状态统计、品质占比）
 *
 * 直接绘制扇区（drawPie），扇区之间用背景色描边做分隔。
 * 支持在扇区内部或外部显示百分比标签。
 */
class PieChart : public QWidget
{
    Q_OBJECT
public:
    explicit PieChart(QWidget *parent = nullptr);

    void setData(const QVector<Chart::PieSlice> &slices, bool showPercent = true);
    /// 是否在标签里带上扇区名称（设计稿中「模具状态统计」带名称、
    /// 「品质占比」只标百分比，故做成可配）
    void setShowNames(bool show) { m_showNames = show; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Chart::PieSlice> m_slices;
    bool m_showPercent = true;
    bool m_showNames = true;
};

// ============================== 进度圆环 ==============================

/**
 * @brief 环形进度指示器（当月计划达成率 83%）
 *
 * 底环为浅灰轨道，前景环按百分比着色（低于阈值转黄/红）。
 * 环内居中显示大号百分比数字。
 */
class ProgressRing : public QWidget
{
    Q_OBJECT
public:
    explicit ProgressRing(QWidget *parent = nullptr);

    /// 设置百分比（0~100）与中心文案
    void setValue(double percent);
    void setCaption(const QString &caption);
    /// 设置达标阈值，低于该值进度环变为警告色
    void setThreshold(double warnBelow, double dangerBelow);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_percent = 0.0;
    QString m_caption;
    double m_warnBelow = 60.0;
    double m_dangerBelow = 30.0;
};

// ============================== 七段数码管 ==============================

/**
 * @brief 七段数码管显示器（零件总数 4027、加工中模具数 059）
 *
 * 手写七段几何：每个数字对应 a~g 七段的开关组合，
 * 未点亮的段以极低透明度绘制，形成「熄灭段」的科技感。
 */
class SevenSegDisplay : public QWidget
{
    Q_OBJECT
public:
    explicit SevenSegDisplay(QWidget *parent = nullptr);

    /// 设置显示内容，仅保留数字字符
    void setNumber(const QString &text);
    /// 设置小数点（右下角）
    void setDecimalPoint(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawSegment(QPainter *p, const QPointF &a, const QPointF &b, qreal width,
                     bool on, const QColor &color);

    QString m_text;
    bool m_decimal = false;
};