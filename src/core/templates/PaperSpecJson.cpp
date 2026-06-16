#include "sleekpr/core/templates/PaperSpecJson.h"

#include <QList>

namespace sleekpr::core {

namespace {

QString kindToString(PaperSpecKind kind)
{
    switch (kind) {
    case PaperSpecKind::Label:
        return QStringLiteral("label");
    case PaperSpecKind::Roll:
        return QStringLiteral("roll");
    case PaperSpecKind::Sheet:
        return QStringLiteral("sheet");
    }
    return QStringLiteral("label");
}

PaperSpecKind kindFromString(const QString& value)
{
    if (value == QStringLiteral("roll")) {
        return PaperSpecKind::Roll;
    }
    if (value == QStringLiteral("sheet")) {
        return PaperSpecKind::Sheet;
    }
    return PaperSpecKind::Label;
}

bool isKnownKind(const QString& value)
{
    return value == QStringLiteral("label")
        || value == QStringLiteral("roll")
        || value == QStringLiteral("sheet");
}

QString orientationToString(PaperOrientation orientation)
{
    switch (orientation) {
    case PaperOrientation::Portrait:
        return QStringLiteral("portrait");
    case PaperOrientation::Landscape:
        return QStringLiteral("landscape");
    }
    return QStringLiteral("portrait");
}

PaperOrientation orientationFromString(const QString& value)
{
    if (value == QStringLiteral("landscape")) {
        return PaperOrientation::Landscape;
    }
    return PaperOrientation::Portrait;
}

bool isKnownOrientation(const QString& value)
{
    return value == QStringLiteral("portrait") || value == QStringLiteral("landscape");
}

QJsonObject gridToJson(const LabelGridSpec& grid)
{
    QJsonObject json;
    json["enabled"] = grid.enabled;
    json["rows"] = grid.rows;
    json["columns"] = grid.columns;
    json["horizontalGapMm"] = grid.horizontalGapMm;
    json["verticalGapMm"] = grid.verticalGapMm;
    return json;
}

LabelGridSpec gridFromJson(const QJsonObject& json)
{
    LabelGridSpec grid;
    grid.enabled = json["enabled"].toBool(grid.enabled);
    grid.rows = json["rows"].toInt(grid.rows);
    grid.columns = json["columns"].toInt(grid.columns);
    grid.horizontalGapMm = json["horizontalGapMm"].toDouble(grid.horizontalGapMm);
    grid.verticalGapMm = json["verticalGapMm"].toDouble(grid.verticalGapMm);
    return grid;
}

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // 匿名命名空间

QJsonObject PaperSpecJson::toJson(const PaperSpec& spec)
{
    QJsonObject json;
    json["schemaVersion"] = spec.schemaVersion;
    json["id"] = spec.id;
    json["name"] = spec.name;
    json["kind"] = kindToString(spec.kind);
    json["orientation"] = orientationToString(spec.orientation);
    json["widthMm"] = spec.widthMm;
    json["heightMm"] = spec.heightMm;
    json["marginLeftMm"] = spec.marginLeftMm;
    json["marginTopMm"] = spec.marginTopMm;
    json["marginRightMm"] = spec.marginRightMm;
    json["marginBottomMm"] = spec.marginBottomMm;
    json["defaultDpi"] = spec.defaultDpi;
    json["labelGrid"] = gridToJson(spec.labelGrid);
    return json;
}

PaperSpec PaperSpecJson::fromJson(const QJsonObject& json)
{
    PaperSpec spec;
    spec.schemaVersion = json["schemaVersion"].toInt(spec.schemaVersion);
    spec.id = json["id"].toString();
    spec.name = json["name"].toString();
    spec.kind = kindFromString(json["kind"].toString(kindToString(spec.kind)));
    spec.orientation = orientationFromString(json["orientation"].toString(orientationToString(spec.orientation)));
    spec.widthMm = json["widthMm"].toDouble(spec.widthMm);
    spec.heightMm = json["heightMm"].toDouble(spec.heightMm);
    spec.marginLeftMm = json["marginLeftMm"].toDouble(spec.marginLeftMm);
    spec.marginTopMm = json["marginTopMm"].toDouble(spec.marginTopMm);
    spec.marginRightMm = json["marginRightMm"].toDouble(spec.marginRightMm);
    spec.marginBottomMm = json["marginBottomMm"].toDouble(spec.marginBottomMm);
    spec.defaultDpi = json["defaultDpi"].toDouble(spec.defaultDpi);
    spec.labelGrid = gridFromJson(json["labelGrid"].toObject());
    return spec;
}

bool PaperSpecJson::validate(const QJsonObject& json, QString* errorMessage)
{
    const int schemaVersion = json["schemaVersion"].toInt(-1);
    if (schemaVersion != 1) {
        setError(errorMessage, QString::fromUtf8("纸张规格 schemaVersion 不受支持"));
        return false;
    }

    const auto id = json["id"].toString();
    if (id.trimmed().isEmpty() || id != id.trimmed()) {
        setError(errorMessage, QString::fromUtf8("纸张规格 id 不能为空或包含首尾空白"));
        return false;
    }

    const auto kind = json["kind"].toString();
    if (!isKnownKind(kind)) {
        setError(errorMessage, QString::fromUtf8("纸张规格 kind 不受支持"));
        return false;
    }

    const auto orientation = json["orientation"].toString();
    if (!isKnownOrientation(orientation)) {
        setError(errorMessage, QString::fromUtf8("纸张方向 orientation 不受支持"));
        return false;
    }

    if (json["widthMm"].toDouble() <= 0.0 || json["heightMm"].toDouble() <= 0.0) {
        setError(errorMessage, QString::fromUtf8("纸张宽高必须大于 0"));
        return false;
    }

    if (json["defaultDpi"].toDouble(300.0) <= 0.0) {
        setError(errorMessage, QString::fromUtf8("纸张默认 DPI 必须大于 0"));
        return false;
    }

    const QList<QString> marginKeys{
        QStringLiteral("marginLeftMm"),
        QStringLiteral("marginTopMm"),
        QStringLiteral("marginRightMm"),
        QStringLiteral("marginBottomMm"),
    };
    for (const auto& key : marginKeys) {
        if (json[key].toDouble(0.0) < 0.0) {
            setError(errorMessage, QString::fromUtf8("纸张边距不能小于 0"));
            return false;
        }
    }

    const auto grid = json["labelGrid"].toObject();
    if (grid["enabled"].toBool(false)) {
        if (grid["rows"].toInt(0) < 1 || grid["columns"].toInt(0) < 1) {
            setError(errorMessage, QString::fromUtf8("多标签网格行列必须大于 0"));
            return false;
        }
        if (grid["horizontalGapMm"].toDouble(0.0) < 0.0 || grid["verticalGapMm"].toDouble(0.0) < 0.0) {
            setError(errorMessage, QString::fromUtf8("多标签网格间距不能小于 0"));
            return false;
        }
    }

    return true;
}

} // 命名空间 sleekpr::core
