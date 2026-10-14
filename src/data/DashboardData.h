/**
 * @file DashboardData.h
 * @brief 看板业务数据模型与模拟数据源
 *
 * 分层设计
 * --------
 *   DashboardData  —— 纯数据容器（结构体），不含任何绘制逻辑
 *   DataSimulator  —— 数据源，负责生成/刷新数据，通过信号通知 UI
 *
 * 这样拆分的好处：
 *   1. 图表控件只认数据，不认业务，接入真实后端时只需替换 DataSimulator；
 *   2. 数据可被单元测试独立验证；
 *   3. 多个面板可共享同一份数据，避免重复拉取。
 *
 * 模拟数据的抖动算法参考了真实产线的特性：
 *   - 数值在基准值附近做有界随机游走（不会突然跳变）
 *   - 合格率类指标按 Beta 分布特性生成，天然聚集在高位
 */
#pragma once

#include <QColor>
#include <QObject>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "../widgets/ChartTypes.h"

/// 模具进度表的一行
struct MoldRow
{
    QString code;       ///< 模具编号
    QString version;   ///< 版本号
    QString type;      ///< 类型
    QString status;    ///< 状态
    QString product;   ///< 产品名称
};

/// 全部看板数据
struct DashboardData
{
    // ---- 当月计划达成率 ----
    double moldPlanRate = 83.0;      ///< 模具达成率（%）
    int moldPlanTarget = 100;        ///< 模具计划数
    int moldAchieved = 83;           ///< 模具达成数
    int partTotal = 4027;            ///< 零件总数

    /// 零件达成率：4 类零件的「计划 / 达成」（类别文字取自参考图）
    QStringList partCategories{ QStringLiteral("模仁"), QStringLiteral("锻件"),
                                QStringLiteral("辅件"), QStringLiteral("电极") };
    QVector<double> partPlan{ 592, 562, 511, 304 };
    QVector<double> partActual{ 583, 285, 283, 304 };

    // ---- 每日工序达成数（31 天 x 3 个工序）----
    QStringList dayLabels;
    QVector<QVector<double>> dailyProcesses;   ///< 每天 3 个工序的完成量

    // ---- 年度产量汇总 ----
    /// 模具产量：设变 / 修模 / 新模，每类两个数值
    QStringList moldYieldLabels{ QStringLiteral("设变"), QStringLiteral("修模"),
                                 QStringLiteral("新模") };
    QVector<double> moldYieldA{ 45, 3, 19 };
    QVector<double> moldYieldB{ 18, 45, 28 };

    /// 每月模具产量趋势（8 个月 x 3 条折线）
    QStringList monthLabels{ QStringLiteral("1月"), QStringLiteral("2月"),
                             QStringLiteral("3月"), QStringLiteral("4月"),
                             QStringLiteral("5月"), QStringLiteral("6月"),
                             QStringLiteral("7月"), QStringLiteral("8月") };
    QVector<QVector<double>> moldMonthly;      ///< [系列][月份]

    /// 零件产量：其他 / 钢件 / 电极
    QStringList partYieldLabels{ QStringLiteral("其他"), QStringLiteral("钢件"),
                                 QStringLiteral("电极") };
    QVector<double> partYieldA{ 7414, 6019, 6885 };
    QVector<double> partYieldB{ 866, 8217, 8313 };

    /// 每月零件产量趋势（8 个月 x 3 条折线）
    QVector<QVector<double>> partMonthly;

    // ---- 模具进度 ----
    QVector<MoldRow> moldRows;
    /// 模具状态统计饼图：正常交付 / 加工中 / 延期
    QVector<Chart::PieSlice> moldStatusPie;
    int machiningMolds = 59;         ///< 加工中模具数量

    // ---- 送检一次合格率 ----
    QStringList rateLabels15;        ///< 1~15 天
    QVector<double> partRate15;      ///< 零件合格率
    QVector<double> electrodeRate15; ///< 电极合格率
    double todayRate = 36.0;         ///< 当天合格率（%）

    /// 模具零件合格率：CNC粗 / CNC精 / EDM / WEDM / 铣床 / 磨床
    QStringList moldPartLabels{ QStringLiteral("CNC粗"), QStringLiteral("CNC精"),
                                QStringLiteral("EDM"), QStringLiteral("WEDM"),
                                QStringLiteral("铣床"), QStringLiteral("磨床") };
    QVector<double> moldPartRates{ 77, 55, 70, 43, 69, 86 };

    // ---- 品质管理 ----
    QVector<Chart::PieSlice> qualityPie;
    /// 班组合格率：CNC / EDM / 线割 / 磨床 / 铣床 / 外协
    QStringList teamLabels{ QStringLiteral("CNC"), QStringLiteral("EDM"),
                            QStringLiteral("线割"), QStringLiteral("磨床"),
                            QStringLiteral("铣床"), QStringLiteral("外协") };
    QVector<double> teamRates{ 45, 64, 45, 59, 71, 70 };
    /// 每日合格率（31 天）
    QVector<double> dailyRate;
};

/**
 * @brief 模拟数据源：定时刷新并通过 dataUpdated() 广播
 *
 * 真实部署时把本类替换为 DataProvider 实现（HTTP / 数据库 / MQTT），
 * 上层 UI 无需改动——只要发同样的 dataUpdated() 信号即可。
 */
class DataSimulator : public QObject
{
    Q_OBJECT
public:
    explicit DataSimulator(QObject *parent = nullptr);

    /// 启动自动刷新（默认 3 秒一次）
    void start(int intervalMs = 3000);
    /// 停止自动刷新
    void stop();

    /// 获取当前数据快照
    const DashboardData &data() const { return m_data; }

signals:
    /// 数据已更新，UI 收到后重新取 data() 并刷新
    void dataUpdated();

private:
    void initData();
    /// 模拟一次数据波动
    void tick();

    /// 有界随机游走：把 v 限制在 [base*0.85, base*1.15] 内小幅变化
    static double jitter(double current, double base, double amplitude,
                         QRandomGenerator *rng);
    /// 生成偏向高位的合格率先验值（Beta 分布特性）
    static double skewedHigh(double min, double max, double mean,
                             QRandomGenerator *rng);

    DashboardData m_data;
    QTimer m_timer;
    QRandomGenerator m_rng;
    int m_tickCount = 0;
};