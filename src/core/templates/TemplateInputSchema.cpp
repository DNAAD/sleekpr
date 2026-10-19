#include "sleekpr/core/templates/TemplateInputSchema.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace sleekpr::core {
namespace {

FieldValueType sampleType(const QJsonValue& value)
{
    if (value.isArray()) {
        return FieldValueType::List;
    }
    if (value.isObject()) {
        return FieldValueType::Object;
    }
    if (value.isBool()) {
        return FieldValueType::Boolean;
    }
    if (value.isDouble()) {
        return FieldValueType::Number;
    }
    return FieldValueType::Text;
}

bool isEmptyValue(const QJsonValue& value)
{
    return value.isNull() || value.isUndefined()
        || (value.isString() && value.toString().trimmed().isEmpty())
        || (value.isArray() && value.toArray().isEmpty())
        || (value.isObject() && value.toObject().isEmpty());
}

QJsonValue inputValue(const QJsonObject& values, const QString& key, bool arrayPath)
{
    // 精确字符串键优先，只有数组数据路径允许点号嵌套，分段规则与 Renderer 一致。
    if (values.contains(key) || !arrayPath) {
        return values.value(key);
    }
    QJsonValue current = values;
    for (const auto& part : key.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        if (!current.isObject()) {
            return QJsonValue(QJsonValue::Undefined);
        }
        current = current.toObject().value(part);
    }
    return current;
}

QStringList placeholderKeys(const QString& text)
{
    QStringList keys;
    qsizetype cursor = 0;
    while (cursor < text.size()) {
        const auto start = text.indexOf(QStringLiteral("${"), cursor);
        if (start < 0) {
            break;
        }
        const auto end = text.indexOf(QLatin1Char('}'), start + 2);
        if (end < 0) {
            break;
        }
        keys.append(text.mid(start + 2, end - start - 2).trimmed());
        cursor = end + 1;
    }
    return keys;
}

QStringList scalarReferenceKeys(const TemplateElement& element)
{
    switch (element.type) {
    case TemplateElementType::BoundField:
        return {element.fieldKey};
    case TemplateElementType::FixedText:
        return placeholderKeys(element.text);
    case TemplateElementType::QrCode:
        return element.payload.trimmed().isEmpty() ? QStringList{element.fieldKey} : placeholderKeys(element.payload);
    case TemplateElementType::ArrayGrid:
    case TemplateElementType::Rectangle:
        return {};
    }
    return {};
}

QString fieldLabel(const FieldDefinition& definition)
{
    const auto key = definition.key.trimmed();
    const auto name = definition.displayName.trimmed();
    return name.isEmpty() || name == key ? key : QStringLiteral("%1（%2）").arg(name, key);
}

TemplateInputIssue fieldIssue(const TemplateInputField& field, TemplateInputProblem problem, const QString& expectedType = {})
{
    TemplateInputIssue issue{field.definition.key.trimmed(), {}, field.elementIds};
    issue.problem = problem;
    issue.expectedType = expectedType;
    issue.message = issue.messageForDisplayNames(fieldLabel(field.definition));
    return issue;
}

QString expectedTypeForInvalidValue(FieldValueType type, const QJsonValue& value)
{
    switch (type) {
    case FieldValueType::Number:
    case FieldValueType::Money:
    case FieldValueType::Weight: {
        bool numeric = value.isDouble();
        double number = value.toDouble();
        if (value.isString()) {
            number = value.toString().trimmed().toDouble(&numeric);
        }
        return numeric && std::isfinite(number) ? QString() : QStringLiteral("有效数字");
    }
    case FieldValueType::Boolean:
        return value.isBool() ? QString() : QStringLiteral("布尔值");
    case FieldValueType::List:
        return value.isArray() ? QString() : QStringLiteral("数组");
    case FieldValueType::Object:
        return value.isObject() ? QString() : QStringLiteral("对象");
    case FieldValueType::Date:
    case FieldValueType::DateTime:
        return value.isString() ? QString() : QStringLiteral("日期文本");
    case FieldValueType::Text:
        // 无可靠样例时使用 Text；数值和布尔值可按 Renderer 的标量规则显示。
        return value.isString() || value.isDouble() || value.isBool() ? QString() : QStringLiteral("文本或标量");
    }
    return QString();
}

void appendGridRowIssues(
    QList<TemplateInputIssue>& issues, const TemplateElement& element, const QJsonObject& values)
{
    const auto key = element.dataPath.trimmed();
    if (key.isEmpty() || element.id.trimmed().isEmpty()) {
        return;
    }
    const auto items = inputValue(values, key, true).toArray();
    const auto cellTemplate = element.arrayGridCellTemplate.trimmed().isEmpty()
        ? QStringLiteral("${value}") : element.arrayGridCellTemplate;
    auto keys = placeholderKeys(cellTemplate);
    keys.removeAll(QString());
    keys.removeDuplicates();
    // 只检查 Renderer 实际消费的格数；没有明细的空数组不要求提供任何行内字段。
    const qint64 capacity = qint64(std::max(1, element.arrayGridRows)) * std::max(1, element.arrayGridColumns);
    const auto count = std::min<qint64>(items.size(), capacity);
    for (qint64 index = 0; index < count; ++index) {
        const auto item = items.at(index);
        const auto row = item.isObject() ? item.toObject() : QJsonObject{{QStringLiteral("value"), item}};
        for (const auto& childKey : keys) {
            // 行对象不回退全局上下文；原始值仅通过 {value: item} 暴露，与 Renderer 一致。
            const auto value = inputValue(row, childKey, true);
            if (!value.isNull() && !value.isUndefined()) {
                continue;
            }
            TemplateInputIssue rowIssue{key, {}, {element.id}};
            rowIssue.problem = TemplateInputProblem::MissingArrayChild;
            rowIssue.childFieldKey = childKey;
            rowIssue.itemNumber = index + 1;
            rowIssue.message = rowIssue.messageForDisplayNames(key, childKey);
            auto existing = std::find_if(issues.begin(), issues.end(), [&](const TemplateInputIssue& issue) {
                return issue.fieldKey == key && issue.message == rowIssue.message;
            });
            if (existing == issues.end()) {
                issues.append(rowIssue);
            } else if (!existing->elementIds.contains(element.id)) {
                existing->elementIds.append(element.id);
            }
        }
    }
}

} // 匿名命名空间

