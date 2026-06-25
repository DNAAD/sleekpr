#include "sleekpr/core/templates/TableElementLayout.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace sleekpr::core {

namespace {

QString valueToText(const QJsonValue& value)
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

QJsonValue valueAtPath(const QJsonObject& object, const QString& path)
{
    const auto normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return QJsonValue();
    }
    if (object.contains(normalizedPath)) {
        return object.value(normalizedPath);
    }

    // 字段路径支持 order.items、product.name 这样的嵌套 JSON；完整 key 优先，避免破坏旧模板。
    QJsonValue current = object;
    for (const auto& part : normalizedPath.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        if (!current.isObject()) {
            return QJsonValue();
        }
        const auto currentObject = current.toObject();
        if (!currentObject.contains(part)) {
            return QJsonValue();
        }
        current = currentObject.value(part);
    }
    return current;
}

double columnFlexWeight(const TableColumn& column)
{
    return column.flexWeight > 0.0 ? column.flexWeight : 1.0;
}

QList<double> resolvedColumnWidths(const TableElement& table)
{
    QList<double> widths;
    widths.reserve(table.columns.size());

    double fixedTotal = 0.0;
    double flexWeightTotal = 0.0;
    for (const auto& column : table.columns) {
        if (column.widthMode == TableColumnWidthMode::Flex) {
            flexWeightTotal += columnFlexWeight(column);
        } else {
            fixedTotal += std::max(0.0, column.widthMm);
        }
    }

    const auto remainingWidth = std::max(0.0, table.width - fixedTotal);
    for (const auto& column : table.columns) {
        if (column.widthMode == TableColumnWidthMode::Flex && flexWeightTotal > 0.0) {
            widths.append(std::max(column.minWidthMm, remainingWidth * columnFlexWeight(column) / flexWeightTotal));
        } else {
            widths.append(std::max(column.minWidthMm, column.widthMm));
        }
    }
    return widths;
}

TableCellStyle styleForColumn(const TableElement& table, const TableColumn& column)
{
    if (!column.defaultCellStyleId.trimmed().isEmpty()) {
        const auto style = std::find_if(table.cellStyles.cbegin(), table.cellStyles.cend(), [&column](const TableCellStyle& candidate) {
            return candidate.id == column.defaultCellStyleId;
        });
        if (style != table.cellStyles.cend()) {
            return *style;
        }
    }

    TableCellStyle style;
    style.fontSizePt = column.fontSizePt > 0.0 ? column.fontSizePt : style.fontSizePt;
    style.bold = column.bold;
    style.alignment = column.alignment;
    style.wrapText = column.wrapText;
    style.ellipsis = column.ellipsis;
    return style;
}

const TableCellStyle* findStyle(const TableElement& table, const QString& styleId)
{
    if (styleId.trimmed().isEmpty()) {
        return nullptr;
    }
    const auto style = std::find_if(table.cellStyles.cbegin(), table.cellStyles.cend(), [&styleId](const TableCellStyle& candidate) {
        return candidate.id == styleId;
    });
    return style == table.cellStyles.cend() ? nullptr : &(*style);
}

TableCellOverflowPolicy overflowPolicyFor(const TableCellStyle& style)
{
    if (style.wrapText) {
        return TableCellOverflowPolicy::Wrap;
    }
    if (style.ellipsis) {
        return TableCellOverflowPolicy::Ellipsis;
    }
    return TableCellOverflowPolicy::Clip;
}

double estimatedLineHeightMm(double fontSizePt)
{
    return std::max(1.0, fontSizePt * 0.352778 * 1.25);
}

int estimatedLineCount(const QString& text, double widthMm, const TableCellStyle& style, int maxLines)
{
    const auto normalizedText = text.isEmpty() ? QStringLiteral(" ") : text;
    if (!style.wrapText && !normalizedText.contains(QLatin1Char('\n'))) {
        return 1;
    }

    const auto contentWidth = std::max(1.0, widthMm - style.paddingLeftMm - style.paddingRightMm);
    const auto averageCharWidth = std::max(0.6, style.fontSizePt * 0.352778 * 0.55);
    const auto charsPerLine = std::max(1, static_cast<int>(std::floor(contentWidth / averageCharWidth)));

    int estimated = 0;
    for (const auto& line : normalizedText.split(QLatin1Char('\n'))) {
        estimated += std::max(1, static_cast<int>(std::ceil(static_cast<double>(line.size()) / charsPerLine)));
    }
    if (maxLines > 0) {
        estimated = std::min(estimated, maxLines);
    }
    return std::max(1, estimated);
}

