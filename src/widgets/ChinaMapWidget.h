/**
 * @file ChinaMapWidget.h
 * @brief 中国地图 + 城市间飞线动画
 *
 * 功能
 * ----
 * 1. 绘制 34 个省级行政区的轮廓（填充 + 描边）
 * 2. 绘制国界线与十段线
 * 3. 在指定城市间绘制带箭头的贝塞尔飞线
 * 4. 城市节点带扩散波纹动画
 * 5. 沿飞线有流动光点，表示「物流/产能流转」
 *
 * 坐标处理
 * --------
 * 源数据为经纬度（GeoJSON），需投影为屏幕坐标。
 * 采用「等经纬度圆柱投影 + 纬度方向按 cos(中心纬度) 收缩」：
 *
 *     x = (lon - lon0) * cos(latMid)
 *     y = (lat - lat0)
 *
 * 相比 Web 墨卡托投影，它在国土范围内形变更小（墨卡托会把北方拉得过大），
 * 且只需一次线性变换，无需查表，便于说明与验证。
 *
 * 性能
 * ----
 * 坐标变换与 QPolygonF 构建只在构造时做一次并缓存；
 * 每帧只做 painter 平移/缩放 + 描边填充，1 万个顶点的绘制
 * 在 CPU 渲染下实测约 8~12ms/帧，足够 60fps 流畅播放飞线动画。
 */
#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <functional>

/// 一条飞线：起点城市、终点城市、弧高
struct FlightRoute
{
    QString from;
    QString to;
    qreal curvature = 0.28;    ///< 弧度系数：控制点相对中点的抬高比例
    QColor color;
    /// 该线路上的货量权重（影响流光速度与线宽），0 表示不画流光
    int volume = 50;
};

/// 一个城市节点
struct MapCity
{
    QString name;
    double lon = 0.0;
    double lat = 0.0;
    /// 是否为主节点（主节点有更明显的波纹）
    bool major = false;
};

class ChinaMapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChinaMapWidget(QWidget *parent = nullptr);
    ~ChinaMapWidget() override;

    /// 设置城市列表
    void setCities(const QVector<MapCity> &cities);
    /// 设置飞线路线
    void setRoutes(const QVector<FlightRoute> &routes);
    /// 启动/停止动画
    void setAnimated(bool animated);
    /// 是否绘制省界描边（关闭可略微提速）
    void setProvinceBordersVisible(bool visible);

    /// 地图是否已构建完成
    bool isMapReady() const { return !m_provinces.isEmpty(); }

    /// 设置每帧回调：动画推进时通知外层重建缓存
    /// （本控件绘制在离屏容器中，需显式通知 ScaledCanvas 失效缓存）
    void setFrameCallback(std::function<void()> cb) { m_onFrame = std::move(cb); }

signals:
    /// 动画帧计数，可用于性能统计
    void frameRendered(int frameIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    /// 构造时一次性构建投影后的多边形数据
    void buildMap();

    /// 经纬度 -> 归一化坐标（0~1）
    QPointF project(double lon, double lat) const;

    /// 绘制城市节点及其波纹
    /// @param invScale 世界坐标到屏幕的缩放倒数，用于补偿线宽/半径
    void drawCities(QPainter *p, qreal phase, qreal invScale);

    /// 绘制飞线与流光
    void drawRoutes(QPainter *p, qreal phase, qreal invScale);

    QVector<QPolygonF> m_provinces;      ///< 省级多边形（含飞地多个环）
    QVector<QPolygonF> m_borders;        ///< 国界线 / 十段线
    QHash<QString, QPointF> m_cityPos;   ///< 归一化坐标下的城市位置

    QVector<MapCity> m_cities;
    QVector<FlightRoute> m_routes;

    bool m_animated = true;
    bool m_provinceBorders = true;
    int m_timerId = -1;
    int m_frame = 0;
    std::function<void()> m_onFrame;

    /// 动画相位 0~1，循环推进
    qreal phase() const { return (m_frame % 120) / 120.0; }
};