QString TemplateInputIssue::messageForDisplayNames(const QString& fieldName, const QString& childFieldName) const
{
    const auto name = fieldName.isEmpty() ? fieldKey : fieldName;
    switch (problem) {
    case TemplateInputProblem::MissingValue:
        return QStringLiteral("缺少字段 %1 的输入值").arg(name);
    case TemplateInputProblem::InvalidType:
        return QStringLiteral("字段 %1 类型不正确，应为%2").arg(name, expectedType);
    case TemplateInputProblem::LiteralKeyRequired:
        return QStringLiteral("字段 %1 同时用于标量和数组，必须提供精确字符串键，标量不支持嵌套读取").arg(name);
    case TemplateInputProblem::MissingArrayChild:
        // 名称和序号同时代入，避免名称中的 %1、%2 被再次当成占位符。
        return QStringLiteral("数组 %1 第%3项缺少子字段 %2").arg(name,
            childFieldName.isEmpty() ? childFieldKey : childFieldName, QString::number(itemNumber));
    case TemplateInputProblem::Unspecified:
        return message;
    }
    return message;
}

QList<TemplateInputField> TemplateInputSchema::fields(const TemplateDocument& document)
{
    QList<TemplateInputField> result;
    QHash<QString, qsizetype> indexes;
    // 显式定义优先且保持顺序与元数据，同一 key 的引用只合并来源元素。
    for (const auto& definition : document.fieldSchema) {
        const auto key = definition.key.trimmed();
        if (!indexes.contains(key)) {
            indexes.insert(key, result.size());
            result.append(TemplateInputField{definition, false, false, {}});
        }
    }

    const auto appendReference = [&](const QString& rawKey, const QString& elementId, bool arrayPath = false) {
        const auto key = rawKey.trimmed();
        if (key.isEmpty()) {
            return;
        }
        if (!indexes.contains(key)) {
            FieldDefinition definition;
            definition.key = key;
            definition.displayName = key;
            definition.type = sampleType(document.sampleData.value(key));
            definition.required = true;
            indexes.insert(key, result.size());
            result.append(TemplateInputField{definition, true, false, {}});
        }
        auto& field = result[indexes.value(key)];
        field.arrayPath = field.arrayPath || arrayPath;
        if (field.inferred && field.arrayPath) {
            // 数组来源是可靠结构信息，优先于样例类型；显式 schema 的元数据绝不覆盖。
            field.definition.type = FieldValueType::List;
        }
        if (!elementId.isEmpty() && !field.elementIds.contains(elementId)) {
            field.elementIds.append(elementId);
        }
    };

    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }
        for (const auto& element : layer.elements) {
            if (!element.visible) {
                continue;
            }
            // 锁定仅限制编辑，不限制打印；单元格模板的所有字段都属于行内上下文。
            for (const auto& key : scalarReferenceKeys(element)) {
                appendReference(key, element.id);
            }
            if (element.type == TemplateElementType::ArrayGrid) {
                appendReference(element.dataPath, element.id, true);
            }
        }
        for (const auto& table : layer.tables) {
            if (!table.visible) {
                continue;
            }
            appendReference(table.dataPath, table.id, true);
            for (const auto& rowBand : table.rowBands) {
                if (rowBand.kind == TableRowBandKind::Detail) {
                    appendReference(rowBand.dataPath, table.id, true);
                }
            }
        }
    }
    // 仅覆盖已发现字段的名称副本，空名称保留原显示名，不能借名称映射新增声明或改变校验语义。
    for (auto& field : result) {
        const auto name = document.fieldDisplayNames.value(field.definition.key.trimmed()).trimmed();
        if (!name.isEmpty()) {
            field.definition.displayName = name;
        }
    }
    return result;
}