double estimatedCellHeightMm(const QString& text, double widthMm, const TableCellStyle& style, int maxLines)
{
    return style.paddingTopMm
        + style.paddingBottomMm
        + estimatedLineCount(text, widthMm, style, maxLines) * estimatedLineHeightMm(style.fontSizePt);
}

QHash<QString, int> columnIndexById(const TableElement& table)
{
    QHash<QString, int> result;
    for (int index = 0; index < table.columns.size(); ++index) {
        result.insert(table.columns[index].id, index);
    }
    return result;
}

QSet<QString> knownRowBandIds(const TableElement& table)
{
    QSet<QString> result;
    if (table.rowBands.isEmpty()) {
        result.insert(QStringLiteral("header"));
        result.insert(QStringLiteral("detail"));
        return result;
    }

    for (const auto& rowBand : table.rowBands) {
        result.insert(rowBand.id);
    }
    return result;
}

const TableCellTemplate* findCellTemplate(const TableElement& table, const QString& rowBandId, const QString& columnId)
{
    const auto cellTemplate = std::find_if(
        table.cellTemplates.cbegin(),
        table.cellTemplates.cend(),
        [&rowBandId, &columnId](const TableCellTemplate& candidate) {
            return candidate.rowBandId == rowBandId && candidate.columnId == columnId;
        });
    return cellTemplate == table.cellTemplates.cend() ? nullptr : &(*cellTemplate);
}

const TableMergeRegion* findMergeStartingAt(const TableElement& table, const QString& rowBandId, const QString& columnId)
{
    const auto merge = std::find_if(
        table.mergeRegions.cbegin(),
        table.mergeRegions.cend(),
        [&rowBandId, &columnId](const TableMergeRegion& candidate) {
            return candidate.rowBandId == rowBandId && candidate.startColumnId == columnId && candidate.startRowOffset == 0;
        });
    return merge == table.mergeRegions.cend() ? nullptr : &(*merge);
}

bool isCoveredByMerge(const TableElement& table, const QString& rowBandId, int columnIndex, const QHash<QString, int>& columns)
{
    for (const auto& merge : table.mergeRegions) {
        if (merge.rowBandId != rowBandId || merge.startRowOffset != 0) {
            continue;
        }
        const auto startIndex = columns.value(merge.startColumnId, -1);
        if (startIndex >= 0 && columnIndex > startIndex && columnIndex < startIndex + merge.colSpan) {
            return true;
        }
    }
    return false;
}

bool validateMergeRegions(const TableElement& table, QString* errorMessage)
{
    const auto columns = columnIndexById(table);
    const auto rowBands = knownRowBandIds(table);

    for (const auto& merge : table.mergeRegions) {
        if (!rowBands.contains(merge.rowBandId)) {
            *errorMessage = QString::fromUtf8("表格 %1 的合并区域 %2 引用了不存在的行带：%3")
                                .arg(table.id, merge.id, merge.rowBandId);
            return false;
        }
        if (!columns.contains(merge.startColumnId)) {
            *errorMessage = QString::fromUtf8("表格 %1 的合并区域 %2 引用了不存在的列：%3")
                                .arg(table.id, merge.id, merge.startColumnId);
            return false;
        }
        if (merge.rowSpan <= 0 || merge.colSpan <= 0) {
            *errorMessage = QString::fromUtf8("表格 %1 的合并区域 %2 跨度必须大于 0").arg(table.id, merge.id);
            return false;
        }
        if (merge.rowSpan > 1) {
            *errorMessage = QString::fromUtf8("表格 %1 的合并区域 %2 暂不支持跨行合并").arg(table.id, merge.id);
            return false;
        }

        const auto startColumnIndex = columns.value(merge.startColumnId);
        if (startColumnIndex + merge.colSpan > table.columns.size()) {
            *errorMessage = QString::fromUtf8("表格 %1 的合并区域 %2 超出列范围：%3")
                                .arg(table.id, merge.id, merge.startColumnId);
            return false;
        }
    }

    return true;
}

