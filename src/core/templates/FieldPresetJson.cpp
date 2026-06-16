#include "sleekpr/core/templates/FieldPresetJson.h"

namespace sleekpr::core {

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool isBlankOrPadded(const QString& value)
{
    return value.trimmed().isEmpty() || value != value.trimmed();
}

} // 命名空间

QJsonObject FieldPresetJson::toJson(const FieldPreset& preset)
{
    QJsonObject json;
    json["schemaVersion"] = preset.schemaVersion;
    json["id"] = preset.id;
    json["name"] = preset.name;
    json["templateId"] = preset.templateId;
    json["values"] = preset.values;
    json["updatedAt"] = preset.updatedAt;
    return json;
}

FieldPreset FieldPresetJson::fromJson(const QJsonObject& json)
{
    FieldPreset preset;
    preset.schemaVersion = json["schemaVersion"].toInt(preset.schemaVersion);
    preset.id = json["id"].toString();
    preset.name = json["name"].toString();
    preset.templateId = json["templateId"].toString();
    preset.values = json["values"].toObject();
    preset.updatedAt = json["updatedAt"].toString();
    return preset;
}

bool FieldPresetJson::validate(const QJsonObject& json, QString* errorMessage)
{
    if (json["schemaVersion"].toInt(1) != 1) {
        setError(errorMessage, QStringLiteral("字段值方案 schemaVersion 不受支持"));
        return false;
    }

    const auto id = json["id"].toString();
    if (isBlankOrPadded(id)) {
        setError(errorMessage, QStringLiteral("字段值方案 id 不能为空，且不能包含首尾空白"));
        return false;
    }

    const auto templateId = json["templateId"].toString();
    if (isBlankOrPadded(templateId)) {
        setError(errorMessage, QStringLiteral("字段值方案 templateId 不能为空，且不能包含首尾空白"));
        return false;
    }

    if (!json.contains("values") || !json["values"].isObject()) {
        setError(errorMessage, QStringLiteral("字段值 values 必须是对象"));
        return false;
    }

    return true;
}

} // 命名空间 sleekpr::core
