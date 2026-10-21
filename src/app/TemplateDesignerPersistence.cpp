#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/SettingsPersistenceNotice.h"
#include "sleekpr/app/LatestPreviewQueue.h"
#include <QMessageBox>
#include <QLabel>
#include <QCloseEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace sleekpr::app {
void TemplateDesignerWindow::attachPersistence(SettingsPersistenceService& service, SettingsSessionId id)
{
    if (m_persistenceService) {
        disconnect(m_persistenceService, nullptr, this, nullptr);
        m_persistenceService->detachSession(m_persistenceSession);
    }
    m_persistenceService = &service; m_persistenceSession = id;
    m_persistenceInitialized = false; m_persistenceOutcomeSerial = 0;
    connect(&service, &SettingsPersistenceService::sessionChanged, this, [this](auto changed) {
        if (changed == m_persistenceSession) refreshPersistenceState();
    });
    refreshPersistenceState();
}
void TemplateDesignerWindow::setupPersistenceNotice()
{
    m_persistenceStateLabel = new QLabel(this);
    m_persistenceStateLabel->setObjectName(QStringLiteral("persistenceStateLabel"));
    static_cast<QVBoxLayout*>(layout())->addWidget(m_persistenceStateLabel);
    m_persistenceNotice = new SettingsPersistenceNotice(this);
    static_cast<QVBoxLayout*>(layout())->addWidget(m_persistenceNotice);
    m_persistenceNotice->setActions([this] { reloadPersistedSettings(); }, [this] {
        if (m_persistenceService) m_persistenceService->retry(m_persistenceSession);
    }, [this] { exportTemplateWithDialog(); }, [this] {
        if (!m_persistenceService) return;
        if (QMessageBox::question(this, QString::fromUtf8("恢复备份"),
                QString::fromUtf8("是否用最近有效备份替换配置并放弃当前草稿？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        m_persistenceService->requestRestore(m_persistenceSession, m_persistenceNotice->observedFailureDigest());
    });
}
void TemplateDesignerWindow::refreshPersistenceState()
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
        const auto& result = view.lastOutcome->storage;
        if (result.baselineReset && result.operation != infrastructure::SettingsStorageOperation::Save) {
            applyPersistenceReload(result.observed);
        } else if (result.writeResult) {
            setProperty("settingsSaveStatus", int(result.writeResult->status));
            if (!result.writeResult->ok()) showPersistenceFailure(*result.writeResult);
            else {
                // 只结算对应修订，绝不能用旧完成快照覆盖当前编辑、光标和撤销历史。
                m_persistenceDirty = view.pending || view.acceptedRevision != view.savedRevision;
                m_persistenceNotice->clear();
                if (m_onSettingsChanged) m_onSettingsChanged(result.observed.settings);
            }
        } else if (!result.baselineReset) {
            showPersistenceFailure({core::SettingsWriteStatus::ReadFailed, result.observed, {}, result.observed.message});
        }
    }
    QString text;
    switch (view.phase) {
    case SettingsSavePhase::Loading: text = QString::fromUtf8("正在读取配置"); break;
    case SettingsSavePhase::Queued: text = QString::fromUtf8("待保存"); break;
    case SettingsSavePhase::Saving: text = QString::fromUtf8("正在保存"); break;
    case SettingsSavePhase::Failed: text = QString::fromUtf8("保存失败，草稿已保留"); break;
    case SettingsSavePhase::Conflict: text = QString::fromUtf8("存在冲突，自动保存已暂停"); break;
    case SettingsSavePhase::Saved: text = hasInvalidPersistenceDraft() ? QString::fromUtf8("未保存的 JSON 草稿") : QString::fromUtf8("已保存"); break;
    }
    m_persistenceStateLabel->setText(text);
}
void TemplateDesignerWindow::showPersistenceFailure(const core::SettingsWriteResult& result)
{
    setProperty("settingsSaveStatus", int(result.status));
    m_persistenceNotice->showFailure(result);
    if (m_statusLabel) m_statusLabel->setText(QString::fromUtf8("未保存，当前草稿已保留"));
}
bool TemplateDesignerWindow::persistDraft()
{
    if (!m_persistenceService) return true;
    m_persistenceDirty = true;
    m_persistenceService->requestSave(m_persistenceSession, m_settings, core::SettingsEditScope::TemplatesOnly);
    // 生产通知只由实际完成触发；纯模型使用者仍保留原来的同步观察回调。
    return false;
}
void TemplateDesignerWindow::reloadPersistedSettings()
{
    if (!m_persistenceService) return;
    if (QMessageBox::question(this, QString::fromUtf8("重新读取配置"),
            QString::fromUtf8("重新读取将放弃当前未保存草稿，是否继续？可先导出当前有效模板。"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_persistenceService->requestReload(m_persistenceSession);
}
void TemplateDesignerWindow::applyPersistenceReload(const core::SettingsReadResult& snapshot)
{
    for (auto* timer : {m_elementAutoApplyTimer, m_tableAutoApplyTimer, m_previewRefreshTimer,
             m_propertyRefreshTimer, m_settingsChangedTimer}) if (timer) timer->stop();
    m_settings = snapshot.settings; m_sampleDataDraftDirty = false; m_persistenceDirty = false;
    m_persistenceNotice->clear(); ensureCurrentTemplateDocument(); refreshAll(); resetDocumentHistory();
    setProperty("settingsSaveStatus", int(core::SettingsWriteStatus::Unchanged));
}
bool TemplateDesignerWindow::hasInvalidPersistenceDraft() const { return m_sampleDataDraftDirty; }
bool TemplateDesignerWindow::hasUnsavedPersistenceChanges() const
{
    if (!m_persistenceService) return false;
    const auto view = m_persistenceService->view(m_persistenceSession);
    return m_persistenceDirty || m_sampleDataDraftDirty || view.running || view.pending
        || view.phase == SettingsSavePhase::Loading || view.phase == SettingsSavePhase::Failed || view.phase == SettingsSavePhase::Conflict
        || (m_settingsChangedTimer && m_settingsChangedTimer->isActive())
        || (m_elementAutoApplyTimer && m_elementAutoApplyTimer->isActive())
        || (m_tableAutoApplyTimer && m_tableAutoApplyTimer->isActive());
}
void TemplateDesignerWindow::flushPersistenceEdits() { flushPendingPropertyEdits(); }
void TemplateDesignerWindow::setExitPaused(bool paused)
{
    if (m_exitPaused == paused) return;
    m_exitPaused = paused;
    if (paused) {
        m_enabledBeforeExit = m_persistenceLoading ? m_persistenceWasEnabled : isEnabled(); setEnabled(false);
        if (m_previewRefreshTimer) m_previewRefreshTimer->stop();
        if (m_previewQueue) m_previewQueue->invalidate();
    } else { setEnabled(!m_persistenceLoading && m_enabledBeforeExit); if (isVisible() && !m_persistenceLoading) refreshPreview(); }
}
ExitDraftState TemplateDesignerWindow::exitDraftState() const
{
    ExitDraftState result; result.invalidDraft = hasInvalidPersistenceDraft();
    if (m_persistenceService) {
        const auto view = m_persistenceService->view(m_persistenceSession);
        if (m_persistenceDirty && !view.pending && !view.running && view.phase == SettingsSavePhase::Saved)
            result.issues.append(QString::fromUtf8("存在尚未加入保存队列的修改"));
    }
    return result;
}
void TemplateDesignerWindow::closeEvent(QCloseEvent* event)
{
    flushPendingPropertyEdits();
    bool blocked = m_sampleDataDraftDirty;
    if (m_persistenceService) {
        const auto phase = m_persistenceService->view(m_persistenceSession).phase;
        blocked = blocked || phase == SettingsSavePhase::Failed || phase == SettingsSavePhase::Conflict;
    }
    if (blocked && QMessageBox::question(this, QString::fromUtf8("存在未保存草稿"),
            QString::fromUtf8("当前草稿尚未保存。关闭后仍保留在本次运行中，再次打开可继续编辑。是否关闭？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { event->ignore(); return; }
    QWidget::closeEvent(event);
}
} // 命名空间 sleekpr::app
