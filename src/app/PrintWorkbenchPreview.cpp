#include "sleekpr/app/PrintWorkbenchWindow.h"
#include "sleekpr/app/LatestPreviewQueue.h"
#include "sleekpr/app/PrintFieldEditorPanel.h"
#include "sleekpr/app/PrintPreviewPanel.h"
#include "sleekpr/infrastructure/preview/TemplatePreviewJob.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <algorithm>

namespace sleekpr::app {

PrintWorkbenchWindow::~PrintWorkbenchWindow()
{
    m_preview->setReadyChangedHandler({});
    // 子输入控件销毁时可能失焦，先解绑语义信号，避免访问已经析构的草稿和预览状态。
    QObject::disconnect(m_fields, nullptr, this, nullptr);
    if (m_prepareQueue) {
        m_prepareQueue->invalidate();
        QObject::disconnect(m_prepareQueue, nullptr, this, nullptr);
    }
    m_prepareTimer->stop();
    m_jobCheckTimer->stop();
}

void PrintWorkbenchWindow::setupPreviewQueue()
{
    m_preview->setReadyChangedHandler([this] {
        if (m_prepared) m_preflightPending = !m_preview->hasCurrentImage() && m_preview->previewError().isEmpty();
        updatePrintState();
    });
    m_prepareQueue = new LatestPreviewQueue(infrastructure::PreviewExecutor::applicationInstance(), this);
    connect(m_prepareQueue, &LatestPreviewQueue::resultReady, this, [this](quint64 generation, const auto& result) {
        setProperty("previewPublishedGeneration", QVariant::fromValue(generation));
        publishPreparedPreview(result);
    });
}

void PrintWorkbenchWindow::prepareNow()
{
    if (m_exitPaused) { m_prepareOnShow = true; return; }
    m_prepareTimer->stop();
    m_jobCheckTimer->stop();
    m_prepareQueue->invalidate();
    m_prepared.reset();
    m_preparedSnapshot.reset();
    m_pendingSnapshot.reset();
    m_preflightReport = {};
    m_preflightPending = true;
    m_preview->clear();
    showStatus({});
    m_summary->clear();
    if (!isVisible()) { m_prepareOnShow = true; updatePrintState(); return; }
    m_prepareOnShow = false;
    const auto fail = [this](const QString& code, const QString& message) {
        m_preflightReport.issues.append({core::PrintPreflightSeverity::Error, code, message});
        m_preflightPending = false;
        updatePrintState();
    };
    if (m_activeTemplateKey.isEmpty()) { fail(QStringLiteral("TEMPLATE_NOT_SELECTED"), QString::fromUtf8("请选择可用模板")); return; }
    if (!m_loadError.isEmpty()) { fail(QStringLiteral("CONFIGURATION_ERROR"), m_loadError.trimmed()); return; }
    bool valid = true;
    QString error;
    const auto values = m_fields->values(&valid, &error);
    if (!valid) {
        const auto key = m_fields->invalidFieldKey();
        m_preflightReport.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("INVALID_VALUES"), error,
            key.isEmpty() ? core::PrintPreflightTarget::Json : core::PrintPreflightTarget::Field, key});
        m_preflightPending = false;
        updatePrintState();
        return;
    }
    infrastructure::TemplatePrintInput input{m_activeTemplateKey, values, m_presetsCombo->currentData().toString(),
        m_printers->currentData().toString(), true};
    QString code;
    m_pendingSnapshot = m_service.resolveSnapshot(m_settings, input, &code, &error);
    if (!m_pendingSnapshot) { fail(code, error); return; }
    infrastructure::TemplatePreviewSnapshot snapshot;
    snapshot.resolved = *m_pendingSnapshot;
    snapshot.renderImage = false;
    snapshot.planningBudget.maxPages = m_settings.localHttpLimits.maxPreviewPages.value_or(100);
    const auto generation = m_prepareQueue->submit([snapshot] { return infrastructure::TemplatePreviewJob::run(snapshot); });
    setProperty("previewRequestedGeneration", QVariant::fromValue(generation));
    updatePrintState();
}

void PrintWorkbenchWindow::publishPreparedPreview(const infrastructure::PreviewWorkResult& result)
{
    // 列表键含来源前缀，不等同于响应 templateKey；新鲜度由队列代号和当前选择共同保证。
    if (m_activeTemplateKey.isEmpty() || selectedTemplateKey() != m_activeTemplateKey || !m_pendingSnapshot) return;
    infrastructure::PerformanceScope timing(infrastructure::PerformanceStage::Publication);
    m_preflightPending = false;
    m_preflightReport = result.preflight;
    if (!result.errorCode.isEmpty() || !result.plan || !m_pendingSnapshot) {
        m_prepared.reset();
        m_preparedSnapshot.reset();
        m_preview->clear();
        m_summary->clear();
        if (!result.errorCode.isEmpty() && std::none_of(m_preflightReport.issues.cbegin(), m_preflightReport.issues.cend(),
                [](const auto& issue) { return issue.severity == core::PrintPreflightSeverity::Error; })) {
            m_preflightReport.issues.append({core::PrintPreflightSeverity::Error, result.errorCode, result.errorMessage});
        }
    } else {
        const auto& frozen = *m_pendingSnapshot;
        m_prepared = infrastructure::PreparedTemplatePrint{frozen.templateKey, frozen.document, *result.plan, frozen.context};
        m_preparedSnapshot = frozen;
        m_preview->setPlan(m_prepared->printPlan);
        const auto& plan = m_prepared->printPlan;
        m_summary->setText(QString::fromUtf8("%1 × %2 mm · %3 DPI").arg(plan.paperSize.widthMm()).arg(plan.paperSize.heightMm()).arg(plan.renderDpi));
    }
    updatePrintState();
}

bool PrintWorkbenchWindow::hasActivePrintSubmission() const { return m_printing; }
void PrintWorkbenchWindow::setSubmissionStateChangedCallback(std::function<void()> callback)
{
    m_submissionStateChanged = std::move(callback);
}
void PrintWorkbenchWindow::setExitPaused(bool paused)
{
    if (paused == m_exitPaused) return;
    m_exitPaused = paused;
    if (paused) {
        m_enabledBeforeExit = isEnabled(); setEnabled(false);
        // 取消退出需恢复被截断的工作；份数校验单独恢复，不为它重建基础版式和图片。
        m_prepareOnShow = m_prepareOnShow || m_prepareTimer->isActive() || (!m_prepared && m_preflightPending);
        m_resumeJobCheckAfterExit = !m_prepareOnShow && (m_resumeJobCheckAfterExit
            || m_jobCheckTimer->isActive() || (m_prepared && m_preflightPending));
        m_prepareTimer->stop(); m_jobCheckTimer->stop();
        if (m_prepareQueue) m_prepareQueue->invalidate();
    } else {
        setEnabled(m_enabledBeforeExit);
        if (m_externalRefreshPending) requestExternalRefresh();
        else if (m_prepareOnShow && isVisible()) schedulePreparation();
        else if (m_resumeJobCheckAfterExit && isVisible()) { m_resumeJobCheckAfterExit = false; scheduleJobCheck(); }
    }
}

} // 命名空间 sleekpr::app
