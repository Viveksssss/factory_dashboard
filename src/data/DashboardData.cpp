#include "DashboardData.h"

#include "../core/Theme.h"

#include <QtMath>

DataSimulator::DataSimulator(QObject *parent)
    : QObject(parent)
    , m_rng(QRandomGenerator::global()->generate())
{
    initData();
}

void DataSimulator::start(int intervalMs)
{
    m_timer.start(intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &DataSimulator::tick,
            Qt::UniqueConnection);
}

void DataSimulator::stop()
{
    m_timer.stop();
}

double DataSimulator::jitter(double current, double base, double amplitude,
                             QRandomGenerator *rng)
{
    // 以基准值为中心做小步随机游走，并硬性夹在允许区间内，
    // 保证数值「有变化但不跳变」，符合大屏刷新观感。
    const double delta = (rng->generateDouble() - 0.5) * 2.0 * amplitude;
    double next = current + delta;
    const double lo = base * 0.85;
    const double hi = base * 1.15;
    if (next < lo) next = lo;
    if (next > hi) next = hi;
    return next;
}

double DataSimulator::skewedHigh(double min, double max, double mean,
                                 QRandomGenerator *rng)
{
    // 合格率天然聚集在 80~95 区间，用「均值 + 三角噪声」模拟这种偏斜分布。
    const double r = rng->generateDouble();
    // 三角分布：两处分布加权，得到集中在 mean 附近的形状
    const double skew = (r - 0.5) + (rng->generateDouble() - 0.5) * 0.6;
    double v = mean + skew * 12.0;
    return qBound(min, v, max);
}

void DataSimulator::initData()
{
    DashboardData &d = m_data;

    // ---- 每日工序达成数：31 天 ----
    for (int i = 1; i <= 31; ++i)
        d.dayLabels.append(QString::number(i));
    d.dailyProcesses.resize(31);
    for (int day = 0; day < 31; ++day) {
        // 每个工序有各自的基准产能，叠加周末效应
        const bool weekend = (day % 7 == 5 || day % 7 == 6);
        const double factor = weekend ? 0.72 : 1.0;
        d.dailyProcesses[day] = {
            18 + m_rng.bounded(12) * factor,       // 工序一
            14 + m_rng.bounded(10) * factor,       // 工序二
            20 + m_rng.bounded(14) * factor        // 工序三
        };
    }

    // ---- 每月产量趋势：8 个月 x 3 系列 ----
    d.moldMonthly.resize(3);
    d.partMonthly.resize(3);
    for (int s = 0; s < 3; ++s) {
        d.moldMonthly[s].resize(8);
        d.partMonthly[s].resize(8);
        for (int m = 0; m < 8; ++m) {
            d.moldMonthly[s][m] = 15 + m_rng.bounded(85);
            d.partMonthly[s][m] = 10 + m_rng.bounded(85);
        }
    }

    // ---- 模具进度表：15 行 ----
    static const char *kTypes[] = { "新模", "模仁", "零件", "电极" };
    static const char *kProducts[] = { "面板", "支架", "夹具", "冲头" };
    for (int i = 1; i <= 15; ++i) {
        MoldRow r;
        r.code = QStringLiteral("FK%1").arg(i);
        r.version = QStringLiteral("V%1").arg(i);
        r.type = QString::fromUtf8(kTypes[i % 4]);
        r.status = (i % 6 == 0) ? QStringLiteral("已交付") : QStringLiteral("加工中");
        r.product = QString::fromUtf8(kProducts[i % 4]);
        d.moldRows.append(r);
    }

    // ---- 模具状态统计 ----
    d.moldStatusPie = {
        { QStringLiteral("正常交付"), 38, Theme::okGreen() },
        { QStringLiteral("加工中"),   31, Theme::accentCyan() },
        { QStringLiteral("延期"),     31, Theme::warnYellow() },
    };

    // ---- 15 天合格率 ----
    for (int i = 1; i <= 15; ++i)
        d.rateLabels15.append(QString::number(i));
    d.partRate15.resize(15);
    d.electrodeRate15.resize(15);
    for (int i = 0; i < 15; ++i) {
        d.partRate15[i] = skewedHigh(35, 100, 70, &m_rng);
        d.electrodeRate15[i] = skewedHigh(35, 100, 62, &m_rng);
    }

    // ---- 品质占比 ----
    d.qualityPie = {
        { QStringLiteral("合格"), 27, Theme::okGreen() },
        { QStringLiteral("让步接收"), 23, Theme::dangerRed() },
        { QStringLiteral("返工"), 30, Theme::warnYellow() },
        { QStringLiteral("报废"), 21, Theme::accentBlue() },
    };

    // ---- 每日合格率（31 天）----
    d.dailyRate.resize(31);
    for (int i = 0; i < 31; ++i)
        d.dailyRate[i] = skewedHigh(40, 100, 72, &m_rng);
}