QJsonValue valueForTemplateField(const QJsonObject* rowObject, const QJsonObject& rootValues, const QString& fieldKey)
{
    if (rowObject != nullptr) {
        const auto rowValue = valueAtPath(*rowObject, fieldKey);
        if (!rowValue.isUndefined()) {
            return rowValue;
        }
    }
    return valueAtPath(rootValues, fieldKey);
}

QString interpolateTemplateText(const QString& text, const QJsonObject* rowObject, const QJsonObject& rootValues)
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
        result.append(valueToText(valueForTemplateField(rowObject, rootValues, fieldKey)));
        cursor = placeholderEnd + 1;
    }

    return result;
}

QString textForCell(const TableColumn& column,
                    const TableCellTemplate* cellTemplate,
                    const QJsonObject* rowObject,
                    const QJsonObject& rootValues,
                    bool header)
{
    if (cellTemplate != nullptr) {
        if (!cellTemplate->textTemplate.isEmpty()) {
            return interpolateTemplateText(cellTemplate->textTemplate, rowObject, rootValues);
        }
        if (!cellTemplate->fieldKey.trimmed().isEmpty()) {
            return valueToText(valueForTemplateField(rowObject, rootValues, cellTemplate->fieldKey));
        }
    }
    return header ? column.title : valueToText(valueAtPath(*rowObject, column.fieldKey));
}

TableLayoutCell makeCell(const TableElement& table,
                         const TableColumn& column,
                         const QString& rowBandId,
                         int sourceRowIndex,
                         const QRectF& rect,
                         const QString& text,
                         const QString& rowKey)
{
    const auto style = styleForColumn(table, column);

    TableLayoutCell cell;
    cell.elementKey = QStringLiteral("%1.%2.%3").arg(table.id, rowKey, column.id);
    cell.rowBandId = rowBandId;
    cell.columnId = column.id;
    cell.sourceRowIndex = sourceRowIndex;
    cell.rect = rect;
    cell.text = text;
    cell.style = style;
    cell.overflowPolicy = overflowPolicyFor(style);
    cell.maxLines = style.wrapText ? 0 : 1;
    return cell;
}

double resolvedRowHeight(const TableElement& table, const TableRowBand& rowBand, const QJsonObject& rowObject)
{
    if (rowBand.heightMode != TableRowHeightMode::Auto) {
        return rowBand.heightMm;
    }

    auto rowHeight = std::max(rowBand.minHeightMm, 0.1);
    const auto widths = resolvedColumnWidths(table);
    for (int columnIndex = 0; columnIndex < table.columns.size(); ++columnIndex) {
        const auto& column = table.columns[columnIndex];
        const auto style = styleForColumn(table, column);
        const auto maxLines = style.wrapText ? 0 : 1;
        const auto text = valueToText(valueAtPath(rowObject, column.fieldKey));
        rowHeight = std::max(rowHeight, estimatedCellHeightMm(text, widths[columnIndex], style, maxLines));
    }
    return rowHeight;
}

void appendColumnCells(TableLayoutPage* page,
                       const TableElement& table,
                       const QString& rowBandId,
                       int sourceRowIndex,
                       double rowY,
                       double rowHeight,
                       const QJsonObject* rowObject,
                       const QJsonObject& rootValues,
                       const QString& rowKey,
                       bool header)
{
    const auto widths = resolvedColumnWidths(table);
    const auto columns = columnIndexById(table);
    double currentX = 0.0;
    for (int columnIndex = 0; columnIndex < table.columns.size(); ++columnIndex) {
        const auto& column = table.columns[columnIndex];
        const auto cellTemplate = findCellTemplate(table, rowBandId, column.id);
        const auto merge = findMergeStartingAt(table, rowBandId, column.id);
        const auto coveredByMerge = isCoveredByMerge(table, rowBandId, columnIndex, columns);
        auto width = widths[columnIndex];
        if (merge != nullptr) {
            width = 0.0;
            for (int spanIndex = 0; spanIndex < merge->colSpan && columnIndex + spanIndex < widths.size(); ++spanIndex) {
                width += widths[columnIndex + spanIndex];
            }
        }

        if (cellTemplate != nullptr && !cellTemplate->visible) {
            currentX += widths[columnIndex];
            continue;
        }

        auto cell = makeCell(
            table,
            column,
            rowBandId,
            sourceRowIndex,
            QRectF(currentX, rowY, width, rowHeight),
            coveredByMerge ? QString() : textForCell(column, cellTemplate, rowObject, rootValues, header),
            rowKey);
        cell.coveredByMerge = coveredByMerge;
        if (cellTemplate != nullptr) {
            if (const auto* style = findStyle(table, cellTemplate->styleId)) {
                cell.style = *style;
            }
            cell.overflowPolicy = cellTemplate->overflowPolicy;
            cell.maxLines = cellTemplate->maxLines;
        }
        page->cells.append(cell);
        currentX += widths[columnIndex];
    }
}

