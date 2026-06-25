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

QString verticalAlignmentToString(TableVerticalAlignment alignment)
{
    switch (alignment) {
    case TableVerticalAlignment::Top:
        return QStringLiteral("top");
    case TableVerticalAlignment::Middle:
        return QStringLiteral("middle");
    case TableVerticalAlignment::Bottom:
        return QStringLiteral("bottom");
    }
    return QStringLiteral("top");
}

TableVerticalAlignment verticalAlignmentFromString(const QString& value)
{
    if (value == QStringLiteral("middle")) {
        return TableVerticalAlignment::Middle;
    }
    if (value == QStringLiteral("bottom")) {
        return TableVerticalAlignment::Bottom;
    }
    return TableVerticalAlignment::Top;
}

QString rowBandKindToString(TableRowBandKind kind)
{
    switch (kind) {
    case TableRowBandKind::Header:
        return QStringLiteral("header");
    case TableRowBandKind::Detail:
        return QStringLiteral("detail");
    case TableRowBandKind::Summary:
        return QStringLiteral("summary");
    case TableRowBandKind::Subtotal:
        return QStringLiteral("subtotal");
    case TableRowBandKind::GroupHeader:
        return QStringLiteral("groupHeader");
    case TableRowBandKind::GroupFooter:
        return QStringLiteral("groupFooter");
    case TableRowBandKind::Footer:
        return QStringLiteral("footer");
    }
    return QStringLiteral("detail");
}

TableRowBandKind rowBandKindFromString(const QString& value)
{
    if (value == QStringLiteral("header")) {
        return TableRowBandKind::Header;
    }
    if (value == QStringLiteral("summary")) {
        return TableRowBandKind::Summary;
    }
    if (value == QStringLiteral("subtotal")) {
        return TableRowBandKind::Subtotal;
    }
    if (value == QStringLiteral("groupHeader")) {
        return TableRowBandKind::GroupHeader;
    }
    if (value == QStringLiteral("groupFooter")) {
        return TableRowBandKind::GroupFooter;
    }
    if (value == QStringLiteral("footer")) {
        return TableRowBandKind::Footer;
    }
    return TableRowBandKind::Detail;
}

QString rowHeightModeToString(TableRowHeightMode mode)
{
    switch (mode) {
    case TableRowHeightMode::Fixed:
        return QStringLiteral("fixed");
    case TableRowHeightMode::Auto:
        return QStringLiteral("auto");
    }
    return QStringLiteral("fixed");
}

TableRowHeightMode rowHeightModeFromString(const QString& value)
{
    if (value == QStringLiteral("auto")) {
        return TableRowHeightMode::Auto;
    }
    return TableRowHeightMode::Fixed;
}

QString rowPrintScopeToString(TableRowPrintScope scope)
{
    switch (scope) {
    case TableRowPrintScope::FirstPage:
        return QStringLiteral("firstPage");
    case TableRowPrintScope::EveryPage:
        return QStringLiteral("everyPage");
    case TableRowPrintScope::LastPage:
        return QStringLiteral("lastPage");
    }
    return QStringLiteral("everyPage");
}

TableRowPrintScope rowPrintScopeFromString(const QString& value)
{
    if (value == QStringLiteral("firstPage")) {
        return TableRowPrintScope::FirstPage;
    }
    if (value == QStringLiteral("lastPage")) {
        return TableRowPrintScope::LastPage;
    }
    return TableRowPrintScope::EveryPage;
}

QString cellOverflowPolicyToString(TableCellOverflowPolicy policy)
{
    switch (policy) {
    case TableCellOverflowPolicy::Clip:
        return QStringLiteral("clip");
    case TableCellOverflowPolicy::Ellipsis:
        return QStringLiteral("ellipsis");
    case TableCellOverflowPolicy::Wrap:
        return QStringLiteral("wrap");
    case TableCellOverflowPolicy::Shrink:
        return QStringLiteral("shrink");
    }
    return QStringLiteral("clip");
}

TableCellOverflowPolicy cellOverflowPolicyFromString(const QString& value)
{
    if (value == QStringLiteral("ellipsis")) {
        return TableCellOverflowPolicy::Ellipsis;
    }
    if (value == QStringLiteral("wrap")) {
        return TableCellOverflowPolicy::Wrap;
    }
    if (value == QStringLiteral("shrink")) {
        return TableCellOverflowPolicy::Shrink;
    }
    return TableCellOverflowPolicy::Clip;
}

QString tableOverflowPolicyToString(TableTableOverflowPolicy policy)
{
    switch (policy) {
    case TableTableOverflowPolicy::Error:
        return QStringLiteral("error");
    case TableTableOverflowPolicy::Clip:
        return QStringLiteral("clip");
    case TableTableOverflowPolicy::Continue:
        return QStringLiteral("continue");
    }
    return QStringLiteral("error");
}

