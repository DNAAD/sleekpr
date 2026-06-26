#include "sleekpr/app/TemplateTableCanvasEditor.h"

#include "sleekpr/core/templates/TableElement.h"

#include <QRectF>

#include <algorithm>
#include <cmath>

namespace sleekpr::app {

namespace {

constexpr double kDefaultCanvasColumnWidthMm = 20.0;

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

} // namespace

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
            hit.band = positionMm.y() <= table.y + std::max(0.1, table.headerRowHeightMm)
                ? TableCanvasBand::Header
                : TableCanvasBand::Detail;
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

} // namespace sleekpr::app
