#pragma once

#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSizeF>

namespace sleekpr::app {

class TemplateDragCoordinateMapper
{
public:
    TemplateDragCoordinateMapper(QSizeF paperSizeMm, QSize previewSizePx);

    QPointF movedTopLeftMm(QPointF currentTopLeftMm, QPoint pixelDelta, QSizeF elementSizeMm) const;

private:
    double millimetersPerPixelX() const;
    double millimetersPerPixelY() const;
    static double clamp(double value, double minimum, double maximum);

    QSizeF m_paperSizeMm;
    QSize m_previewSizePx;
};

} // 命名空间 sleekpr::app
