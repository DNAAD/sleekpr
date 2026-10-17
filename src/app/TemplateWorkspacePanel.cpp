#include "sleekpr/app/TemplateWorkspacePanel.h"

#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/TemplateWorkspaceToolbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolButton>
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
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    m_toolbar = new TemplateWorkspaceToolbar(this);

    auto* insertionBar = new QWidget(this);
    insertionBar->setObjectName(QStringLiteral("designerCanvasHeader"));
    m_insertionLayout = new QHBoxLayout(insertionBar);
    m_insertionLayout->setContentsMargins(12, 6, 12, 6);
    m_insertionLayout->setSpacing(6);
    m_insertionLayout->addStretch(1);
    m_fitCanvasButton = createWorkspaceButton(QString(), QStringLiteral("designerFitCanvasButton"), insertionBar);
    configureIconButton(m_fitCanvasButton, QStyle::SP_TitleBarMaxButton, QString::fromUtf8("适合画布"));
    m_fitCanvasButton->setCheckable(true);
    m_fitCanvasButton->setChecked(true);
    m_insertionLayout->addWidget(m_fitCanvasButton);

    m_previewLabel = new TemplatePreviewLabel(this);
    m_previewLabel->setObjectName(QStringLiteral("designerPreviewLabel"));
    m_previewLabel->setMinimumSize(1, 1);
    m_previewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setDraggingEnabled(true);
    m_previewLabel->setRulerVisible(true);
    m_previewLabel->setRulerPrecisionMm(1.0);
    m_previewLabel->setDesignAidVisible(false);

    auto* previewScrollArea = new QScrollArea(this);
    previewScrollArea->setObjectName(QStringLiteral("designerCanvasScrollArea"));
    previewScrollArea->setFrameShape(QFrame::NoFrame);
    previewScrollArea->setWidgetResizable(false);
    previewScrollArea->setAlignment(Qt::AlignCenter);
    m_previewViewport = previewScrollArea->viewport();
    m_previewViewport->setObjectName(QStringLiteral("designerCanvasViewport"));
    // 预览标签保持渲染图原始比例，滚动区只承载画布，不参与排版和缩放。
    previewScrollArea->setWidget(m_previewLabel);

    m_sampleDataPanel = new QWidget(this);
    m_sampleDataPanel->setObjectName(QStringLiteral("designerSampleDataPanel"));
    auto* dataLayout = new QVBoxLayout(m_sampleDataPanel);
    dataLayout->setContentsMargins(12, 8, 12, 8);
    dataLayout->setSpacing(6);
    auto* sampleDataTitleLabel = new QLabel(QStringLiteral("JSON"), m_sampleDataPanel);
    auto* sampleDataHeaderLayout = new QHBoxLayout();
    sampleDataHeaderLayout->setContentsMargins(0, 0, 0, 0);
    m_saveSampleDataButton =
        createWorkspaceButton(QString::fromUtf8("保存模拟数据"), QStringLiteral("saveTemplateSampleDataButton"), m_sampleDataPanel);
    m_saveSampleDataButton->setProperty("buttonRole", QStringLiteral("quiet"));
    sampleDataHeaderLayout->addWidget(sampleDataTitleLabel);
    sampleDataHeaderLayout->addStretch(1);
    sampleDataHeaderLayout->addWidget(m_saveSampleDataButton);

    m_sampleDataEdit = new QPlainTextEdit(m_sampleDataPanel);
    m_sampleDataEdit->setObjectName(QStringLiteral("templateSampleDataEdit"));
    m_sampleDataEdit->setFixedHeight(132);
    m_sampleDataEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_sampleDataEdit->setPlaceholderText(QStringLiteral("{}"));
    m_sampleDataEdit->setPlainText(initialSampleDataJson);

    m_sampleDataErrorLabel = new QLabel(m_sampleDataPanel);
    m_sampleDataErrorLabel->setObjectName(QStringLiteral("templateSampleDataErrorLabel"));
    m_sampleDataErrorLabel->setWordWrap(true);
    m_sampleDataErrorLabel->setVisible(false);

    dataLayout->addLayout(sampleDataHeaderLayout);
    dataLayout->addWidget(m_sampleDataEdit);
    dataLayout->addWidget(m_sampleDataErrorLabel);
    m_sampleDataPanel->hide();

    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("designerCanvasFooter"));
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(10, 4, 12, 4);
    footerLayout->setSpacing(12);
    m_sampleDataToggle = new QToolButton(footer);
    m_sampleDataToggle->setObjectName(QStringLiteral("designerSampleDataToggle"));
    m_sampleDataToggle->setText(QString::fromUtf8("模拟数据"));
    m_sampleDataToggle->setCheckable(true);
    m_sampleDataToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_sampleDataToggle->setArrowType(Qt::RightArrow);
    m_sampleDataToggle->setToolTip(QString::fromUtf8("展开模拟数据"));
    connect(m_sampleDataToggle, &QToolButton::toggled, this, [this](bool expanded) {
        m_sampleDataPanel->setVisible(expanded);
        m_sampleDataToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        m_sampleDataToggle->setToolTip(expanded ? QString::fromUtf8("收起模拟数据") : QString::fromUtf8("展开模拟数据"));
    });
    m_statusLabel = new QLabel(QString::fromUtf8("就绪"), footer);
    m_statusLabel->setObjectName(QStringLiteral("designerStatusLabel"));
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_statusLabel->setMinimumWidth(0);
    m_paperSummaryLabel = new QLabel(footer);
    m_paperSummaryLabel->setObjectName(QStringLiteral("designerPaperSummary"));
    footerLayout->addWidget(m_sampleDataToggle);
    footerLayout->addWidget(m_statusLabel, 1);
    footerLayout->addWidget(m_paperSummaryLabel);

    workspaceLayout->addWidget(m_toolbar);
    workspaceLayout->addWidget(insertionBar);
    workspaceLayout->addWidget(previewScrollArea, 1);
    workspaceLayout->addWidget(m_sampleDataPanel);
    workspaceLayout->addWidget(footer);
}

QWidget* TemplateWorkspacePanel::toolbarWidget() const
{
    layout()->removeWidget(m_toolbar);
    return m_toolbar;
}

void TemplateWorkspacePanel::setInsertionTools(QWidget* tools)
{
    if (tools != nullptr) {
        if (auto* parent = tools->parentWidget(); parent != nullptr && parent->layout() != nullptr) {
            parent->layout()->removeWidget(tools);
        }
        m_insertionLayout->insertWidget(0, tools);
        tools->show();
    }
}

void TemplateWorkspacePanel::setSampleDataExpanded(bool expanded)
{
    m_sampleDataToggle->setChecked(expanded);
}

QSize TemplateWorkspacePanel::previewViewportSize() const { return m_previewViewport->size(); }
QWidget* TemplateWorkspacePanel::previewViewportWidget() const { return m_previewViewport; }
QPushButton* TemplateWorkspacePanel::fitCanvasButton() const { return m_fitCanvasButton; }

void TemplateWorkspacePanel::setPaperSummary(QSizeF paperSizeMm)
{
    m_paperSummaryLabel->setText(QStringLiteral("%1 x %2 mm")
                                   .arg(paperSizeMm.width(), 0, 'g', 4)
                                   .arg(paperSizeMm.height(), 0, 'g', 4));
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
