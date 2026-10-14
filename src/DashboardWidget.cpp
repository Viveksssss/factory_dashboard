#include "DashboardWidget.h"

#include "../src/core/Theme.h"

#include <QPainter>
#include <QPalette>

// ============================================================================
// 布局常量表
// ============================================================================
// 参考设计稿尺寸 2265x1218，本项目设计基准 1920x1080。
// 下列坐标由 tools/scale_layout.py 依据参考图实测面板边界等比换算。
//
// 三栏：左 609（x 0..609） / 中 826（x 613..1439） / 右 476（x 1444..1920）
// 栏间留 4px 缝隙，作为视觉分隔。
namespace {

constexpr int kLeftX = 0;
constexpr int kLeftW = 609;
constexpr int kCenterX = 613;
constexpr int kCenterW = 826;
constexpr int kRightX = 1444;
constexpr int kRightW = 476;

/// 大标题条高度（如「当月计划达成率」「年度产量汇总」）
constexpr int kTitleH = 41;
/// 子面板标题条高度
constexpr int kSubTitleH = 42;

/// 矩形辅助
inline QRectF R(int x, int y, int w, int h) { return QRectF(x, y, w, h); }

/**
 * @brief 面板宿主控件：每个图表面板的外框，负责铺面板底色
 *
 * 为什么不用 autoFillBackground + QPalette（看起来更「Qt 味」）？
 *   在 ScaledCanvas 的离屏 render() 绘制路径下，声明了
 *   WA_OpaquePaintEvent 的控件会被 Qt 视为「自己会画满整个区域」，
 *   于是跳过由调色板触发的背景填充。结果是面板底色根本没被画出来，
 *   露出离屏容器的底色（系统默认浅灰 #F0F0F0），
 *   整屏面板全变成浅色、白字几乎不可见。
 *   改成在 paintEvent 里显式 fillRect 后，行为与渲染路径完全无关。
 */
class PanelHost : public QWidget
{
public:
    explicit PanelHost(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Theme::panelBody());
    }
};

} // namespace

/**
 * @brief 铺满全画布的背景层：负责大屏底色与顶部系统标题条
 *
 * 单独做成控件而非在 DashboardWidget::paintContent 里绘制，
 * 是因为 ScaledCanvas 采用 render() 渲染内容容器，
 * 子控件需要自己绘制才能进入渲染流程。
 */
class BackgroundWidget : public QWidget
{
public:
    explicit BackgroundWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        // ---- 大屏底色 ----
        p.fillRect(rect(), Theme::background());

        // ---- 顶部系统标题条 ----
        const QRectF bar(613, 0, 826, 88);

        // 横向渐变：两端暗、中间亮
        QLinearGradient g(bar.topLeft(), bar.topRight());
        g.setColorAt(0.00, QColor(0x06, 0x09, 0x2A));
        g.setColorAt(0.30, QColor(0x14, 0x18, 0x46));
        g.setColorAt(0.50, QColor(0x1E, 0x22, 0x58));
        g.setColorAt(0.70, QColor(0x14, 0x18, 0x46));
        g.setColorAt(1.00, QColor(0x06, 0x09, 0x2A));
        p.fillRect(bar, g);

        // 底部发光分隔线
        QColor edge = Theme::accentCyan();
        edge.setAlpha(70);
        p.setPen(QPen(edge, 1.2));
        p.drawLine(bar.bottomLeft(), bar.bottomRight());

        // 两侧斜切装饰线
        p.setPen(QPen(QColor(edge.red(), edge.green(), edge.blue(), 110), 1.4));
        for (int i = 0; i < 5; ++i) {
            const qreal y = bar.top() + 10 + i * 5;
            const qreal inset = 8 + i * 7;
            p.drawLine(QPointF(bar.left() + inset, y),
                       QPointF(bar.left() + inset + 22, y));
            p.drawLine(QPointF(bar.right() - inset, y),
                       QPointF(bar.right() - inset - 22, y));
        }

        // 标题文字：多层外发光 + 深色描边 + 本体
        const QString title = QStringLiteral("可视化大屏电子看板系统");
        p.setFont(Theme::titleFont(31));
        const QRectF tr(bar.left(), 18, bar.width(), 48);

