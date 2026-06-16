#include "sleekpr/core/templates/TemplateDocumentJson.h"

#include <QJsonArray>
#include <QSet>

namespace sleekpr::core {

namespace {

double positiveOrDefault(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}

QString elementTypeToString(TemplateElementType type)
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
    }
    return QStringLiteral("fixedText");
}

bool isKnownElementType(const QString& value)
{
    return value == QStringLiteral("fixedText")
        || value == QStringLiteral("boundField")
        || value == QStringLiteral("qrCode")
        || value == QStringLiteral("rectangle");
}

TemplateElementType elementTypeFromString(const QString& value)
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
    return TemplateElementType::FixedText;
}

QJsonObject elementToJson(const TemplateElement& element)
{
    QJsonObject json;
    json["id"] = element.id;
    json["type"] = elementTypeToString(element.type);
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
    json["rotationDegrees"] = element.rotationDegrees;
    json["maxLines"] = element.maxLines;
    json["ellipsis"] = element.ellipsis;
    return json;
}

TemplateElement elementFromJson(const QJsonObject& json)
{
    TemplateElement element;
    element.id = json["id"].toString();
    element.type = elementTypeFromString(json["type"].toString());
    element.displayName = json["displayName"].toString();
    element.layerId = json["layerId"].toString();
    element.zIndex = json["zIndex"].toInt(element.zIndex);
    element.visible = json["visible"].toBool(element.visible);
    element.locked = json["locked"].toBool(element.locked);
    element.x = json["x"].toDouble(element.x);
    element.y = json["y"].toDouble(element.y);
    element.width = json["width"].toDouble(element.width);
    element.height = json["height"].toDouble(element.height);
    element.text = json["text"].toString();
    element.fieldKey = json["fieldKey"].toString();
    element.payload = json["payload"].toString();
    element.fontSizePt = json["fontSizePt"].toDouble(element.fontSizePt);
    element.bold = json["bold"].toBool(element.bold);
    element.rotationDegrees = json["rotationDegrees"].toDouble(element.rotationDegrees);
    element.maxLines = json["maxLines"].toInt(element.maxLines);
    element.ellipsis = json["ellipsis"].toBool(element.ellipsis);
    return element;
}

QJsonArray elementsToJson(const QList<TemplateElement>& elements)
{
    QJsonArray result;
    for (const auto& element : elements) {
        result.append(elementToJson(element));
    }
    return result;
}

QList<TemplateElement> elementsFromJson(const QJsonArray& json)
{
    QList<TemplateElement> result;
    for (const auto& value : json) {
        result.append(elementFromJson(value.toObject()));
    }
    return result;
}

QJsonObject layerToJson(const TemplateLayer& layer)
{
    QJsonObject json;
    json["id"] = layer.id;
    json["name"] = layer.name;
    json["visible"] = layer.visible;
    json["locked"] = layer.locked;
    json["elements"] = elementsToJson(layer.elements);
    return json;
}

TemplateLayer layerFromJson(const QJsonObject& json)
{
    TemplateLayer layer;
    layer.id = json["id"].toString();
    layer.name = json["name"].toString();
    layer.visible = json["visible"].toBool(layer.visible);
    layer.locked = json["locked"].toBool(layer.locked);
    layer.elements = elementsFromJson(json["elements"].toArray());
    return layer;
}

QJsonArray layersToJson(const QList<TemplateLayer>& layers)
{
    QJsonArray result;
    for (const auto& layer : layers) {
        result.append(layerToJson(layer));
    }
    return result;
}

QList<TemplateLayer> layersFromJson(const QJsonArray& json)
{
    QList<TemplateLayer> result;
    for (const auto& value : json) {
        result.append(layerFromJson(value.toObject()));
    }
    return result;
}

QJsonObject versionToJson(const TemplateVersion& version)
{
    QJsonObject json;
    json["id"] = version.id;
    json["name"] = version.name;
    json["createdAt"] = version.createdAt;
    json["note"] = version.note;
    json["layersSnapshot"] = layersToJson(version.layersSnapshot);
    return json;
}

TemplateVersion versionFromJson(const QJsonObject& json)
{
    TemplateVersion version;
    version.id = json["id"].toString();
    version.name = json["name"].toString();
    version.createdAt = json["createdAt"].toString();
    version.note = json["note"].toString();
    version.layersSnapshot = layersFromJson(json["layersSnapshot"].toArray());
    return version;
}

QJsonArray versionsToJson(const QList<TemplateVersion>& versions)
{
    QJsonArray result;
    for (const auto& version : versions) {
        result.append(versionToJson(version));
    }
    return result;
}

QList<TemplateVersion> versionsFromJson(const QJsonArray& json)
{
    QList<TemplateVersion> result;
    for (const auto& value : json) {
        result.append(versionFromJson(value.toObject()));
    }
    return result;
}

