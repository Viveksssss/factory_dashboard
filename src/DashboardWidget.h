/**
 * @file DashboardWidget.h
 * @brief 主看板：按参考设计稿的三栏布局装配全部面板
 *
 * 布局结构（设计分辨率 1920x1080，坐标由 tools/scale_layout.py 换算）
 * -------------------------------------------------------------------
 *  ┌────── 左栏 609px ──────┬──── 中栏 826px ────┬── 右栏 476px ──┐
 *  │ 当月计划达成率 (标题)     │ 可视化大屏电子看板系统 │ 年度产量汇总    │
 *  │  ├ 模具达成率(环+文本)   │   (标题由 Banner 绘制)│  ├ 模具产量    │
 *  │  ├ 零件达成率(分组柱)   │                      │  ├ 每月模具趋势│
 *  │  └ 零件数(仪表+数码管)   │   中国地图 + 飞线动画 │  ├ 零件产量    │
 *  │ 每日工序达成数 (堆叠柱)  │                      │  └ 每月零件趋势│
 *  │ 模具进度 (标题)          │ 送检一次合格率 (标题) │ 品质管理 (标题) │
 *  │  ├ 模具进度表           │  ├ 零件合格率(15天)   │  ├ 品质占比   │
 *  │  ├ 模具状态统计(饼图)    │  ├ 电极合格率(15天)   │  └ 班组合格率 │
 *  │  └ 加工中数量(仪表)      │  ├ 当天合格率(半圆盘) │ 每日合格率统计  │
 *  │                        │  └ 模具零件合格率      │              │
 *  └────────────────────────┴──────────────────────┴──────────────┘
 *
 * 所有面板坐标在此写死，ScaledCanvas 统一做等比缩放，
 * 保证 1080p / 1440p / 4K 下比例完全一致、图形不变形。
 */
#pragma once

#include <QVector>
#include <QWidget>

#include "./data/DashboardData.h"
#include "./core/ScaledCanvas.h"
#include "./widgets/ChartWidgets.h"
#include "./widgets/ChinaMapWidget.h"
#include "./widgets/GaugeWidgets.h"
#include "./widgets/PanelWidgets.h"

class DashboardWidget : public ScaledCanvas
{
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget *parent = nullptr);
    ~DashboardWidget() override;

protected:
    // 背景与顶部系统标题条由 buildPanels() 中创建的 BackgroundWidget 负责，
    // 因为 ScaledCanvas 采用 render() 渲染内容容器，子控件需自行绘制。

private:
    /// 创建全部面板并装配信号连接
    void buildPanels();
    /// 把数据推送到各图表控件
    void applyData();

    /// 创建纯底色容器
    QWidget *makeHost(const QRectF &rect);
    /// 创建标题条面板
    TitlePanel *makeTitle(const QRectF &rect, const QString &title);

    DataSimulator m_simulator;

    // ---- 左栏：当月计划达成率 ----
    ProgressRing *m_moldRateRing = nullptr;
    StatPanel *m_moldRateStats = nullptr;
    GroupedBarChart *m_partRateChart = nullptr;
    GaugeWidget *m_partCountGauge = nullptr;
    SevenSegDisplay *m_partCountDigits = nullptr;

    // ---- 左栏：每日工序 ----
    StackedBarChart *m_dailyStackChart = nullptr;

    // ---- 左栏：模具进度 ----
    DataTablePanel *m_moldTable = nullptr;
    PieChart *m_moldStatusPie = nullptr;
    GaugeWidget *m_machiningGauge = nullptr;
    SevenSegDisplay *m_machiningDigits = nullptr;

    // ---- 中栏 ----
    ChinaMapWidget *m_map = nullptr;
    LineChart *m_partRateLine = nullptr;
    LineChart *m_electrodeRateLine = nullptr;
    SemicircleDial *m_todayDial = nullptr;
    GroupedBarChart *m_moldPartRateChart = nullptr;

    // ---- 右栏 ----
    HorizontalBarChart *m_moldYieldChart = nullptr;
    LineChart *m_moldMonthlyChart = nullptr;
    HorizontalBarChart *m_partYieldChart = nullptr;
    LineChart *m_partMonthlyChart = nullptr;
    PieChart *m_qualityPie = nullptr;
    GroupedBarChart *m_teamRateChart = nullptr;
    LineChart *m_dailyRateChart = nullptr;

    QVector<QWidget *> m_hosts;   ///< 所有底色容器，统一管理生命周期
};