QList<TemplateInputIssue> TemplateInputSchema::validate(const TemplateDocument& document, const QJsonObject& values)
{
    QList<TemplateInputIssue> issues;
    QSet<QString> scalarKeys;
    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }
        for (const auto& element : layer.elements) {
            if (element.visible) {
                for (const auto& key : scalarReferenceKeys(element)) {
                    scalarKeys.insert(key.trimmed());
                }
            }
        }
    }
    const auto inputFields = fields(document);
    for (const auto& field : inputFields) {
        const auto key = field.definition.key.trimmed();
        if (key.isEmpty()) {
            continue;
        }
        const auto value = inputValue(values, key, field.arrayPath);
        // 同一 dotted key 混用于标量和数组时，嵌套值不能掩盖标量 Renderer 的字面键缺失。
        if (field.arrayPath && key.contains(QLatin1Char('.')) && scalarKeys.contains(key) && !values.contains(key)
            && (field.inferred || field.definition.required || (!value.isNull() && !value.isUndefined()))) {
            issues.append(fieldIssue(field, TemplateInputProblem::LiteralKeyRequired));
            continue;
        }
        const bool missing = field.inferred
            ? value.isUndefined() || value.isNull()
            : field.definition.required && isEmptyValue(value);
        if (missing) {
            issues.append(fieldIssue(field, TemplateInputProblem::MissingValue));
            continue;
        }
        // 缺省可选字段可跳过；留空标记必须匹配类型，不能用空对象或空数组绕过类型校验。
        if (value.isNull() || value.isUndefined()) {
            continue;
        }
        if (!field.arrayPath && field.definition.type != FieldValueType::List && field.definition.type != FieldValueType::Object
            && value.isString() && value.toString().trimmed().isEmpty()) {
            continue;
        }
        const auto expectedType = field.arrayPath && !value.isArray()
            ? QStringLiteral("数组（数组数据路径）")
            : expectedTypeForInvalidValue(field.definition.type, value);
        if (!expectedType.isEmpty()) {
            issues.append(fieldIssue(field, TemplateInputProblem::InvalidType, expectedType));
        }
    }
    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }
        for (const auto& element : layer.elements) {
            if (element.visible && element.type == TemplateElementType::ArrayGrid) {
                appendGridRowIssues(issues, element, values);
            }
        }
    }
    return issues;
}

} // 命名空间 sleekpr::core
