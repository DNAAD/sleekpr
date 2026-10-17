#include "sleekpr/app/TemplateWorkspaceToolbar.h"
#include "sleekpr/app/AppTheme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QString>

namespace sleekpr::app {

namespace {

QPushButton* createToolbarButton(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

} // 命名空间

TemplateWorkspaceToolbar::TemplateWorkspaceToolbar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("designerTopToolbar"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    m_saveToLibraryButton = createToolbarButton(QString::fromUtf8("保存"), QStringLiteral("saveTemplateToLibraryButton"), this);
    m_saveToLibraryButton->setProperty("buttonRole", QStringLiteral("primary"));
    m_saveToLibraryButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    m_saveToLibraryButton->setToolTip(QString::fromUtf8("保存当前模板 (Ctrl+S)"));
    m_saveToLibraryButton->setAccessibleName(QString::fromUtf8("保存当前模板"));
    m_prePrintTemplateButton = createToolbarButton(QString::fromUtf8("预打印"), QStringLiteral("prePrintCurrentTemplateButton"), this);
    m_prePrintTemplateButton->setToolTip(QString::fromUtf8("预打印当前模板"));
    m_prePrintTemplateButton->setAccessibleName(QString::fromUtf8("预打印当前模板"));
    m_importTemplateButton = createToolbarButton(QString::fromUtf8("导入"), QStringLiteral("importTemplateButton"), this);
    m_exportTemplateButton = createToolbarButton(QString::fromUtf8("导出"), QStringLiteral("exportTemplateButton"), this);
    m_undoButton = createToolbarButton(QString::fromUtf8("撤销"), QStringLiteral("designerUndoButton"), this);
    m_redoButton = createToolbarButton(QString::fromUtf8("重做"), QStringLiteral("designerRedoButton"), this);
    configureIconButton(m_importTemplateButton, QStyle::SP_DialogOpenButton, QString::fromUtf8("导入模板 (Ctrl+O)"));
    configureIconButton(m_exportTemplateButton, QStyle::SP_DialogSaveButton, QString::fromUtf8("导出模板"));
    configureIconButton(m_undoButton, QStyle::SP_ArrowBack, QString::fromUtf8("撤销 (Ctrl+Z)"));
    configureIconButton(m_redoButton, QStyle::SP_ArrowForward, QString::fromUtf8("重做 (Ctrl+Y)"));
    m_importTemplateButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    m_undoButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+Z")));
    m_redoButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+Y")));

    m_zoomCombo = new QComboBox(this);
    m_zoomCombo->setObjectName(QStringLiteral("designerZoomCombo"));
    m_zoomCombo->addItem(QStringLiteral("75%"), 0.75);
    m_zoomCombo->addItem(QStringLiteral("100%"), 1.0);
    m_zoomCombo->addItem(QStringLiteral("125%"), 1.25);
    m_zoomCombo->addItem(QStringLiteral("150%"), 1.5);
    m_zoomCombo->addItem(QStringLiteral("200%"), 2.0);
    m_zoomCombo->setCurrentIndex(m_zoomCombo->findData(1.0));
    m_zoomCombo->setFixedWidth(72);
    m_zoomCombo->setToolTip(QString::fromUtf8("画布缩放比例"));
    m_zoomCombo->setAccessibleName(QString::fromUtf8("画布缩放比例"));

    m_rulerPrecisionCombo = new QComboBox(this);
    m_rulerPrecisionCombo->setObjectName(QStringLiteral("designerRulerPrecisionCombo"));
    m_rulerPrecisionCombo->addItem(QStringLiteral("0.5 mm"), 0.5);
    m_rulerPrecisionCombo->addItem(QStringLiteral("1 mm"), 1.0);
    m_rulerPrecisionCombo->addItem(QStringLiteral("5 mm"), 5.0);
    m_rulerPrecisionCombo->setCurrentIndex(m_rulerPrecisionCombo->findData(1.0));
    m_rulerPrecisionCombo->setFixedWidth(88);
    m_rulerPrecisionCombo->setToolTip(QString::fromUtf8("标尺精度，单位毫米"));
    m_rulerPrecisionCombo->setAccessibleName(QString::fromUtf8("标尺精度"));

    m_designAidCheck = new QCheckBox(QString::fromUtf8("辅助线"), this);
    m_designAidCheck->setObjectName(QStringLiteral("designerDesignAidCheck"));
    m_designAidCheck->setChecked(false);
    m_designAidCheck->setToolTip(QString::fromUtf8("显示画布辅助线和对象尺寸"));
    m_designAidCheck->setAccessibleName(QString::fromUtf8("显示画布辅助线和对象尺寸"));

    m_paperSpecCombo = new QComboBox(this);
    m_paperSpecCombo->setObjectName(QStringLiteral("paperSpecCombo"));
    // 固定组合框宽度，避免长纸张名称把整个窗口的最小宽度撑大。
    m_paperSpecCombo->setFixedWidth(132);
    m_paperSpecCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_paperSpecCombo->setMinimumContentsLength(0);
    m_paperSpecCombo->setToolTip(QString::fromUtf8("当前模板纸张规格"));
    m_paperSpecCombo->setAccessibleName(QString::fromUtf8("纸张规格"));
    m_openPaperSpecManagerButton = createToolbarButton(
        QString::fromUtf8("管理纸张规格"),
        QStringLiteral("openPaperSpecManagerFromDesignerButton"),
        this);
    m_openFieldPresetManagerButton = createToolbarButton(
        QString::fromUtf8("管理字段预设"),
        QStringLiteral("openFieldPresetManagerFromDesignerButton"),
        this);
    configureIconButton(m_openPaperSpecManagerButton, QStyle::SP_FileDialogDetailedView, QString::fromUtf8("管理纸张规格"));
    configureIconButton(m_openFieldPresetManagerButton, QStyle::SP_FileDialogListView, QString::fromUtf8("管理字段预设"));

    layout->addWidget(m_saveToLibraryButton);
    layout->addWidget(m_prePrintTemplateButton);
    layout->addWidget(m_importTemplateButton);
    layout->addWidget(m_exportTemplateButton);
    layout->addWidget(m_undoButton);
    layout->addWidget(m_redoButton);
    layout->addWidget(new QLabel(QString::fromUtf8("缩放"), this));
    layout->addWidget(m_zoomCombo);
    layout->addWidget(new QLabel(QString::fromUtf8("标尺"), this));
    layout->addWidget(m_rulerPrecisionCombo);
    layout->addWidget(m_designAidCheck);
    layout->addStretch(1);
    layout->addWidget(new QLabel(QString::fromUtf8("纸张"), this));
    layout->addWidget(m_paperSpecCombo);
    layout->addWidget(m_openPaperSpecManagerButton);
    layout->addWidget(m_openFieldPresetManagerButton);
}

QPushButton* TemplateWorkspaceToolbar::saveToLibraryButton() const { return m_saveToLibraryButton; }
QPushButton* TemplateWorkspaceToolbar::prePrintTemplateButton() const { return m_prePrintTemplateButton; }
QPushButton* TemplateWorkspaceToolbar::importTemplateButton() const { return m_importTemplateButton; }
QPushButton* TemplateWorkspaceToolbar::exportTemplateButton() const { return m_exportTemplateButton; }
QPushButton* TemplateWorkspaceToolbar::undoButton() const { return m_undoButton; }
QPushButton* TemplateWorkspaceToolbar::redoButton() const { return m_redoButton; }
QComboBox* TemplateWorkspaceToolbar::zoomCombo() const { return m_zoomCombo; }
QComboBox* TemplateWorkspaceToolbar::rulerPrecisionCombo() const { return m_rulerPrecisionCombo; }
QCheckBox* TemplateWorkspaceToolbar::designAidCheck() const { return m_designAidCheck; }
QComboBox* TemplateWorkspaceToolbar::paperSpecCombo() const { return m_paperSpecCombo; }
QPushButton* TemplateWorkspaceToolbar::openPaperSpecManagerButton() const { return m_openPaperSpecManagerButton; }
QPushButton* TemplateWorkspaceToolbar::openFieldPresetManagerButton() const { return m_openFieldPresetManagerButton; }

} // 命名空间 sleekpr::app
