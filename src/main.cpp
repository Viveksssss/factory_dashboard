/**
 * @file main.cpp
 * @brief 应用入口
 *
 * 职责：
 *   1. 设置高 DPI 缩放策略（Qt6 默认已开启高分屏支持，此处做显式配置）
 *   2. 创建主窗口并全屏显示（无边框、覆盖任务栏）
 *   3. 注册 F11 全屏切换、Esc 退出、F 冻结动画等快捷键
 *
 * 关于高分屏：
 *   Qt6 默认 AA_EnableHighDpiScaling 已废弃且始终开启，
 *   实际缩放由 QT_SCALE_FACTOR_ROUNDING_POLICY 控制。
 *   本项目自身已通过 ScaledCanvas 做等比缩放，
 *   因此把 rounding 设为 PassThrough，避免 Qt 再做一次取整造成边缘模糊。
 */
#include <QApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QScreen>
#include <QStringList>
#include <QTimer>

#include "DashboardWidget.h"
#include "core/Theme.h"
#include "widgets/ChinaMapWidget.h"

/// 主窗口：铺满整个屏幕，隐藏标题栏
class DashboardWindow : public QMainWindow
{
public:
    explicit DashboardWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
        , m_dashboard(new DashboardWidget(this))
    {
        setCentralWidget(m_dashboard);
        // setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setStyleSheet(QStringLiteral("QMainWindow { background: %1; }")
                          .arg(Theme::background().name()));
    }

    /// 铺满当前屏幕
    void fillScreen()
    {
        // if (QScreen *sc = QGuiApplication::primaryScreen())
        //     this->setGeometry(sc->geometry());
        resize(1080,720);
        show();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Escape:
            QApplication::quit();
            return;
        case Qt::Key_F11: {
            // 全屏 / 窗口 切换
            if (isFullScreen())
                showNormal();
            else
                showFullScreen();
            return;
        }
        case Qt::Key_F: {
            // 冻结/恢复地图飞线动画，便于截图取证
            m_frozen = !m_frozen;
            if (auto *map = m_dashboard->findChild<ChinaMapWidget *>())
                map->setAnimated(!m_frozen);
            return;
        }
        case Qt::Key_R: {
            // 强制重建离屏缓存（窗口尺寸变化时缓存本身仍然有效）
            if (m_dashboard) m_dashboard->invalidateCache();
            return;
        }
        default:
            break;
        }
        QMainWindow::keyPressEvent(event);
    }

private:
    DashboardWidget *m_dashboard = nullptr;
    bool m_frozen = false;
};

int main(int argc, char *argv[])
{
    // Qt6 下禁用取整缩放，交给 ScaledCanvas 统一处理，保证图形清晰
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FactoryDashboard"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("MES"));

    DashboardWindow w;

    // ---- 截图模式：--shot <路径> -------------------------------------------
    // 以 1920x1080 显示约 0.9s（留出首帧布局与离屏缓存构建的时间），
    // 然后把整屏渲染结果保存为 PNG 并退出。
    // 用途：自动化验证与文档配图，无需人工点截图键。
    QString shotPath;
    const QStringList args = QApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QStringLiteral("--shot") && i + 1 < args.size()) {
            shotPath = args.at(i + 1);
            break;
        }
    }

    if (!shotPath.isEmpty()) {
        w.resize(1080, 720);
        w.show();
        QTimer::singleShot(900, &app, [&w, &app, shotPath]() {
            const bool ok = w.grab().save(shotPath);
            qInfo("截图%s: %s", ok ? "成功" : "失败", qPrintable(shotPath));
            app.quit();
        });
    } else {
        w.fillScreen();
    }

    return app.exec();
}
