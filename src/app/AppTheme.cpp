#include "sleekpr/app/AppTheme.h"

namespace sleekpr::app {

QString sleekprAppStyleSheet()
{
    // 主题只作用于 Qt 外壳控件，不参与标签正式预览和打印渲染链路。
    return QStringLiteral(R"qss(
QWidget {
    background: #f4f6f8;
    color: #172033;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}

QWidget#settingsWorkbenchShell,
QWidget#designerWorkbenchShell {
    background: #f4f6f8;
}

QWidget#settingsNavigationRail {
    background: #172033;
    border-radius: 8px;
}

QWidget#settingsContentPanel,
QWidget#settingsOverviewPanel,
QWidget#designerLayerPanel,
QWidget#designerLibrarySection,
QWidget#designerLayerSection,
QWidget#designerWorkspacePanel,
QWidget#designerInspectorPanel,
QWidget#designerTopToolbar {
    background: #ffffff;
    border: 1px solid #d9e0ea;
    border-radius: 6px;
}

QWidget#designerWorkspacePanel {
    background: #eef2f6;
}

QWidget#designerTopToolbar {
    border-radius: 5px;
}

QTabWidget#designerInspectorTabs::pane {
    border: 0;
    background: transparent;
}

QComboBox#designerZoomCombo {
    min-width: 76px;
}

QTabBar::tab {
    min-height: 28px;
    padding: 5px 10px;
    margin-right: 4px;
    border: 1px solid #d9e0ea;
    border-bottom: 0;
    border-top-left-radius: 5px;
    border-top-right-radius: 5px;
    background: #edf2f7;
    color: #536174;
}

QTabBar::tab:selected {
    background: #ffffff;
    color: #172033;
    font-weight: 600;
}

QSplitter::handle {
    background: #d9e0ea;
}

QSplitter::handle:horizontal {
    width: 6px;
}

QSplitter::handle:hover {
    background: #a9b7c8;
}

QLabel#settingsBrandTitle {
    background: transparent;
    color: #ffffff;
    font-size: 20px;
    font-weight: 700;
}

QLabel#settingsBrandSubtitle,
QLabel#settingsNavigationHint {
    background: transparent;
    color: #aeb9ca;
}

QLabel#settingsOverviewTitle,
QLabel#designerPanelTitle {
    background: transparent;
    color: #111827;
    font-size: 15px;
    font-weight: 700;
}

QLabel#settingsServiceStatusLabel,
QLabel#settingsPrinterStatusLabel,
QLabel#settingsTemplateStatusLabel,
QLabel#designerStatusLabel,
QLabel#paperSpecStatusLabel,
QLabel#fieldPresetStatusLabel {
    background: #eef6ff;
    border: 1px solid #cfe2ff;
    border-radius: 5px;
    color: #1d4f91;
    padding: 6px 8px;
}

QLabel#templateSampleDataErrorLabel {
    background: #fff1f2;
    border: 1px solid #fecdd3;
    border-radius: 5px;
    color: #be123c;
    padding: 6px 8px;
}

QPushButton {
    min-height: 30px;
    padding: 5px 11px;
    border: 1px solid #c8d2df;
    border-radius: 5px;
    background: #ffffff;
    color: #172033;
}

QPushButton:hover {
    background: #f1f5f9;
    border-color: #9fb0c3;
}

QPushButton[buttonRole="primary"] {
    background: #2563eb;
    border-color: #1d4ed8;
    color: #ffffff;
    font-weight: 600;
}

QPushButton[buttonRole="primary"]:hover {
    background: #1d4ed8;
}

QPushButton[buttonRole="danger"] {
    background: #fff1f2;
    border-color: #fecdd3;
    color: #be123c;
}

QPushButton[buttonRole="danger"]:hover {
    background: #ffe4e6;
}

QPushButton[buttonRole="nav"] {
    background: transparent;
    border-color: transparent;
    color: #e5edf8;
    text-align: left;
    padding-left: 12px;
}

QPushButton[buttonRole="nav"]:hover {
    background: #243145;
    border-color: #35445a;
}

QComboBox,
QLineEdit,
QPlainTextEdit,
QDoubleSpinBox,
QSpinBox {
    min-height: 28px;
    border: 1px solid #c8d2df;
    border-radius: 5px;
    background: #ffffff;
    padding: 3px 6px;
    selection-background-color: #2563eb;
}

QPlainTextEdit {
    font-family: "Cascadia Mono", "Consolas", monospace;
}

QListWidget {
    border: 1px solid #d9e0ea;
    border-radius: 6px;
    background: #ffffff;
    outline: none;
}

QListWidget::item {
    padding: 5px 7px;
    border-radius: 4px;
}

QListWidget::item:selected {
    background: #e8f0ff;
    color: #1d4ed8;
}

QGroupBox {
    border: 1px solid #d9e0ea;
    border-radius: 6px;
    margin-top: 12px;
    padding: 8px;
    background: #ffffff;
    font-weight: 600;
}

QScrollArea {
    border: 1px solid #d9e0ea;
    border-radius: 6px;
    background: #f8fafc;
}

QLabel#settingsPreviewLabel,
QLabel#designerPreviewLabel {
    background: #ffffff;
    border: 1px solid #111827;
}
)qss");
}

} // 命名空间 sleekpr::app
