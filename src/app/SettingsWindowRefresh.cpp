#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QLabel>
#include <QShowEvent>

namespace sleekpr::app {

bool SettingsWindow::hasLocalSettingsDraft() const
{
    const auto form = readGeneralSettingsForm();
    const auto& baseline = m_lastRequestedForm ? *m_lastRequestedForm : m_formBaseline;
    // 普通表单只比较少量标量，不在 GUI 的通知链中序列化整份模板配置。
    const auto& a = form.localHttpLimits;
    const auto& b = baseline.localHttpLimits;
    return form.defaultPrinter != baseline.defaultPrinter || form.allowedOrigins != baseline.allowedOrigins
        || form.labelOffset.x != baseline.labelOffset.x || form.labelOffset.y != baseline.labelOffset.y
        || a.maxHeaderBytes != b.maxHeaderBytes || a.maxContentLengthBytes != b.maxContentLengthBytes
        || a.maxPreviewBatchItems != b.maxPreviewBatchItems || a.maxPreviewPages != b.maxPreviewPages
        || a.maxPreviewResponseBytes != b.maxPreviewResponseBytes || m_modelEditRevision != m_requestedModelRevision;
}

void SettingsWindow::requestExternalRefresh(bool includeDevices)
{
    m_externalRefreshPending = true;
    m_externalDevicesPending = m_externalDevicesPending || includeDevices;
    if (!isVisible()) {
        infrastructure::ClientPerformanceMetrics::applicationMetrics().record(infrastructure::PerformanceStage::DeferredRefresh, 0);
        return;
    }
    if (hasUnsavedPersistenceChanges() || m_exitPaused) {
        m_statusLabel->setText(QString::fromUtf8("检测到外部配置更新，当前草稿已保留；保存时将检查冲突"));
        return;
    }
    const bool devices = m_externalDevicesPending;
    m_externalRefreshPending = false;
    m_externalDevicesPending = false;
    infrastructure::PerformanceScope timing(infrastructure::PerformanceStage::ExternalRefresh);
    // 模板通知不重新枚举打印机，也不把用户当前输入当成已经保存的配置。
    reloadSettings(devices);
}

void SettingsWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_externalRefreshPending) requestExternalRefresh();
    if (m_previewNeedsRender) renderPreview(m_settings);
}

} // 命名空间 sleekpr::app
