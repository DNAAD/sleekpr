#include "sleekpr/app/TemplateTableCanvasEditor.h"

#include "sleekpr/core/templates/TableElement.h"

#include <QRectF>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace sleekpr::app {

namespace {

constexpr double kDefaultCanvasColumnWidthMm = 20.0;
constexpr double kMinimumCanvasRowHeightMm = 0.1;

double columnFlexWeight(const sleekpr::core::TableColumn& column)
{
    return column.flexWeight > 0.0 ? column.flexWeight : 1.0;
}

QList<double> resolvedColumnWidths(const sleekpr::core::TableElement& table)
{
    QList<double> widths;
    widths.reserve(table.columns.size());

    double fixedTotal = 0.0;
    double flexWeightTotal = 0.0;
    for (const auto& column : table.columns) {
        if (column.widthMode == sleekpr::core::TableColumnWidthMode::Flex) {
            flexWeightTotal += columnFlexWeight(column);
        } else {
            fixedTotal += std::max(0.0, column.widthMm);
        }
    }

    const auto remainingWidth = std::max(0.0, table.width - fixedTotal);
    for (const auto& column : table.columns) {
        if (column.widthMode == sleekpr::core::TableColumnWidthMode::Flex && flexWeightTotal > 0.0) {
            widths.append(std::max(column.minWidthMm, remainingWidth * columnFlexWeight(column) / flexWeightTotal));
        } else {
            widths.append(std::max(column.minWidthMm, column.widthMm));
        }
    }
    return widths;
}

int columnIndexAt(const sleekpr::core::TableElement& table, double xMm)
{
    const auto widths = resolvedColumnWidths(table);
    auto currentX = table.x;
    for (int index = 0; index < widths.size(); ++index) {
        const auto nextX = currentX + widths[index];
        if (xMm >= currentX && xMm <= nextX) {
            return index;
        }
        currentX = nextX;
    }
    return -1;
}

double columnLeftAt(const sleekpr::core::TableElement& table, int columnIndex)
{
    const auto widths = resolvedColumnWidths(table);
    auto currentX = table.x;
    for (int index = 0; index < columnIndex && index < widths.size(); ++index) {
        currentX += widths[index];
    }
    return currentX;
}

double columnWidthAt(const sleekpr::core::TableElement& table, int columnIndex)
{
    const auto widths = resolvedColumnWidths(table);
    if (columnIndex < 0 || columnIndex >= widths.size()) {
        return 0.0;
    }
    return widths[columnIndex];
}

QString rowBandIdFor(TableCanvasBand band)
{
    return band == TableCanvasBand::Header ? QStringLiteral("header") : QStringLiteral("detail");
}

QString rowBandIdForHit(const TableCanvasHit& hit)
{
    return hit.rowBandId.trimmed().isEmpty() ? rowBandIdFor(hit.band) : hit.rowBandId.trimmed();
}

bool hasColumnId(const sleekpr::core::TableElement& table, const QString& columnId)
{
    return std::any_of(table.columns.cbegin(), table.columns.cend(), [&columnId](const sleekpr::core::TableColumn& column) {
        return column.id == columnId;
    });
}

QString uniqueColumnId(const sleekpr::core::TableElement& table, QString preferredId)
{
    auto base = preferredId.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("column");
    }

    auto candidate = base;
    auto suffix = 2;
    while (hasColumnId(table, candidate)) {
        candidate = QStringLiteral("%1_%2").arg(base).arg(suffix);
        ++suffix;
    }
    return candidate;
}

sleekpr::core::TableColumn createDefaultColumn(const sleekpr::core::TableElement& table)
{
    auto nextIndex = 1;
    while (hasColumnId(table, QStringLiteral("column%1").arg(nextIndex))) {
        ++nextIndex;
    }

    sleekpr::core::TableColumn column;
    column.id = QStringLiteral("column%1").arg(nextIndex);
    column.title = QString::fromUtf8("新列");
    column.fieldKey = QStringLiteral("field%1").arg(nextIndex);
    column.widthMode = sleekpr::core::TableColumnWidthMode::Fixed;
    column.widthMm = kDefaultCanvasColumnWidthMm;
    return column;
}

