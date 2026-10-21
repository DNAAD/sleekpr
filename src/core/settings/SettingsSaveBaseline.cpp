#include "sleekpr/core/settings/SettingsSaveBaseline.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include "SettingsJsonPath.h"

namespace sleekpr::core {
SettingsSaveBaseline::SettingsSaveBaseline(const SettingsReadResult& observed) { reset(observed); }
void SettingsSaveBaseline::reset(const SettingsReadResult& observed)
{
    m_expected = observed.settings;
    m_sourceExpected = observed.sourceJson;
}
const PrintClientSettings& SettingsSaveBaseline::expectedSettings() const { return m_expected; }
SettingsChangeSet SettingsSaveBaseline::changesFor(const PrintClientSettings& edited, SettingsEditScope scope) const
{
    return SettingsChangeSet::between(m_expected, edited, scope).withSourceBaseline(m_sourceExpected);
}
void SettingsSaveBaseline::acknowledge(const SettingsChangeSet& changes, const SettingsWriteResult& result)
{
    if (!result.ok()) return;
    auto expected = PrintClientSettingsJson::toJson(m_expected);
    const auto actual = PrintClientSettingsJson::toJson(result.snapshot.settings);
    // 只确认本会话真正提交的路径；顺带读到的外部修改不等于用户已经观察并接受。
    for (const auto& change : changes.changes) {
        settings_detail::setValueAt(expected, change.path, 0, settings_detail::valueAt(actual, change.path));
        settings_detail::setValueAt(m_sourceExpected, change.path, 0,
            settings_detail::valueAt(result.snapshot.sourceJson, change.path));
    }
    m_expected = PrintClientSettingsJson::fromJson(expected);
}
} // 命名空间 sleekpr::core
