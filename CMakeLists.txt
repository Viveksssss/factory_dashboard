# ============================================================================
#  FactoryDashboard —— 基于 Qt 6 的工业数据可视化大屏
#
#  构建：
#    cmake -B build -DCMAKE_PREFIX_PATH=<Qt6 安装目录>
#    cmake --build build --config Release
#
#  Qt 版本要求：6.2 及以上（用到 QRandomGenerator::globalInstance）
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(FactoryDashboard
    VERSION 1.0.0
    DESCRIPTION "基于 Qt6 的工业数据可视化大屏系统"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# MSVC 专用：统一步进，避免 /W3 与 /W4 差异带来的告警噪声
if(MSVC)
    add_compile_options(/utf-8 /W3)
    # 发布版不要生成独立的 PDB（减少产物数量）
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
endif()

# 关闭 Qt 一些与本项目无关的模块，加快构建
set(QT_FEATURE_standard_libs ON)
set(QT_NO_CREATE_VERSIONLESS_TARGETS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Core Gui)

# ---------------------------------------------------------------------------
# 可执行目标
# ---------------------------------------------------------------------------
qt_add_executable(FactoryDashboard
    WIN32                       # Windows 下不弹控制台窗口

    src/main.cpp
    src/DashboardWidget.cpp
    src/DashboardWidget.h

    src/core/Theme.cpp
    src/core/Theme.h
    src/core/ScaledCanvas.cpp
    src/core/ScaledCanvas.h

    src/data/DashboardData.cpp
    src/data/DashboardData.h
    src/data/china_map_data.h    # 自动生成的坐标表，仅头文件

    src/widgets/ChartWidgets_Bar.cpp
    src/widgets/ChartWidgets_Bar2.cpp
    src/widgets/ChartWidgets_Line.cpp
    src/widgets/ChartWidgets_Basic.cpp
    src/widgets/ChartWidgets.h
    src/widgets/ChartTypes.h
    src/widgets/GaugeWidgets.cpp
    src/widgets/GaugeWidgets.h
    src/widgets/ChinaMapWidget.cpp
    src/widgets/ChinaMapWidget.h
    src/widgets/PanelWidgets.cpp
    src/widgets/PanelWidgets.h
)

target_include_directories(FactoryDashboard PRIVATE src)

target_link_libraries(FactoryDashboard PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
)

# Windows 下设置为 PerMonitorV2 才能拿到正确的屏幕分辨率
if(WIN32)
    set_target_properties(FactoryDashboard PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# ---------------------------------------------------------------------------
# 安装
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS FactoryDashboard
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---------------------------------------------------------------------------
# 提示信息
# ---------------------------------------------------------------------------
message(STATUS "--------------------------------------------------------")
message(STATUS " FactoryDashboard ${PROJECT_VERSION}")
message(STATUS " Qt version:  ${Qt6_VERSION}")
message(STATUS " Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS " Design size:  1920 x 1080 (等比缩放适配实际屏幕)")
message(STATUS "--------------------------------------------------------")