int currentColumnIndexById(const sleekpr::core::TableElement& table, const QString& columnId)
{
    for (int index = 0; index < table.columns.size(); ++index) {
        if (table.columns[index].id == columnId) {
            return index;
        }
    }
    return -1;
}

QString columnIdForHit(const sleekpr::core::TableElement& table, const TableCanvasHit& hit)
{
    if (!hit.columnId.trimmed().isEmpty()) {
        return hit.columnId.trimmed();
    }
    if (hit.columnIndex >= 0 && hit.columnIndex < table.columns.size()) {
        return table.columns[hit.columnIndex].id;
    }
    return QString();
}

QString uniqueId(QString base, const QSet<QString>& usedIds)
{
    base = base.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("item");
    }

    auto candidate = base;
    auto suffix = 2;
    while (usedIds.contains(candidate)) {
        candidate = QStringLiteral("%1_%2").arg(base).arg(suffix);
        ++suffix;
    }
    return candidate;
}

QString uniqueCellTemplateId(const sleekpr::core::TableElement& table, const QString& rowBandId, const QString& columnId)
{
    QSet<QString> usedIds;
    for (const auto& cell : table.cellTemplates) {
        usedIds.insert(cell.id);
    }
    return uniqueId(QStringLiteral("canvas-cell-%1-%2").arg(rowBandId, columnId), usedIds);
}

QString uniqueCellStyleId(const sleekpr::core::TableElement& table, const QString& rowBandId, const QString& columnId)
{
    QSet<QString> usedIds;
    for (const auto& style : table.cellStyles) {
        usedIds.insert(style.id);
    }
    return uniqueId(QStringLiteral("canvas-style-%1-%2").arg(rowBandId, columnId), usedIds);
}

bool isCanvasCellStyleId(const QString& styleId)
{
    return styleId.startsWith(QStringLiteral("canvas-style-"));
}

QString uniqueMergeRegionId(const sleekpr::core::TableElement& table, const QString& rowBandId, const QString& columnId, int rowOffset)
{
    QSet<QString> usedIds;
    for (const auto& merge : table.mergeRegions) {
        usedIds.insert(merge.id);
    }
    return uniqueId(QStringLiteral("canvas-merge-%1-%2-%3").arg(rowBandId, columnId).arg(rowOffset), usedIds);
}

void removeColumnReferences(sleekpr::core::TableElement* table, const QString& columnId, int columnIndex)
{
    if (table == nullptr || columnId.trimmed().isEmpty()) {
        return;
    }

    table->cellTemplates.erase(
        std::remove_if(table->cellTemplates.begin(), table->cellTemplates.end(), [&columnId](const sleekpr::core::TableCellTemplate& cell) {
            return cell.columnId == columnId;
        }),
        table->cellTemplates.end());

    table->mergeRegions.erase(
        std::remove_if(
            table->mergeRegions.begin(),
            table->mergeRegions.end(),
            [table, columnId, columnIndex](const sleekpr::core::TableMergeRegion& merge) {
                if (merge.startColumnId == columnId) {
                    return true;
                }
                const auto startIndex = currentColumnIndexById(*table, merge.startColumnId);
                if (startIndex < 0 || columnIndex < 0) {
                    return false;
                }
                // 删除被合并区域覆盖的列时，直接移除该合并关系，避免留下跨不存在列的布局配置。
                return columnIndex >= startIndex && columnIndex < startIndex + std::max(1, merge.colSpan);
            }),
        table->mergeRegions.end());
}

bool validColumnIndex(const sleekpr::core::TableElement* table, int columnIndex)
{
    return table != nullptr && columnIndex >= 0 && columnIndex < table->columns.size();
}

