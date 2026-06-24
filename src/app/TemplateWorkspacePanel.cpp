#include "sleekpr/app/TemplateWorkspacePanel.h"

#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/TemplateWorkspaceToolbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>

namespace sleekpr::app {

namespace {

QPushButton* createWorkspaceButton(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

} // 命名空间

TemplateWorkspacePanel::TemplateWorkspacePanel(const QString& initialSampleDataJson, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("designerWorkspacePanel"));
    auto* workspaceLayout = new QVBoxLayout(this);
    workspaceLayout->setContentsMargins(10, 10, 10, 10);
    workspaceLayout->setSpacing(8);

    m_toolbar = new TemplateWorkspaceToolbar(this);

    m_previewLabel = new TemplatePreviewLabel(this);
    m_previewLabel->setObjectName(QStringLiteral("designerPreviewLabel"));
    m_previewLabel->setMinimumSize(800, 300);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setDraggingEnabled(true);
    m_previewLabel->setRulerVisible(true);
    m_previewLabel->setRulerPrecisionMm(1.0);
    m_previewLabel->setDesignAidVisible(true);

    auto* previewScrollArea = new QScrollArea(this);
    previewScrollArea->setWidgetResizable(false);
    previewScrollArea->setAlignment(Qt::AlignCenter);
    // 预览标签保持渲染图原始比例，滚动区只承载画布，不参与排版和缩放。
    previewScrollArea->setWidget(m_previewLabel);

    auto* sampleDataTitleLabel = new QLabel(QString::fromUtf8("模拟数据"), this);
    auto* sampleDataHeaderLayout = new QHBoxLayout();
    sampleDataHeaderLayout->setContentsMargins(0, 0, 0, 0);
    m_saveSampleDataButton =
        createWorkspaceButton(QString::fromUtf8("保存模拟数据"), QStringLiteral("saveTemplateSampleDataButton"), this);
    sampleDataHeaderLayout->addWidget(sampleDataTitleLabel);
    sampleDataHeaderLayout->addStretch(1);
    sampleDataHeaderLayout->addWidget(m_saveSampleDataButton);

    m_sampleDataEdit = new QPlainTextEdit(this);
    m_sampleDataEdit->setObjectName(QStringLiteral("templateSampleDataEdit"));
    m_sampleDataEdit->setFixedHeight(124);
    m_sampleDataEdit->setPlaceholderText(QString::fromUtf8("输入 JSON，例如：{\"product_name\":\"足金串搭项链\"}"));
    m_sampleDataEdit->setPlainText(initialSampleDataJson);

    m_sampleDataErrorLabel = new QLabel(this);
    m_sampleDataErrorLabel->setObjectName(QStringLiteral("templateSampleDataErrorLabel"));
    m_sampleDataErrorLabel->setWordWrap(true);
    m_sampleDataErrorLabel->setVisible(false);

    m_statusLabel = new QLabel(QString::fromUtf8("模板设计器已就绪"), this);
    m_statusLabel->setObjectName(QStringLiteral("designerStatusLabel"));

    workspaceLayout->addWidget(m_toolbar);
    workspaceLayout->addWidget(previewScrollArea, 1);
    workspaceLayout->addLayout(sampleDataHeaderLayout);
    workspaceLayout->addWidget(m_sampleDataEdit);
    workspaceLayout->addWidget(m_sampleDataErrorLabel);
    workspaceLayout->addWidget(m_statusLabel);
}

QPushButton* TemplateWorkspacePanel::saveToLibraryButton() const { return m_toolbar->saveToLibraryButton(); }
QPushButton* TemplateWorkspacePanel::prePrintTemplateButton() const { return m_toolbar->prePrintTemplateButton(); }
QPushButton* TemplateWorkspacePanel::importTemplateButton() const { return m_toolbar->importTemplateButton(); }
QPushButton* TemplateWorkspacePanel::exportTemplateButton() const { return m_toolbar->exportTemplateButton(); }
QPushButton* TemplateWorkspacePanel::undoButton() const { return m_toolbar->undoButton(); }
QPushButton* TemplateWorkspacePanel::redoButton() const { return m_toolbar->redoButton(); }
QComboBox* TemplateWorkspacePanel::zoomCombo() const { return m_toolbar->zoomCombo(); }
QComboBox* TemplateWorkspacePanel::rulerPrecisionCombo() const { return m_toolbar->rulerPrecisionCombo(); }
QCheckBox* TemplateWorkspacePanel::designAidCheck() const { return m_toolbar->designAidCheck(); }
QComboBox* TemplateWorkspacePanel::paperSpecCombo() const { return m_toolbar->paperSpecCombo(); }
QPushButton* TemplateWorkspacePanel::openPaperSpecManagerButton() const { return m_toolbar->openPaperSpecManagerButton(); }
QPushButton* TemplateWorkspacePanel::openFieldPresetManagerButton() const { return m_toolbar->openFieldPresetManagerButton(); }
TemplatePreviewLabel* TemplateWorkspacePanel::previewLabel() const { return m_previewLabel; }
QPushButton* TemplateWorkspacePanel::saveSampleDataButton() const { return m_saveSampleDataButton; }
QPlainTextEdit* TemplateWorkspacePanel::sampleDataEdit() const { return m_sampleDataEdit; }
QLabel* TemplateWorkspacePanel::sampleDataErrorLabel() const { return m_sampleDataErrorLabel; }
QLabel* TemplateWorkspacePanel::statusLabel() const { return m_statusLabel; }

} // 命名空间 sleekpr::app