        const QColor glow(0x00, 0xDC, 0xFF);
        for (int i = 4; i >= 1; --i) {
            QColor c = glow;
            c.setAlpha(20 + (4 - i) * 6);
            p.setPen(c);
            p.drawText(tr.adjusted(-i * 0.6, -i * 0.6, i * 0.6, i * 0.6),
                       Qt::AlignCenter, title);
        }
        p.setPen(QColor(0x00, 0x00, 0x30, 200));
        p.drawText(tr.adjusted(1, 1, 1, 1), Qt::AlignCenter, title);
        p.setPen(glow);
        p.drawText(tr, Qt::AlignCenter, title);
    }
};

// namespace {

// ============================================================================

DashboardWidget::DashboardWidget(QWidget *parent)
    : ScaledCanvas(parent)
{
    buildPanels();
    m_simulator.start(3000);
    connect(&m_simulator, &DataSimulator::dataUpdated,
            this, &DashboardWidget::applyData);
    applyData();
}

DashboardWidget::~DashboardWidget() = default;

QWidget *DashboardWidget::makeHost(const QRectF &rect)
{
    // 注意：面板必须加到 content() 容器上，而不是 ScaledCanvas 本身。
    // ScaledCanvas 会对整个容器做等比缩放，若直接加到 ScaledCanvas 上，
    // 面板不会跟随缩放，在非1080p 屏幕上会挤在左上角。
    auto *w = new PanelHost(content());
    w->setGeometry(rect.toRect());
    m_hosts.append(w);
    return w;
}

TitlePanel *DashboardWidget::makeTitle(const QRectF &rect, const QString &title)
{
    auto *t = new TitlePanel(title, content());
    t->setGeometry(rect.toRect());
    return t;
}

