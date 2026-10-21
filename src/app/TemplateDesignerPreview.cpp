#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/LatestPreviewQueue.h"
#include "sleekpr/app/TemplateCanvasEditor.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/TemplateWorkspacePanel.h"
#include "sleekpr/app/TemplateTablePaginationOverlayPlanner.h"
#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"
#include "sleekpr/infrastructure/preview/TemplatePreviewJob.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <algorithm>

namespace sleekpr::app {

void TemplateDesignerWindow::setupPreviewQueue()
{
    m_previewQueue = new LatestPreviewQueue(infrastructure::PreviewExecutor::applicationInstance(), this);
    connect(m_previewQueue, &LatestPreviewQueue::resultReady, this, [this](quint64 generation, const auto& result) {
        setProperty("previewPublishedGeneration", QVariant::fromValue(generation));
        publishPreview(result);
    });
}

void TemplateDesignerWindow::hideEvent(QHideEvent* event)
{
    if (m_previewQueue) m_previewQueue->invalidate();
    m_previewDirty = true;
    QWidget::hideEvent(event);
}

void TemplateDesignerWindow::refreshPreview(bool reusePlan)
{
    ensureCurrentTemplateDocument();
    if (!m_previewLabel || !m_previewQueue) return;
    if (m_previewRefreshTimer) m_previewRefreshTimer->stop();
    m_previewDirty = true;
    if (!reusePlan) { m_previewPlanDirty = true; m_cachedPreviewPlan.reset(); }
    m_previewQueue->invalidate();
    setProperty("previewPreparing", false);
    setProperty("previewReady", false);
    if (!isVisible() || m_exitPaused) return;

    infrastructure::TemplatePreviewSnapshot snapshot;
    bool dataOk = false;
    QString error;
    snapshot.resolved.context = sampleRenderContext(&dataOk, &error);
    if (!dataOk) {
        updateSampleDataErrorLabel(error);
        m_statusLabel->setText(error);
        m_previewLabel->clear();
        return;
    }
    updateSampleDataErrorLabel({});
    const auto paper = currentPaperSizeMm();
    snapshot.resolved.document = m_settings.templateDocuments.value(m_templateKey);
    snapshot.resolved.templateKey = m_templateKey;
    snapshot.resolved.labelPlan = core::LabelRenderPlanner().createPlan(
        infrastructure::PreviewLabelFactory::createDemoLabel(core::LabelTemplateKey::Default80x30));
    snapshot.resolved.labelPlan.paperSize = core::LabelPaperSize(paper.width(), paper.height());
    snapshot.resolved.labelOffset = m_settings.labelOffset;
    snapshot.resolved.deviceProfile = core::DeviceProfileResolver::resolve(snapshot.resolved.document.deviceProfiles,
        m_deviceProfilePrinterEdit ? m_deviceProfilePrinterEdit->text() : QString());
    snapshot.designMode = true;
    snapshot.planningBudget.maxPages = m_settings.localHttpLimits.maxPreviewPages.value_or(100);
    snapshot.maxPreviewDpi = kDesignerInteractivePreviewDpi;
    snapshot.zoom = m_previewZoomFactor;
    snapshot.devicePixelRatio = std::max(1.0, m_previewLabel->devicePixelRatioF());
    if (reusePlan && !m_previewPlanDirty && m_cachedPreviewPlan) snapshot.preparedPlan = m_cachedPreviewPlan;
#if defined(SLEEKPR_ENABLE_TABLE_DESIGNER) && SLEEKPR_ENABLE_TABLE_DESIGNER
    if (const auto* table = currentTable()) snapshot.selectedTable = *table;
#endif
    m_previewDirty = false;
    setProperty("previewPreparing", true);
    const auto generation = m_previewQueue->submit([snapshot] { return infrastructure::TemplatePreviewJob::run(snapshot); });
    setProperty("previewRequestedGeneration", QVariant::fromValue(generation));
}

void TemplateDesignerWindow::publishPreview(const infrastructure::PreviewWorkResult& result)
{
    infrastructure::PerformanceScope timing(infrastructure::PerformanceStage::Publication);
    setProperty("previewPreparing", false);
    if (!result.errorCode.isEmpty() || result.image.isNull() || !result.plan) {
        m_previewLabel->clear();
        m_previewCommands.clear();
        m_previewLabel->setDesignAidCommands({});
        m_statusLabel->setText(result.errorMessage.isEmpty() ? QString::fromUtf8("预览生成失败") : result.errorMessage);
        setProperty("previewReady", false);
        return;
    }
    m_cachedPreviewPlan = result.plan;
    m_previewPlanDirty = false;
    m_previewBaseDpi = result.baseDpi;
    m_previewCommands = result.plan->pages.first().commands;
    const auto paper = QSizeF(result.plan->paperSize.widthMm(), result.plan->paperSize.heightMm());
    // 只有发布阶段创建 QPixmap 并更新控件，工作线程始终只持有不可变快照和 QImage。
    const auto pixmap = QPixmap::fromImage(result.image);
    m_previewLabel->setPixmap(pixmap);
    m_previewLabel->setRulerPaperSizeMm(paper);
    m_previewLabel->setDesignAidCommands(m_previewCommands);
    m_previewLabel->setDesignAidSelectedElementKey(currentElementId());
    m_tablePaginationOverlays.clear();
    if (result.tableLayout) {
        if (const auto* table = currentTable(); table && table->id == result.tableId) {
            m_tablePaginationOverlays = TemplateTablePaginationOverlayPlanner::planForTable(*table, *result.tableLayout);
        }
    }
    m_previewLabel->setTablePaginationOverlays(m_tablePaginationOverlays);
    const auto origin = m_previewLabel->printableImageOriginPx();
    const auto size = pixmap.deviceIndependentSize().toSize();
    const int margin = m_canvasEditor ? 24 : 0;
    m_previewLabel->setFixedSize(size.width() + origin.x() + margin, size.height() + origin.y() + margin);
    if (m_canvasEditor) m_canvasEditor->refreshOverlay();
    if (m_workspacePanel) m_workspacePanel->setPaperSummary(paper);
    for (const auto& issue : result.preflight.issues) {
        if (issue.severity == core::PrintPreflightSeverity::Error) {
            m_statusLabel->setText(QString::fromUtf8("模板校验失败：%1").arg(issue.message));
            break;
        }
    }
    setProperty("previewReady", true);
}

} // 命名空间 sleekpr::app
