#pragma once

#include "sleekpr/core/native/NativeDrawCommand.h"

#include <QList>
#include <QPoint>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <optional>

namespace sleekpr::app {

class TemplateElementHitTester
{
public:
    // 根据预览图像素坐标反查模板元素 key，供设置窗口实现画布点击选中。
    std::optional<QString> hitTest(
        const QList<sleekpr::core::NativeDrawCommand>& commands,
        QSizeF paperSizeMm,
        QSize previewSizePx,
        QPoint positionPx) const;
};

} // 命名空间 sleekpr::app
