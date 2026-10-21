#include "sleekpr/infrastructure/settings/SettingsSaveSession.h"
#include <utility>

namespace sleekpr::infrastructure {
using namespace core;
SettingsSaveSession::SettingsSaveSession(QString path, SettingsRepository::AtomicWriter writer)
    : m_repository(std::move(path), std::move(writer)) {}
SettingsStorageOutcome SettingsSaveSession::outcome(SettingsStorageOperation kind,
    std::optional<SettingsWriteResult> result, bool reset) const
{
    return {kind, m_observed, m_baseline.expectedSettings(), std::move(result), reset};
}
SettingsStorageOutcome SettingsSaveSession::read(SettingsStorageOperation kind)
{
    m_observed = m_repository.read();
    m_readable = m_observed.status == SettingsReadStatus::Ready || m_observed.status == SettingsReadStatus::Missing;
    if (m_readable) { m_baseline.reset(m_observed); m_conflict.reset(); }
    return outcome(kind, {}, m_readable);
}
SettingsStorageOutcome SettingsSaveSession::open() { return read(SettingsStorageOperation::Open); }
SettingsStorageOutcome SettingsSaveSession::reload() { return read(SettingsStorageOperation::Reload); }
SettingsStorageOutcome SettingsSaveSession::saveEdited(const PrintClientSettings& edited, SettingsEditScope scope)
{
    if (m_conflict) return outcome(SettingsStorageOperation::Save, m_conflict);
    if (!m_readable) return outcome(SettingsStorageOperation::Save,
        SettingsWriteResult{SettingsWriteStatus::ReadFailed, m_observed, {}, m_observed.message});
    const auto changes = m_baseline.changesFor(edited, scope);
    auto result = m_repository.apply(changes);
    m_observed = result.snapshot;
    // 失败不推进期望基线；冲突必须由显式成功重读或恢复解除。
    if (result.ok()) m_baseline.acknowledge(changes, result);
    else if (result.status == SettingsWriteStatus::Conflict) m_conflict = result;
    return outcome(SettingsStorageOperation::Save, std::move(result));
}
SettingsStorageOutcome SettingsSaveSession::restoreBackup(const QByteArray& digest)
{
    auto result = m_repository.restoreBackup(digest);
    m_observed = result.snapshot;
    if (result.ok()) {
        m_baseline.reset(m_observed); m_conflict.reset(); m_readable = true;
    }
    return outcome(SettingsStorageOperation::Restore, result, result.ok());
}
} // 命名空间 sleekpr::infrastructure
