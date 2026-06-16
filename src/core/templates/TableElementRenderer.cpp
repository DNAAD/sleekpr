#include "sleekpr/core/templates/TableElementRenderer.h"

#include "sleekpr/core/native/NativeDrawCommandType.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace sleekpr::core {

namespace {

constexpr double textPaddingX = 0.6;
constexpr double textPaddingY = 0.4;

NativeDrawCommand rectangleCommand(double x, double y, double width, double height, const QString& key)
{
    return NativeDrawCommand{
        NativeDrawCommandType::Rectangle,
        x,
        y,
        width,
        height,
        QString(),
        0.0,
        false,
        0.0,
        0,
        false,
        key,
    };
}

NativeDrawCommand textCommand(double x, double y, double width, double height, const QString& text, const TableColumn& column, const QString& key)
{
    return NativeDrawCommand{
        NativeDrawCommandType::Text,
        x + textPaddingX,
        y + textPaddingY,
        std::max(0.1, width - textPaddingX * 2.0),
        std::max(0.1, height - textPaddingY * 2.0),
        text,
        column.fontSizePt > 0.0 ? column.fontSizePt : 8.0,
        column.bold,
        0.0,
        1,
        column.ellipsis,
        key,
    };
}

QString valueToText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return QString();
}

void appendCellCommands(QList<NativeDrawCommand>& commands,
                        const QString& tableId,
                        const QString& rowKey,
                        const TableColumn& column,
                        double x,
                        double y,
                        double height,
                        const QString& text,
                        bool drawBorders)
{
    const auto cellKey = QStringLiteral("%1.%2.%3").arg(tableId, rowKey, column.id);
    if (drawBorders) {
        commands.append(rectangleCommand(x, y, column.widthMm, height, cellKey + QStringLiteral(".border")));
    }
    commands.append(textCommand(x, y, column.widthMm, height, text, column, cellKey));
}

int detailCapacity(const TableElement& table, bool drawHeader)
{
    const auto usableHeight = table.height - (drawHeader ? table.headerRowHeightMm : 0.0);
    if (usableHeight < 0.0 || table.detailRowHeightMm <= 0.0) {
        return -1;
    }
    return static_cast<int>(std::floor(usableHeight / table.detailRowHeightMm));
}

void appendHeaderCommands(QList<NativeDrawCommand>& commands,
                          const TableElement& table,
                          const QString& tableId,
                          const QString& rowKey,
                          double originX,
                          double originY)
{
    double currentX = originX;
    for (const auto& column : table.columns) {
        appendCellCommands(
            commands,
            tableId,
            rowKey,
            column,
            currentX,
            originY,
            table.headerRowHeightMm,
            column.title,
            table.drawBorders);
        currentX += column.widthMm;
    }
}

void appendDetailRowCommands(QList<NativeDrawCommand>& commands,
                             const TableElement& table,
                             const QString& tableId,
                             const QJsonObject& row,
                             int originalRowIndex,
                             int pageRowIndex,
                             double originX,
                             double originY,
                             bool drawHeader)
{
    const auto rowY =
        originY + (drawHeader ? table.headerRowHeightMm : 0.0) + pageRowIndex * table.detailRowHeightMm;
    double currentX = originX;
    for (const auto& column : table.columns) {
        appendCellCommands(
            commands,
            tableId,
            QStringLiteral("row%1").arg(originalRowIndex),
            column,
            currentX,
            rowY,
            table.detailRowHeightMm,
            valueToText(row.value(column.fieldKey)),
            table.drawBorders);
        currentX += column.widthMm;
    }
}

} // 命名空间

bool TableRenderResult::success() const
{
    return errorMessage.isEmpty();
}

