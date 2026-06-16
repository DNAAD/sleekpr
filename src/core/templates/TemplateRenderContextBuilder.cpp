#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"

#include <QJsonArray>
#include <QJsonValue>

namespace sleekpr::core {

namespace {

bool hasUsableValue(const QJsonObject& values, const QString& key)
{
    if (!values.contains(key)) {
        return false;
    }

    const auto value = values.value(key);
    return !value.isNull() && !value.isUndefined();
}

bool isEmptyRequiredValue(const QJsonValue& value)
{
    if (value.isNull() || value.isUndefined()) {
        return true;
    }
    if (value.isString()) {
        return value.toString().trimmed().isEmpty();
    }
    if (value.isArray()) {
        return value.toArray().isEmpty();
    }
    if (value.isObject()) {
        return value.toObject().isEmpty();
    }
    return false;
}

QString fieldDisplayName(const FieldDefinition& field, const QString& normalizedKey)
{
    const auto displayName = field.displayName.trimmed();
    return displayName.isEmpty() ? normalizedKey : displayName;
}

} // 命名空间

bool TemplateRenderContext::hasMissingRequiredFields() const
{
    return !missingRequiredFieldNames.isEmpty();
}

TemplateRenderContext TemplateRenderContextBuilder::build(const QList<FieldDefinition>& fieldSchema,
                                                          const QJsonObject& presetValues,
                                                          const QJsonObject& requestValues)
{
    TemplateRenderContext context;

    for (const auto& field : fieldSchema) {
        const auto key = field.key.trimmed();
        if (key.isEmpty()) {
            continue;
        }

        // 合并优先级：本次请求值 > 字段值方案 > 字段默认值 > 空值。
        QJsonValue value = field.defaultValue;
        if (hasUsableValue(presetValues, key)) {
            value = presetValues.value(key);
        }
        if (hasUsableValue(requestValues, key)) {
            value = requestValues.value(key);
        }

        context.values.insert(key, value);
        if (field.required && isEmptyRequiredValue(value)) {
            context.missingRequiredFieldNames.append(fieldDisplayName(field, key));
        }
    }

    return context;
}

} // 命名空间 sleekpr::core
