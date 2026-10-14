/**
 * @file GaugeWidgets.h
 * @brief 仪表盘控件：环形仪表、半圆刻度盘
 *
 * 设计稿中出现两类仪表：
 *   1. GaugeWidget  —— 270° 环形刻度 + 指针（零件数、加工中模具数量）
 *   2. SemicircleDial —— 180° 半圆刻度 + 中心大号数字（当天合格率 36%）
 *
 * 关键实现点：刻度弧需要「沿弧长做多段渐变」。
 * QLinearGradient 只能沿直线渐变，无法沿圆弧渐变，
 * 这里的解法是把渐变拆成 N 个小色块逐段填充，
 * 每段的颜色按其在弧上的位置采样渐变区间——见 paintArcGradient()。
 */
#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

/**
 * @brief 270° 环形刻度仪表盘
 *
 * 弧线缺口朝下（从 7 点半方向顺时针扫 270° 到 4 点半方向），符合设计稿造型。
 * 外圈为多段渐变刻度环（青 -> 蓝 -> 品红），代表量程分区；
 * 内圈为细指针，从圆心指向当前值。
 *
 * 角度约定见 GaugeWidgets.cpp 中的 polar()：0° = 12 点方向、顺时针为正。
 * 不复用 QPainter::drawArc 的数学约定，因为它 0° 在 3 点方向、逆时针为正、
 * y 轴向上，与屏幕坐标相反，极易把弧线画到对侧半圆上去。
 */
class GaugeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GaugeWidget(QWidget *parent = nullptr);

    /// 设置当前值
    void setValue(double value);
    /// 设置量程 [min, max]
    void setRange(double min, double max);
    /// 设置刻度标签（如 0 / 1104 / 2208 / 3312 / 4416）
    void setTickLabels(const QStringList &labels);
    /// 设置中心说明文字与数值文字
    void setCenterText(const QString &unit, const QString &valueText);
    /// 是否绘制指针
    void setNeedleVisible(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// 沿圆弧绘制多段渐变
    void paintArcGradient(QPainter *p, const QPointF &center, qreal radius,
                          qreal startDeg, qreal spanDeg, qreal lineWidth,
                          const QVector<QColor> &colors) const;

    double m_value = 0.0;
    double m_min = 0.0;
    double m_max = 4100.0;
    QStringList m_tickLabels;
    QString m_unit;
    QString m_valueText;
    bool m_needle = true;
};

/**
 * @brief 180° 半圆刻度盘
 *
 * 用于「当天合格率」：外圈密集刻度线（青 -> 浅蓝渐变），
 * 顶部有一个高亮圆点指示当前值位置，中心显示大号百分比与说明文字。
 */
class SemicircleDial : public QWidget
{
    Q_OBJECT
public:
    explicit SemicircleDial(QWidget *parent = nullptr);

    void setValue(double value);
    void setRange(double min, double max);
    void setCaption(const QString &caption);
    /// 刻度数量（如 0/10/20/.../100 共 11 个标签）
    void setTickCount(int count);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_value = 36.0;
    double m_min = 0.0;
    double m_max = 100.0;
    QString m_caption;
    int m_tickCount = 11;
};