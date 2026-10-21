#pragma once
#include "sleekpr/app/SettingsPersistenceState.h"
#include "sleekpr/infrastructure/settings/SettingsPersistenceExecutor.h"
#include <QMap>
#include <memory>

namespace sleekpr::app {
class SettingsPersistenceService final : public QObject
{
    Q_OBJECT
public:
    using AtomicWriter = infrastructure::SettingsRepository::AtomicWriter;
    explicit SettingsPersistenceService(QString path, AtomicWriter writer = {}, QObject* parent = nullptr);
    ~SettingsPersistenceService() override;
    std::optional<SettingsSessionId> openSession(QString title);
    std::optional<SettingsSaveTicket> requestSave(SettingsSessionId, core::PrintClientSettings, core::SettingsEditScope);
    bool retry(SettingsSessionId);
    bool requestReload(SettingsSessionId);
    bool requestRestore(SettingsSessionId, QByteArray expectedDigest);
    void detachSession(SettingsSessionId);
    SettingsSessionView view(SettingsSessionId) const;
    QList<SettingsSessionId> sessions() const;
    void setAcceptingSaves(bool);
    void setPendingSuspended(bool);
    void discardPending(SettingsSessionId);
    bool hasRunningWork() const;
    bool hasControlWork() const;
    bool isIdle() const;
    bool shutdownIfIdle();
signals:
    void sessionChanged(sleekpr::app::SettingsSessionId id);
    void activityChanged();
private:
    struct Entry;
    bool control(SettingsSessionId, infrastructure::SettingsStorageOperation, QByteArray = {});
    void pump();
    void publish(SettingsSessionId);
    void complete(SettingsSessionId, infrastructure::SettingsStorageOperation,
        std::optional<SettingsSaveTicket>, infrastructure::SettingsStorageOutcome);
    QString m_path;
    AtomicWriter m_writer;
    infrastructure::SettingsPersistenceExecutor m_executor;
    QMap<SettingsSessionId, std::shared_ptr<Entry>> m_sessions;
    SettingsSessionId m_nextId = 0, m_cursor = 0;
    bool m_running = false, m_controlRunning = false, m_accepting = true, m_suspended = false, m_stopped = false;
};
} // 命名空间 sleekpr::app
