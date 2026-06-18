#include "sleekpr/core/templates/TableElementJson.h"

#include <QJsonArray>
#include <QSet>

namespace sleekpr::core {

namespace {

QString alignmentToString(TableCellAlignment alignment)
{
    switch (alignment) {
    case TableCellAlignment::Left:
        return QStringLiteral("left");
    case TableCellAlignment::Center:
        return QStringLiteral("center");
    case TableCellAlignment::Right:
        return QStringLiteral("right");
    }
    return QStringLiteral("left");
}

TableCellAlignment alignmentFromString(const QString& value)
{
    if (value == QStringLiteral("center")) {
        return TableCellAlignment::Center;
    }
    if (value == QStringLiteral("right")) {
        return TableCellAlignment::Right;
    }
    return TableCellAlignment::Left;
}

QString widthModeToString(TableColumnWidthMode widthMode)
{
    switch (widthMode) {
    case TableColumnWidthMode::Fixed:
        return QStringLiteral("fixed");
    case TableColumnWidthMode::Flex:
        return QStringLiteral("flex");
    }
    return QStringLiteral("fixed");
}

TableColumnWidthMode widthModeFromString(const QString& value)
{
    if (value == QStringLiteral("flex")) {
        return TableColumnWidthMode::Flex;
    }
    return TableColumnWidthMode::Fixed;
}

bool isKnownAlignment(const QString& value)
{
    return value == QStringLiteral("left")
        || value == QStringLiteral("center")
        || value == QStringLiteral("right");
}

bool isKnownWidthMode(const QString& value)
{
    return value == QStringLiteral("fixed") || value == QStringLiteral("flex");
}

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

QJsonObject columnToJson(const TableColumn& column)
{
    QJsonObject json;
    json["id"] = column.id;
    json["title"] = column.title;
    json["fieldKey"] = column.fieldKey;
    json["widthMm"] = column.widthMm;
    json["widthMode"] = widthModeToString(column.widthMode);
    json["flexWeight"] = column.flexWeight;
    json["alignment"] = alignmentToString(column.alignment);
    json["fontSizePt"] = column.fontSizePt;
    json["bold"] = column.bold;
    json["ellipsis"] = column.ellipsis;
    return json;
}

TableColumn columnFromJson(const QJsonObject& json)
{
    TableColumn column;
    column.id = json["id"].toString();
    column.title = json["title"].toString();
    column.fieldKey = json["fieldKey"].toString();
    column.widthMm = json["widthMm"].toDouble(column.widthMm);
    column.widthMode = widthModeFromString(json["widthMode"].toString(widthModeToString(column.widthMode)));
    column.flexWeight = json["flexWeight"].toDouble(column.flexWeight);
    column.alignment = alignmentFromString(json["alignment"].toString(alignmentToString(column.alignment)));
    column.fontSizePt = json["fontSizePt"].toDouble(column.fontSizePt);
    column.bold = json["bold"].toBool(column.bold);
    column.ellipsis = json["ellipsis"].toBool(column.ellipsis);
    return column;
}

QJsonArray columnsToJson(const QList<TableColumn>& columns)
{
    QJsonArray result;
    for (const auto& column : columns) {
        result.append(columnToJson(column));
    }
    return result;
}

QList<TableColumn> columnsFromJson(const QJsonArray& json)
{
    QList<TableColumn> result;
    for (const auto& value : json) {
        result.append(columnFromJson(value.toObject()));
    }
    return result;
}

bool validateColumn(const QJsonObject& column, QSet<QString>& columnIds, QString* errorMessage)
{
    const auto columnId = column["id"].toString();
    if (isBlankOrPadded(columnId)) {
        setError(errorMessage, QStringLiteral("表格列 id 不能为空，且不能包含首尾空白"));
        return false;
    }
    if (columnIds.contains(columnId)) {
        setError(errorMessage, QStringLiteral("表格列 id 重复：%1").arg(columnId));
        return false;
    }
    columnIds.insert(columnId);

    const auto fieldKey = column["fieldKey"].toString();
    if (isBlankOrPadded(fieldKey)) {
        setError(errorMessage, QStringLiteral("表格列 fieldKey 不能为空，且不能包含首尾空白：%1").arg(columnId));
        return false;
    }

    if (column["widthMm"].toDouble(0.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格列宽必须大于 0：%1").arg(columnId));
        return false;
    }

    const auto widthMode = column["widthMode"].toString(QStringLiteral("fixed"));
    if (!isKnownWidthMode(widthMode)) {
        setError(errorMessage, QString::fromUtf8("表格列宽模式不受支持：%1").arg(widthMode));
        return false;
    }
    if (widthMode == QStringLiteral("flex") && column["flexWeight"].toDouble(1.0) <= 0.0) {
        setError(errorMessage, QString::fromUtf8("表格弹性列权重必须大于 0：%1").arg(columnId));
        return false;
    }

    const auto alignment = column["alignment"].toString(QStringLiteral("left"));
    if (!isKnownAlignment(alignment)) {
        setError(errorMessage, QStringLiteral("表格列对齐方式不受支持：%1").arg(alignment));
        return false;
    }

    return true;
}

} // 命名空间

QJsonObject TableElementJson::toJson(const TableElement& table)
{
    QJsonObject json;
    json["id"] = table.id;
    json["layerId"] = table.layerId;
    json["zIndex"] = table.zIndex;
    json["visible"] = table.visible;
    json["locked"] = table.locked;
    json["displayName"] = table.displayName;
    json["x"] = table.x;
    json["y"] = table.y;
    json["width"] = table.width;
    json["height"] = table.height;
    json["dataPath"] = table.dataPath;
    json["headerRowHeightMm"] = table.headerRowHeightMm;
    json["detailRowHeightMm"] = table.detailRowHeightMm;
    json["drawBorders"] = table.drawBorders;
    json["repeatHeaderOnPage"] = table.repeatHeaderOnPage;
    json["columns"] = columnsToJson(table.columns);
    return json;
}

TableElement TableElementJson::fromJson(const QJsonObject& json, const QString& parentLayerId)
{
    TableElement table;
    table.id = json["id"].toString();
    table.layerId = json["layerId"].toString();
    if (table.layerId.trimmed().isEmpty() && !parentLayerId.trimmed().isEmpty()) {
        table.layerId = parentLayerId;
    }
    table.zIndex = json["zIndex"].toInt(table.zIndex);
    table.visible = json["visible"].toBool(table.visible);
    table.locked = json["locked"].toBool(table.locked);
    table.displayName = json["displayName"].toString();
    table.x = json["x"].toDouble(table.x);
    table.y = json["y"].toDouble(table.y);
    table.width = json["width"].toDouble(table.width);
    table.height = json["height"].toDouble(table.height);
    table.dataPath = json["dataPath"].toString();
    table.headerRowHeightMm = json["headerRowHeightMm"].toDouble(table.headerRowHeightMm);
    table.detailRowHeightMm = json["detailRowHeightMm"].toDouble(table.detailRowHeightMm);
    table.drawBorders = json["drawBorders"].toBool(table.drawBorders);
    table.repeatHeaderOnPage = json["repeatHeaderOnPage"].toBool(table.repeatHeaderOnPage);
    table.columns = columnsFromJson(json["columns"].toArray());
    return table;
}

bool TableElementJson::validate(const QJsonObject& json, const QString& parentLayerId, QString* errorMessage)
{
    const auto tableId = json["id"].toString();
    if (isBlankOrPadded(tableId)) {
        setError(errorMessage, QStringLiteral("表格 id 不能为空，且不能包含首尾空白"));
        return false;
    }

    const auto layerId = json["layerId"].toString();
    const auto normalizedLayerId = layerId.trimmed();
    if (!layerId.isEmpty() && layerId != normalizedLayerId) {
        setError(errorMessage, QStringLiteral("表格 layerId 不能包含首尾空白：%1").arg(tableId));
        return false;
    }
    if (!parentLayerId.trimmed().isEmpty() && !normalizedLayerId.isEmpty() && normalizedLayerId != parentLayerId.trimmed()) {
        setError(errorMessage, QStringLiteral("表格 layerId 与所在图层不一致：%1").arg(tableId));
        return false;
    }

    if (json["width"].toDouble(0.0) <= 0.0 || json["height"].toDouble(0.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格宽高必须大于 0：%1").arg(tableId));
        return false;
    }
    if (json["headerRowHeightMm"].toDouble(6.0) <= 0.0 || json["detailRowHeightMm"].toDouble(5.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格行高必须大于 0：%1").arg(tableId));
        return false;
    }
    if (isBlankOrPadded(json["dataPath"].toString())) {
        setError(errorMessage, QStringLiteral("表格 dataPath 不能为空，且不能包含首尾空白：%1").arg(tableId));
        return false;
    }
    if (!json.contains("columns") || !json["columns"].isArray() || json["columns"].toArray().isEmpty()) {
        setError(errorMessage, QStringLiteral("表格列 columns 必须是非空数组：%1").arg(tableId));
        return false;
    }

    QSet<QString> columnIds;
    for (const auto& value : json["columns"].toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("表格列必须是对象：%1").arg(tableId));
            return false;
        }
        if (!validateColumn(value.toObject(), columnIds, errorMessage)) {
            return false;
        }
    }

    return true;
}

} // 命名空间 sleekpr::core
