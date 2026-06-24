#include "sleekpr/app/TemplateElementHitTester.h"

#include <QRect>

#include <limits>

namespace sleekpr::app {

namespace {

QString editableElementKey(const QString& commandKey)
{
    auto normalizedKey = commandKey.trimmed();
    if (normalizedKey.endsWith(QStringLiteral(".border"))) {
        normalizedKey.chop(QStringLiteral(".border").size());
    }

    // 底层绘制会把数组网格和表格拆成多个单元格命令，命中后必须回到模板元素本身。
    const auto cellMarkerIndex = normalizedKey.indexOf(QStringLiteral(".cell"));
    if (cellMarkerIndex > 0) {
        const auto cellNumberStart = cellMarkerIndex + QStringLiteral(".cell").size();
        auto cursor = cellNumberStart;
        while (cursor < normalizedKey.size() && normalizedKey.at(cursor).isDigit()) {
            ++cursor;
        }
        if (cursor > cellNumberStart && cursor == normalizedKey.size()) {
            return normalizedKey.left(cellMarkerIndex);
        }
    }

    const auto headerMarkerIndex = normalizedKey.indexOf(QStringLiteral(".header."));
    if (headerMarkerIndex > 0) {
        return normalizedKey.left(headerMarkerIndex);
    }

    const auto rowMarkerIndex = normalizedKey.indexOf(QStringLiteral(".row"));
    if (rowMarkerIndex > 0) {
        auto cursor = rowMarkerIndex + QStringLiteral(".row").size();
        while (cursor < normalizedKey.size() && normalizedKey.at(cursor).isDigit()) {
            ++cursor;
        }
        if (cursor > rowMarkerIndex + QStringLiteral(".row").size()
            && (cursor == normalizedKey.size() || normalizedKey.at(cursor) == QChar('.'))) {
            return normalizedKey.left(rowMarkerIndex);
        }
    }

    return normalizedKey;
}

} // 匿名命名空间

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
