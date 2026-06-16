#include "sleekpr/app/TemplateDragCoordinateMapper.h"

#include <algorithm>

namespace sleekpr::app {

TemplateDragCoordinateMapper::TemplateDragCoordinateMapper(QSizeF paperSizeMm, QSize previewSizePx)
    : m_paperSizeMm(paperSizeMm)
    , m_previewSizePx(previewSizePx)
{
}

QPointF TemplateDragCoordinateMapper::movedTopLeftMm(QPointF currentTopLeftMm, QPoint pixelDelta, QSizeF elementSizeMm) const
{
    // 拖拽事件来自 Qt 像素坐标，设置和领域规划始终写入毫米坐标。
    const double nextX = currentTopLeftMm.x() + pixelDelta.x() * millimetersPerPixelX();
    const double nextY = currentTopLeftMm.y() + pixelDelta.y() * millimetersPerPixelY();

    // 元素锚点限制在纸张内，避免误拖后保存出明显不可见的模板位置。
    const double maxX = std::max(0.0, m_paperSizeMm.width() - elementSizeMm.width());
    const double maxY = std::max(0.0, m_paperSizeMm.height() - elementSizeMm.height());
    return QPointF(clamp(nextX, 0.0, maxX), clamp(nextY, 0.0, maxY));
}

double TemplateDragCoordinateMapper::millimetersPerPixelX() const
{
    return m_previewSizePx.width() > 0 ? m_paperSizeMm.width() / m_previewSizePx.width() : 0.0;
}

double TemplateDragCoordinateMapper::millimetersPerPixelY() const
{
    return m_previewSizePx.height() > 0 ? m_paperSizeMm.height() / m_previewSizePx.height() : 0.0;
}

double TemplateDragCoordinateMapper::clamp(double value, double minimum, double maximum)
{
    return std::clamp(value, minimum, maximum);
}

} // 命名空间 sleekpr::app
