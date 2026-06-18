#include "sleekpr/core/templates/TemplateDocumentValidator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRectF>
#include <QSet>

#include <algorithm>

namespace sleekpr::core {
namespace {

constexpr double kGeometryEpsilon = 0.001;

QString issueTargetName(const QString& elementId)
{
    return elementId.trimmed().isEmpty() ? QString::fromUtf8("未命名元素") : elementId.trimmed();
}

QString jsonValueToText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        const auto number = value.toDouble();
        const auto integer = static_cast<qint64>(number);
        if (static_cast<double>(integer) == number) {
            return QString::number(integer);
        }
        return QString::number(number, 'g', 15);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

QSet<QString> builtInFieldKeys()
{
    // 旧版标签字段仍由 LabelRenderPlan 提供，不能要求用户在自定义 schema 中重复声明。
    return QSet<QString>{
        QStringLiteral("identifierText"),
        QStringLiteral("productName"),
        QStringLiteral("finishedWeightLabel"),
        QStringLiteral("finishedWeightValue"),
        QStringLiteral("finishedWeightText"),
        QStringLiteral("roughWeightLabel"),
        QStringLiteral("roughWeightValue"),
        QStringLiteral("roughWeightText"),
        QStringLiteral("salesCode"),
        QStringLiteral("standardText"),
        QStringLiteral("addressText"),
        QStringLiteral("qrPayload"),
        QStringLiteral("goldPurityText"),
        QStringLiteral("priceText"),
        QStringLiteral("additionalPriceText"),
        QStringLiteral("tagWeightText"),
        QStringLiteral("qrNote"),
        QStringLiteral("footerText"),
    };
}

QSet<QString> schemaFieldKeys(const QList<FieldDefinition>& fields)
{
    QSet<QString> result;
    for (const auto& field : fields) {
        const auto key = field.key.trimmed();
        if (!key.isEmpty()) {
            result.insert(key);
        }
    }
    return result;
}

const FieldDefinition* findField(const QList<FieldDefinition>& fields, const QString& key)
{
    const auto normalized = key.trimmed();
    for (const auto& field : fields) {
        if (field.key.trimmed() == normalized) {
            return &field;
        }
    }
    return nullptr;
}

QString validationFieldValue(const TemplateDocument& document, const QJsonObject& fieldValues, const QString& key)
{
    const auto normalized = key.trimmed();
    if (normalized.isEmpty()) {
        return QString();
    }
    if (fieldValues.contains(normalized)) {
        return jsonValueToText(fieldValues.value(normalized)).trimmed();
    }

    const auto* field = findField(document.fieldSchema, normalized);
    if (field == nullptr) {
        return QString();
    }
    if (!field->defaultValue.trimmed().isEmpty()) {
        return field->defaultValue.trimmed();
    }
    return field->sampleValue.trimmed();
}

QString interpolateValidationText(const TemplateDocument& document, const QJsonObject& fieldValues, const QString& text)
{
    QString result;
    result.reserve(text.size());

    int cursor = 0;
    while (cursor < text.size()) {
        const auto placeholderStart = text.indexOf(QStringLiteral("${"), cursor);
        if (placeholderStart < 0) {
            result.append(text.mid(cursor));
            break;
        }

        result.append(text.mid(cursor, placeholderStart - cursor));
        const auto placeholderEnd = text.indexOf(QLatin1Char('}'), placeholderStart + 2);
        if (placeholderEnd < 0) {
            result.append(text.mid(placeholderStart));
            break;
        }

        const auto fieldKey = text.mid(placeholderStart + 2, placeholderEnd - placeholderStart - 2).trimmed();
        // 二维码容量校验必须使用实际替换后的内容，和渲染阶段保持一致。
        result.append(validationFieldValue(document, fieldValues, fieldKey));
        cursor = placeholderEnd + 1;
    }

    return result;
}

bool isKnownFieldKey(const QString& key, const QSet<QString>& schemaKeys)
{
    const auto normalized = key.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    if (normalized.startsWith(QStringLiteral("partRow:"))) {
        return true;
    }
    return builtInFieldKeys().contains(normalized) || schemaKeys.contains(normalized);
}

void appendIssue(
    TemplateValidationResult* result,
    const QString& code,
    const QString& message,
    const QString& layerId,
    const QString& elementId)
{
    result->issues.append(TemplateValidationIssue{
        TemplateValidationSeverity::Error,
        code,
        message,
        layerId,
        elementId,
    });
}

void validateBounds(
    TemplateValidationResult* result,
    const QString& layerId,
    const QString& elementId,
    const QRectF& bounds,
    const QSizeF& paperSizeMm)
{
    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        appendIssue(
            result,
            QStringLiteral("ELEMENT_INVALID_SIZE"),
            QString::fromUtf8("元素 %1 的宽高必须大于 0").arg(issueTargetName(elementId)),
            layerId,
            elementId);
        return;
    }

