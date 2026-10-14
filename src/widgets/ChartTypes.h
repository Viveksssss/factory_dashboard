/**
 * @file ChartTypes.h
 * @brief 图表数据结构定义
 *
 * 所有图表控件的数据都以「值 -> 结构」的纯数据形式传入，
 * 控件本身不持有业务语义，只负责把数据画出来。
 * 这样同一套绘图代码既能用于当月计划达成率，也能用于年度产量汇总。
 */
#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Chart {

/// 一组数据系列（如「计划值」「实际值」）
struct Series
{
    QString name;                          ///< 系列名，用于图例
    QVector<double> values;                ///< 与 categoryLabels 等长
    QColor color;                          ///< 系列颜色
    /// 面积图专用：是否在折线下方填充半透明渐变
    bool filled = false;
    /// 柱状图专用：逐柱配色（长度与 categories 一致时生效）。
    /// 用于「一根柱子一个状态」的场景，如按合格/不合格分别着色。
    QVector<QColor> perBarColors;
};

/// 图表的公共配置
struct Config
{
    /// 纵轴最小值 / 最大值（<=0 表示自动计算）
    double yMin = 0.0;
    double yMax = 0.0;
    /// 纵轴刻度段数
    int yTicks = 5;
    /// 是否绘制横向网格线
    bool gridLines = true;
    /// 网格线颜色
    QColor gridColor = QColor(0x4A, 0x50, 0x7A);
    /// 坐标轴标签字号
    int axisFontSize = 10;
    /// X 轴标签字号
    int xLabelFontSize = 11;
    /// 是否在柱顶/数据点上方显示数值
    bool showValues = true;
    /// 数值字号
    int valueFontSize = 11;
    /// 留白：绘图区相对控件四边的内边距
    int marginLeft = 30;
    int marginRight = 10;
    int marginTop = 18;
    int marginBottom = 20;
};

/// 堆叠柱状图每一行的数据（每日工序达成数：31 天 x 3 个工序）
struct StackRow
{
    QVector<double> parts;    ///< 各分段的值，按顺序堆叠
    QVector<QColor> colors;   ///< 与 parts 等长
};

/// 饼图/环形图的一个扇区
struct PieSlice
{
    QString label;
    double value = 0.0;
    QColor color;
};

/// 横向条形图的一行
struct BarRow
{
    QString label;
    double value = 0.0;
    QColor color;
    /// 数值显示文本，为空则自动用 value 的整数形式
    QString valueText;
};

} // namespace Chart