bool validCellHit(const sleekpr::core::TableElement* table, const TableCanvasHit& hit)
{
    return validColumnIndex(table, hit.columnIndex) && !columnIdForHit(*table, hit).isEmpty();
}

bool mergeCoversHit(const sleekpr::core::TableElement& table, const sleekpr::core::TableMergeRegion& merge, const TableCanvasHit& hit)
{
    const auto rowBandId = rowBandIdForHit(hit);
    if (merge.rowBandId != rowBandId) {
        return false;
    }
    const auto startIndex = currentColumnIndexById(table, merge.startColumnId);
    if (startIndex < 0) {
        return false;
    }

    const auto colSpan = std::max(1, merge.colSpan);
    const auto rowSpan = std::max(1, merge.rowSpan);
    const auto rowCovered = hit.rowOffset >= merge.startRowOffset && hit.rowOffset < merge.startRowOffset + rowSpan;
    const auto columnCovered = hit.columnIndex >= startIndex && hit.columnIndex < startIndex + colSpan;
    return rowCovered && columnCovered;
}

QRectF selectionRectMm(
    const sleekpr::core::TableElement& table,
    const QString& rowBandId,
    int startColumnIndex,
    int endColumnIndex,
    int startRowOffset,
    int endRowOffset)
{
    const auto left = columnLeftAt(table, startColumnIndex);
    const auto right = columnLeftAt(table, endColumnIndex) + columnWidthAt(table, endColumnIndex);
    const auto headerRowHeight = std::max(kMinimumCanvasRowHeightMm, table.headerRowHeightMm);
    const auto detailRowHeight = std::max(kMinimumCanvasRowHeightMm, table.detailRowHeightMm);

    // 画布选区只描述当前可见的表头/明细网格，用于编辑反馈，不参与真实打印排版。
    if (rowBandId == QStringLiteral("header")) {
        return QRectF(left, table.y, right - left, headerRowHeight);
    }

    const auto top = table.y + headerRowHeight + std::max(0, startRowOffset) * detailRowHeight;
    const auto bottom = std::min(
        table.y + table.height,
        table.y + headerRowHeight + (std::max(startRowOffset, endRowOffset) + 1) * detailRowHeight);
    return QRectF(left, top, right - left, std::max(kMinimumCanvasRowHeightMm, bottom - top));
}

bool mergeOverlapsSelection(
    const sleekpr::core::TableElement& table,
    const sleekpr::core::TableMergeRegion& merge,
    const TableCanvasSelection& selection)
{
    if (merge.rowBandId != selection.rowBandId) {
        return false;
    }

    const auto mergeStartColumn = currentColumnIndexById(table, merge.startColumnId);
    if (mergeStartColumn < 0) {
        return false;
    }

    const auto mergeEndColumn = mergeStartColumn + std::max(1, merge.colSpan) - 1;
    const auto mergeEndRow = merge.startRowOffset + std::max(1, merge.rowSpan) - 1;
    const auto columnsOverlap = mergeStartColumn <= selection.endColumnIndex && mergeEndColumn >= selection.startColumnIndex;
    const auto rowsOverlap = merge.startRowOffset <= selection.endRowOffset && mergeEndRow >= selection.startRowOffset;
    return columnsOverlap && rowsOverlap;
}

sleekpr::core::TableCellStyle styleFromColumn(const sleekpr::core::TableColumn& column)
{
    sleekpr::core::TableCellStyle style;
    style.fontSizePt = column.fontSizePt > 0.0 ? column.fontSizePt : style.fontSizePt;
    style.bold = column.bold;
    style.alignment = column.alignment;
    style.wrapText = column.wrapText;
    style.ellipsis = column.ellipsis;
    return style;
}