    const auto right = bounds.x() + bounds.width();
    const auto bottom = bounds.y() + bounds.height();
    if (bounds.x() < -kGeometryEpsilon
        || bounds.y() < -kGeometryEpsilon
        || right > paperSizeMm.width() + kGeometryEpsilon
        || bottom > paperSizeMm.height() + kGeometryEpsilon) {
        appendIssue(
            result,
            QStringLiteral("ELEMENT_OUT_OF_PAPER"),
            QString::fromUtf8("元素 %1 超出纸张范围").arg(issueTargetName(elementId)),
            layerId,
            elementId);
    }
}

void validateFieldReference(
    TemplateValidationResult* result,
    const QString& layerId,
    const QString& elementId,
    const QString& fieldKey,
    const QSet<QString>& schemaKeys)
{
    const auto normalized = fieldKey.trimmed();
    if (normalized.isEmpty()) {
        appendIssue(
            result,
            QStringLiteral("FIELD_KEY_EMPTY"),
            QString::fromUtf8("元素 %1 缺少绑定字段").arg(issueTargetName(elementId)),
            layerId,
            elementId);
        return;
    }

    if (!isKnownFieldKey(normalized, schemaKeys)) {
        appendIssue(
            result,
            QStringLiteral("UNKNOWN_FIELD"),
            QString::fromUtf8("元素 %1 绑定了未声明字段 %2").arg(issueTargetName(elementId), normalized),
            layerId,
            elementId);
    }
}

QString resolvedQrPayload(
    const TemplateElement& element,
    const TemplateDocument& document,
    const QJsonObject& fieldValues)
{
    const auto staticPayload = element.payload.trimmed();
    if (!staticPayload.isEmpty()) {
        return interpolateValidationText(document, fieldValues, staticPayload).trimmed();
    }

    const auto key = element.fieldKey.trimmed();
    return validationFieldValue(document, fieldValues, key);
}

void validateQrPayload(
    TemplateValidationResult* result,
    const TemplateDocument& document,
    const QString& layerId,
    const TemplateElement& element,
    const QJsonObject& fieldValues)
{
    const auto payload = resolvedQrPayload(element, document, fieldValues);
    if (payload.isEmpty()) {
        return;
    }

    const auto payloadBytes = payload.toUtf8().size();
    if (payloadBytes > TemplateDocumentValidator::kMaxQrPayloadBytes) {
        appendIssue(
            result,
            QStringLiteral("QR_PAYLOAD_TOO_LONG"),
            QString::fromUtf8("二维码 %1 内容为 %2 个 UTF-8 字节，当前最多支持 %3 个")
                .arg(issueTargetName(element.id))
                .arg(payloadBytes)
                .arg(TemplateDocumentValidator::kMaxQrPayloadBytes),
            layerId,
            element.id);
    }
}

void validateElement(
    TemplateValidationResult* result,
    const TemplateDocument& document,
    const TemplateLayer& layer,
    const TemplateElement& element,
    const QSizeF& paperSizeMm,
    const QSet<QString>& schemaKeys,
    const QJsonObject& fieldValues)
{
    validateBounds(result, layer.id, element.id, QRectF(element.x, element.y, element.width, element.height), paperSizeMm);

    if (element.type == TemplateElementType::BoundField) {
        validateFieldReference(result, layer.id, element.id, element.fieldKey, schemaKeys);
    }
    if (element.type == TemplateElementType::QrCode) {
        if (element.payload.trimmed().isEmpty()) {
            validateFieldReference(result, layer.id, element.id, element.fieldKey, schemaKeys);
        }
        validateQrPayload(result, document, layer.id, element, fieldValues);
    }
}

void validateTableColumns(TemplateValidationResult* result, const TemplateLayer& layer, const TableElement& table)
{
    if (table.dataPath.trimmed().isEmpty()) {
        appendIssue(
            result,
            QStringLiteral("TABLE_DATA_PATH_EMPTY"),
            QString::fromUtf8("表格 %1 缺少数据路径").arg(issueTargetName(table.id)),
            layer.id,
            table.id);
    }
    if (table.columns.isEmpty()) {
        appendIssue(
            result,
            QStringLiteral("TABLE_COLUMNS_EMPTY"),
            QString::fromUtf8("表格 %1 至少需要一列").arg(issueTargetName(table.id)),
            layer.id,
            table.id);
        return;
    }
    if (table.headerRowHeightMm <= 0.0 || table.detailRowHeightMm <= 0.0) {
        appendIssue(
            result,
            QStringLiteral("TABLE_ROW_HEIGHT_INVALID"),
            QString::fromUtf8("表格 %1 的表头和明细行高必须大于 0").arg(issueTargetName(table.id)),
            layer.id,
            table.id);
    }

    double fixedColumnWidth = 0.0;
    QSet<QString> columnIds;
    for (const auto& column : table.columns) {
        const auto columnId = column.id.trimmed();
        const auto fieldKey = column.fieldKey.trimmed();
        if (columnId.isEmpty() || columnIds.contains(columnId)) {
            appendIssue(
                result,
                QStringLiteral("TABLE_COLUMN_ID_INVALID"),
                QString::fromUtf8("表格 %1 存在空列 id 或重复列 id").arg(issueTargetName(table.id)),
                layer.id,
                table.id);
        }
        if (!columnId.isEmpty()) {
            columnIds.insert(columnId);
        }
        if (fieldKey.isEmpty()) {
            appendIssue(
                result,
                QStringLiteral("TABLE_COLUMN_FIELD_EMPTY"),
                QString::fromUtf8("表格 %1 存在未绑定字段的列").arg(issueTargetName(table.id)),
                layer.id,
                table.id);
        }
        if (column.widthMm <= 0.0) {
            appendIssue(
                result,
                QStringLiteral("TABLE_COLUMN_WIDTH_INVALID"),
                QString::fromUtf8("表格 %1 的列宽必须大于 0").arg(issueTargetName(table.id)),
                layer.id,
                table.id);
        }
        if (column.widthMode == TableColumnWidthMode::Flex) {
            if (column.flexWeight <= 0.0) {
                appendIssue(
                    result,
                    QStringLiteral("TABLE_COLUMN_FLEX_WEIGHT_INVALID"),
                    QString::fromUtf8("表格 %1 的弹性列权重必须大于 0").arg(issueTargetName(table.id)),
                    layer.id,
                    table.id);
            }
        } else {
            // 固定列先占用明确宽度，弹性列只分配剩余空间，因此不参与固定宽度超限判断。
            fixedColumnWidth += std::max(0.0, column.widthMm);
        }
    }

    if (fixedColumnWidth > table.width + kGeometryEpsilon) {
        appendIssue(
            result,
            QStringLiteral("TABLE_COLUMNS_TOO_WIDE"),
            QString::fromUtf8("表格 %1 的列宽总和超过表格宽度").arg(issueTargetName(table.id)),
            layer.id,
            table.id);
    }
}

void validateTable(
    TemplateValidationResult* result,
    const TemplateLayer& layer,
    const TableElement& table,
    const QSizeF& paperSizeMm)
{
    validateBounds(result, layer.id, table.id, QRectF(table.x, table.y, table.width, table.height), paperSizeMm);
    validateTableColumns(result, layer, table);
}

} // namespace

