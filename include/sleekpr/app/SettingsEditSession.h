#pragma once

#include "sleekpr/infrastructure/settings/SettingsRepository.h"

#include <optional>

namespace sleekpr::app {

class SettingsEditSession
{
public:
    explicit SettingsEditSession(QString path, infrastructure::SettingsRepository::AtomicWriter writer = {});
    const core::SettingsReadResult& baseline() const;
    core::SettingsReadResult reload();
    core::SettingsWriteResult saveEdited(const core::PrintClientSettings& edited, core::SettingsEditScope scope);
    core::SettingsWriteResult saveChanges(const core::SettingsChangeSet& changes);
    core::SettingsWriteResult restoreBackup(const QByteArray& expectedDigest);

private:
    infrastructure::SettingsRepository m_repository;
    core::SettingsReadResult m_baseline;
    std::optional<core::SettingsWriteResult> m_conflict;
};

} // 命名空间 sleekpr::app
