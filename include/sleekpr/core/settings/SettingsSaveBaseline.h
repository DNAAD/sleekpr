#pragma once
#include "sleekpr/core/settings/SettingsChangeSet.h"
#include "sleekpr/core/settings/SettingsPersistenceResult.h"

namespace sleekpr::core {
class SettingsSaveBaseline
{
public:
    explicit SettingsSaveBaseline(const SettingsReadResult& observed = {});
    SettingsChangeSet changesFor(const PrintClientSettings&, SettingsEditScope) const;
    void acknowledge(const SettingsChangeSet&, const SettingsWriteResult&);
    const PrintClientSettings& expectedSettings() const;
    void reset(const SettingsReadResult&);
private:
    PrintClientSettings m_expected;
    QJsonObject m_sourceExpected;
};
} // 命名空间 sleekpr::core