sleekpr::core::TableCellTemplate* ensureCellTemplate(sleekpr::core::TableElement* table, const TableCanvasHit& hit)
{
    if (!validCellHit(table, hit)) {
        return nullptr;
    }

    const auto rowBandId = rowBandIdForHit(hit);
    const auto columnId = columnIdForHit(*table, hit);
    auto cell = std::find_if(
        table->cellTemplates.begin(),
        table->cellTemplates.end(),
        [&rowBandId, &columnId](const sleekpr::core::TableCellTemplate& candidate) {
            return candidate.rowBandId == rowBandId && candidate.columnId == columnId;
        });
    if (cell != table->cellTemplates.end()) {
        return &(*cell);
    }

    sleekpr::core::TableCellTemplate created;
    created.id = uniqueCellTemplateId(*table, rowBandId, columnId);
    created.rowBandId = rowBandId;
    created.columnId = columnId;
    table->cellTemplates.append(created);
    return &table->cellTemplates.last();
}

sleekpr::core::TableCellStyle* findStyle(sleekpr::core::TableElement* table, const QString& styleId)
{
    if (table == nullptr || styleId.trimmed().isEmpty()) {
        return nullptr;
    }

    auto style = std::find_if(
        table->cellStyles.begin(),
        table->cellStyles.end(),
        [&styleId](const sleekpr::core::TableCellStyle& candidate) {
            return candidate.id == styleId;
        });
    return style == table->cellStyles.end() ? nullptr : &(*style);
}

sleekpr::core::TableCellStyle* ensureCellStyle(sleekpr::core::TableElement* table, const TableCanvasHit& hit)
{
    auto* cell = ensureCellTemplate(table, hit);
    if (cell == nullptr) {
        return nullptr;
    }

    const auto rowBandId = rowBandIdForHit(hit);
    const auto columnId = columnIdForHit(*table, hit);
    auto* existing = findStyle(table, cell->styleId);
    if (existing != nullptr && isCanvasCellStyleId(cell->styleId)) {
        return existing;
    }

    // 画布上的单元格样式修改必须生成当前单元格专属样式，避免误改被多个单元格复用的共享样式。
    auto created = existing != nullptr ? *existing : styleFromColumn(table->columns[hit.columnIndex]);
    created.id = uniqueCellStyleId(*table, rowBandId, columnId);
    table->cellStyles.append(created);
    cell->styleId = table->cellStyles.last().id;
    return &table->cellStyles.last();
}

} // namespace

bool TableCanvasSelection::isValid() const
{
    return !tableId.trimmed().isEmpty()
        && !rowBandId.trimmed().isEmpty()
        && startColumnIndex >= 0
        && endColumnIndex >= startColumnIndex
        && startRowOffset >= 0
        && endRowOffset >= startRowOffset
        && !rectMm.isNull();
}

bool TableCanvasSelection::isSingleCell() const
{
    return isValid() && startColumnIndex == endColumnIndex && startRowOffset == endRowOffset;
}

