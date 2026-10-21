#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <optional>

namespace sleekpr::core {

enum class SettingsEditScope { TemplatesOnly, AllChangedFields };

struct SettingsFieldChange
{
    QStringList path;
    QJsonValue expected = QJsonValue(QJsonValue::Undefined);
    QJsonValue value = QJsonValue(QJsonValue::Undefined);
    std::optional<QJsonValue> sourceExpected;
};

struct SettingsMergeResult
{
    bool changed = false;
    QJsonObject document;
    QStringList conflicts;
};

class SettingsChangeSet
{
public:
    QList<SettingsFieldChange> changes;

    static SettingsChangeSet between(const PrintClientSettings& base, const PrintClientSettings& edited, SettingsEditScope scope);
    SettingsChangeSet withSourceBaseline(const QJsonObject& source) const;
    SettingsMergeResult merge(const QJsonObject& latest) const;
};

} // 命名空间 sleekpr::core