bool TemplateValidationResult::hasErrors() const
{
    return std::any_of(issues.cbegin(), issues.cend(), [](const TemplateValidationIssue& issue) {
        return issue.severity == TemplateValidationSeverity::Error;
    });
}

bool TemplateValidationResult::containsCode(const QString& code) const
{
    return std::any_of(issues.cbegin(), issues.cend(), [&code](const TemplateValidationIssue& issue) {
        return issue.code == code;
    });
}

QStringList TemplateValidationResult::errorMessages() const
{
    QStringList messages;
    for (const auto& issue : issues) {
        if (issue.severity == TemplateValidationSeverity::Error) {
            messages.append(issue.message);
        }
    }
    return messages;
}

QString TemplateValidationResult::firstErrorMessage() const
{
    const auto messages = errorMessages();
    return messages.isEmpty() ? QString() : messages.first();
}

TemplateValidationResult TemplateDocumentValidator::validate(
    const TemplateDocument& document,
    const QSizeF& paperSizeMm,
    const QJsonObject& fieldValues) const
{
    // 校验器只判断模板结构和设计期可确定的内容，不读取文件，也不触发打印副作用。
    TemplateValidationResult result;
    if (paperSizeMm.width() <= 0.0 || paperSizeMm.height() <= 0.0) {
        appendIssue(
            &result,
            QStringLiteral("PAPER_SIZE_INVALID"),
            QString::fromUtf8("纸张尺寸必须大于 0"),
            QString(),
            QString());
        return result;
    }

    const auto schemaKeys = schemaFieldKeys(document.fieldSchema);
    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }
        for (const auto& element : layer.elements) {
            if (element.visible) {
                validateElement(&result, document, layer, element, paperSizeMm, schemaKeys, fieldValues);
            }
        }
        for (const auto& table : layer.tables) {
            if (table.visible) {
                validateTable(&result, layer, table, paperSizeMm);
            }
        }
    }

    return result;
}

} // 命名空间 sleekpr::core