std::optional<TableCanvasHit> TemplateTableCanvasEditor::hitTest(
    const sleekpr::core::TemplateDocument& document,
    QSizeF paperSizeMm,
    QSize previewImageSizePx,
    QPoint imagePositionPx)
{
    if (paperSizeMm.isEmpty() || previewImageSizePx.isEmpty()) {
        return std::nullopt;
    }

    const QPointF positionMm(
        imagePositionPx.x() * paperSizeMm.width() / previewImageSizePx.width(),
        imagePositionPx.y() * paperSizeMm.height() / previewImageSizePx.height());

    for (auto layerIt = document.layers.crbegin(); layerIt != document.layers.crend(); ++layerIt) {
        if (!layerIt->visible || layerIt->locked) {
            continue;
        }

        for (auto tableIt = layerIt->tables.crbegin(); tableIt != layerIt->tables.crend(); ++tableIt) {
            const auto& table = *tableIt;
            if (!table.visible || table.locked || table.columns.isEmpty()) {
                continue;
            }
            if (!QRectF(table.x, table.y, table.width, table.height).contains(positionMm)) {
                continue;
            }

            const auto columnIndex = columnIndexAt(table, positionMm.x());
            if (columnIndex < 0) {
                continue;
            }

            TableCanvasHit hit;
            hit.tableId = table.id;
            hit.columnIndex = columnIndex;
            hit.columnId = table.columns[columnIndex].id;
            const auto headerRowHeight = std::max(kMinimumCanvasRowHeightMm, table.headerRowHeightMm);
            const auto detailRowHeight = std::max(kMinimumCanvasRowHeightMm, table.detailRowHeightMm);
            const auto columnLeft = columnLeftAt(table, columnIndex);
            const auto columnWidth = columnWidthAt(table, columnIndex);
            if (positionMm.y() <= table.y + headerRowHeight) {
                hit.band = TableCanvasBand::Header;
                hit.rowBandId = QStringLiteral("header");
                hit.rowOffset = 0;
                hit.cellRectMm = QRectF(columnLeft, table.y, columnWidth, headerRowHeight);
            } else {
                const auto detailY = table.y + headerRowHeight;
                hit.band = TableCanvasBand::Detail;
                hit.rowBandId = QStringLiteral("detail");
                hit.rowOffset = std::max(0, static_cast<int>(std::floor((positionMm.y() - detailY) / detailRowHeight)));
                const auto rowTop = detailY + hit.rowOffset * detailRowHeight;
                const auto rowHeight = std::min(detailRowHeight, std::max(kMinimumCanvasRowHeightMm, table.y + table.height - rowTop));
                hit.cellRectMm = QRectF(columnLeft, rowTop, columnWidth, rowHeight);
            }
            return hit;
        }
    }

    return std::nullopt;
}

bool TemplateTableCanvasEditor::setColumnTitle(sleekpr::core::TableElement* table, int columnIndex, const QString& title)
{
    if (!validColumnIndex(table, columnIndex)) {
        return false;
    }

    const auto normalizedTitle = title.trimmed();
    if (normalizedTitle.isEmpty() || table->columns[columnIndex].title == normalizedTitle) {
        return false;
    }

    table->columns[columnIndex].title = normalizedTitle;
    return true;
}

bool TemplateTableCanvasEditor::setColumnFieldKey(sleekpr::core::TableElement* table, int columnIndex, const QString& fieldKey)
{
    if (!validColumnIndex(table, columnIndex)) {
        return false;
    }

    const auto normalizedFieldKey = fieldKey.trimmed();
    if (normalizedFieldKey.isEmpty() || table->columns[columnIndex].fieldKey == normalizedFieldKey) {
        return false;
    }

    // 画布编辑字段名时保留 column.id，不破坏合并区域、列宽和单元格模板对稳定列 ID 的引用。
    table->columns[columnIndex].fieldKey = normalizedFieldKey;
    return true;
}

bool TemplateTableCanvasEditor::insertColumnAfter(sleekpr::core::TableElement* table, int columnIndex)
{
    if (table == nullptr || columnIndex < -1 || columnIndex >= table->columns.size()) {
        return false;
    }

    table->columns.insert(columnIndex + 1, createDefaultColumn(*table));
    return true;
}

bool TemplateTableCanvasEditor::duplicateColumn(sleekpr::core::TableElement* table, int columnIndex)
{
    if (!validColumnIndex(table, columnIndex)) {
        return false;
    }

    auto duplicated = table->columns[columnIndex];
    duplicated.id = uniqueColumnId(*table, duplicated.id + QStringLiteral("_copy"));
    table->columns.insert(columnIndex + 1, duplicated);
    return true;
}

