#include "sleekpr/app/TemplateWorkspaceToolbar.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_saveToLibraryButton = createToolbarButton(QString::fromUtf8("保存"), QStringLiteral("saveTemplateToLibraryButton"), this);
    m_saveToLibraryButton->setProperty("buttonRole", QStringLiteral("primary"));
    m_prePrintTemplateButton = createToolbarButton(QString::fromUtf8("预打印"), QStringLiteral("prePrintCurrentTemplateButton"), this);
    m_prePrintTemplateButton->setProperty("buttonRole", QStringLiteral("primary"));
    m_importTemplateButton = createToolbarButton(QString::fromUtf8("导入"), QStringLiteral("importTemplateButton"), this);
    m_exportTemplateButton = createToolbarButton(QString::fromUtf8("导出"), QStringLiteral("exportTemplateButton"), this);
    m_undoButton = createToolbarButton(QString::fromUtf8("撤销"), QStringLiteral("designerUndoButton"), this);
    m_redoButton = createToolbarButton(QString::fromUtf8("重做"), QStringLiteral("designerRedoButton"), this);

    m_zoomCombo = new QComboBox(this);
    m_zoomCombo->setObjectName(QStringLiteral("designerZoomCombo"));
    m_zoomCombo->addItem(QStringLiteral("75%"), 0.75);
    m_zoomCombo->addItem(QStringLiteral("100%"), 1.0);
    m_zoomCombo->addItem(QStringLiteral("125%"), 1.25);
    m_zoomCombo->addItem(QStringLiteral("150%"), 1.5);
    m_zoomCombo->addItem(QStringLiteral("200%"), 2.0);
    m_zoomCombo->setCurrentIndex(m_zoomCombo->findData(1.0));

    m_rulerPrecisionCombo = new QComboBox(this);
    m_rulerPrecisionCombo->setObjectName(QStringLiteral("designerRulerPrecisionCombo"));
    m_rulerPrecisionCombo->addItem(QStringLiteral("0.5 mm"), 0.5);
    m_rulerPrecisionCombo->addItem(QStringLiteral("1 mm"), 1.0);
    m_rulerPrecisionCombo->addItem(QStringLiteral("5 mm"), 5.0);
    m_rulerPrecisionCombo->setCurrentIndex(m_rulerPrecisionCombo->findData(1.0));

    m_designAidCheck = new QCheckBox(QString::fromUtf8("辅助线/尺寸"), this);
    m_designAidCheck->setObjectName(QStringLiteral("designerDesignAidCheck"));
    m_designAidCheck->setChecked(true);

    m_paperSpecCombo = new QComboBox(this);
    m_paperSpecCombo->setObjectName(QStringLiteral("paperSpecCombo"));
    m_openPaperSpecManagerButton = createToolbarButton(
        QString::fromUtf8("管理纸张规格"),
        QStringLiteral("openPaperSpecManagerFromDesignerButton"),
        this);
    m_openFieldPresetManagerButton = createToolbarButton(
        QString::fromUtf8("管理字段预设"),
        QStringLiteral("openFieldPresetManagerFromDesignerButton"),
        this);

    layout->addWidget(m_saveToLibraryButton);
    layout->addWidget(m_prePrintTemplateButton);
    layout->addWidget(m_importTemplateButton);
    layout->addWidget(m_exportTemplateButton);
    layout->addWidget(m_undoButton);
    layout->addWidget(m_redoButton);
    layout->addWidget(new QLabel(QString::fromUtf8("缩放"), this));
    layout->addWidget(m_zoomCombo);
    layout->addWidget(new QLabel(QString::fromUtf8("标尺精度"), this));
    layout->addWidget(m_rulerPrecisionCombo);
    layout->addWidget(m_designAidCheck);
    layout->addStretch(1);
    layout->addWidget(new QLabel(QString::fromUtf8("纸张规格"), this));
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
