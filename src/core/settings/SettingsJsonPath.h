#pragma once
#include <QJsonObject>
#include <QStringList>

namespace sleekpr::core::settings_detail {
inline QJsonValue valueAt(const QJsonObject& object, const QStringList& path)
{
    QJsonValue value(object);
    for (const auto& part : path) {
        if (!value.isObject()) return QJsonValue(QJsonValue::Undefined);
        value = value.toObject().value(part);
    }
    return value;
}
inline void setValueAt(QJsonObject& object, const QStringList& path, qsizetype index, const QJsonValue& value)
{
    if (index >= path.size()) return;
    const auto& key = path[index];
    if (index == path.size() - 1) {
        if (value.isUndefined()) object.remove(key);
        else object.insert(key, value);
        return;
    }
    auto child = object.value(key).toObject();
    setValueAt(child, path, index + 1, value);
    object.insert(key, child);
}
} // 命名空间 sleekpr::core::settings_detail
