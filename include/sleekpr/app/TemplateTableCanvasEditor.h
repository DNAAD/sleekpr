#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QPoint>
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
    TableCanvasBand band = TableCanvasBand::Detail;
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
};

} // namespace sleekpr::app