void DashboardWidget::buildPanels()
{
    // ---- 背景与顶部标题条 ----
    // 因为 ScaledCanvas 用 render() 绘制内容容器，
    // 这里用一个铺满全画布的 BackgroundWidget 同时承担
    //「整屏底色」与「顶部系统标题条」两件事。
    auto *bg = new BackgroundWidget(content());
    bg->setGeometry(0, 0, 1920, 1080);
    bg->lower();

    // ========================================================================
    // 左栏
    // ========================================================================

    // ---- 当月计划达成率 ----
    makeTitle(R(kLeftX, 0, kLeftW, kTitleH), QStringLiteral("当月计划达成率"));
    makeTitle(R(kLeftX, 48, 170, kSubTitleH), QStringLiteral("模具达成率"));
    makeTitle(R(174, 48, 212, kSubTitleH), QStringLiteral("零件达成率"));
    makeTitle(R(392, 48, 218, kSubTitleH), QStringLiteral("零件数"));

    // 模具达成率：左侧细环 + 右侧计划/达成文本
    // 尺寸与位置按参考图实测：环直径 63px，居中于面板左侧；
    // 文本在环右侧，两者不重叠 —— 重叠会触发 Qt 的遮挡剔除，
    // 后创建且声明了 WA_OpaquePaintEvent 的控件会把先创建的整个跳过，
    // 表现为「环图完全不显示」。
    {
        auto *host = makeHost(R(kLeftX, 90, 170, 190));
        m_moldRateRing = new ProgressRing(host);
        m_moldRateRing->setGeometry(8, 63, 63, 63);
        m_moldRateStats = new StatPanel(host);
        m_moldRateStats->setGeometry(76, 62, 90, 66);
    }

    // 零件达成率：分组柱状图
    {
        auto *host = makeHost(R(174, 90, 212, 190));
        m_partRateChart = new GroupedBarChart(host);
        m_partRateChart->setGeometry(0, 0, 212, 190);
    }

    // 零件数：仪表 + 七段数码管
    {
        auto *host = makeHost(R(392, 90, 218, 190));
        m_partCountGauge = new GaugeWidget(host);
        m_partCountGauge->setGeometry(6, 2, 206, 148);
        m_partCountDigits = new SevenSegDisplay(host);
        m_partCountDigits->setGeometry(14, 142, 190, 46);
    }

    // ---- 每日工序达成数 ----
    makeTitle(R(kLeftX, 287, kLeftW, kTitleH), QStringLiteral("每日工序达成数"));
    {
        auto *host = makeHost(R(kLeftX, 328, kLeftW, 204));
        m_dailyStackChart = new StackedBarChart(host);
        m_dailyStackChart->setGeometry(0, 0, kLeftW, 204);
    }

    // ---- 模具进度 ----
    makeTitle(R(kLeftX, 531, kLeftW, kTitleH), QStringLiteral("模具进度"));
    makeTitle(R(kLeftX, 572, kLeftW, kSubTitleH), QStringLiteral("模具进度"));
    makeTitle(R(424, 572, 186, kSubTitleH), QStringLiteral("模具状态统计"));
    makeTitle(R(424, 882, 186, kSubTitleH), QStringLiteral("加工中模具数量"));

    // 模具进度表
    {
        auto *host = makeHost(R(kLeftX, 614, 420, 466));
        m_moldTable = new DataTablePanel(host);
        m_moldTable->setGeometry(0, 0, 420, 466);
        m_moldTable->setColumns({
            { QStringLiteral("序号"), 54, 0 },
            { QStringLiteral("模具编号"), 76, 1 },
            { QStringLiteral("版本号"), 64, 2 },
            { QStringLiteral("类型"), 64, 3 },
            { QStringLiteral("状态"), 72, 4 },
            { QStringLiteral("产品名称"), 90, 5 },
        });
    }

    // 模具状态统计：饼图
    {
        auto *host = makeHost(R(424, 614, 186, 264));
        m_moldStatusPie = new PieChart(host);
        m_moldStatusPie->setGeometry(0, 0, 186, 264);
    }

    // 加工中模具数量：仪表 + 数码管
    {
        auto *host = makeHost(R(424, 924, 186, 156));
        m_machiningGauge = new GaugeWidget(host);
        m_machiningGauge->setGeometry(4, 0, 178, 150);
        m_machiningDigits = new SevenSegDisplay(host);
        m_machiningDigits->setGeometry(28, 112, 130, 42);
    }

    // ========================================================================
    // 中栏
    // ========================================================================

    // 地图（顶部系统标题条由 BackgroundWidget 绘制）
    {
        auto *host = makeHost(R(kCenterX, 88, kCenterW, 446));
        m_map = new ChinaMapWidget(host);
        m_map->setGeometry(0, 0, kCenterW, 446);
        // 地图每帧动画推进时通知外层重建离屏缓存
        m_map->setFrameCallback([this]() { invalidateCache(); });
    }

    // ---- 送检一次合格率 ----
    makeTitle(R(kCenterX, 534, kCenterW, kTitleH), QStringLiteral("送检一次合格率"));
    makeTitle(R(kCenterX, 575, 413, kSubTitleH), QStringLiteral("零件合格率（15天）"));
    makeTitle(R(1028, 575, 411, kSubTitleH), QStringLiteral("电极合格率（15天）"));
    {
        auto *host = makeHost(R(kCenterX, 617, kCenterW, 217));
        m_partRateLine = new LineChart(host);
        m_partRateLine->setGeometry(0, 0, 413, 217);
        m_electrodeRateLine = new LineChart(host);
        m_electrodeRateLine->setGeometry(415, 0, 411, 217);
    }

    makeTitle(R(kCenterX, 834, 305, kTitleH), QStringLiteral("当天合格率"));
    {
        auto *host = makeHost(R(kCenterX, 875, 305, 205));
        m_todayDial = new SemicircleDial(host);
        m_todayDial->setGeometry(0, 0, 305, 205);
        m_todayDial->setRange(0, 100);
        m_todayDial->setTickCount(11);
        m_todayDial->setCaption(QStringLiteral("合格率"));
    }

    makeTitle(R(924, 834, 515, kTitleH), QStringLiteral("模具零件合格率"));
    {
        auto *host = makeHost(R(924, 875, 515, 205));
        m_moldPartRateChart = new GroupedBarChart(host);
        m_moldPartRateChart->setGeometry(0, 0, 515, 205);
    }

    // ========================================================================
    // 右栏
    // ========================================================================

    // ---- 年度产量汇总 ----
    makeTitle(R(kRightX, 0, kRightW, kTitleH), QStringLiteral("年度产量汇总"));
    makeTitle(R(kRightX, 41, 215, kSubTitleH), QStringLiteral("模具产量"));
    makeTitle(R(1661, 41, 259, kSubTitleH), QStringLiteral("每月模具产量趋势图"));
    {
        auto *host = makeHost(R(kRightX, 83, kRightW, 183));
        m_moldYieldChart = new HorizontalBarChart(host);
        m_moldYieldChart->setGeometry(0, 0, 215, 183);
        m_moldMonthlyChart = new LineChart(host);
        m_moldMonthlyChart->setGeometry(217, 0, 259, 183);
    }

    makeTitle(R(kRightX, 266, 215, kSubTitleH), QStringLiteral("零件产量"));
    makeTitle(R(1661, 266, 259, kSubTitleH), QStringLiteral("每月零件产量趋势图"));
    {
        auto *host = makeHost(R(kRightX, 308, kRightW, 182));
        m_partYieldChart = new HorizontalBarChart(host);
        m_partYieldChart->setGeometry(0, 0, 215, 182);
        m_partMonthlyChart = new LineChart(host);
        m_partMonthlyChart->setGeometry(217, 0, 259, 182);
    }

    // ---- 品质管理 ----
    makeTitle(R(kRightX, 489, kRightW, kTitleH), QStringLiteral("品质管理"));
    makeTitle(R(kRightX, 530, 181, kSubTitleH), QStringLiteral("品质占比"));
    makeTitle(R(1628, 530, 292, kSubTitleH), QStringLiteral("班组合格率"));
    {
        auto *host = makeHost(R(kRightX, 572, kRightW, 240));
        m_qualityPie = new PieChart(host);
        m_qualityPie->setGeometry(0, 0, 181, 240);
        m_teamRateChart = new GroupedBarChart(host);
        m_teamRateChart->setGeometry(184, 0, 292, 240);
    }

    makeTitle(R(kRightX, 812, kRightW, kTitleH), QStringLiteral("每日合格率统计"));
    {
        auto *host = makeHost(R(kRightX, 853, kRightW, 227));
        m_dailyRateChart = new LineChart(host);
        m_dailyRateChart->setGeometry(0, 0, kRightW, 227);
    }
}