bool TemplateTableCanvasEditor::deleteColumn(sleekpr::core::TableElement* table, int columnIndex)
{
    if (!validColumnIndex(table, columnIndex) || table->columns.size() <= 1) {
        return false;
    }

    const auto removedColumnId = table->columns[columnIndex].id;
    removeColumnReferences(table, removedColumnId, columnIndex);
    table->columns.removeAt(columnIndex);
    return true;
}

bool TemplateTableCanvasEditor::moveColumnLeft(sleekpr::core::TableElement* table, int columnIndex)
{
    if (!validColumnIndex(table, columnIndex) || columnIndex <= 0) {
        return false;
    }

    table->columns.swapItemsAt(columnIndex, columnIndex - 1);
    return true;
}

bool TemplateTableCanvasEditor::moveColumnRight(sleekpr::core::TableElement* table, int columnIndex)
{
    if (!validColumnIndex(table, columnIndex) || columnIndex >= table->columns.size() - 1) {
        return false;
    }

    table->columns.swapItemsAt(columnIndex, columnIndex + 1);
    return true;
}

std::optional<TableCanvasSelection> TemplateTableCanvasEditor::selectionFromHits(
    const sleekpr::core::TableElement& table,
    const TableCanvasHit& anchor,
    const TableCanvasHit& target)
{
    if (!validCellHit(&table, anchor) || !validCellHit(&table, target)) {
        return std::nullopt;
    }
    if (anchor.tableId != table.id || target.tableId != table.id || anchor.tableId != target.tableId) {
        return std::nullopt;
    }

    const auto rowBandId = rowBandIdForHit(anchor);
    if (rowBandId != rowBandIdForHit(target)) {
        return std::nullopt;
    }

    TableCanvasSelection selection;
    selection.tableId = table.id;
    selection.startColumnIndex = std::min(anchor.columnIndex, target.columnIndex);
    selection.endColumnIndex = std::max(anchor.columnIndex, target.columnIndex);
    selection.rowBandId = rowBandId;
    selection.startRowOffset = std::min(anchor.rowOffset, target.rowOffset);
    selection.endRowOffset = std::max(anchor.rowOffset, target.rowOffset);
    selection.rectMm = selectionRectMm(
        table,
        rowBandId,
        selection.startColumnIndex,
        selection.endColumnIndex,
        selection.startRowOffset,
        selection.endRowOffset);
    return selection.isValid() ? std::make_optional(selection) : std::nullopt;
}

bool TemplateTableCanvasEditor::selectionContainsHit(const TableCanvasSelection& selection, const TableCanvasHit& hit)
{
    if (!selection.isValid() || hit.tableId != selection.tableId || rowBandIdForHit(hit) != selection.rowBandId) {
        return false;
    }
    return hit.columnIndex >= selection.startColumnIndex
        && hit.columnIndex <= selection.endColumnIndex
        && hit.rowOffset >= selection.startRowOffset
        && hit.rowOffset <= selection.endRowOffset;
}

bool TemplateTableCanvasEditor::mergeSelection(sleekpr::core::TableElement* table, const TableCanvasSelection& selection)
{
    if (table == nullptr
        || table->id != selection.tableId
        || !selection.isValid()
        || selection.isSingleCell()
        || selection.endColumnIndex >= table->columns.size()) {
        return false;
    }

    for (const auto& merge : table->mergeRegions) {
        // 新合并区不能与已有合并区交叠，否则渲染阶段会出现同一单元格被多个区域覆盖。
        if (mergeOverlapsSelection(*table, merge, selection)) {
            return false;
        }
    }

    const auto startColumnId = table->columns[selection.startColumnIndex].id;
    sleekpr::core::TableMergeRegion merge;
    merge.id = uniqueMergeRegionId(*table, selection.rowBandId, startColumnId, selection.startRowOffset);
    merge.rowBandId = selection.rowBandId;
    merge.startRowOffset = selection.startRowOffset;
    merge.startColumnId = startColumnId;
    merge.rowSpan = selection.endRowOffset - selection.startRowOffset + 1;
    merge.colSpan = selection.endColumnIndex - selection.startColumnIndex + 1;
    table->mergeRegions.append(merge);
    return true;
}