QList<TableRowBand> headerBands(const TableElement& table)
{
    QList<TableRowBand> result;
    if (table.rowBands.isEmpty()) {
        TableRowBand rowBand;
        rowBand.id = QStringLiteral("header");
        rowBand.kind = TableRowBandKind::Header;
        rowBand.heightMm = table.headerRowHeightMm;
        result.append(rowBand);
        return result;
    }

    for (const auto& rowBand : table.rowBands) {
        if (rowBand.kind == TableRowBandKind::Header || rowBand.kind == TableRowBandKind::GroupHeader) {
            result.append(rowBand);
        }
    }
    return result;
}

QList<TableRowBand> detailBands(const TableElement& table)
{
    QList<TableRowBand> result;
    if (table.rowBands.isEmpty()) {
        TableRowBand rowBand;
        rowBand.id = QStringLiteral("detail");
        rowBand.kind = TableRowBandKind::Detail;
        rowBand.heightMm = table.detailRowHeightMm;
        result.append(rowBand);
        return result;
    }

    for (const auto& rowBand : table.rowBands) {
        if (rowBand.kind == TableRowBandKind::Detail) {
            result.append(rowBand);
        }
    }
    return result;
}

} // namespace

bool TableLayoutResult::success() const
{
    return errorMessage.isEmpty();
}

TableLayoutResult TableElementLayout::layout(const TableElement& table, const TemplateRenderContext& context)
{
    TableLayoutResult result;
    const auto tableId = table.id.trimmed();
    if (tableId.isEmpty()) {
        result.errorMessage = QString::fromUtf8("表格 id 不能为空");
        return result;
    }
    if (table.columns.isEmpty()) {
        result.errorMessage = QString::fromUtf8("表格 %1 至少需要一列").arg(tableId);
        return result;
    }

    QString mergeError;
    if (!validateMergeRegions(table, &mergeError)) {
        result.errorMessage = mergeError;
        return result;
    }

    const auto dataPath = table.dataPath.trimmed();
    const auto rowsValue = valueAtPath(context.values, dataPath);
    if (!rowsValue.isArray()) {
        result.errorMessage = QString::fromUtf8("表格数据 %1 必须是数组").arg(dataPath);
        return result;
    }

    const auto rows = rowsValue.toArray();
    TableLayoutPage page;
    page.pageNumber = 1;
    page.firstRowIndex = 0;
    page.rowCount = rows.size();

    double currentY = 0.0;
    for (const auto& rowBand : headerBands(table)) {
        appendColumnCells(&page, table, rowBand.id, -1, currentY, rowBand.heightMm, nullptr, context.values, rowBand.id, true);
        currentY += rowBand.heightMm;
    }

    const auto details = detailBands(table);
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        if (!rows[rowIndex].isObject()) {
            result.errorMessage = QString::fromUtf8("表格数据 %1 的第 %2 行必须是对象").arg(dataPath).arg(rowIndex + 1);
            return result;
        }
        const auto row = rows[rowIndex].toObject();
        for (const auto& rowBand : details) {
            const auto rowKey = QStringLiteral("row%1").arg(rowIndex);
            const auto rowHeight = resolvedRowHeight(table, rowBand, row);
            appendColumnCells(&page, table, rowBand.id, rowIndex, currentY, rowHeight, &row, context.values, rowKey, false);
            currentY += rowHeight;
        }
    }

    result.pages.append(page);
    return result;
}

} // namespace sleekpr::core
