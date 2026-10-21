#pragma once
#include "sleekpr/core/settings/SettingsSaveBaseline.h"
#include "sleekpr/infrastructure/settings/SettingsRepository.h"
#include <optional>

namespace sleekpr::infrastructure {
enum class SettingsStorageOperation { Open, Save, Reload, Restore };
struct SettingsStorageOutcome
{
    SettingsStorageOperation operation = SettingsStorageOperation::Open;
    core::SettingsReadResult observed;
    core::PrintClientSettings expectedSettings;
    std::optional<core::SettingsWriteResult> writeResult;
    bool baselineReset = false;
    bool executionFailed = false;
};

// 由专用存储线程独占；窗口仅接收值快照，不跨线程共享可变基线。
class SettingsSaveSession
{
public:
    explicit SettingsSaveSession(QString path, SettingsRepository::AtomicWriter writer = {});
    SettingsStorageOutcome open();
    SettingsStorageOutcome saveEdited(const core::PrintClientSettings&, core::SettingsEditScope);
    SettingsStorageOutcome reload();
    SettingsStorageOutcome restoreBackup(const QByteArray& expectedDigest);
private:
    SettingsStorageOutcome read(SettingsStorageOperation);
    SettingsStorageOutcome outcome(SettingsStorageOperation, std::optional<core::SettingsWriteResult> = {}, bool reset = false) const;
    SettingsRepository m_repository;
    core::SettingsSaveBaseline m_baseline;
    core::SettingsReadResult m_observed;
    std::optional<core::SettingsWriteResult> m_conflict;
    bool m_readable = false;
};
} // 命名空间 sleekpr::infrastructure
