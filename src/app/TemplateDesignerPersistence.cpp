#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/SettingsPersistenceNotice.h"
#include <QMessageBox>
#include <QLabel>
#include <QCloseEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace sleekpr::app {

void TemplateDesignerWindow::setSettingsSaveHandler(SettingsSaveHandler handler) { m_settingsSaveHandler = std::move(handler); }
void TemplateDesignerWindow::setSettingsReloadHandler(SettingsReloadHandler handler) { m_settingsReloadHandler = std::move(handler); }
void TemplateDesignerWindow::setSettingsRestoreHandler(SettingsRestoreHandler handler) { m_settingsRestoreHandler = std::move(handler); }

void TemplateDesignerWindow::setPersistenceSnapshot(const core::SettingsReadResult& snapshot)
{
    // 这里只建立持久化基线，不能用读盘结果覆盖正在编辑的草稿。
    m_persistenceBaseline = snapshot;
    m_persistenceConflict.reset();
    m_persistenceDirty = false;
    if (snapshot.status == core::SettingsReadStatus::InvalidJson || snapshot.status == core::SettingsReadStatus::IoError) {
        showPersistenceFailure({core::SettingsWriteStatus::ReadFailed, snapshot, {}, snapshot.message});
    } else {
        m_persistenceNotice->clear();
    }
}

void TemplateDesignerWindow::setupPersistenceNotice()
{
    m_persistenceNotice = new SettingsPersistenceNotice(this);
    static_cast<QVBoxLayout*>(layout())->addWidget(m_persistenceNotice);
    m_persistenceNotice->setActions([this] { reloadPersistedSettings(); }, [this] { notifySettingsChanged(); },
        [this] { exportTemplateWithDialog(); }, [this] {
            if (!m_settingsRestoreHandler) return;
            if (QMessageBox::question(this, QString::fromUtf8("恢复备份"),
                    QString::fromUtf8("是否用最近有效备份替换配置并放弃当前草稿？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
            const auto result = m_settingsRestoreHandler(m_persistenceNotice->observedFailureDigest());
            if (result.ok()) applyPersistenceReload(result.snapshot);
            else showPersistenceFailure(result);
        });
}

void TemplateDesignerWindow::showPersistenceFailure(const core::SettingsWriteResult& result)
{
    setProperty("settingsSaveStatus", int(result.status));
    m_persistenceNotice->showFailure(result);
    if (m_statusLabel) m_statusLabel->setText(QString::fromUtf8("未保存，当前草稿已保留"));
}

bool TemplateDesignerWindow::persistDraft()
{
    if (!m_settingsSaveHandler) return true;
    m_persistenceDirty = true;
    if (m_persistenceConflict) { showPersistenceFailure(*m_persistenceConflict); return false; }
    if (m_persistenceBaseline.status == core::SettingsReadStatus::InvalidJson
        || m_persistenceBaseline.status == core::SettingsReadStatus::IoError) {
        showPersistenceFailure({core::SettingsWriteStatus::ReadFailed, m_persistenceBaseline, {}, m_persistenceBaseline.message});
        return false;
    }
    const auto result = m_settingsSaveHandler(core::SettingsChangeSet::between(
        m_persistenceBaseline.settings, m_settings, core::SettingsEditScope::TemplatesOnly)
            .withSourceBaseline(m_persistenceBaseline.sourceJson));
    setProperty("settingsSaveStatus", int(result.status));
    if (!result.ok()) {
        if (result.status == core::SettingsWriteStatus::Conflict) m_persistenceConflict = result;
        showPersistenceFailure(result);
        return false;
    }
    // 只在成功后推进基线；历史继续记录本地文档，失败不会丢失撤销点。
    m_persistenceBaseline = result.snapshot;
    m_persistenceDirty = false;
    m_settings = result.snapshot.settings;
    m_persistenceNotice->clear();
    return true;
}

void TemplateDesignerWindow::reloadPersistedSettings()
{
    if (!m_settingsReloadHandler) return;
    if (QMessageBox::question(this, QString::fromUtf8("重新读取配置"),
            QString::fromUtf8("重新读取将放弃当前未保存草稿，是否继续？可先导出当前有效模板。"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    const auto snapshot = m_settingsReloadHandler();
    if (snapshot.status == core::SettingsReadStatus::InvalidJson || snapshot.status == core::SettingsReadStatus::IoError) {
        showPersistenceFailure({core::SettingsWriteStatus::ReadFailed, snapshot, {}, snapshot.message});
        return;
    }
    applyPersistenceReload(snapshot);
}

void TemplateDesignerWindow::applyPersistenceReload(const core::SettingsReadResult& snapshot)
{
    for (auto* timer : {m_elementAutoApplyTimer, m_tableAutoApplyTimer, m_previewRefreshTimer,
             m_propertyRefreshTimer, m_settingsChangedTimer}) {
        if (timer) timer->stop();
    }
    m_settings = snapshot.settings;
    m_sampleDataDraftDirty = false;
    setPersistenceSnapshot(snapshot);
    ensureCurrentTemplateDocument();
    refreshAll();
    resetDocumentHistory();
    setProperty("settingsSaveStatus", int(core::SettingsWriteStatus::Unchanged));
}

bool TemplateDesignerWindow::hasUnsavedPersistenceChanges() const
{
    return bool(m_settingsSaveHandler) && (!m_persistenceNotice->isHidden()
        || m_persistenceDirty
        || m_sampleDataDraftDirty
        || (m_settingsChangedTimer && m_settingsChangedTimer->isActive())
        || (m_elementAutoApplyTimer && m_elementAutoApplyTimer->isActive())
        || (m_tableAutoApplyTimer && m_tableAutoApplyTimer->isActive()));
}

void TemplateDesignerWindow::closeEvent(QCloseEvent* event)
{
    flushPendingPropertyEdits();
    if (hasUnsavedPersistenceChanges() && QMessageBox::question(this, QString::fromUtf8("存在未保存草稿"),
            QString::fromUtf8("当前草稿尚未保存。关闭后仍保留在本次运行中，再次打开可继续编辑。是否关闭？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

} // 命名空间 sleekpr::app
