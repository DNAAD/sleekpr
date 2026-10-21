#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/app/SettingsPersistenceNotice.h"
#include "sleekpr/app/LatestPreviewQueue.h"
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QMessageBox>

namespace sleekpr::app {
void SettingsWindow::setupPersistence()
{
    const auto id = m_persistenceService->openSession(QString::fromUtf8("设置"));
    if (!id) { setEnabled(false); m_persistenceStateLabel->setText(QString::fromUtf8("程序正在退出")); return; }
    m_persistenceSession = *id;
    connect(m_persistenceService, &SettingsPersistenceService::sessionChanged, this, [this](auto changed) {
        if (changed == m_persistenceSession) refreshPersistenceState();
    });
    const auto edited = [this] { if (!m_loadingForm) refreshPersistenceState(); };
    connect(m_printerCombo, &QComboBox::currentIndexChanged, this, edited);
    connect(m_allowedOriginsEdit, &QPlainTextEdit::textChanged, this, edited);
    for (auto* spin : {m_offsetXSpin, m_offsetYSpin}) connect(spin, &QDoubleSpinBox::valueChanged, this, edited);
    for (auto* spin : {m_httpMaxHeaderKbSpin, m_httpMaxBodyMbSpin, m_httpMaxPreviewBatchItemsSpin,
             m_httpMaxPreviewPagesSpin, m_httpMaxPreviewResponseMbSpin}) connect(spin, &QSpinBox::valueChanged, this, edited);
    refreshPersistenceState();
}

void SettingsWindow::refreshPersistenceState()
{
    if (!m_persistenceService) return;
    const auto view = m_persistenceService->view(m_persistenceSession);
    setProperty("persistenceReady", view.phase != SettingsSavePhase::Loading);
    setProperty("settingsSavePhase", int(view.phase));
    const bool loading = view.phase == SettingsSavePhase::Loading;
    if (loading && !m_persistenceLoading) {
        m_persistenceWasEnabled = m_exitPaused ? m_enabledBeforeExit : isEnabled();
        m_persistenceLoading = true; setEnabled(false);
    } else if (!loading && m_persistenceLoading) {
        m_persistenceLoading = false;
        if (m_exitPaused) m_enabledBeforeExit = m_persistenceWasEnabled;
        else setEnabled(m_persistenceWasEnabled);
    }
    if (!loading) m_persistenceInitialized = true;
    if (view.lastOutcome && view.outcomeSerial != m_persistenceOutcomeSerial) {
        m_persistenceOutcomeSerial = view.outcomeSerial;
        const auto& outcome = *view.lastOutcome;
        const auto& result = outcome.storage;
        m_expectedSettings = result.expectedSettings;
        if (result.baselineReset && result.operation != infrastructure::SettingsStorageOperation::Save) {
            m_saveContexts.clear(); m_lastRequestedForm.reset(); m_modelEditRevision = m_requestedModelRevision = 0;
            applyPersistenceSnapshot(result.observed, m_pendingDeviceRefresh); m_pendingDeviceRefresh = false;
        } else if (result.writeResult) {
            setProperty("settingsSaveStatus", int(result.writeResult->status));
            if (!result.writeResult->ok()) {
                m_persistenceNotice->showFailure(*result.writeResult);
                m_statusLabel->setText(QString::fromUtf8("未保存，当前草稿已保留"));
            } else {
                m_persistenceNotice->clear();
                if (outcome.ticket && m_saveContexts.contains(outcome.ticket->revision)) {
                    const auto context = m_saveContexts.take(outcome.ticket->revision);
                    // 推进显示基线但不写控件，用户在途输入回原值仍然是一份真实新草稿。
                    m_formBaseline = context.form;
                    if (context.runtimeSequence == m_runtimeApplySequence && m_onSettingsApplied)
                        m_onSettingsApplied(result.observed.settings);
                }
                // 累计目标成功后，之前失败或被合并的票据均已被取代，不得继续占据上下文槽位。
                if (outcome.ticket) {
                    for (auto it = m_saveContexts.begin(); it != m_saveContexts.end() && it.key() <= outcome.ticket->revision;)
                        it = m_saveContexts.erase(it);
                }
            }
        } else {
            if (result.operation == infrastructure::SettingsStorageOperation::Open)
                m_formBaseline = readGeneralSettingsForm();
            m_persistenceNotice->showFailure({core::SettingsWriteStatus::ReadFailed, result.observed, {}, result.observed.message});
        }
    }
    QString text;
    switch (view.phase) {
    case SettingsSavePhase::Loading: text = QString::fromUtf8("正在读取配置"); break;
    case SettingsSavePhase::Saving: text = QString::fromUtf8("正在保存"); break;
    case SettingsSavePhase::Queued: text = QString::fromUtf8("待保存"); break;
    case SettingsSavePhase::Failed: text = QString::fromUtf8("保存失败，草稿已保留"); break;
    case SettingsSavePhase::Conflict: text = QString::fromUtf8("存在冲突，保存已暂停"); break;
    case SettingsSavePhase::Saved: text = hasLocalSettingsDraft() ? QString::fromUtf8("未保存") : QString::fromUtf8("已保存"); break;
    }
    m_persistenceStateLabel->setText(text);
}

void SettingsWindow::applyWithoutSaving()
{
    if (m_exitPaused) return;
    collectGeneralSettings(); ++m_runtimeApplySequence;
    renderPreview(m_settings); refreshOverview();
    if (m_onSettingsApplied) m_onSettingsApplied(m_settings);
    m_statusLabel->setText(QString::fromUtf8("已应用，尚未保存"));
    refreshPersistenceState();
}
void SettingsWindow::saveSettings()
{
    if (!m_persistenceService) return;
    flushPersistenceEdits();
    const auto form = readGeneralSettingsForm();
    const auto ticket = m_persistenceService->requestSave(m_persistenceSession, m_settings, core::SettingsEditScope::AllChangedFields);
    if (!ticket) { m_statusLabel->setText(QString::fromUtf8("当前暂不能保存，草稿仍保留")); return; }
    m_saveContexts.insert(ticket->revision, SaveContext{form, m_runtimeApplySequence, m_modelEditRevision});
    m_lastRequestedForm = form; m_requestedModelRevision = m_modelEditRevision;
    // 同会话最多保留进行中和最新待处理上下文，合并请求不能造成票据历史无限增长。
    const auto running = m_persistenceService->view(m_persistenceSession).runningTicket;
    for (auto it = m_saveContexts.begin(); it != m_saveContexts.end();) {
        if (it.key() != ticket->revision && (!running || it.key() != running->revision)) it = m_saveContexts.erase(it);
        else ++it;
    }
    refreshPersistenceState();
}
void SettingsWindow::confirmPersistenceReload()
{
    if (QMessageBox::question(this, QString::fromUtf8("重新读取配置"), QString::fromUtf8("确认放弃当前未保存草稿并重新读取？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) reloadFromDisk();
}
bool SettingsWindow::hasUnsavedPersistenceChanges() const
{
    if (!m_persistenceService) return hasLocalSettingsDraft();
    const auto view = m_persistenceService->view(m_persistenceSession);
    return hasLocalSettingsDraft() || view.running || view.pending || view.phase != SettingsSavePhase::Saved;
}
void SettingsWindow::flushPersistenceEdits()
{
    for (auto* spin : findChildren<QAbstractSpinBox*>()) spin->interpretText();
    collectGeneralSettings();
}
void SettingsWindow::saveForExit() { saveSettings(); }
void SettingsWindow::setExitPaused(bool paused)
{
    if (m_exitPaused == paused) return;
    m_exitPaused = paused;
    if (paused) {
        m_enabledBeforeExit = m_persistenceLoading ? m_persistenceWasEnabled : isEnabled();
        setEnabled(false); if (m_previewQueue) m_previewQueue->invalidate();
    } else {
        setEnabled(!m_persistenceLoading && m_enabledBeforeExit);
        if (isVisible() && !m_persistenceLoading && m_previewNeedsRender) renderPreview(m_settings);
    }
}
ExitDraftState SettingsWindow::exitDraftState() const
{
    ExitDraftState result;
    result.manualDraft = m_persistenceInitialized && !m_persistenceLoading && hasLocalSettingsDraft();
    return result;
}
} // 命名空间 sleekpr::app
