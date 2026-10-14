/**
 * @file PanelWidgets.h
 * @brief 组合式面板：标题条、键值文本区、数据表格
 *
 * 这些面板与 ChartWidgets 的定位不同：
 *   ChartWidgets 负责「把数据画成图」，本身不含标题；
 *   PanelWidgets 负责「把文字排成版式」，本身不含图表。
 *
 * 单独抽出来的原因：设计稿中同一版式重复出现多次
 * （如「零件合格率(15天)」与「电极合格率(15天)」两个面板结构完全一致），
 * 提取后只需传不同参数即可，新增面板的代码量降到 3 行。
 */
#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

/**
 * @brief 纯标题条面板（自动绘制渐变底、标题文字与右上角装饰）
 */
class TitlePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TitlePanel(const QString &title, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    QString title() const { return m_title; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
};

/**
 * @brief 键值文本面板：竖排若干行「标签: 数值」
 *
 * 用于「计划: 100 / 达成: 83」这类无图形的纯文本信息区。
 */
class StatPanel : public QWidget
{
    Q_OBJECT
public:
    /// 一行文本
    struct Line
    {
        QString text;
        QColor color;
        int fontSize = 13;
        bool bold = true;
    };

    explicit StatPanel(QWidget *parent = nullptr);

    void setLines(const QVector<Line> &lines);
    /// 便捷设置：等宽字体、同一颜色
    void setSimpleLines(const QStringList &texts, int fontSize = 13);

    /// 内容垂直居中区域（相对控件顶部的高度占比 0~1）
    void setVerticalCenterRatio(qreal ratio) { m_centerRatio = ratio; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Line> m_lines;
    qreal m_centerRatio = 0.5;
};

/**
 * @brief 数据表格面板
 *
 * 深色主题下 QTableView / QTableWidget 的样式定制需要覆盖大量
 * QSS 且在部分平台上仍不一致，因此这里直接自绘：
 * 表格结构固定（列宽可配），只负责画网格线与文字，代码量更小且表现可控。
 */
class DataTablePanel : public QWidget
{
    Q_OBJECT
public:
    /// 列定义
    struct Column
    {
        QString title;
        int width = 60;
        /// 单元格取值的字段索引，-1 表示用序号列
        int fieldIndex = 0;
    };

    explicit DataTablePanel(QWidget *parent = nullptr);

    /// 设置表头与列宽
    void setColumns(const QVector<Column> &columns);
    /// 设置数据行（每行为 QStringList，与 columns 等长或更长）
    void setRows(const QVector<QStringList> &rows);
    /// 交替行底色
    void setZebraStripes(bool on) { m_zebra = on; }

    /// 表格主体的背景色
    void setBodyColor(const QColor &c) { m_bodyColor = c; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Column> m_columns;
    QVector<QStringList> m_rows;
    bool m_zebra = true;
    QColor m_bodyColor = QColor(0x26, 0x29, 0x4A);
    int m_rowHeight = 26;
    int m_headerHeight = 28;
};