QJsonObject profileToJson(const DeviceProfile& profile)
{
    QJsonObject json;
    json["id"] = profile.id;
    json["printerName"] = profile.printerName;
    json["dpi"] = profile.dpi;
    json["scaleX"] = profile.scaleX;
    json["scaleY"] = profile.scaleY;
    json["offsetX"] = profile.offsetX;
    json["offsetY"] = profile.offsetY;
    json["notes"] = profile.notes;
    return json;
}

DeviceProfile profileFromJson(const QJsonObject& json)
{
    DeviceProfile profile;
    profile.id = json["id"].toString();
    profile.printerName = json["printerName"].toString();
    // 设备校准值参与真实打印换算，读取时钳制为安全正数。
    profile.dpi = positiveOrDefault(json["dpi"].toDouble(300.0), 300.0);
    profile.scaleX = positiveOrDefault(json["scaleX"].toDouble(1.0), 1.0);
    profile.scaleY = positiveOrDefault(json["scaleY"].toDouble(1.0), 1.0);
    profile.offsetX = json["offsetX"].toDouble(profile.offsetX);
    profile.offsetY = json["offsetY"].toDouble(profile.offsetY);
    profile.notes = json["notes"].toString();
    return profile;
}

QJsonArray profilesToJson(const QList<DeviceProfile>& profiles)
{
    QJsonArray result;
    for (const auto& profile : profiles) {
        result.append(profileToJson(profile));
    }
    return result;
}

QList<DeviceProfile> profilesFromJson(const QJsonArray& json)
{
    QList<DeviceProfile> result;
    for (const auto& value : json) {
        result.append(profileFromJson(value.toObject()));
    }
    return result;
}

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool validateLayerCollection(const QJsonArray& layers, const QString& context, QString* errorMessage)
{
    QSet<QString> layerIds;
    QSet<QString> elementIds;

    for (const auto& layerValue : layers) {
        const auto layer = layerValue.toObject();
        const auto layerId = layer["id"].toString();
        if (!layerId.isEmpty()) {
            if (layerIds.contains(layerId)) {
                setError(errorMessage, QStringLiteral("%1图层 id 重复：%2").arg(context, layerId));
                return false;
            }
            layerIds.insert(layerId);
        }

        for (const auto& elementValue : layer["elements"].toArray()) {
            const auto element = elementValue.toObject();
            const auto type = element["type"].toString();
            if (!isKnownElementType(type)) {
                setError(errorMessage, QStringLiteral("%1存在未知元素类型：%2").arg(context, type));
                return false;
            }

            const auto elementId = element["id"].toString();
            if (!elementId.isEmpty()) {
                if (elementIds.contains(elementId)) {
                    setError(errorMessage, QStringLiteral("%1元素 id 重复：%2").arg(context, elementId));
                    return false;
                }
                elementIds.insert(elementId);
            }
        }
    }

    return true;
}

} // namespace

QJsonObject TemplateDocumentJson::toJson(const TemplateDocument& document)
{
    QJsonObject json;
    json["schemaVersion"] = document.schemaVersion;
    json["id"] = document.id;
    json["name"] = document.name;
    json["templateKey"] = document.templateKey;
    json["activeVersionId"] = document.activeVersionId;
    json["layers"] = layersToJson(document.layers);
    json["versions"] = versionsToJson(document.versions);
    json["deviceProfiles"] = profilesToJson(document.deviceProfiles);
    return json;
}

TemplateDocument TemplateDocumentJson::fromJson(const QJsonObject& json)
{
    TemplateDocument document;
    document.schemaVersion = json["schemaVersion"].toInt(document.schemaVersion);
    document.id = json["id"].toString();
    document.name = json["name"].toString();
    document.templateKey = json["templateKey"].toString(document.templateKey);
    document.activeVersionId = json["activeVersionId"].toString();
    document.layers = layersFromJson(json["layers"].toArray());
    document.versions = versionsFromJson(json["versions"].toArray());
    document.deviceProfiles = profilesFromJson(json["deviceProfiles"].toArray());
    return document;
}

bool TemplateDocumentJson::validateForImport(const QJsonObject& json, QString* errorMessage)
{
    if (!json.contains("schemaVersion")) {
        setError(errorMessage, QStringLiteral("模板缺少 schemaVersion"));
        return false;
    }

    const auto schemaVersion = json["schemaVersion"].toInt(-1);
    if (schemaVersion < 1 || schemaVersion > 1) {
        setError(errorMessage, QStringLiteral("模板 schemaVersion 不受支持"));
        return false;
    }

    // 导入入口严格检查结构，避免未知元素类型或重复 id 写入 settings.json。
    if (!validateLayerCollection(json["layers"].toArray(), QStringLiteral("模板"), errorMessage)) {
        return false;
    }

    for (const auto& versionValue : json["versions"].toArray()) {
        if (!validateLayerCollection(
                versionValue.toObject()["layersSnapshot"].toArray(),
                QStringLiteral("版本快照"),
                errorMessage)) {
            return false;
        }
    }

    return true;
}

} // namespace sleekpr::core