TableRenderResult TableElementRenderer::renderSinglePage(const TableElement& table,
                                                         const TemplateRenderContext& context,
                                                         double offsetX,
                                                         double offsetY)
{
    TableRenderResult result;
    const auto tableId = table.id.trimmed();
    if (tableId.isEmpty()) {
        result.errorMessage = QStringLiteral("表格 id 不能为空");
        return result;
    }

    const auto dataPath = table.dataPath.trimmed();
    const auto rowsValue = context.values.value(dataPath);
    if (!rowsValue.isArray()) {
        result.errorMessage = QStringLiteral("表格数据 %1 必须是数组").arg(dataPath);
        return result;
    }

    const auto rows = rowsValue.toArray();
    const auto capacity = detailCapacity(table, true);
    if (capacity < 0 || rows.size() > capacity) {
        // 单页入口继续保持严格行为，避免调用方误以为超出部分已经被打印。
        result.errorMessage = QStringLiteral("表格 %1 高度无法容纳全部明细行").arg(tableId);
        return result;
    }

    const auto originX = table.x + offsetX;
    const auto originY = table.y + offsetY;

    appendHeaderCommands(result.commands, table, tableId, QStringLiteral("header"), originX, originY);

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        if (!rows[rowIndex].isObject()) {
            result.commands.clear();
            result.errorMessage = QStringLiteral("表格数据 %1 的第 %2 行必须是对象").arg(dataPath).arg(rowIndex + 1);
            return result;
        }

        const auto row = rows[rowIndex].toObject();
        appendDetailRowCommands(result.commands, table, tableId, row, rowIndex, rowIndex, originX, originY, true);
    }

    TableRenderPage page;
    page.pageNumber = 1;
    page.firstRowIndex = 0;
    page.rowCount = static_cast<int>(rows.size());
    page.commands = result.commands;
    result.pages.append(page);
    return result;
}

TableRenderResult TableElementRenderer::renderPages(const TableElement& table,
                                                    const TemplateRenderContext& context,
                                                    double offsetX,
                                                    double offsetY)
{
    TableRenderResult result;
    const auto tableId = table.id.trimmed();
    if (tableId.isEmpty()) {
        result.errorMessage = QStringLiteral("表格 id 不能为空");
        return result;
    }

    const auto dataPath = table.dataPath.trimmed();
    const auto rowsValue = context.values.value(dataPath);
    if (!rowsValue.isArray()) {
        result.errorMessage = QStringLiteral("表格数据 %1 必须是数组").arg(dataPath);
        return result;
    }

    const auto rows = rowsValue.toArray();
    const auto originX = table.x + offsetX;
    const auto originY = table.y + offsetY;

    if (rows.isEmpty()) {
        TableRenderPage page;
        page.pageNumber = 1;
        appendHeaderCommands(page.commands, table, tableId, QStringLiteral("header"), originX, originY);
        result.pages.append(page);
        result.commands = page.commands;
        return result;
    }

    int rowIndex = 0;
    int pageNumber = 1;
    while (rowIndex < rows.size()) {
        const auto drawHeader = pageNumber == 1 || table.repeatHeaderOnPage;
        const auto capacity = detailCapacity(table, drawHeader);
        if (capacity <= 0) {
            result.pages.clear();
            result.commands.clear();
            result.errorMessage = QStringLiteral("表格 %1 高度无法容纳单行明细").arg(tableId);
            return result;
        }

        TableRenderPage page;
        page.pageNumber = pageNumber;
        page.firstRowIndex = rowIndex;
        if (drawHeader) {
            const auto headerKey =
                pageNumber == 1 ? QStringLiteral("header") : QStringLiteral("page%1.header").arg(pageNumber);
            appendHeaderCommands(page.commands, table, tableId, headerKey, originX, originY);
        }

        const auto remainingRows = static_cast<int>(rows.size()) - rowIndex;
        const auto rowsOnPage = std::min(capacity, remainingRows);
        for (int pageRowIndex = 0; pageRowIndex < rowsOnPage; ++pageRowIndex) {
            const auto originalRowIndex = rowIndex + pageRowIndex;
            if (!rows[originalRowIndex].isObject()) {
                result.pages.clear();
                result.commands.clear();
                result.errorMessage =
                    QStringLiteral("表格数据 %1 的第 %2 行必须是对象").arg(dataPath).arg(originalRowIndex + 1);
                return result;
            }

            appendDetailRowCommands(
                page.commands,
                table,
                tableId,
                rows[originalRowIndex].toObject(),
                originalRowIndex,
                pageRowIndex,
                originX,
                originY,
                drawHeader);
        }

        page.rowCount = rowsOnPage;
        result.pages.append(page);
        rowIndex += rowsOnPage;
        ++pageNumber;
    }

    if (!result.pages.isEmpty()) {
        result.commands = result.pages.first().commands;
    }
    return result;
}

} // 命名空间 sleekpr::core
