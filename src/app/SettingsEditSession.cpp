#include "sleekpr/app/SettingsEditSession.h"

#include <utility>

namespace sleekpr::app {

SettingsEditSession::SettingsEditSession(QString path, infrastructure::SettingsRepository::AtomicWriter writer)
    : m_repository(std::move(path), std::move(writer))
    , m_baseline(m_repository.read())
{
}

const core::SettingsReadResult& SettingsEditSession::baseline() const { return m_baseline; }

core::SettingsReadResult SettingsEditSession::reload()
{
    const auto snapshot = m_repository.read();
    if (snapshot.status == core::SettingsReadStatus::Ready || snapshot.status == core::SettingsReadStatus::Missing) {
        m_baseline = snapshot;
        m_conflict.reset();
    }
    return snapshot;
}

core::SettingsWriteResult SettingsEditSession::saveEdited(const core::PrintClientSettings& edited, core::SettingsEditScope scope)
{
    return saveChanges(core::SettingsChangeSet::between(m_baseline.settings, edited, scope));
}

core::SettingsWriteResult SettingsEditSession::saveChanges(const core::SettingsChangeSet& changes)
{
    // 冲突后保留原基线和草稿；必须显式成功重读，不能反复用旧快照覆盖。
    if (m_conflict.has_value()) return *m_conflict;
    if (m_baseline.status == core::SettingsReadStatus::InvalidJson || m_baseline.status == core::SettingsReadStatus::IoError) {
        return {core::SettingsWriteStatus::ReadFailed, m_baseline, {}, m_baseline.message};
    }
    const auto result = m_repository.apply(changes.withSourceBaseline(m_baseline.sourceJson));
    if (result.ok()) m_baseline = result.snapshot;
    else if (result.status == core::SettingsWriteStatus::Conflict) m_conflict = result;
    return result;
}

core::SettingsWriteResult SettingsEditSession::restoreBackup(const QByteArray& expectedDigest)
{
    const auto result = m_repository.restoreBackup(expectedDigest);
    if (result.ok()) {
        m_baseline = result.snapshot;
        m_conflict.reset();
    }
    return result;
}

} // 命名空间 sleekpr::app