bool TemplateTableCanvasEditor::splitSelection(sleekpr::core::TableElement* table, const TableCanvasSelection& selection)
{
    if (table == nullptr || table->id != selection.tableId || !selection.isValid()) {
        return false;
    }

    const auto beforeSize = table->mergeRegions.size();
    table->mergeRegions.erase(
        std::remove_if(
            table->mergeRegions.begin(),
            table->mergeRegions.end(),
            [table, selection](const sleekpr::core::TableMergeRegion& merge) {
                return mergeOverlapsSelection(*table, merge, selection);
            }),
        table->mergeRegions.end());
    return table->mergeRegions.size() != beforeSize;
}

bool TemplateTableCanvasEditor::mergeCellRight(sleekpr::core::TableElement* table, const TableCanvasHit& hit)
{
    if (!validCellHit(table, hit) || hit.columnIndex >= table->columns.size() - 1) {
        return false;
    }

    for (const auto& merge : table->mergeRegions) {
        auto nextHit = hit;
        nextHit.columnIndex = hit.columnIndex + 1;
        nextHit.columnId = table->columns[nextHit.columnIndex].id;
        if (mergeCoversHit(*table, merge, hit) || mergeCoversHit(*table, merge, nextHit)) {
            return false;
        }
    }

    const auto rowBandId = rowBandIdForHit(hit);
    const auto columnId = columnIdForHit(*table, hit);
    sleekpr::core::TableMergeRegion merge;
    merge.id = uniqueMergeRegionId(*table, rowBandId, columnId, hit.rowOffset);
    merge.rowBandId = rowBandId;
    merge.startRowOffset = hit.rowOffset;
    merge.startColumnId = columnId;
    merge.rowSpan = 1;
    merge.colSpan = 2;
    table->mergeRegions.append(merge);
    return true;
}

bool TemplateTableCanvasEditor::splitCell(sleekpr::core::TableElement* table, const TableCanvasHit& hit)
{
    if (!validCellHit(table, hit)) {
        return false;
    }

    const auto beforeSize = table->mergeRegions.size();
    table->mergeRegions.erase(
        std::remove_if(
            table->mergeRegions.begin(),
            table->mergeRegions.end(),
            [table, hit](const sleekpr::core::TableMergeRegion& merge) {
                return mergeCoversHit(*table, merge, hit);
            }),
        table->mergeRegions.end());
    return table->mergeRegions.size() != beforeSize;
}

bool TemplateTableCanvasEditor::toggleCellBold(sleekpr::core::TableElement* table, const TableCanvasHit& hit)
{
    auto* style = ensureCellStyle(table, hit);
    if (style == nullptr) {
        return false;
    }

    style->bold = !style->bold;
    return true;
}

bool TemplateTableCanvasEditor::toggleCellWrap(sleekpr::core::TableElement* table, const TableCanvasHit& hit)
{
    auto* style = ensureCellStyle(table, hit);
    auto* cell = ensureCellTemplate(table, hit);
    if (style == nullptr || cell == nullptr) {
        return false;
    }

    style->wrapText = !style->wrapText;
    cell->overflowPolicy = style->wrapText
        ? sleekpr::core::TableCellOverflowPolicy::Wrap
        : sleekpr::core::TableCellOverflowPolicy::Clip;
    cell->maxLines = style->wrapText ? 0 : 1;
    return true;
}

bool TemplateTableCanvasEditor::setCellAlignment(
    sleekpr::core::TableElement* table,
    const TableCanvasHit& hit,
    sleekpr::core::TableCellAlignment alignment)
{
    auto* style = ensureCellStyle(table, hit);
    if (style == nullptr || style->alignment == alignment) {
        return false;
    }

    style->alignment = alignment;
    return true;
}

} // namespace sleekpr::app
