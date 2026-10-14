#include "ScaledCanvas.h"

#include "Theme.h"

#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

ScaledCanvas::ScaledCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::NoFocus);

    // 关键：离屏容器不作为本窗口的子控件显示。
    // WA_DontShowOnScreen 让它不参与窗口的子控件绘制流程，
    // 因此不会出现「缩放版 + 未缩放版」的双重绘制。
    // 我们只通过 render() 显式把它画进缓存图。
    m_content = new QWidget(nullptr);
    m_content->setAttribute(Qt::WA_DontShowOnScreen, true);

    // 离屏容器没有父控件，调色板是系统默认的浅色（#F0F0F0）。
    // render() 带 DrawWindowBackground 时会用这个颜色铺满整张缓存图，
    // 任何没被面板画满的地方都会露出浅灰。这里统一改成大屏底色，
    // 保证即使某个面板漏画也不会出现刺眼的浅色块。
    QPalette cpal = m_content->palette();
    cpal.setColor(QPalette::Window, Theme::background());
    m_content->setPalette(cpal);

    m_content->setFixedSize(kDesignWidth, kDesignHeight);
}

ScaledCanvas::~ScaledCanvas()
{
    // 容器不是窗口的子控件，需要手动释放
    if (m_content)
        delete m_content.data();
}

void ScaledCanvas::invalidateCache()
{
    m_cacheDirty = true;
    update();
}

void ScaledCanvas::recalcTransform()
{
    if (width() <= 0 || height() <= 0) {
        m_scale = 1.0;
        m_offset = QPoint(0, 0);
        return;
    }
    // 等比缩放，取宽高两个方向的较小值，保证内容完整可见（letterbox）
    const qreal sx = qreal(width()) / kDesignWidth;
    const qreal sy = qreal(height()) / kDesignHeight;
    m_scale = qMin(sx, sy);
    m_offset = QPoint(qRound((width() - kDesignWidth * m_scale) / 2.0),
                      qRound((height() - kDesignHeight * m_scale) / 2.0));
}

void ScaledCanvas::rebuildCache()
{
    if (!m_content)
        return;
    if (m_cache.size() != QSize(kDesignWidth, kDesignHeight))
        m_cache = QImage(kDesignWidth, kDesignHeight, QImage::Format_ARGB32_Premultiplied);
    m_cache.fill(Qt::transparent);

    QPainter p(&m_cache);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 把离屏容器及其全部子控件渲染进缓存图
    m_content->render(&p, QPoint(0, 0), QRegion(),
                      QWidget::DrawWindowBackground | QWidget::DrawChildren);
    p.end();

    m_cacheDirty = false;
}

void ScaledCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    recalcTransform();
    // 窗口尺寸变化只影响贴图阶段的几何，不需要重建缓存
    update();
}

void ScaledCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // 填充整屏底色（覆盖 letterbox 黑边区域）
    painter.fillRect(rect(), Theme::background());

    if (!m_content)
        return;

    // 缓存失效时重建（含动画触发的每帧）
    if (m_cacheDirty)
        rebuildCache();
    if (m_cache.isNull())
        return;

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 等比缩放后居中贴图
    const QRectF target(m_offset.x(), m_offset.y(),
                        kDesignWidth * m_scale, kDesignHeight * m_scale);
    painter.drawImage(target, m_cache, QRectF(m_cache.rect()));
}