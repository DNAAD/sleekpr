#include "sleekpr/core/native/SilverNativeLabelDrawingPlanner.h"

#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"

#include <algorithm>

namespace sleekpr::core {

NativeLabelDrawingPlan SilverNativeLabelDrawingPlanner::createPlan(
    const LabelRenderPlan& labelPlan,
    const LabelOffset& offset,
    const QHash<QString, TemplateElementOverride>& overrides,
    const QList<TemplateElement>& templateElements) const
{
    const auto offsetX = offset.x;
    const auto offsetY = offset.y;

    QList<NativeDrawCommand> commands;
    commands.append(text(0.0 + offsetX, 0.0 + offsetY, 9.2, 1.5, labelPlan.identifierText, 4.5, false, "identifier"));
    // 银标同样保持“合格证”竖排，避免 Qt 与 GDI 在窄中文区域自动换行上产生差异。
    commands.append(text(0.0 + offsetX, 2.8 + offsetY, 1.5, 8.5, QString::fromUtf8("合\n格\n证"), 4.6, false, "qualityMark"));
    commands.append(qrCode(1.8 + offsetX, 1.5 + offsetY, 8.8, labelPlan.qrPayload, "qrCode"));
    commands.append(text(10.2 + offsetX, 0.0 + offsetY, 15.2, 5.4, labelPlan.productName, 4.2, false, "productName", 0.0, 3, true));
    commands.append(text(
        0.0 + offsetX,
        10.8 + offsetY,
        26.0,
        1.8,
        QString::fromUtf8("%1  标签约%2g").arg(labelPlan.standardText, labelPlan.tagWeightText),
        3.4,
        false,
        "standardText"));
    commands.append(text(16.0 + offsetX, 12.05 + offsetY, 10.0, 1.8, labelPlan.priceText, 4.5, true, "priceText"));
    commands.append(text(0.0 + offsetX, 14.6 + offsetY, 9.0, 1.8, labelPlan.salesCode, 4.2, true, "salesCode"));

    addWeightCommands(commands, labelPlan.finishedWeightText, 10.2 + offsetX, 6.85 + offsetY, "finishedWeight");
    addWeightCommands(commands, labelPlan.roughWeightText, 10.2 + offsetX, 8.8 + offsetY, "roughWeight");
    if (!labelPlan.additionalPriceText.trimmed().isEmpty()) {
        commands.append(text(12.0 + offsetX, 14.6 + offsetY, 12.0, 1.8, labelPlan.additionalPriceText, 4.2, false, "additionalPrice"));
    }

    addPartCommands(commands, labelPlan, offsetX, offsetY);

    if (!labelPlan.footerText.trimmed().isEmpty()) {
        const auto footerY = calculateFooterY(labelPlan);
        const auto footerHeight = std::max(1.4, labelPlan.paperSize.heightMm() - footerY - 0.4);
        commands.append(text(0.0 + offsetX, footerY + offsetY, 22.8, footerHeight, labelPlan.footerText, 3.8, false, "footerText"));
    }

    commands.append(text(22.5 + offsetX, 15.6 + offsetY, 2.2, 10.0, labelPlan.identifierText, 4.2, true, "verticalIdentifier", 90.0));

    const auto overriddenCommands = NativeLabelDrawingPlanner::applyOverrides(commands, overrides, offsetX, offsetY);
    return NativeLabelDrawingPlan{
        labelPlan.paperSize,
        NativeLabelDrawingPlanner::appendTemplateElements(overriddenCommands, labelPlan, templateElements, offsetX, offsetY),
        300.0,
    };
}

NativeDrawCommand SilverNativeLabelDrawingPlanner::text(
    double x,
    double y,
    double width,
    double height,
    const QString& value,
    double fontSizePt,
    bool bold,
    const QString& elementKey,
    double rotationDegrees,
    int maxLines,
    bool ellipsis)
{
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::Text;
    command.x = x;
    command.y = y;
    command.width = width;
    command.height = height;
    command.text = value;
    command.fontSizePt = fontSizePt;
    command.bold = bold;
    command.rotationDegrees = rotationDegrees;
    command.maxLines = maxLines;
    command.ellipsis = ellipsis;
    command.elementKey = elementKey;
    return command;
}

NativeDrawCommand SilverNativeLabelDrawingPlanner::qrCode(double x, double y, double size, const QString& payload, const QString& elementKey)
{
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::QrCode;
    command.x = x;
    command.y = y;
    command.width = size;
    command.height = size;
    command.text = payload;
    command.elementKey = elementKey;
    return command;
}

void SilverNativeLabelDrawingPlanner::addWeightCommands(QList<NativeDrawCommand>& commands, const QString& value, double x, double y, const QString& keyPrefix)
{
    const auto splitIndex = value.lastIndexOf(' ');
    if (splitIndex < 0 || splitIndex == value.size() - 1) {
        commands.append(text(x, y, 15.2, 1.8, value, 5.2, false, keyPrefix + "Value"));
        return;
    }

    const auto label = value.left(splitIndex + 1);
    const auto number = value.mid(splitIndex + 1);
    commands.append(text(x, y + 0.18, 7.8, 1.8, label, 3.9, false, keyPrefix + "Label"));
    commands.append(text(x + 7.9, y - 0.08, 7.3, 1.9, number, 5.2, true, keyPrefix + "Value"));
}

void SilverNativeLabelDrawingPlanner::addPartCommands(QList<NativeDrawCommand>& commands, const LabelRenderPlan& labelPlan, double offsetX, double offsetY)
{
    for (int index = 0; index < labelPlan.parts.size(); ++index) {
        const auto& part = labelPlan.parts[index];
        const auto column = index % 2;
        const auto row = index / 2;
        const auto x = column == 0 ? 0.0 : 11.5;
        const auto y = 16.4 + row * 2.05;
        commands.append(text(
            x + offsetX,
            y + offsetY,
            11.0,
            1.8,
            QString("%1:%2g").arg(part.categoryName, part.partWeightText),
            4.0,
            false,
            "partRow"));
    }
}

double SilverNativeLabelDrawingPlanner::calculateFooterY(const LabelRenderPlan& labelPlan)
{
    if (labelPlan.parts.isEmpty()) {
        return 23.1;
    }
    const auto rows = (labelPlan.parts.size() + 1) / 2;
    return 16.4 + rows * 2.05 + 0.65;
}

} // 命名空间 sleekpr::core
