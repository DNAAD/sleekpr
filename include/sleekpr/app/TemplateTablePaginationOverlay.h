#pragma once

#include <QRectF>
#include <QString>

namespace sleekpr::app {

enum class TablePaginationOverlayKind
{
    PageBreak,
    RepeatedHeader,
    Warning,
};

struct TablePaginationOverlay
{
    QString tableId;
    TablePaginationOverlayKind kind = TablePaginationOverlayKind::PageBreak;
    QRectF rectMm;
    QString text;
    int pageNumber = 1;
    bool severe = false;
};

} // namespace sleekpr::app
