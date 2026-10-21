#include "sleekpr/core/settings/SettingsChangeSet.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include "SettingsJsonPath.h"

#include <QSet>
#include <QHash>
#include <QJsonArray>

namespace sleekpr::core {
namespace {
using settings_detail::valueAt;
using settings_detail::setValueAt;

QString arrayIdentity(const QJsonValue& value)
{
    const auto object = value.toObject();
    for (const auto& field : {QStringLiteral("id"), QStringLiteral("key")}) {
        const auto identity = object.value(field).toString();
        if (!identity.isEmpty()) return field + QLatin1Char(':') + identity;
    }
    return {};
}

QHash<QString, QJsonValue> indexedArray(const QJsonArray& array)
{
    QHash<QString, int> counts;
    for (const auto& value : array) ++counts[arrayIdentity(value)];
    QHash<QString, QJsonValue> result;
    for (qsizetype i = 0; i < array.size(); ++i) {
        const auto identity = arrayIdentity(array[i]);
        // 元素和字段优先按稳定身份匹配；无身份或重复身份时不猜测，保留位置语义。
        const auto key = !identity.isEmpty() && counts.value(identity) == 1
            ? identity : QStringLiteral("index:%1").arg(i);
        result.insert(key, array[i]);
    }
    return result;
}

QJsonValue unknownMembers(const QJsonValue& raw, const QJsonValue& known)
{
    if (known.isUndefined()) return raw;
    QJsonObject result;
    if (raw.isObject() && known.isObject()) {
        const auto object = raw.toObject();
        const auto schema = known.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            const auto unknown = unknownMembers(it.value(), schema.value(it.key()));
            if (!unknown.isUndefined()) result.insert(it.key(), unknown);
        }
    } else if (raw.isArray() && known.isArray()) {
        const auto items = indexedArray(raw.toArray());
        const auto schema = indexedArray(known.toArray());
        for (auto it = items.begin(); it != items.end(); ++it) {
            const auto unknown = unknownMembers(it.value(), schema.value(it.key(), QJsonValue::Undefined));
            if (!unknown.isUndefined()) result.insert(it.key(), unknown);
        }
    }
    return result.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(result);
}

QJsonValue mergeKnownChanges(const QJsonValue& raw, const QJsonValue& before, const QJsonValue& after)
{
    if (before == after) return raw;
    if (before.isObject() && after.isObject()) {
        auto result = raw.toObject();
        const auto oldObject = before.toObject();
        const auto newObject = after.toObject();
        for (auto it = oldObject.begin(); it != oldObject.end(); ++it) {
            if (!newObject.contains(it.key())) result.remove(it.key());
        }
        for (auto it = newObject.begin(); it != newObject.end(); ++it) {
            if (oldObject.value(it.key()) == it.value()) continue;
            result.insert(it.key(), mergeKnownChanges(result.value(it.key()), oldObject.value(it.key()), it.value()));
        }
        return result;
    }
    if (before.isArray() && after.isArray()) {
        const auto originals = indexedArray(raw.toArray());
        const auto previous = indexedArray(before.toArray());
        const auto next = after.toArray();
        QHash<QString, int> counts;
        for (const auto& value : next) ++counts[arrayIdentity(value)];
        QJsonArray result;
        for (qsizetype i = 0; i < next.size(); ++i) {
            const auto identity = arrayIdentity(next[i]);
            const auto key = !identity.isEmpty() && counts.value(identity) == 1
                ? identity : QStringLiteral("index:%1").arg(i);
            result.append(mergeKnownChanges(originals.value(key, QJsonValue::Undefined),
                previous.value(key, QJsonValue::Undefined), next[i]));
        }
        return result;
    }
    return after;
}

void appendChanges(SettingsChangeSet& result, const QJsonValue& base, const QJsonValue& edited,
    const QStringList& path, int mapDepth)
{
    if (base == edited) return;
    if (mapDepth == 0) {
        result.changes.append({path, base, edited});
        return;
    }
    const auto oldObject = base.toObject();
    const auto newObject = edited.toObject();
    QSet<QString> keys;
    for (auto it = oldObject.constBegin(); it != oldObject.constEnd(); ++it) keys.insert(it.key());
    for (auto it = newObject.constBegin(); it != newObject.constEnd(); ++it) keys.insert(it.key());
    auto orderedKeys = keys.values();
    orderedKeys.sort();
    for (const auto& key : orderedKeys) {
        auto childPath = path;
        childPath.append(key);
        appendChanges(result, oldObject.value(key), newObject.value(key), childPath, mapDepth - 1);
    }
}

} // 匿名命名空间

SettingsChangeSet SettingsChangeSet::between(const PrintClientSettings& base, const PrintClientSettings& edited, SettingsEditScope scope)
{
    // 双方使用相同的读取默认值，避免旧标签缺省纸张规格造成伪冲突。
    const auto oldJson = PrintClientSettingsJson::toJson(PrintClientSettingsJson::fromJson(PrintClientSettingsJson::toJson(base)));
    const auto newJson = PrintClientSettingsJson::toJson(PrintClientSettingsJson::fromJson(PrintClientSettingsJson::toJson(edited)));
    SettingsChangeSet result;
    const auto append = [&](const QString& key, int depth) {
        appendChanges(result, oldJson.value(key), newJson.value(key), {key}, depth);
    };
    // 模板以槽位为事务单元；路径段保留原文，不拆分用户键中的点号或斜杠。
    append(QStringLiteral("templateDocuments"), 1);
    if (scope == SettingsEditScope::TemplatesOnly) return result;
    append(QStringLiteral("defaultPrinter"), 0);
    append(QStringLiteral("allowedOrigins"), 0);
    append(QStringLiteral("labelOffset"), 1);
    append(QStringLiteral("localHttpLimits"), 1);
    append(QStringLiteral("templateElements"), 1);
    append(QStringLiteral("templateOverrides"), 2);
    return result;
}

SettingsChangeSet SettingsChangeSet::withSourceBaseline(const QJsonObject& source) const
{
    auto result = *this;
    for (auto& change : result.changes) change.sourceExpected = valueAt(source, change.path);
    return result;
}

SettingsMergeResult SettingsChangeSet::merge(const QJsonObject& latest) const
{
    SettingsMergeResult result{false, latest, {}};
    const auto normalized = PrintClientSettingsJson::toJson(PrintClientSettingsJson::fromJson(latest));
    // 先检查全部期望值，再修改原始对象；任何冲突都不能留下其他字段的半次更新。
    for (const auto& change : changes) {
        if (change.path.isEmpty()) {
            result.conflicts.append(QStringLiteral("invalid-path"));
            continue;
        }
        const auto current = valueAt(normalized, change.path);
        const auto sourceChanged = change.sourceExpected && unknownMembers(*change.sourceExpected, change.expected)
            != unknownMembers(valueAt(latest, change.path), current);
        if ((current != change.expected && current != change.value) || sourceChanged)
            result.conflicts.append(change.path.join(QLatin1Char('/')));
    }
    if (!result.conflicts.isEmpty()) return result;
    for (const auto& change : changes) {
        if (valueAt(normalized, change.path) == change.value) continue;
        // 只写已知语义变化，不能整槽覆盖丢弃新版本或外部工具留下的扩展字段。
        setValueAt(result.document, change.path, 0,
            mergeKnownChanges(valueAt(latest, change.path), change.expected, change.value));
        result.changed = true;
    }
    return result;
}

} // 命名空间 sleekpr::core
