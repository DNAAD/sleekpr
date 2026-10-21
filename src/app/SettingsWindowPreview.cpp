#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/app/LatestPreviewQueue.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/labels/TemplateOverrideKey.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"
#include "sleekpr/infrastructure/preview/TemplatePreviewJob.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QHideEvent>
#include <QVariant>
#include <algorithm>

namespace sleekpr::app {

SettingsWindow::~SettingsWindow()
{
    if (m_persistenceService) {
        QObject::disconnect(m_persistenceService, nullptr, this, nullptr);
        m_persistenceService->detachSession(m_persistenceSession);
    }
    if (m_previewQueue) m_previewQueue->invalidate();
    // 先解绑子控件失焦及任务完成信号，再让文档成员和控件按各自顺序析构。
    for (auto* child : findChildren<QObject*>()) QObject::disconnect(child, nullptr, this, nullptr);
}

void SettingsWindow::setupPreviewQueue()
{
    m_previewQueue = new LatestPreviewQueue(infrastructure::PreviewExecutor::applicationInstance(), this);
    connect(m_previewQueue, &LatestPreviewQueue::resultReady, this, [this](quint64 generation, const auto& result) {
        infrastructure::PerformanceScope timing(infrastructure::PerformanceStage::Publication);
        setProperty("previewPreparing", false);
        setProperty("previewPublishedGeneration", QVariant::fromValue(generation));
        if (!result.errorCode.isEmpty() || result.image.isNull()) {
            m_previewImageSize = {};
            m_previewLabel->clear();
            m_previewLabel->setText(result.errorMessage.isEmpty() ? QString::fromUtf8("预览图生成失败") : result.errorMessage);
            setProperty("previewReady", false);
        } else {
            const auto pixmap = QPixmap::fromImage(result.image);
            m_previewImageSize = pixmap.deviceIndependentSize().toSize();
            m_previewLabel->setPixmap(pixmap);
            m_previewLabel->setFixedSize(m_previewImageSize);
            setProperty("previewReady", true);
            fitPreviewToViewport();
        }
        updatePreviewDraggingState();
    });
}

void SettingsWindow::hideEvent(QHideEvent* event)
{
    if (m_previewQueue) m_previewQueue->invalidate();
    m_previewNeedsRender = true;
    QWidget::hideEvent(event);
}

void SettingsWindow::renderPreview(const core::PrintClientSettings& settings)
{
    m_previewNeedsRender = true;
    if (m_previewQueue) m_previewQueue->invalidate();
    setProperty("previewReady", false);
    if (!m_previewQueue || !isVisible() || m_exitPaused) return;
    infrastructure::TemplatePreviewSnapshot snapshot;
    snapshot.planningBudget.maxPages = settings.localHttpLimits.maxPreviewPages.value_or(100);
    const auto key = currentTemplateOverrideKey();
    std::optional<core::TemplateDocument> document;
    if (settings.templateDocuments.contains(key)) document = settings.templateDocuments.value(key);
    else if (m_libraryTemplateDocuments.contains(key)) document = m_libraryTemplateDocuments.value(key);
    if (document) {
        // 选择的库文档已在 GUI 读取；放入本次值快照，后台只规划此版本，不能重新查库。
        auto frozenSettings = settings;
        frozenSettings.templateDocuments[key] = *document;
        infrastructure::TemplatePrintInput input{key, document->sampleData, {}, settings.defaultPrinter, false};
        QString code, error;
        const auto resolved = infrastructure::TemplatePrintService(m_settingsPath).resolveSnapshot(frozenSettings, input, &code, &error);
        if (!resolved) {
            m_previewLabel->clear();
            m_previewLabel->setText(error);
            return;
        }
        snapshot.resolved = *resolved;
        snapshot.designMode = true;
    } else {
        snapshot.resolved.labelPlan = core::LabelRenderPlanner().createPlan(
            infrastructure::PreviewLabelFactory::createDemoLabel(currentLabelTemplateKey()));
        snapshot.resolved.templateKey = core::templateOverrideKey(snapshot.resolved.labelPlan.templateKey);
        snapshot.resolved.labelOffset = settings.labelOffset;
        snapshot.legacySettings = settings;
    }
    snapshot.devicePixelRatio = std::max(1.0, m_previewLabel->devicePixelRatioF());
    m_previewNeedsRender = false;
    setProperty("previewPreparing", true);
    const auto generation = m_previewQueue->submit([snapshot] { return infrastructure::TemplatePreviewJob::run(snapshot); });
    setProperty("previewRequestedGeneration", QVariant::fromValue(generation));
}

} // 命名空间 sleekpr::app
