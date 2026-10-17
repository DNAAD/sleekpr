// Hallmark 设计复核：理念5、层级4、实现4、场景5、克制5、差异4；原生桌面工作区。
#include "sleekpr/app/AppTheme.h"

#include <QPushButton>
#include <QApplication>
#include <QStyleFactory>
#include <QSize>

namespace sleekpr::app {

namespace {

// 所有外壳颜色集中为语义令牌，预览与打印渲染不使用此调色板。
struct ThemeColor {
    const char* token;
    const char* value;
};

constexpr ThemeColor themeColors[] = {
    {"@canvas@", "#f3f4f5"},
    {"@surface@", "#ffffff"},
    {"@text@", "#292d32"},
    {"@muted@", "#616970"},
    {"@border@", "#e1e5e9"},
    {"@borderHover@", "#adb6be"},
    {"@hover@", "#f1f4f5"},
    {"@pressed@", "#e7ebee"},
    {"@accent@", "#147d73"},
    {"@accentHover@", "#10685f"},
    {"@accentPressed@", "#0c544d"},
    {"@accentSoft@", "#f0f8f6"},
    {"@focusRing@", "#09221f"},
    {"@accentBorder@", "#b6d8d2"},
    {"@disabledText@", "#959da5"},
    {"@disabledSurface@", "#f0f2f4"},
    {"@warning@", "#93620e"},
    {"@warningSoft@", "#fff4db"},
    {"@warningBorder@", "#e9cf94"},
    {"@danger@", "#bd3039"},
    {"@dangerSoft@", "#fff0f1"},
    {"@dangerHover@", "#fbe2e4"},
    {"@dangerPressed@", "#f4cdd1"},
    {"@dangerBorder@", "#e7afb3"}
};

} // 匿名命名空间

void applySleekprTheme(QApplication& application)
{
    // Fusion 是 Qt 自带的原生控件风格，确保组合框箭头等基础控件在统一样式下完整绘制。
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    application.setStyleSheet(sleekprAppStyleSheet());
}

void configureIconButton(QPushButton* button, QStyle::StandardPixmap icon, const QString& tooltip)
{
    if (button == nullptr) {
        return;
    }
    button->setText(QString());
    if (icon == QStyle::SP_TitleBarMenuButton) {
        // 标准标题栏图标可能回退为 Qt 标识，更多菜单使用通用省略号避免误导。
        button->setIcon(QIcon());
        button->setText(QStringLiteral("..."));
    } else {
        button->setIcon(button->style()->standardIcon(icon));
    }
    button->setIconSize(QSize(16, 16));
    button->setFixedSize(30, 30);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setProperty("buttonRole", QStringLiteral("icon"));
}

QString sleekprAppStyleSheet()
{
    // 样式只管理原生界面的外观，不改变标签毫米坐标或设备像素。
    auto styleSheet = QStringLiteral(R"qss(
QWidget {
    background: @canvas@;
    color: @text@;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}
QWidget#settingsWorkbenchShell, QWidget#designerWorkbenchShell {
    background: @canvas@;
}
QWidget#settingsNavigationRail, QWidget#settingsContentPanel,
QWidget#settingsOverviewPanel, QWidget#designerLayerPanel,
QWidget#designerLibrarySection, QWidget#designerLayerSection,
QWidget#designerInspectorPanel, QWidget#designerElementListSection,
QWidget#designerObjectsSection, QWidget#designerObjectSection,
QWidget#designerElementInspectorTab, QWidget#designerTableInspectorTab,
QWidget#designerArrayGridInspectorTab, QWidget#designerDeviceInspectorTab,
QWidget#designerInsertionTools, QWidget#designerSampleDataPanel {
    background: @surface@;
    border: 0;
    border-radius: 0;
}
QWidget#designerCanvasHeader, QWidget#designerCanvasFooter,
QWidget#designerContentSection, QWidget#designerGeometrySection,
QWidget#designerElementCommonSection, QWidget#designerLayoutSection,
QWidget#designerAutoFitRangeSection,
QWidget#designerTextStyleSection, QWidget#settingsPrintPage,
QWidget#settingsSecurityPage, QWidget#settingsFooter {
    background: @surface@;
}
QWidget#designerCanvasHeader { border-bottom: 1px solid @border@; }
QWidget#designerCanvasFooter { border-top: 1px solid @border@; }
QWidget#designerCanvasViewport { background: @canvas@; }
QWidget#designerInsertionTools QPushButton {
    border-color: transparent; background: transparent;
}
QWidget#designerInsertionTools QPushButton:hover {
    background: @accentSoft@; color: @accent@;
}
QWidget#designerInsertionTools QPushButton:pressed {
    background: @pressed@;
}
QWidget#settingsNavigationRail, QWidget#designerLayerPanel {
    border-right: 1px solid @border@;
}
QWidget#designerInspectorPanel { border-left: 1px solid @border@; }
QWidget#designerWorkspacePanel { background: @canvas@; border: 0; }
QWidget#designerTopToolbar {
    background: @surface@;
    border: 0;
    border-bottom: 1px solid @border@;
    border-radius: 0;
}
QLabel { background: transparent; }
QLabel#settingsBrandTitle { font-size: 18px; font-weight: 600; color: @text@; }
QLabel#settingsBrandSubtitle, QLabel#settingsNavigationHint { color: @muted@; font-size: 12px; }
QLabel#settingsOverviewTitle { font-size: 16px; font-weight: 600; }
QLabel#designerPanelTitle { font-size: 14px; font-weight: 600; }
QLabel#settingsServiceStatusLabel, QLabel#settingsPrinterStatusLabel,
QLabel#settingsTemplateStatusLabel, QLabel#designerStatusLabel,
QLabel#paperSpecStatusLabel, QLabel#fieldPresetStatusLabel {
    background: @accentSoft@; color: @accent@;
    border: 1px solid @accentBorder@; border-radius: 4px; padding: 5px 8px;
}
QLabel#designerStatusLabel, QLabel#designerPaperSummary {
    background: transparent; border: 0; padding: 0; color: @muted@; font-size: 12px;
}
QToolButton#designerSampleDataToggle {
    border: 0; background: transparent; padding: 4px 6px;
}
QToolButton#designerSampleDataToggle:hover, QToolButton#designerSampleDataToggle:checked {
    background: @accentSoft@; color: @accent@;
}
QLabel[statusRole="warning"] {
    background: @warningSoft@; color: @warning@;
    border: 1px solid @warningBorder@; border-radius: 4px; padding: 5px 8px;
}
QLabel#templateSampleDataErrorLabel, QLabel[statusRole="danger"] {
    background: @dangerSoft@; color: @danger@;
    border: 1px solid @dangerBorder@; border-radius: 4px; padding: 5px 8px;
}
QPushButton, QToolButton {
    min-height: 20px; padding: 4px 9px;
    border: 1px solid @border@; border-radius: 4px;
    background: @surface@; color: @text@;
}
QPushButton:hover, QToolButton:hover { background: @hover@; border-color: @borderHover@; }
QPushButton:pressed, QToolButton:pressed { background: @pressed@; border-color: @muted@; }
QPushButton:checked, QToolButton:checked {
    background: @accentSoft@; border-color: @accent@; color: @accent@;
}
QPushButton[buttonRole="primary"] {
    background: @accent@; border-color: @accent@; color: @surface@; font-weight: 600;
}
QPushButton[buttonRole="primary"]:hover { background: @accentHover@; border-color: @accentHover@; }
QPushButton[buttonRole="primary"]:pressed,
QPushButton[buttonRole="primary"]:checked { background: @accentPressed@; border-color: @accentPressed@; }
QPushButton[buttonRole="danger"] {
    background: @dangerSoft@; border-color: @dangerBorder@; color: @danger@;
}
QPushButton[buttonRole="danger"]:hover { background: @dangerHover@; border-color: @danger@; }
QPushButton[buttonRole="danger"]:pressed,
QPushButton[buttonRole="danger"]:checked { background: @dangerPressed@; border-color: @danger@; }
QPushButton[buttonRole="icon"] { min-width: 0; min-height: 0; padding: 0; }
QPushButton[buttonRole="icon"][destructive="true"] { color: @danger@; }
QPushButton[buttonRole="icon"][destructive="true"]:hover { background: @dangerSoft@; border-color: @dangerBorder@; }
QPushButton[buttonRole="icon"][destructive="true"]:pressed { background: @dangerHover@; border-color: @danger@; }
QPushButton::menu-indicator { subcontrol-position: bottom right; width: 7px; height: 7px; }
QPushButton[buttonRole="nav"], QPushButton#settingsPrintNavButton, QPushButton#settingsSecurityNavButton {
    background: transparent; border-color: transparent; color: @muted@;
    text-align: left; padding: 7px 10px;
}
QPushButton[buttonRole="nav"]:hover,
QPushButton#settingsPrintNavButton:hover, QPushButton#settingsSecurityNavButton:hover {
    background: @hover@; color: @text@;
}
QPushButton[buttonRole="nav"]:checked,
QPushButton#settingsPrintNavButton:checked, QPushButton#settingsSecurityNavButton:checked {
    background: @accentSoft@; color: @accent@; border-color: @accentBorder@; font-weight: 600;
}
QPushButton[buttonRole="nav"]:pressed,
QPushButton#settingsPrintNavButton:pressed, QPushButton#settingsSecurityNavButton:pressed {
    background: @pressed@; color: @accent@;
}
QPushButton:focus, QPushButton[buttonRole="icon"]:focus,
QPushButton[buttonRole="primary"]:focus, QPushButton[buttonRole="danger"]:focus,
QPushButton[buttonRole="nav"]:focus, QToolButton:focus,
QPushButton#settingsPrintNavButton:focus, QPushButton#settingsSecurityNavButton:focus {
    border: 1px solid @accent@;
}
QPushButton:disabled, QToolButton:disabled,
QPushButton[buttonRole="primary"]:disabled, QPushButton[buttonRole="danger"]:disabled,
QPushButton[buttonRole="icon"]:disabled, QPushButton[buttonRole="nav"]:disabled,
QPushButton#settingsPrintNavButton:disabled, QPushButton#settingsSecurityNavButton:disabled {
    background: @disabledSurface@; border-color: @border@; color: @disabledText@;
}
QPushButton[buttonRole="primary"]:focus { border-color: @focusRing@; }
QComboBox, QLineEdit, QPlainTextEdit, QDoubleSpinBox, QSpinBox {
    min-height: 20px; border: 1px solid @border@; border-radius: 4px;
    background: @surface@; padding: 4px 6px;
    selection-background-color: @accent@; selection-color: @surface@;
}
QComboBox:hover, QLineEdit:hover, QPlainTextEdit:hover, QDoubleSpinBox:hover, QSpinBox:hover {
    border-color: @borderHover@;
}
QComboBox:focus, QLineEdit:focus, QPlainTextEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus {
    border-color: @accent@;
}
QComboBox:disabled, QLineEdit:disabled, QPlainTextEdit:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled {
    background: @disabledSurface@; border-color: @border@; color: @disabledText@;
}
QPlainTextEdit#templateSampleDataEdit, QPlainTextEdit[contentRole="json"] {
    font-family: "Consolas", "Cascadia Mono", monospace; font-size: 12px;
}
QComboBox { padding-right: 6px; }
QComboBox::drop-down {
    subcontrol-origin: border; subcontrol-position: top right; width: 20px;
    border-left: 1px solid @border@;
}
QComboBox::drop-down:hover { background: @hover@; }
QComboBox::drop-down:pressed { background: @pressed@; }
QComboBox::down-arrow { image: url(:/sleekpr/ui/caret-down.png); width: 10px; height: 10px; }
QComboBox#designerZoomCombo, QComboBox#designerRulerPrecisionCombo { min-width: 0; }
QSpinBox, QDoubleSpinBox { padding-right: 22px; }
QSpinBox::up-button, QDoubleSpinBox::up-button {
    subcontrol-origin: border; subcontrol-position: top right;
    width: 18px; border-left: 1px solid @border@; border-bottom: 1px solid @border@;
    background: @hover@;
}
QSpinBox::down-button, QDoubleSpinBox::down-button {
    subcontrol-origin: border; subcontrol-position: bottom right;
    width: 18px; border-left: 1px solid @border@; background: @hover@;
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background: @accentSoft@; }
QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,
QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed { background: @pressed@; }
QSpinBox::up-button:disabled, QDoubleSpinBox::up-button:disabled,
QSpinBox::down-button:disabled, QDoubleSpinBox::down-button:disabled { background: @disabledSurface@; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow,
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { width: 7px; height: 7px; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/sleekpr/ui/caret-up.png); }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/sleekpr/ui/caret-down.png); }
QListWidget, QComboBox QAbstractItemView {
    border: 1px solid @border@; border-radius: 4px; background: @surface@;
    alternate-background-color: @hover@; outline: none;
}
QListWidget::item { min-height: 20px; padding: 5px 7px; border-radius: 0; }
QListWidget::item:hover { background: @hover@; }
QListWidget::item:selected, QComboBox QAbstractItemView::item:selected {
    background: @accentSoft@; color: @accent@;
}
QListWidget::item:selected:!active { background: @pressed@; color: @text@; }
QListWidget:focus { border-color: @accent@; }
QListWidget:disabled { background: @disabledSurface@; color: @disabledText@; }
QListWidget#templateLayerList::item { min-height: 18px; padding: 3px 6px; font-size: 12px; }
QTabWidget::pane { border: 0; background: @surface@; }
QTabWidget#designerInspectorTabs::pane, QTabWidget#designerResourceTabs::pane { border: 0; }
QTabBar::tab {
    min-height: 24px; padding: 5px 10px; margin: 0;
    border: 0; border-bottom: 2px solid @border@;
    background: @surface@; color: @muted@;
}
QTabBar::tab:hover { background: @hover@; color: @text@; }
QTabBar::tab:selected { border-bottom-color: @accent@; color: @accent@; font-weight: 600; }
QTabBar::tab:disabled { color: @disabledText@; background: @disabledSurface@; }
QGroupBox {
    border: 0; border-top: 1px solid @border@; border-radius: 0;
    margin-top: 14px; padding: 12px 0 4px; background: @surface@; font-weight: 600;
}
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px 0 0; }
QGroupBox > QWidget { background: @surface@; }
QScrollArea { border: 0; background: @canvas@; }
QWidget#designerInspectorPanel QScrollArea { background: @surface@; }
QSplitter::handle { background: @border@; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical { height: 4px; }
QSplitter::handle:hover { background: @borderHover@; }
QCheckBox { background: transparent; spacing: 6px; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid @borderHover@; border-radius: 3px; background: @surface@; }
QCheckBox::indicator:checked { image: url(:/sleekpr/ui/check.png); border-color: @accent@; background: @accentSoft@; }
QCheckBox::indicator:hover { border-color: @accent@; }
QCheckBox::indicator:disabled { border-color: @border@; background: @disabledSurface@; }
QCheckBox:hover { color: @accent@; }
QCheckBox:focus { color: @accent@; }
QCheckBox:disabled { color: @disabledText@; }
QMenu {
    background: @surface@; border: 1px solid @border@; padding: 4px;
}
QMenu::item { padding: 6px 24px 6px 8px; border-radius: 4px; }
QMenu::item:selected { background: @accentSoft@; color: @accent@; }
QMenu::item:disabled { color: @disabledText@; }
QMenu::separator { height: 1px; background: @border@; margin: 4px 2px; }
QToolTip {
    background: @text@; color: @surface@; border: 0; padding: 5px 7px;
}
QLabel#settingsPreviewLabel, QLabel#designerPreviewLabel {
    background: @surface@; border: 1px solid @border@;
}
)qss");
    for (const auto& color : themeColors) {
        styleSheet.replace(QString::fromLatin1(color.token), QString::fromLatin1(color.value));
    }
    return styleSheet;
}

} // 命名空间 sleekpr::app
