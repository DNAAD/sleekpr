#pragma once

#include "sleekpr/core/settings/SettingsChangeSet.h"
#include "sleekpr/core/settings/SettingsPersistenceResult.h"

#include <functional>

namespace sleekpr::infrastructure {

class SettingsRepository
{
public:
    using AtomicWriter = std::function<bool(const QString&, const QByteArray&, QString*)>;

    explicit SettingsRepository(QString path, AtomicWriter writer = {});
    core::SettingsReadResult read() const;
    core::SettingsWriteResult apply(const core::SettingsChangeSet& changes) const;
    core::SettingsWriteResult replace(const core::PrintClientSettings& settings) const;
    core::SettingsWriteResult restoreBackup(const QByteArray& expectedCurrentDigest) const;

private:
    using WriteOperation = std::function<core::SettingsWriteResult(const core::SettingsReadResult&)>;
    core::SettingsWriteResult lockedWrite(const WriteOperation& operation) const;
    core::SettingsWriteResult writeDocument(const core::SettingsReadResult& previous, const QJsonObject& document,
        bool restoringBackup = false) const;
    QString m_path;
    AtomicWriter m_writer;
};

} // 命名空间 sleekpr::infrastructure
