#include "sleekpr/app/TemplateElementHitTester.h"

#include <QRect>

#include <limits>

namespace sleekpr::app {

std::optional<QString> TemplateElementHitTester::hitTest(
    const QList<sleekpr::core::NativeDrawCommand>& commands,
    QSizeF paperSizeMm,
    QSize previewSizePx,
    QPoint positionPx) const
{
    if (paperSizeMm.isEmpty() || previewSizePx.isEmpty()) {
        return std::nullopt;
    }

    const double scaleX = previewSizePx.width() / paperSizeMm.width();
    const double scaleY = previewSizePx.height() / paperSizeMm.height();
    QString bestKey;
    double bestArea = std::numeric_limits<double>::max();
    int bestIndex = -1;

    for (int index = 0; index < commands.size(); ++index) {
        const auto& command = commands[index];
        if (command.elementKey.isEmpty() || command.width <= 0.0 || command.height <= 0.0) {
            continue;
        }

        const QRect rect(
            static_cast<int>(command.x * scaleX),
            static_cast<int>(command.y * scaleY),
            static_cast<int>(command.width * scaleX),
            static_cast<int>(command.height * scaleY));
        if (!rect.adjusted(-4, -4, 4, 4).contains(positionPx)) {
            continue;
        }

        const double area = command.width * command.height;
        // 多元素重叠时优先选择更小的编辑目标；面积相同则选择后绘制的元素，贴近用户看到的层级。
        if (area < bestArea || (area == bestArea && index > bestIndex)) {
            bestArea = area;
            bestIndex = index;
            bestKey = command.elementKey;
        }
    }

    if (bestKey.isEmpty()) {
        return std::nullopt;
    }
    return bestKey;
}

} // 命名空间 sleekpr::app
