/**
 * @file ScaledCanvas.h
 * @brief 以 1920x1080 为设计分辨率的等比缩放画布
 *
 * 问题背景
 * --------
 * 大屏项目要在 1920x1080 / 2560x1440 / 3840x2160 等多种分辨率的
 * 显示器上显示。若按实际像素硬编码布局，换个分辨率就会错位；
 * 若改用布局管理器自适应，各面板宽高比会变化，
 * 圆形仪表会被拉成椭圆，图表比例也会失调。
 *
 * 解决方案
 * --------
 * 采用「设计分辨率 + 离屏渲染 + 等比缩放」：
 *   1. 所有面板按 1920x1080 设计坐标摆放在离屏容器中；
 *   2. 把容器渲染到一张 1920x1080 的 QImage（离屏缓存）；
 *   3. 绘制时按 min(w/1920, h/1080) 求缩放系数 s，
 *      把缓存图等比缩放后居中贴到屏幕上，余下留黑边（letterbox）。
 *
 * 为什么用离屏容器而不是直接挂在窗口上？
 * -------------------------------------
 * 若把面板作为窗口的真实子控件，Qt 会在 paintEvent 返回后
 * 额外用原生几何再画一遍，造成「缩放版 + 未缩放版」双重绘制，
 * 在非 1080p 屏幕上会看到重叠残影。
 * 用 WA_DontShowOnScreen 的离屏容器可彻底绕开 Qt 的自动绘制流程，
 * 它只在我们显式调用 render() 时才被渲染一次。
 *
 * 为什么用离屏而不是「逐个 setGeometry 缩放」？
 * ---------------------------------------------
 * 那样文字字号与线宽不会跟着放大，4K 屏上会显得又小又细；
 * 离屏渲染时 painter 已带缩放变换，
 * 子控件的字号、线宽、渐变端点全部被等比放大，正是大屏需要的效果。
 *
 * 缓存策略
 * --------
 * 缓存图只在「内容真正变化」时重建：
 *   - 窗口尺寸变化：无需重建（缩放由 drawImage 阶段完成）
 *   - 数据刷新 / 动画帧：调用 invalidateCache() 标记失效
 * 未失效时直接复用上一帧的 QImage，避免每帧重绘 20+ 面板。
 */
#pragma once

#include <QImage>
#include <QPointer>
#include <QWidget>

class ScaledCanvas : public QWidget
{
    Q_OBJECT
public:
    /// 设计基准分辨率
    static constexpr int kDesignWidth = 1920;
    static constexpr int kDesignHeight = 1080;

    explicit ScaledCanvas(QWidget *parent = nullptr);
    ~ScaledCanvas() override;

    qreal scaleFactor() const { return m_scale; }

    /// 承载全部面板的离屏容器（请把面板加到这个容器上，而非 ScaledCanvas）
    QWidget *content() const { return m_content; }

    /// 标记离屏缓存失效，下一帧重建
    void invalidateCache();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void recalcTransform();
    /// 重建离屏缓存图
    void rebuildCache();

    qreal m_scale = 1.0;
    QPoint m_offset;

    /// 离屏容器，固定为设计尺寸，永远不显示在屏幕上
    QPointer<QWidget> m_content;
    QImage m_cache;
    bool m_cacheDirty = true;
};