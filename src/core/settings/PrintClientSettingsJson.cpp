#include "sleekpr/core/settings/PrintClientSettingsJson.h"

#include "sleekpr/core/templates/TemplateDocumentJson.h"

#include <QJsonArray>
#include <cmath>

namespace sleekpr::core {

namespace {

QString templateElementTypeToString(TemplateElementType type)
{
    switch (type) {
    case TemplateElementType::FixedText:
        return QStringLiteral("fixedText");
    case TemplateElementType::BoundField:
        return QStringLiteral("boundField");
    case TemplateElementType::QrCode:
        return QStringLiteral("qrCode");
    case TemplateElementType::Rectangle:
        return QStringLiteral("rectangle");
    case TemplateElementType::ArrayGrid:
        return QStringLiteral("arrayGrid");
    }
    return QStringLiteral("fixedText");
}

TemplateElementType templateElementTypeFromString(const QString& value)
{
    if (value == QStringLiteral("boundField")) {
        return TemplateElementType::BoundField;
    }
    if (value == QStringLiteral("qrCode")) {
        return TemplateElementType::QrCode;
    }
    if (value == QStringLiteral("rectangle")) {
        return TemplateElementType::Rectangle;
    }
    if (value == QStringLiteral("arrayGrid")) {
        return TemplateElementType::ArrayGrid;
    }
    return TemplateElementType::FixedText;
}

QJsonObject overrideToJson(const TemplateElementOverride& value)
{
    QJsonObject json;
    // 只写入用户真正覆盖过的字段，避免保存文件把默认模板值固化死。
    if (value.x.has_value()) {
        json["x"] = value.x.value();
    }
    if (value.y.has_value()) {
        json["y"] = value.y.value();
    }
    if (value.fontSizePt.has_value()) {
        json["fontSizePt"] = value.fontSizePt.value();
    }
    if (value.bold.has_value()) {
        json["bold"] = value.bold.value();
    }
    return json;
}

TemplateElementOverride overrideFromJson(const QJsonObject& json)
{
    TemplateElementOverride value;
    // 兼容旧版本的可选字段结构，缺失字段保持 std::nullopt。
    if (json.contains("x")) {
        value.x = json["x"].toDouble();
    }
    if (json.contains("y")) {
        value.y = json["y"].toDouble();
    }
    if (json.contains("fontSizePt")) {
        value.fontSizePt = json["fontSizePt"].toDouble();
    }
    if (json.contains("bold")) {
        value.bold = json["bold"].toBool();
    }
    return value;
}

template <typename T>
void writeOptionalNumber(QJsonObject& json, const QString& key, const std::optional<T>& value)
{
    if (value.has_value()) {
        json[key] = static_cast<double>(value.value());
    }
}

std::optional<qsizetype> optionalSizeFromJson(const QJsonObject& json, const QString& key)
{
    if (!json.contains(key) || !json.value(key).isDouble()) {
        return std::nullopt;
    }

    const auto value = json.value(key).toDouble();
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return static_cast<qsizetype>(std::llround(value));
}

std::optional<int> optionalIntFromJson(const QJsonObject& json, const QString& key)
{
    if (!json.contains(key) || !json.value(key).isDouble()) {
        return std::nullopt;
    }

    const auto value = json.value(key).toDouble();
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return static_cast<int>(std::llround(value));
}

QJsonObject localHttpLimitsToJson(const LocalHttpLimitSettings& limits)
{
    QJsonObject json;
    // 只保存显式覆盖值；运行时默认值继续由 LocalHttpLimits 统一维护。
    writeOptionalNumber(json, QStringLiteral("maxHeaderBytes"), limits.maxHeaderBytes);
    writeOptionalNumber(json, QStringLiteral("maxContentLengthBytes"), limits.maxContentLengthBytes);
    writeOptionalNumber(json, QStringLiteral("maxPreviewBatchItems"), limits.maxPreviewBatchItems);
    writeOptionalNumber(json, QStringLiteral("maxPreviewPages"), limits.maxPreviewPages);
    writeOptionalNumber(json, QStringLiteral("maxPreviewResponseBytes"), limits.maxPreviewResponseBytes);
    return json;
}

LocalHttpLimitSettings localHttpLimitsFromJson(const QJsonObject& json)
{
    LocalHttpLimitSettings limits;
    // 兼容旧 settings.json：没有 localHttpLimits 时保持空覆盖，交给运行时默认值兜底。
    limits.maxHeaderBytes = optionalSizeFromJson(json, QStringLiteral("maxHeaderBytes"));
    limits.maxContentLengthBytes = optionalSizeFromJson(json, QStringLiteral("maxContentLengthBytes"));
    limits.maxPreviewBatchItems = optionalIntFromJson(json, QStringLiteral("maxPreviewBatchItems"));
    limits.maxPreviewPages = optionalIntFromJson(json, QStringLiteral("maxPreviewPages"));
    limits.maxPreviewResponseBytes = optionalSizeFromJson(json, QStringLiteral("maxPreviewResponseBytes"));
    return limits;
}

QJsonObject templateElementToJson(const TemplateElement& element)
{
    QJsonObject json;
    json["id"] = element.id;
    json["type"] = templateElementTypeToString(element.type);
    json["displayName"] = element.displayName;
    json["layerId"] = element.layerId;
    json["zIndex"] = element.zIndex;
    json["visible"] = element.visible;
    json["locked"] = element.locked;
    json["x"] = element.x;
    json["y"] = element.y;
    json["width"] = element.width;
    json["height"] = element.height;
    json["text"] = element.text;
    json["fieldKey"] = element.fieldKey;
    json["payload"] = element.payload;
    json["fontSizePt"] = element.fontSizePt;
    json["bold"] = element.bold;
    json["autoFitFont"] = element.autoFitFont;
    json["autoFitMinFontSizePt"] = element.autoFitMinFontSizePt;
    json["autoFitMaxFontSizePt"] = element.autoFitMaxFontSizePt;
    json["rotationDegrees"] = element.rotationDegrees;
    json["verticalText"] = element.verticalText;
    json["dataPath"] = element.dataPath;
    json["arrayGridRows"] = element.arrayGridRows;
    json["arrayGridColumns"] = element.arrayGridColumns;
    json["arrayGridRowHeightMm"] = element.arrayGridRowHeightMm;
    json["arrayGridCellTemplate"] = element.arrayGridCellTemplate;
    json["arrayGridDrawBorders"] = element.arrayGridDrawBorders;
    json["maxLines"] = element.maxLines;
    json["ellipsis"] = element.ellipsis;
    return json;
}

TemplateElement templateElementFromJson(const QJsonObject& json)
{
    TemplateElement element;
    element.id = json["id"].toString();
    element.type = templateElementTypeFromString(json["type"].toString());
    element.displayName = json["displayName"].toString();
    element.layerId = json["layerId"].toString();
    if (json.contains("zIndex")) {
        element.zIndex = json["zIndex"].toInt(element.zIndex);
    }
    if (json.contains("visible")) {
        element.visible = json["visible"].toBool(element.visible);
    }
    if (json.contains("locked")) {
        element.locked = json["locked"].toBool(element.locked);
    }
    if (json.contains("x")) {
        element.x = json["x"].toDouble(element.x);
    }
    if (json.contains("y")) {
        element.y = json["y"].toDouble(element.y);
    }
    if (json.contains("width")) {
        element.width = json["width"].toDouble(element.width);
    }
    if (json.contains("height")) {
        element.height = json["height"].toDouble(element.height);
    }
    element.text = json["text"].toString();
    element.fieldKey = json["fieldKey"].toString();
    element.payload = json["payload"].toString();
    if (json.contains("fontSizePt")) {
        element.fontSizePt = json["fontSizePt"].toDouble(element.fontSizePt);
    }
    if (json.contains("bold")) {
        element.bold = json["bold"].toBool(element.bold);
    }
    if (json.contains("autoFitFont")) {
        element.autoFitFont = json["autoFitFont"].toBool(element.autoFitFont);
    }
    if (json.contains("autoFitMinFontSizePt")) {
        element.autoFitMinFontSizePt = json["autoFitMinFontSizePt"].toDouble(element.autoFitMinFontSizePt);
    }
    if (json.contains("autoFitMaxFontSizePt")) {
        element.autoFitMaxFontSizePt = json["autoFitMaxFontSizePt"].toDouble(element.autoFitMaxFontSizePt);
    }
    if (json.contains("rotationDegrees")) {
        element.rotationDegrees = json["rotationDegrees"].toDouble(element.rotationDegrees);
    }
    if (json.contains("verticalText")) {
        element.verticalText = json["verticalText"].toBool(element.verticalText);
    }
    element.dataPath = json["dataPath"].toString(element.dataPath);
    if (json.contains("arrayGridRows")) {
        element.arrayGridRows = json["arrayGridRows"].toInt(element.arrayGridRows);
    }
    if (json.contains("arrayGridColumns")) {
        element.arrayGridColumns = json["arrayGridColumns"].toInt(element.arrayGridColumns);
    }
    if (json.contains("arrayGridRowHeightMm")) {
        element.arrayGridRowHeightMm = json["arrayGridRowHeightMm"].toDouble(element.arrayGridRowHeightMm);
    }
    element.arrayGridCellTemplate = json["arrayGridCellTemplate"].toString(element.arrayGridCellTemplate);
    if (json.contains("arrayGridDrawBorders")) {
        element.arrayGridDrawBorders = json["arrayGridDrawBorders"].toBool(element.arrayGridDrawBorders);
    }
    if (json.contains("maxLines")) {
        element.maxLines = json["maxLines"].toInt(element.maxLines);
    }
    if (json.contains("ellipsis")) {
        element.ellipsis = json["ellipsis"].toBool(element.ellipsis);
    }
    return element;
}

} // 命名空间

QJsonObject PrintClientSettingsJson::toJson(const PrintClientSettings& settings)
{
    QJsonObject root;
    root["defaultPrinter"] = settings.defaultPrinter;

    QJsonObject offset;
    offset["x"] = settings.labelOffset.x;
    offset["y"] = settings.labelOffset.y;
    root["labelOffset"] = offset;

    QJsonArray origins;
    for (const auto& origin : settings.allowedOrigins) {
        origins.append(origin);
    }
    root["allowedOrigins"] = origins;

    const auto localHttpLimits = localHttpLimitsToJson(settings.localHttpLimits);
    if (!localHttpLimits.isEmpty()) {
        root["localHttpLimits"] = localHttpLimits;
    }

    QJsonObject templateOverrides;
    for (auto templateIt = settings.templateOverrides.begin(); templateIt != settings.templateOverrides.end(); ++templateIt) {
        QJsonObject elementOverrides;
        for (auto elementIt = templateIt.value().begin(); elementIt != templateIt.value().end(); ++elementIt) {
            // 覆盖仍按默认元素 key 保存，确保老配置和旧设置窗口行为继续可用。
            elementOverrides[elementIt.key()] = overrideToJson(elementIt.value());
        }
        templateOverrides[templateIt.key()] = elementOverrides;
    }
    root["templateOverrides"] = templateOverrides;

    QJsonObject templateElements;
    for (auto templateIt = settings.templateElements.begin(); templateIt != settings.templateElements.end(); ++templateIt) {
        QJsonArray elements;
        for (const auto& element : templateIt.value()) {
            elements.append(templateElementToJson(element));
        }
        templateElements[templateIt.key()] = elements;
    }
    root["templateElements"] = templateElements;

    QJsonObject templateDocuments;
    for (auto templateIt = settings.templateDocuments.begin(); templateIt != settings.templateDocuments.end(); ++templateIt) {
        // 完整模板文档使用集中 JSON 映射，避免 settings 和导入导出字段漂移。
        templateDocuments[templateIt.key()] = TemplateDocumentJson::toJson(templateIt.value());
    }
    root["templateDocuments"] = templateDocuments;

    return root;
}

PrintClientSettings PrintClientSettingsJson::fromJson(const QJsonObject& json)
{
    PrintClientSettings settings;
    settings.defaultPrinter = json["defaultPrinter"].toString();

    const auto offset = json["labelOffset"].toObject();
    settings.labelOffset.x = offset["x"].toDouble();
    settings.labelOffset.y = offset["y"].toDouble();

    for (const auto origin : json["allowedOrigins"].toArray()) {
        settings.allowedOrigins.append(origin.toString());
    }

    settings.localHttpLimits = localHttpLimitsFromJson(json["localHttpLimits"].toObject());

    const auto templates = json["templateOverrides"].toObject();
    for (auto templateIt = templates.begin(); templateIt != templates.end(); ++templateIt) {
        const auto elements = templateIt.value().toObject();
        for (auto elementIt = elements.begin(); elementIt != elements.end(); ++elementIt) {
            // 第一层 key 是模板名，第二层 key 是默认元素名，与历史 JSON 结构保持一致。
            settings.templateOverrides[templateIt.key()][elementIt.key()] =
                overrideFromJson(elementIt.value().toObject());
        }
    }

    const auto customTemplates = json["templateElements"].toObject();
    for (auto templateIt = customTemplates.begin(); templateIt != customTemplates.end(); ++templateIt) {
        QList<TemplateElement> elements;
        for (const auto& elementValue : templateIt.value().toArray()) {
            const auto element = templateElementFromJson(elementValue.toObject());
            if (!element.id.trimmed().isEmpty()) {
                elements.append(element);
            }
        }
        settings.templateElements[templateIt.key()] = elements;
    }

    const auto templateDocuments = json["templateDocuments"].toObject();
    for (auto templateIt = templateDocuments.begin(); templateIt != templateDocuments.end(); ++templateIt) {
        settings.templateDocuments[templateIt.key()] =
            TemplateDocumentJson::fromJson(templateIt.value().toObject());
    }

    return settings;
}

} // 命名空间 sleekpr::core
