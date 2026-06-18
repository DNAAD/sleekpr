#include "sleekpr/app/TemplateElementHitTester.h"

#include <QRect>

#include <limits>

namespace sleekpr::app {

namespace {

QString editableElementKey(const QString& commandKey)
{
    const auto cellMarkerIndex = commandKey.indexOf(QStringLiteral(".cell"));
    if (cellMarkerIndex <= 0) {
        return commandKey;
    }

    const auto cellNumberStart = cellMarkerIndex + QStringLiteral(".cell").size();
    auto cursor = cellNumberStart;
    while (cursor < commandKey.size() && commandKey.at(cursor).isDigit()) {
        ++cursor;
    }

    const auto suffix = commandKey.mid(cursor);
    // 数组网格会把一个元素拆成多个单元格绘制命令，交互命中时必须回到元素本身才能拖动和编辑。
    if (cursor > cellNumberStart && (suffix.isEmpty() || suffix == QStringLiteral(".border"))) {
        return commandKey.left(cellMarkerIndex);
    }
    return commandKey;
}

} // namespace

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
            bestKey = editableElementKey(command.elementKey);
        }
    }

    if (bestKey.isEmpty()) {
        return std::nullopt;
    }
    return bestKey;
}

} // 命名空间 sleekpr::app