// ============================================================================
// 数据推送
// ============================================================================

void DashboardWidget::applyData()
{
    const DashboardData &d = m_simulator.data();

    // 数据已变，离屏缓存失效，下一帧重建
    invalidateCache();

    // ---- 当月计划达成率 ----
    m_moldRateRing->setValue(d.moldPlanRate);
    m_moldRateStats->setSimpleLines({
        QStringLiteral("计划: %1").arg(d.moldPlanTarget),
        QStringLiteral("达成: %1").arg(d.moldAchieved)
    }, 13);

    {
        Chart::Config cfg;
        cfg.marginLeft = 34;
        cfg.marginTop = 20;
        cfg.marginBottom = 22;
        cfg.axisFontSize = 10;
        cfg.xLabelFontSize = 11;
        // 212px 宽放 4 组 × 2 柱，单柱只有约 21px；
        // 数值字号必须压到 9 才不会和邻柱的数值叠在一起。
        cfg.valueFontSize = 9;
        // 量程固定 0~700（与参考图刻度一致）。
        // 交给自动取整会算成 0~1000，柱子视觉上会明显变矮。
        cfg.yMin = 0;
        cfg.yMax = 700;
        cfg.yTicks = 7;
        QVector<Chart::Series> series;
        Chart::Series s1;
        s1.name = QStringLiteral("计划");
        s1.values = d.partPlan;
        s1.color = Theme::accentCyan();
        Chart::Series s2;
        s2.name = QStringLiteral("达成");
        s2.values = d.partActual;
        s2.color = Theme::accentBlue();
        series << s1 << s2;
        m_partRateChart->setData(d.partCategories, series, cfg);
    }

    // 刻度按参考图：量程 0..4100 均分 9 格（0 / 513 / 1025 / ... / 4100）
    QStringList partCountTicks;
    for (int i = 0; i < 9; ++i)
        partCountTicks << QString::number(qRound(4100.0 * i / 8.0));
    m_partCountGauge->setRange(0, 4100);
    m_partCountGauge->setTickLabels(partCountTicks);
    m_partCountGauge->setValue(d.partTotal);
    // 中心只放说明文字：数值由下方七段数码管承担，
    // 两处都写数字会与指针叠在一起糊成一团。
    m_partCountGauge->setCenterText(QStringLiteral("零件数"), QString());
    m_partCountDigits->setNumber(QString::number(d.partTotal));

    // ---- 每日工序达成数 ----
    {
        QVector<Chart::StackRow> rows;
        rows.reserve(d.dailyProcesses.size());
        for (const QVector<double> &parts : d.dailyProcesses) {
            Chart::StackRow row;
            row.parts = parts;
            row.colors = { Theme::accentCyan(), Theme::accentBlue(),
                           Theme::warnYellow() };
            rows.append(row);
        }
        Chart::Config cfg;
        cfg.marginLeft = 30;
        cfg.marginTop = 10;
        cfg.marginBottom = 20;
        cfg.axisFontSize = 10;
        cfg.xLabelFontSize = 10;
        cfg.yTicks = 4;
        cfg.showValues = false;   // 31 天密集柱，不显示数值
        m_dailyStackChart->setData(d.dayLabels, rows, cfg);
    }

    // ---- 模具进度表 ----
    {
        QVector<QStringList> rows;
        rows.reserve(d.moldRows.size());
        for (int i = 0; i < d.moldRows.size(); ++i) {
            const MoldRow &r = d.moldRows.at(i);
            rows.append({ QString::number(i + 1), r.code, r.version,
                          r.type, r.status, r.product });
        }
        m_moldTable->setRows(rows);
    }

    m_moldStatusPie->setData(d.moldStatusPie, true);

    m_machiningGauge->setRange(0, 100);
    // 参考图为 9 个主刻度（0 / 13 / 25 / 38 / 50 / 63 / 75 / 88 / 100）
    QStringList machiningTicks;
    for (int i = 0; i < 9; ++i)
        machiningTicks << QString::number(qRound(100.0 * i / 8.0));
    m_machiningGauge->setTickLabels(machiningTicks);
    m_machiningGauge->setValue(d.machiningMolds);
    // 数值交给下方数码管显示，中心只留说明文字
    m_machiningGauge->setCenterText(QStringLiteral("模具数"), QString());
    m_machiningDigits->setNumber(QString::number(d.machiningMolds).rightJustified(3, QChar('0')));

    // ---- 送检一次合格率 ----
    {
        Chart::Config cfg;
        cfg.marginLeft = 30;
        cfg.marginTop = 14;
        cfg.marginBottom = 20;
        cfg.axisFontSize = 10;
        cfg.xLabelFontSize = 10;
        cfg.yTicks = 5;
        cfg.yMin = 0;
        cfg.yMax = 120;      // 合格率可能因返工统计口径超过 100%
        cfg.showValues = false;

        Chart::Series s1;
        s1.name = QStringLiteral("零件合格率");
        s1.values = d.partRate15;
        s1.color = Theme::accentCyan();
        s1.filled = true;
        m_partRateLine->setData(d.rateLabels15, { s1 }, cfg);

        Chart::Series s2;
        s2.name = QStringLiteral("电极合格率");
        s2.values = d.electrodeRate15;
        s2.color = Theme::accentBlue();
        s2.filled = true;
        m_electrodeRateLine->setData(d.rateLabels15, { s2 }, cfg);
    }

    m_todayDial->setValue(d.todayRate);

    {
        Chart::Config cfg;
        cfg.marginLeft = 36;
        cfg.marginTop = 20;
        cfg.marginBottom = 22;
        cfg.axisFontSize = 10;
        cfg.xLabelFontSize = 10;
        cfg.valueFontSize = 12;
        cfg.yTicks = 5;

        // 单系列 + 逐柱配色：每个类别一根柱子，按合格/不合格着色
        Chart::Series s;
        s.name = QStringLiteral("合格率");
        s.values = d.moldPartRates;
        for (double v : d.moldPartRates)
            s.perBarColors.append(v >= 60 ? Theme::warnYellow() : Theme::dangerRed());

        m_moldPartRateChart->setData(d.moldPartLabels, { s }, cfg);
    }

    // ---- 年度产量汇总 ----
    {
        QVector<Chart::BarRow> rows;
        for (int i = 0; i < d.moldYieldLabels.size(); ++i) {
            Chart::BarRow a;
            a.label = d.moldYieldLabels.at(i);
            a.value = d.moldYieldA.at(i);
            a.color = Theme::accentBlue();
            rows.append(a);
            Chart::BarRow b = a;
            b.value = d.moldYieldB.at(i);
            b.color = Theme::accentCyan();
            rows.append(b);
        }
        Chart::Config cfg;
        // marginRight 是右侧数值栏的宽度，必须显式给足，
        // 否则条形顶到右边界、两位以上的数值会被裁掉。
        cfg.marginLeft = 40;
        cfg.marginRight = 42;
        cfg.marginTop = 10;
        cfg.marginBottom = 8;
        cfg.valueFontSize = 11;
        m_moldYieldChart->setData(rows, cfg);
    }
    {
        QVector<Chart::BarRow> rows;
        for (int i = 0; i < d.partYieldLabels.size(); ++i) {
            Chart::BarRow a;
            a.label = d.partYieldLabels.at(i);
            a.value = d.partYieldA.at(i);
            a.color = Theme::accentBlue();
            rows.append(a);
            Chart::BarRow b = a;
            b.value = d.partYieldB.at(i);
            b.color = Theme::accentCyan();
            rows.append(b);
        }
        Chart::Config cfg;
        cfg.marginLeft = 40;
        cfg.marginRight = 42;
        cfg.marginTop = 10;
        cfg.marginBottom = 8;
        cfg.valueFontSize = 11;
        m_partYieldChart->setData(rows, cfg);
    }

    // 每月趋势：3 条折线
    {
        Chart::Config cfg;
        cfg.marginLeft = 28;
        cfg.marginTop = 14;
        cfg.marginBottom = 20;
        cfg.axisFontSize = 9;
        cfg.xLabelFontSize = 9;
        cfg.yTicks = 5;
        cfg.showValues = false;

        QVector<Chart::Series> series;
        const QVector<QColor> cols = { Theme::warnYellow(), Theme::accentCyan(),
                                       Theme::accentBlue() };
        for (int s = 0; s < d.moldMonthly.size(); ++s) {
            Chart::Series ser;
            ser.name = QStringLiteral("系列%1").arg(s + 1);
            ser.values = d.moldMonthly.at(s);
            ser.color = cols.value(s, cols.back());
            series << ser;
        }
        m_moldMonthlyChart->setData(d.monthLabels, series, cfg);

        for (int s = 0; s < d.partMonthly.size(); ++s) {
            series[s].values = d.partMonthly.at(s);
        }
        m_partMonthlyChart->setData(d.monthLabels, series, cfg);
    }

    // ---- 品质管理 ----
    // 品质占比扇区较小，只标百分比；带名称会挤成一团（与设计稿一致）
    m_qualityPie->setShowNames(false);
    m_qualityPie->setData(d.qualityPie, true);

    {
        Chart::Config cfg;
        cfg.marginLeft = 32;
        cfg.marginTop = 20;
        cfg.marginBottom = 22;
        cfg.axisFontSize = 9;
        cfg.xLabelFontSize = 9;
        cfg.valueFontSize = 11;
        cfg.yTicks = 5;

        Chart::Series s;
        s.name = QStringLiteral("合格率");
        s.values = d.teamRates;
        for (double v : d.teamRates)
            s.perBarColors.append(v >= 60 ? Theme::warnYellow() : Theme::dangerRed());

        m_teamRateChart->setData(d.teamLabels, { s }, cfg);
    }

    {
        QStringList dayLabels;
        for (int i = 1; i <= d.dailyRate.size(); ++i)
            dayLabels.append(QString::number(i));
        Chart::Config cfg;
        cfg.marginLeft = 34;
        cfg.marginTop = 14;
        cfg.marginBottom = 20;
        cfg.axisFontSize = 10;
        cfg.xLabelFontSize = 9;
        cfg.yTicks = 6;
        cfg.showValues = false;
        QVector<Chart::Series> series;
        Chart::Series s;
        s.name = QStringLiteral("合格率");
        s.values = d.dailyRate;
        s.color = Theme::accentCyan();
        series << s;
        m_dailyRateChart->setData(dayLabels, series, cfg);
    }
}