void DataSimulator::tick()
{
    DashboardData &d = m_data;
    ++m_tickCount;

    // ---- 当月计划达成率：小幅爬升，不会倒退 ----
    if (m_tickCount % 3 == 0 && d.moldAchieved < d.moldPlanTarget) {
        ++d.moldAchieved;
        d.moldPlanRate = qreal(d.moldAchieved) / d.moldPlanTarget * 100.0;
    }

    // ---- 零件总数缓慢增长 ----
    d.partTotal += m_rng.bounded(0, 6);

    // ---- 零件达成率 ----
    for (int i = 0; i < d.partPlan.size(); ++i) {
        d.partActual[i] = qRound(jitter(d.partActual.at(i), d.partPlan.at(i),
                                        d.partPlan.at(i) * 0.05, &m_rng));
    }

    // ---- 每日工序达成数：只刷新当天（最后一天）的数据 ----
    const int last = d.dailyProcesses.size() - 1;
    if (last >= 0) {
        for (int k = 0; k < d.dailyProcesses.at(last).size(); ++k) {
            const double base = 20 + k * 4;
            d.dailyProcesses[last][k] = qRound(
                jitter(d.dailyProcesses.at(last).at(k), base, 2.5, &m_rng));
        }
    }

    // ---- 月度趋势：随机挑一段刷新，模拟「新月份数据回填」 ----
    if (m_tickCount % 2 == 0) {
        const int s = m_rng.bounded(3);
        const int m = m_rng.bounded(8);
        d.moldMonthly[s][m] = qRound(jitter(d.moldMonthly.at(s).at(m), 55, 12, &m_rng));
        d.partMonthly[s][m] = qRound(jitter(d.partMonthly.at(s).at(m), 50, 14, &m_rng));
    }

    // ---- 加工中模具数量 ----
    d.machiningMolds += m_rng.bounded(-1, 2);

    // ---- 15 天合格率：只更新最后一天 ----
    d.partRate15[14] = skewedHigh(35, 100, 70, &m_rng);
    d.electrodeRate15[14] = skewedHigh(35, 100, 62, &m_rng);

    // ---- 当天合格率 ----
    d.todayRate = qRound(jitter(d.todayRate, 36, 4, &m_rng));

    // ---- 模具零件合格率 ----
    for (int i = 0; i < d.moldPartRates.size(); ++i)
        d.moldPartRates[i] = qRound(jitter(d.moldPartRates.at(i),
                                          d.moldPartRates.at(i), 6, &m_rng));

    // ---- 班组合格率 ----
    for (int i = 0; i < d.teamRates.size(); ++i)
        d.teamRates[i] = qRound(jitter(d.teamRates.at(i),
                                       d.teamRates.at(i), 5, &m_rng));

    // ---- 每日合格率：更新最后一天 ----
    d.dailyRate[30] = skewedHigh(40, 100, 72, &m_rng);

    emit dataUpdated();
}
