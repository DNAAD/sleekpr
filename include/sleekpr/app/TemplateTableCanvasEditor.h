#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <optional>

namespace sleekpr::app {

enum class TableCanvasBand
{
    Header,
    Detail,
};

struct TableCanvasHit
{
    QString tableId;
    int columnIndex = -1;
    QString columnId;
    TableCanvasBand band = TableCanvasBand::Detail;
    QString rowBandId;
    int rowOffset = 0;
    QRectF cellRectMm;
};

struct TableCanvasSelection
{
    QString tableId;
    int startColumnIndex = -1;
    int endColumnIndex = -1;
    QString rowBandId;
    int startRowOffset = 0;
    int endRowOffset = 0;
    QRectF rectMm;

    bool isValid() const;
    bool isSingleCell() const;
};

class TemplateTableCanvasEditor final
{
public:
    static std::optional<TableCanvasHit> hitTest(
        const sleekpr::core::TemplateDocument& document,
        QSizeF paperSizeMm,
        QSize previewImageSizePx,
        QPoint imagePositionPx);

    static bool setColumnTitle(sleekpr::core::TableElement* table, int columnIndex, const QString& title);
    static bool setColumnFieldKey(sleekpr::core::TableElement* table, int columnIndex, const QString& fieldKey);
    static bool insertColumnAfter(sleekpr::core::TableElement* table, int columnIndex);
    static bool duplicateColumn(sleekpr::core::TableElement* table, int columnIndex);
    static bool deleteColumn(sleekpr::core::TableElement* table, int columnIndex);
    static bool moveColumnLeft(sleekpr::core::TableElement* table, int columnIndex);
    static bool moveColumnRight(sleekpr::core::TableElement* table, int columnIndex);
    static std::optional<TableCanvasSelection> selectionFromHits(
        const sleekpr::core::TableElement& table,
        const TableCanvasHit& anchor,
        const TableCanvasHit& target);
    static bool selectionContainsHit(const TableCanvasSelection& selection, const TableCanvasHit& hit);
    static bool mergeSelection(sleekpr::core::TableElement* table, const TableCanvasSelection& selection);
    static bool splitSelection(sleekpr::core::TableElement* table, const TableCanvasSelection& selection);
    static bool mergeCellRight(sleekpr::core::TableElement* table, const TableCanvasHit& hit);
    static bool splitCell(sleekpr::core::TableElement* table, const TableCanvasHit& hit);
    static bool toggleCellBold(sleekpr::core::TableElement* table, const TableCanvasHit& hit);
    static bool toggleCellWrap(sleekpr::core::TableElement* table, const TableCanvasHit& hit);
    static bool setCellAlignment(
        sleekpr::core::TableElement* table,
        const TableCanvasHit& hit,
        sleekpr::core::TableCellAlignment alignment);
};

} // namespace sleekpr::app