TableTableOverflowPolicy tableOverflowPolicyFromString(const QString& value)
{
    if (value == QStringLiteral("clip")) {
        return TableTableOverflowPolicy::Clip;
    }
    if (value == QStringLiteral("continue")) {
        return TableTableOverflowPolicy::Continue;
    }
    return TableTableOverflowPolicy::Error;
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

bool isKnownVerticalAlignment(const QString& value)
{
    return value == QStringLiteral("top")
        || value == QStringLiteral("middle")
        || value == QStringLiteral("bottom");
}

bool isKnownRowBandKind(const QString& value)
{
    return value == QStringLiteral("header")
        || value == QStringLiteral("detail")
        || value == QStringLiteral("summary")
        || value == QStringLiteral("subtotal")
        || value == QStringLiteral("groupHeader")
        || value == QStringLiteral("groupFooter")
        || value == QStringLiteral("footer");
}

bool isKnownRowHeightMode(const QString& value)
{
    return value == QStringLiteral("fixed") || value == QStringLiteral("auto");
}

bool isKnownRowPrintScope(const QString& value)
{
    return value == QStringLiteral("firstPage")
        || value == QStringLiteral("everyPage")
        || value == QStringLiteral("lastPage");
}

bool isKnownCellOverflowPolicy(const QString& value)
{
    return value == QStringLiteral("clip")
        || value == QStringLiteral("ellipsis")
        || value == QStringLiteral("wrap")
        || value == QStringLiteral("shrink");
}

bool isKnownTableOverflowPolicy(const QString& value)
{
    return value == QStringLiteral("error")
        || value == QStringLiteral("clip")
        || value == QStringLiteral("continue");
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

QJsonValue jsonValue(const QJsonObject& json, const char* key)
{
    return json.value(QString::fromLatin1(key));
}

bool jsonContains(const QJsonObject& json, const char* key)
{
    return json.contains(QString::fromLatin1(key));
}

void setJsonValue(QJsonObject& json, const char* key, const QJsonValue& value)
{
    json.insert(QString::fromLatin1(key), value);
}

QJsonObject columnToJson(const TableColumn& column)
{
    QJsonObject json;
    setJsonValue(json, "id", column.id);
    setJsonValue(json, "title", column.title);
    setJsonValue(json, "fieldKey", column.fieldKey);
    setJsonValue(json, "widthMm", column.widthMm);
    setJsonValue(json, "widthMode", widthModeToString(column.widthMode));
    setJsonValue(json, "flexWeight", column.flexWeight);
    setJsonValue(json, "alignment", alignmentToString(column.alignment));
    setJsonValue(json, "fontSizePt", column.fontSizePt);
    setJsonValue(json, "bold", column.bold);
    setJsonValue(json, "ellipsis", column.ellipsis);
    if (column.wrapText) {
        setJsonValue(json, "wrapText", true);
    }
    if (column.minWidthMm != 1.0) {
        setJsonValue(json, "minWidthMm", column.minWidthMm);
    }
    if (!column.defaultCellStyleId.trimmed().isEmpty()) {
        setJsonValue(json, "defaultCellStyleId", column.defaultCellStyleId);
    }
    return json;
}

TableColumn columnFromJson(const QJsonObject& json)
{
    TableColumn column;
    column.id = jsonValue(json, "id").toString();
    column.title = jsonValue(json, "title").toString();
    column.fieldKey = jsonValue(json, "fieldKey").toString();
    column.widthMm = jsonValue(json, "widthMm").toDouble(column.widthMm);
    column.widthMode = widthModeFromString(jsonValue(json, "widthMode").toString(widthModeToString(column.widthMode)));
    column.flexWeight = jsonValue(json, "flexWeight").toDouble(column.flexWeight);
    column.alignment = alignmentFromString(jsonValue(json, "alignment").toString(alignmentToString(column.alignment)));
    column.fontSizePt = jsonValue(json, "fontSizePt").toDouble(column.fontSizePt);
    column.bold = jsonValue(json, "bold").toBool(column.bold);
    column.ellipsis = jsonValue(json, "ellipsis").toBool(column.ellipsis);
    column.wrapText = jsonValue(json, "wrapText").toBool(column.wrapText);
    column.minWidthMm = jsonValue(json, "minWidthMm").toDouble(column.minWidthMm);
    column.defaultCellStyleId = jsonValue(json, "defaultCellStyleId").toString();
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

QJsonObject cellStyleToJson(const TableCellStyle& style)
{
    QJsonObject json;
    setJsonValue(json, "id", style.id);
    setJsonValue(json, "fontSizePt", style.fontSizePt);
    setJsonValue(json, "bold", style.bold);
    setJsonValue(json, "alignment", alignmentToString(style.alignment));
    setJsonValue(json, "verticalAlignment", verticalAlignmentToString(style.verticalAlignment));
    setJsonValue(json, "paddingLeftMm", style.paddingLeftMm);
    setJsonValue(json, "paddingTopMm", style.paddingTopMm);
    setJsonValue(json, "paddingRightMm", style.paddingRightMm);
    setJsonValue(json, "paddingBottomMm", style.paddingBottomMm);
    setJsonValue(json, "drawBorder", style.drawBorder);
    setJsonValue(json, "borderWidthMm", style.borderWidthMm);
    if (!style.backgroundColor.trimmed().isEmpty()) {
        setJsonValue(json, "backgroundColor", style.backgroundColor);
    }
    if (!style.textColor.trimmed().isEmpty()) {
        setJsonValue(json, "textColor", style.textColor);
    }
    setJsonValue(json, "wrapText", style.wrapText);
    setJsonValue(json, "ellipsis", style.ellipsis);
    return json;
}

TableCellStyle cellStyleFromJson(const QJsonObject& json)
{
    TableCellStyle style;
    style.id = jsonValue(json, "id").toString();
    style.fontSizePt = jsonValue(json, "fontSizePt").toDouble(style.fontSizePt);
    style.bold = jsonValue(json, "bold").toBool(style.bold);
    style.alignment = alignmentFromString(jsonValue(json, "alignment").toString(alignmentToString(style.alignment)));
    style.verticalAlignment = verticalAlignmentFromString(
        jsonValue(json, "verticalAlignment").toString(verticalAlignmentToString(style.verticalAlignment)));
    style.paddingLeftMm = jsonValue(json, "paddingLeftMm").toDouble(style.paddingLeftMm);
    style.paddingTopMm = jsonValue(json, "paddingTopMm").toDouble(style.paddingTopMm);
    style.paddingRightMm = jsonValue(json, "paddingRightMm").toDouble(style.paddingRightMm);
    style.paddingBottomMm = jsonValue(json, "paddingBottomMm").toDouble(style.paddingBottomMm);
    style.drawBorder = jsonValue(json, "drawBorder").toBool(style.drawBorder);
    style.borderWidthMm = jsonValue(json, "borderWidthMm").toDouble(style.borderWidthMm);
    style.backgroundColor = jsonValue(json, "backgroundColor").toString();
    style.textColor = jsonValue(json, "textColor").toString();
    style.wrapText = jsonValue(json, "wrapText").toBool(style.wrapText);
    style.ellipsis = jsonValue(json, "ellipsis").toBool(style.ellipsis);
    return style;
}

QJsonArray cellStylesToJson(const QList<TableCellStyle>& styles)
{
    QJsonArray result;
    for (const auto& style : styles) {
        result.append(cellStyleToJson(style));
    }
    return result;
}

QList<TableCellStyle> cellStylesFromJson(const QJsonArray& json)
{
    QList<TableCellStyle> result;
    for (const auto& value : json) {
        result.append(cellStyleFromJson(value.toObject()));
    }
    return result;
}

QJsonObject rowBandToJson(const TableRowBand& rowBand)
{
    QJsonObject json;
    setJsonValue(json, "id", rowBand.id);
    setJsonValue(json, "kind", rowBandKindToString(rowBand.kind));
    setJsonValue(json, "title", rowBand.title);
    if (!rowBand.dataPath.trimmed().isEmpty()) {
        setJsonValue(json, "dataPath", rowBand.dataPath);
    }
    setJsonValue(json, "heightMode", rowHeightModeToString(rowBand.heightMode));
    setJsonValue(json, "heightMm", rowBand.heightMm);
    setJsonValue(json, "minHeightMm", rowBand.minHeightMm);
    setJsonValue(json, "repeatOnPage", rowBand.repeatOnPage);
    setJsonValue(json, "printOn", rowPrintScopeToString(rowBand.printOn));
    return json;
}

TableRowBand rowBandFromJson(const QJsonObject& json)
{
    TableRowBand rowBand;
    rowBand.id = jsonValue(json, "id").toString();
    rowBand.kind = rowBandKindFromString(jsonValue(json, "kind").toString(rowBandKindToString(rowBand.kind)));
    rowBand.title = jsonValue(json, "title").toString();
    rowBand.dataPath = jsonValue(json, "dataPath").toString();
    rowBand.heightMode = rowHeightModeFromString(jsonValue(json, "heightMode").toString(rowHeightModeToString(rowBand.heightMode)));
    rowBand.heightMm = jsonValue(json, "heightMm").toDouble(rowBand.heightMm);
    rowBand.minHeightMm = jsonValue(json, "minHeightMm").toDouble(rowBand.minHeightMm);
    rowBand.repeatOnPage = jsonValue(json, "repeatOnPage").toBool(rowBand.repeatOnPage);
    rowBand.printOn = rowPrintScopeFromString(jsonValue(json, "printOn").toString(rowPrintScopeToString(rowBand.printOn)));
    return rowBand;
}

QJsonArray rowBandsToJson(const QList<TableRowBand>& rowBands)
{
    QJsonArray result;
    for (const auto& rowBand : rowBands) {
        result.append(rowBandToJson(rowBand));
    }
    return result;
}

QList<TableRowBand> rowBandsFromJson(const QJsonArray& json)
{
    QList<TableRowBand> result;
    for (const auto& value : json) {
        result.append(rowBandFromJson(value.toObject()));
    }
    return result;
}

QJsonObject cellTemplateToJson(const TableCellTemplate& cell)
{
    QJsonObject json;
    setJsonValue(json, "id", cell.id);
    setJsonValue(json, "rowBandId", cell.rowBandId);
    setJsonValue(json, "columnId", cell.columnId);
    setJsonValue(json, "textTemplate", cell.textTemplate);
    setJsonValue(json, "fieldKey", cell.fieldKey);
    setJsonValue(json, "styleId", cell.styleId);
    setJsonValue(json, "overflowPolicy", cellOverflowPolicyToString(cell.overflowPolicy));
    setJsonValue(json, "maxLines", cell.maxLines);
    setJsonValue(json, "colSpan", cell.colSpan);
    setJsonValue(json, "rowSpan", cell.rowSpan);
    setJsonValue(json, "visible", cell.visible);
    return json;
}

TableCellTemplate cellTemplateFromJson(const QJsonObject& json)
{
    TableCellTemplate cell;
    cell.id = jsonValue(json, "id").toString();
    cell.rowBandId = jsonValue(json, "rowBandId").toString();
    cell.columnId = jsonValue(json, "columnId").toString();
    cell.textTemplate = jsonValue(json, "textTemplate").toString();
    cell.fieldKey = jsonValue(json, "fieldKey").toString();
    cell.styleId = jsonValue(json, "styleId").toString();
    cell.overflowPolicy = cellOverflowPolicyFromString(
        jsonValue(json, "overflowPolicy").toString(cellOverflowPolicyToString(cell.overflowPolicy)));
    cell.maxLines = jsonValue(json, "maxLines").toInt(cell.maxLines);
    cell.colSpan = jsonValue(json, "colSpan").toInt(cell.colSpan);
    cell.rowSpan = jsonValue(json, "rowSpan").toInt(cell.rowSpan);
    cell.visible = jsonValue(json, "visible").toBool(cell.visible);
    return cell;
}

QJsonArray cellTemplatesToJson(const QList<TableCellTemplate>& cells)
{
    QJsonArray result;
    for (const auto& cell : cells) {
        result.append(cellTemplateToJson(cell));
    }
    return result;
}

QList<TableCellTemplate> cellTemplatesFromJson(const QJsonArray& json)
{
    QList<TableCellTemplate> result;
    for (const auto& value : json) {
        result.append(cellTemplateFromJson(value.toObject()));
    }
    return result;
}

QJsonObject mergeRegionToJson(const TableMergeRegion& merge)
{
    QJsonObject json;
    setJsonValue(json, "id", merge.id);
    setJsonValue(json, "rowBandId", merge.rowBandId);
    setJsonValue(json, "startRowOffset", merge.startRowOffset);
    setJsonValue(json, "startColumnId", merge.startColumnId);
    setJsonValue(json, "rowSpan", merge.rowSpan);
    setJsonValue(json, "colSpan", merge.colSpan);
    return json;
}

TableMergeRegion mergeRegionFromJson(const QJsonObject& json)
{
    TableMergeRegion merge;
    merge.id = jsonValue(json, "id").toString();
    merge.rowBandId = jsonValue(json, "rowBandId").toString();
    merge.startRowOffset = jsonValue(json, "startRowOffset").toInt(merge.startRowOffset);
    merge.startColumnId = jsonValue(json, "startColumnId").toString();
    merge.rowSpan = jsonValue(json, "rowSpan").toInt(merge.rowSpan);
    merge.colSpan = jsonValue(json, "colSpan").toInt(merge.colSpan);
    return merge;
}

QJsonArray mergeRegionsToJson(const QList<TableMergeRegion>& merges)
{
    QJsonArray result;
    for (const auto& merge : merges) {
        result.append(mergeRegionToJson(merge));
    }
    return result;
}

QList<TableMergeRegion> mergeRegionsFromJson(const QJsonArray& json)
{
    QList<TableMergeRegion> result;
    for (const auto& value : json) {
        result.append(mergeRegionFromJson(value.toObject()));
    }
    return result;
}

QJsonObject paginationToJson(const TablePaginationPolicy& pagination)
{
    QJsonObject json;
    setJsonValue(json, "repeatHeaderOnPage", pagination.repeatHeaderOnPage);
    setJsonValue(json, "keepGroupTogether", pagination.keepGroupTogether);
    setJsonValue(json, "allowRowSplit", pagination.allowRowSplit);
    setJsonValue(json, "maxPages", pagination.maxPages);
    setJsonValue(json, "overflowPolicy", tableOverflowPolicyToString(pagination.overflowPolicy));
    setJsonValue(json, "orphanDetailRows", pagination.orphanDetailRows);
    return json;
}

TablePaginationPolicy paginationFromJson(const QJsonObject& json, bool legacyRepeatHeaderOnPage)
{
    TablePaginationPolicy pagination;
    pagination.repeatHeaderOnPage = legacyRepeatHeaderOnPage;
    if (json.isEmpty()) {
        return pagination;
    }

    pagination.repeatHeaderOnPage = jsonValue(json, "repeatHeaderOnPage").toBool(pagination.repeatHeaderOnPage);
    pagination.keepGroupTogether = jsonValue(json, "keepGroupTogether").toBool(pagination.keepGroupTogether);
    pagination.allowRowSplit = jsonValue(json, "allowRowSplit").toBool(pagination.allowRowSplit);
    pagination.maxPages = jsonValue(json, "maxPages").toInt(pagination.maxPages);
    pagination.overflowPolicy = tableOverflowPolicyFromString(
        jsonValue(json, "overflowPolicy").toString(tableOverflowPolicyToString(pagination.overflowPolicy)));
    pagination.orphanDetailRows = jsonValue(json, "orphanDetailRows").toInt(pagination.orphanDetailRows);
    return pagination;
}

bool hasNonDefaultPagination(const TablePaginationPolicy& pagination, bool legacyRepeatHeaderOnPage)
{
    const TablePaginationPolicy defaults;
    return pagination.repeatHeaderOnPage != legacyRepeatHeaderOnPage
        || pagination.keepGroupTogether != defaults.keepGroupTogether
        || pagination.allowRowSplit != defaults.allowRowSplit
        || pagination.maxPages != defaults.maxPages
        || pagination.overflowPolicy != defaults.overflowPolicy
        || pagination.orphanDetailRows != defaults.orphanDetailRows;
}

bool validateColumn(const QJsonObject& column, QSet<QString>& columnIds, QString* errorMessage)
{
    const auto columnId = jsonValue(column, "id").toString();
    if (isBlankOrPadded(columnId)) {
        setError(errorMessage, QStringLiteral("表格列 id 不能为空，且不能包含首尾空白"));
        return false;
    }
    if (columnIds.contains(columnId)) {
        setError(errorMessage, QStringLiteral("表格列 id 重复：%1").arg(columnId));
        return false;
    }
    columnIds.insert(columnId);

    const auto fieldKey = jsonValue(column, "fieldKey").toString();
    if (isBlankOrPadded(fieldKey)) {
        setError(errorMessage, QStringLiteral("表格列 fieldKey 不能为空，且不能包含首尾空白：%1").arg(columnId));
        return false;
    }

    if (jsonValue(column, "widthMm").toDouble(0.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格列宽必须大于 0：%1").arg(columnId));
        return false;
    }
    if (jsonContains(column, "minWidthMm") && jsonValue(column, "minWidthMm").toDouble(0.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格列最小宽度必须大于 0：%1").arg(columnId));
        return false;
    }

    const auto widthMode = jsonValue(column, "widthMode").toString(QStringLiteral("fixed"));
    if (!isKnownWidthMode(widthMode)) {
        setError(errorMessage, QStringLiteral("表格列宽模式不受支持：%1").arg(widthMode));
        return false;
    }
    if (widthMode == QStringLiteral("flex") && jsonValue(column, "flexWeight").toDouble(1.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格弹性列权重必须大于 0：%1").arg(columnId));
        return false;
    }

    const auto alignment = jsonValue(column, "alignment").toString(QStringLiteral("left"));
    if (!isKnownAlignment(alignment)) {
        setError(errorMessage, QStringLiteral("表格列对齐方式不受支持：%1").arg(alignment));
        return false;
    }

    return true;
}

bool validateOptionalArray(const QJsonObject& json, const QString& key, const QString& tableId, QString* errorMessage)
{
    if (json.contains(key) && !json.value(key).isArray()) {
        setError(errorMessage, QStringLiteral("表格 %1 的 %2 必须是数组").arg(tableId, key));
        return false;
    }
    return true;
}

} // 命名空间

QJsonObject TableElementJson::toJson(const TableElement& table)
{
    QJsonObject json;
    setJsonValue(json, "id", table.id);
    setJsonValue(json, "layerId", table.layerId);
    setJsonValue(json, "zIndex", table.zIndex);
    setJsonValue(json, "visible", table.visible);
    setJsonValue(json, "locked", table.locked);
    setJsonValue(json, "displayName", table.displayName);
    setJsonValue(json, "x", table.x);
    setJsonValue(json, "y", table.y);
    setJsonValue(json, "width", table.width);
    setJsonValue(json, "height", table.height);
    setJsonValue(json, "dataPath", table.dataPath);
    setJsonValue(json, "headerRowHeightMm", table.headerRowHeightMm);
    setJsonValue(json, "detailRowHeightMm", table.detailRowHeightMm);
    setJsonValue(json, "drawBorders", table.drawBorders);
    setJsonValue(json, "repeatHeaderOnPage", table.repeatHeaderOnPage);
    setJsonValue(json, "columns", columnsToJson(table.columns));
    if (!table.cellStyles.isEmpty()) {
        setJsonValue(json, "cellStyles", cellStylesToJson(table.cellStyles));
    }
    if (!table.rowBands.isEmpty()) {
        setJsonValue(json, "rowBands", rowBandsToJson(table.rowBands));
    }
    if (!table.cellTemplates.isEmpty()) {
        setJsonValue(json, "cellTemplates", cellTemplatesToJson(table.cellTemplates));
    }
    if (!table.mergeRegions.isEmpty()) {
        setJsonValue(json, "mergeRegions", mergeRegionsToJson(table.mergeRegions));
    }
    if (hasNonDefaultPagination(table.pagination, table.repeatHeaderOnPage)) {
        setJsonValue(json, "pagination", paginationToJson(table.pagination));
    }
    return json;
}

TableElement TableElementJson::fromJson(const QJsonObject& json, const QString& parentLayerId)
{
    TableElement table;
    table.id = jsonValue(json, "id").toString();
    table.layerId = jsonValue(json, "layerId").toString();
    if (table.layerId.trimmed().isEmpty() && !parentLayerId.trimmed().isEmpty()) {
        table.layerId = parentLayerId;
    }
    table.zIndex = jsonValue(json, "zIndex").toInt(table.zIndex);
    table.visible = jsonValue(json, "visible").toBool(table.visible);
    table.locked = jsonValue(json, "locked").toBool(table.locked);
    table.displayName = jsonValue(json, "displayName").toString();
    table.x = jsonValue(json, "x").toDouble(table.x);
    table.y = jsonValue(json, "y").toDouble(table.y);
    table.width = jsonValue(json, "width").toDouble(table.width);
    table.height = jsonValue(json, "height").toDouble(table.height);
    table.dataPath = jsonValue(json, "dataPath").toString();
    table.headerRowHeightMm = jsonValue(json, "headerRowHeightMm").toDouble(table.headerRowHeightMm);
    table.detailRowHeightMm = jsonValue(json, "detailRowHeightMm").toDouble(table.detailRowHeightMm);
    table.drawBorders = jsonValue(json, "drawBorders").toBool(table.drawBorders);
    table.repeatHeaderOnPage = jsonValue(json, "repeatHeaderOnPage").toBool(table.repeatHeaderOnPage);
    table.columns = columnsFromJson(jsonValue(json, "columns").toArray());
    table.cellStyles = cellStylesFromJson(jsonValue(json, "cellStyles").toArray());
    table.rowBands = rowBandsFromJson(jsonValue(json, "rowBands").toArray());
    table.cellTemplates = cellTemplatesFromJson(jsonValue(json, "cellTemplates").toArray());
    table.mergeRegions = mergeRegionsFromJson(jsonValue(json, "mergeRegions").toArray());
    table.pagination = paginationFromJson(jsonValue(json, "pagination").toObject(), table.repeatHeaderOnPage);
    return table;
}

bool TableElementJson::validate(const QJsonObject& json, const QString& parentLayerId, QString* errorMessage)
{
    const auto tableId = jsonValue(json, "id").toString();
    if (isBlankOrPadded(tableId)) {
        setError(errorMessage, QStringLiteral("表格 id 不能为空，且不能包含首尾空白"));
        return false;
    }

    const auto layerId = jsonValue(json, "layerId").toString();
    const auto normalizedLayerId = layerId.trimmed();
    if (!layerId.isEmpty() && layerId != normalizedLayerId) {
        setError(errorMessage, QStringLiteral("表格 layerId 不能包含首尾空白：%1").arg(tableId));
        return false;
    }
    if (!parentLayerId.trimmed().isEmpty() && !normalizedLayerId.isEmpty() && normalizedLayerId != parentLayerId.trimmed()) {
        setError(errorMessage, QStringLiteral("表格 layerId 与所在图层不一致：%1").arg(tableId));
        return false;
    }

    if (jsonValue(json, "width").toDouble(0.0) <= 0.0 || jsonValue(json, "height").toDouble(0.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格宽高必须大于 0：%1").arg(tableId));
        return false;
    }
    if (jsonValue(json, "headerRowHeightMm").toDouble(6.0) <= 0.0 || jsonValue(json, "detailRowHeightMm").toDouble(5.0) <= 0.0) {
        setError(errorMessage, QStringLiteral("表格行高必须大于 0：%1").arg(tableId));
        return false;
    }
    if (isBlankOrPadded(jsonValue(json, "dataPath").toString())) {
        setError(errorMessage, QStringLiteral("表格 dataPath 不能为空，且不能包含首尾空白：%1").arg(tableId));
        return false;
    }
    if (!jsonContains(json, "columns") || !jsonValue(json, "columns").isArray() || jsonValue(json, "columns").toArray().isEmpty()) {
        setError(errorMessage, QStringLiteral("表格列 columns 必须是非空数组：%1").arg(tableId));
        return false;
    }

    QSet<QString> columnIds;
    for (const auto& value : jsonValue(json, "columns").toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("表格列必须是对象：%1").arg(tableId));
            return false;
        }
        if (!validateColumn(value.toObject(), columnIds, errorMessage)) {
            return false;
        }
    }

    if (!validateOptionalArray(json, QStringLiteral("cellStyles"), tableId, errorMessage)
        || !validateOptionalArray(json, QStringLiteral("rowBands"), tableId, errorMessage)
        || !validateOptionalArray(json, QStringLiteral("cellTemplates"), tableId, errorMessage)
        || !validateOptionalArray(json, QStringLiteral("mergeRegions"), tableId, errorMessage)) {
        return false;
    }

    QSet<QString> styleIds;
    for (const auto& value : jsonValue(json, "cellStyles").toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("表格样式必须是对象：%1").arg(tableId));
            return false;
        }
        const auto style = value.toObject();
        const auto styleId = jsonValue(style, "id").toString();
        if (isBlankOrPadded(styleId) || styleIds.contains(styleId)) {
            setError(errorMessage, QStringLiteral("表格样式 id 为空或重复：%1").arg(styleId));
            return false;
        }
        if (jsonValue(style, "fontSizePt").toDouble(8.0) <= 0.0) {
            setError(errorMessage, QStringLiteral("表格样式字号必须大于 0：%1").arg(styleId));
            return false;
        }
        if (jsonValue(style, "paddingLeftMm").toDouble(0.6) < 0.0
            || jsonValue(style, "paddingTopMm").toDouble(0.4) < 0.0
            || jsonValue(style, "paddingRightMm").toDouble(0.6) < 0.0
            || jsonValue(style, "paddingBottomMm").toDouble(0.4) < 0.0
            || jsonValue(style, "borderWidthMm").toDouble(0.1) < 0.0) {
            setError(errorMessage, QStringLiteral("表格样式内边距和边框宽度不能小于 0：%1").arg(styleId));
            return false;
        }
        const auto alignment = jsonValue(style, "alignment").toString(QStringLiteral("left"));
        if (!isKnownAlignment(alignment)) {
            setError(errorMessage, QStringLiteral("表格样式对齐方式不受支持：%1").arg(alignment));
            return false;
        }
        const auto verticalAlignment = jsonValue(style, "verticalAlignment").toString(QStringLiteral("top"));
        if (!isKnownVerticalAlignment(verticalAlignment)) {
            setError(errorMessage, QStringLiteral("表格样式垂直对齐方式不受支持：%1").arg(verticalAlignment));
            return false;
        }
        styleIds.insert(styleId);
    }

    for (const auto& value : jsonValue(json, "columns").toArray()) {
        const auto column = value.toObject();
        const auto styleId = jsonValue(column, "defaultCellStyleId").toString();
        if (!styleId.isEmpty() && !styleIds.isEmpty() && !styleIds.contains(styleId)) {
            setError(errorMessage, QStringLiteral("表格列引用了不存在的默认样式：%1").arg(styleId));
            return false;
        }
    }

    QSet<QString> rowBandIds;
    for (const auto& value : jsonValue(json, "rowBands").toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("表格行带必须是对象：%1").arg(tableId));
            return false;
        }
        const auto rowBand = value.toObject();
        const auto rowBandId = jsonValue(rowBand, "id").toString();
        if (isBlankOrPadded(rowBandId) || rowBandIds.contains(rowBandId)) {
            setError(errorMessage, QStringLiteral("表格行带 id 为空或重复：%1").arg(rowBandId));
            return false;
        }
        const auto kind = jsonValue(rowBand, "kind").toString(QStringLiteral("detail"));
        if (!isKnownRowBandKind(kind)) {
            setError(errorMessage, QStringLiteral("表格行带类型不受支持：%1").arg(kind));
            return false;
        }
        const auto heightMode = jsonValue(rowBand, "heightMode").toString(QStringLiteral("fixed"));
        if (!isKnownRowHeightMode(heightMode)) {
            setError(errorMessage, QStringLiteral("表格行高模式不受支持：%1").arg(heightMode));
            return false;
        }
        const auto printOn = jsonValue(rowBand, "printOn").toString(QStringLiteral("everyPage"));
        if (!isKnownRowPrintScope(printOn)) {
            setError(errorMessage, QStringLiteral("表格行带打印范围不受支持：%1").arg(printOn));
            return false;
        }
        if (jsonValue(rowBand, "heightMm").toDouble(5.0) <= 0.0 || jsonValue(rowBand, "minHeightMm").toDouble(4.0) <= 0.0) {
            setError(errorMessage, QStringLiteral("表格行带高度必须大于 0：%1").arg(rowBandId));
            return false;
        }
        rowBandIds.insert(rowBandId);
    }

    for (const auto& value : jsonValue(json, "cellTemplates").toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("表格单元格模板必须是对象：%1").arg(tableId));
            return false;
        }
        const auto cell = value.toObject();
        const auto rowBandId = jsonValue(cell, "rowBandId").toString();
        const auto columnId = jsonValue(cell, "columnId").toString();
        const auto styleId = jsonValue(cell, "styleId").toString();
        if (!rowBandIds.isEmpty() && !rowBandIds.contains(rowBandId)) {
            setError(errorMessage, QStringLiteral("单元格模板引用了不存在的行带：%1").arg(rowBandId));
            return false;
        }
        if (!columnIds.contains(columnId)) {
            setError(errorMessage, QStringLiteral("单元格模板引用了不存在的列：%1").arg(columnId));
            return false;
        }
        if (!styleId.isEmpty() && !styleIds.isEmpty() && !styleIds.contains(styleId)) {
            setError(errorMessage, QStringLiteral("单元格模板引用了不存在的样式：%1").arg(styleId));
            return false;
        }
        const auto overflowPolicy = jsonValue(cell, "overflowPolicy").toString(QStringLiteral("clip"));
        if (!isKnownCellOverflowPolicy(overflowPolicy)) {
            setError(errorMessage, QStringLiteral("单元格溢出策略不受支持：%1").arg(overflowPolicy));
            return false;
        }
        if (jsonValue(cell, "colSpan").toInt(1) <= 0 || jsonValue(cell, "rowSpan").toInt(1) <= 0 || jsonValue(cell, "maxLines").toInt(1) <= 0) {
            setError(errorMessage, QStringLiteral("单元格模板跨度和最大行数必须大于 0：%1").arg(jsonValue(cell, "id").toString()));
            return false;
        }
    }

    for (const auto& value : jsonValue(json, "mergeRegions").toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("表格合并区域必须是对象：%1").arg(tableId));
            return false;
        }
        const auto merge = value.toObject();
        const auto rowBandId = jsonValue(merge, "rowBandId").toString();
        const auto startColumnId = jsonValue(merge, "startColumnId").toString();
        if (!rowBandIds.isEmpty() && !rowBandIds.contains(rowBandId)) {
            setError(errorMessage, QStringLiteral("合并区域引用了不存在的行带：%1").arg(rowBandId));
            return false;
        }
        if (!columnIds.contains(startColumnId)) {
            setError(errorMessage, QStringLiteral("合并区域引用了不存在的列：%1").arg(startColumnId));
            return false;
        }
        if (jsonValue(merge, "startRowOffset").toInt(0) < 0) {
            setError(errorMessage, QStringLiteral("合并区域起始行偏移不能小于 0：%1").arg(jsonValue(merge, "id").toString()));
            return false;
        }
        if (jsonValue(merge, "rowSpan").toInt(1) <= 0 || jsonValue(merge, "colSpan").toInt(1) <= 0) {
            setError(errorMessage, QStringLiteral("合并区域跨度必须大于 0：%1").arg(jsonValue(merge, "id").toString()));
            return false;
        }
    }

    if (jsonContains(json, "pagination")) {
        if (!jsonValue(json, "pagination").isObject()) {
            setError(errorMessage, QStringLiteral("表格分页策略必须是对象：%1").arg(tableId));
            return false;
        }
        const auto pagination = jsonValue(json, "pagination").toObject();
        if (jsonValue(pagination, "maxPages").toInt(100) <= 0) {
            setError(errorMessage, QStringLiteral("表格分页最大页数必须大于 0：%1").arg(tableId));
            return false;
        }
        if (jsonValue(pagination, "orphanDetailRows").toInt(1) < 0) {
            setError(errorMessage, QStringLiteral("表格分页孤行保护不能小于 0：%1").arg(tableId));
            return false;
        }
        const auto overflowPolicy = jsonValue(pagination, "overflowPolicy").toString(QStringLiteral("error"));
        if (!isKnownTableOverflowPolicy(overflowPolicy)) {
            setError(errorMessage, QStringLiteral("表格分页溢出策略不受支持：%1").arg(overflowPolicy));
            return false;
        }
    }

    return true;
}

} // 命名空间 sleekpr::core
