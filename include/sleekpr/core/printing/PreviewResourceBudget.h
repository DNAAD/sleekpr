#pragma once

#include <QSize>
#include <QSizeF>
#include <QString>

namespace sleekpr::core {

enum class PreviewImageError { None, InvalidInput, ResourceLimit, AllocationFailed, PaintFailed, EncodeFailed };

struct PreviewResourceBudget
{
    int maxSidePixels = 16384;
    quint64 maxPixels = 64000000;
    quint64 maxImageBytes = 256000000;
};

struct PreviewImageRequest
{
    QSizeF paperSizeMm;
    double dpi = 300;
    double zoom = 1;
    double devicePixelRatio = 1;
};

struct PreviewImageCheck
{
    PreviewImageError error = PreviewImageError::None;
    QSize pixelSize;
    double rasterDpi = 0;
    QString message;
    int dotsPerMeter = 0;
    bool ok() const { return error == PreviewImageError::None; }
};

PreviewImageCheck checkPreviewImageRequest(const PreviewImageRequest& request, const PreviewResourceBudget& budget = {});

enum class PreviewPlanningLimit { None, Pages, Commands, Cells };

struct PreviewPlanningBudget
{
    qsizetype maxPages = 100;
    qsizetype maxCommands = 200000;
    qsizetype maxCells = 200000;
    qsizetype usedPages = 0;
    qsizetype usedCommands = 0;
    qsizetype usedCells = 0;
    PreviewPlanningLimit exceeded = PreviewPlanningLimit::None;
    QString message;

    bool consumePages(qsizetype count);
    bool ensurePages(qsizetype count);
    bool consumeCommands(qsizetype count);
    bool consumeCells(qsizetype count);
    bool ensureCells(qsizetype count);
    bool ok() const { return exceeded == PreviewPlanningLimit::None; }
};

} // 命名空间 sleekpr::core
