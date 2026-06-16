#include "sleekpr/core/templates/FieldSchemaJson.h"

#include <QJsonArray>
#include <QSet>

namespace sleekpr::core {

namespace {

QString typeToString(FieldValueType type)
{
    switch (type) {
    case FieldValueType::Text:
        return QStringLiteral("text");
    case FieldValueType::Number:
        return QStringLiteral("number");
    case FieldValueType::Money:
        return QStringLiteral("money");
    case FieldValueType::Weight:
        return QStringLiteral("weight");
    case FieldValueType::Date:
        return QStringLiteral("date");
    case FieldValueType::DateTime:
        return QStringLiteral("datetime");
    case FieldValueType::Boolean:
        return QStringLiteral("boolean");
    case FieldValueType::List:
        return QStringLiteral("list");
    case FieldValueType::Object:
        return QStringLiteral("object");
    }
    return QStringLiteral("text");
}

FieldValueType typeFromString(const QString& value)
{
    if (value == QStringLiteral("number")) {
        return FieldValueType::Number;
    }
    if (value == QStringLiteral("money")) {
        return FieldValueType::Money;
    }
    if (value == QStringLiteral("weight")) {
        return FieldValueType::Weight;
    }
    if (value == QStringLiteral("date")) {
        return FieldValueType::Date;
    }
    if (value == QStringLiteral("datetime")) {
        return FieldValueType::DateTime;
    }
    if (value == QStringLiteral("boolean")) {
        return FieldValueType::Boolean;
    }
    if (value == QStringLiteral("list")) {
        return FieldValueType::List;
    }
    if (value == QStringLiteral("object")) {
        return FieldValueType::Object;
    }
    return FieldValueType::Text;
}

bool isKnownType(const QString& value)
{
    return value == QStringLiteral("text")
        || value == QStringLiteral("number")
        || value == QStringLiteral("money")
        || value == QStringLiteral("weight")
        || value == QStringLiteral("date")
        || value == QStringLiteral("datetime")
        || value == QStringLiteral("boolean")
        || value == QStringLiteral("list")
        || value == QStringLiteral("object");
}

QString sourceToString(FieldSource source)
{
    switch (source) {
    case FieldSource::BuiltIn:
        return QStringLiteral("builtIn");
    case FieldSource::Custom:
        return QStringLiteral("custom");
    }
    return QStringLiteral("custom");
}

FieldSource sourceFromString(const QString& value)
{
    if (value == QStringLiteral("builtIn")) {
        return FieldSource::BuiltIn;
    }
    return FieldSource::Custom;
}

bool isKnownSource(const QString& value)
{
    return value == QStringLiteral("builtIn") || value == QStringLiteral("custom");
}

QJsonObject fieldToJson(const FieldDefinition& field)
{
    QJsonObject json;
    json["key"] = field.key;
    json["displayName"] = field.displayName;
    json["type"] = typeToString(field.type);
    json["required"] = field.required;
    json["defaultValue"] = field.defaultValue;
    json["sampleValue"] = field.sampleValue;
    json["format"] = field.format;
    json["source"] = sourceToString(field.source);
    return json;
}

FieldDefinition fieldFromJson(const QJsonObject& json)
{
    FieldDefinition field;
    field.key = json["key"].toString();
    field.displayName = json["displayName"].toString();
    field.type = typeFromString(json["type"].toString(typeToString(field.type)));
    field.required = json["required"].toBool(field.required);
    field.defaultValue = json["defaultValue"].toString();
    field.sampleValue = json["sampleValue"].toString();
    field.format = json["format"].toString();
    field.source = sourceFromString(json["source"].toString(sourceToString(field.source)));
    return field;
}

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // 命名空间

QJsonObject FieldSchemaJson::toJson(const QList<FieldDefinition>& fields)
{
    QJsonArray fieldArray;
    for (const auto& field : fields) {
        fieldArray.append(fieldToJson(field));
    }

    QJsonObject json;
    json["fields"] = fieldArray;
    return json;
}

QList<FieldDefinition> FieldSchemaJson::fromJson(const QJsonObject& json)
{
    QList<FieldDefinition> fields;
    for (const auto& value : json["fields"].toArray()) {
        fields.append(fieldFromJson(value.toObject()));
    }
    return fields;
}

bool FieldSchemaJson::validate(const QJsonObject& json, QString* errorMessage)
{
    if (!json.contains("fields") || !json["fields"].isArray()) {
        setError(errorMessage, QStringLiteral("字段定义 fields 必须是数组"));
        return false;
    }

    QSet<QString> fieldKeys;
    for (const auto& value : json["fields"].toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("字段定义项必须是对象"));
            return false;
        }

        const auto field = value.toObject();
        const auto key = field["key"].toString();
        const auto normalizedKey = key.trimmed();
        if (normalizedKey.isEmpty()) {
            setError(errorMessage, QStringLiteral("字段 key 不能为空"));
            return false;
        }
        if (key != normalizedKey) {
            setError(errorMessage, QStringLiteral("字段 key 不能包含首尾空白：%1").arg(normalizedKey));
            return false;
        }
        if (fieldKeys.contains(normalizedKey)) {
            setError(errorMessage, QStringLiteral("字段 key 重复：%1").arg(normalizedKey));
            return false;
        }
        fieldKeys.insert(normalizedKey);

        const auto type = field["type"].toString(QStringLiteral("text"));
        if (!isKnownType(type)) {
            setError(errorMessage, QStringLiteral("字段类型不受支持：%1").arg(type));
            return false;
        }

        const auto source = field["source"].toString(QStringLiteral("custom"));
        if (!isKnownSource(source)) {
            setError(errorMessage, QStringLiteral("字段来源不受支持：%1").arg(source));
            return false;
        }
    }

    return true;
}

} // 命名空间 sleekpr::core
