#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"

#include "sleekpr/core/native/SilverNativeLabelDrawingPlanner.h"

#include <QStringList>

#include <algorithm>

namespace sleekpr::core {

namespace {

QString weightLabelPart(const QString& value)
{
    const auto splitIndex = value.lastIndexOf(' ');
    if (splitIndex < 0 || splitIndex == value.size() - 1) {
        return value;
    }
    return value.left(splitIndex + 1);
}

QString weightValuePart(const QString& value)
{
    const auto splitIndex = value.lastIndexOf(' ');
    if (splitIndex < 0 || splitIndex == value.size() - 1) {
        return value;
    }
    return value.mid(splitIndex + 1);
}

QString partRowText(const LabelRenderPlan& labelPlan, const QString& fieldKey)
{
    const QString prefix = QStringLiteral("partRow:");
    if (!fieldKey.startsWith(prefix)) {
        return QString();
    }

    bool ok = false;
    const auto index = fieldKey.mid(prefix.size()).toInt(&ok);
    if (!ok || index < 0 || index >= labelPlan.parts.size()) {
        return QString();
    }

    // 明细行 fieldKey 保存行号，渲染时再从当前业务计划取值，避免模板启动时固化演示数据。
    const auto& part = labelPlan.parts[index];
    return QStringLiteral("%1:%2g").arg(part.categoryName, part.partWeightText);
}

} // 匿名命名空间

NativeLabelDrawingPlan NativeLabelDrawingPlanner::createPlan(
    const LabelRenderPlan& labelPlan,
    const LabelOffset& offset,
    const QHash<QString, TemplateElementOverride>& overrides,
    const QList<TemplateElement>& templateElements) const
{
    if (labelPlan.templateKey == LabelTemplateKey::Silver80x30) {
        return SilverNativeLabelDrawingPlanner().createPlan(labelPlan, offset, overrides, templateElements);
    }

    const auto offsetX = offset.x;
    const auto offsetY = offset.y;

    QList<NativeDrawCommand> commands;

    // 坐标刻意对齐 .NET GDI 规划器；领域层保持毫米单位，方便 Qt 预览和打印后端共用同一份计划。
    commands.append(text(0.0 + offsetX, 0.0 + offsetY, 9.2, 1.5, labelPlan.identifierText, 4.5, false, "identifier"));
    // Qt 与 GDI 对窄区域中文自动换行不完全一致，显式拆成三行以保持“合格证”竖排样式稳定。
    commands.append(text(0.0 + offsetX, 2.5 + offsetY, 2.0, 8.5, QString::fromUtf8("合\n格\n证"), 4.6, false, "qualityMark"));
    // Default80x30 预览沿用旧 HTML 标签 8mm 二维码尺寸，避免压住下方“标签约0.20g”说明。
    commands.append(qrCode(1.8 + offsetX, 1.0 + offsetY, 7.8, labelPlan.qrPayload, "qrCode"));
    commands.append(text(1.8 + offsetX, 9.0 + offsetY, 9.0, 1.2, QString::fromUtf8("标签约%1g").arg(labelPlan.tagWeightText), 3.6, false, "qrNote"));
    commands.append(text(10.2 + offsetX, 0.0 + offsetY, 16.3, 5.4, labelPlan.productName, 4.2, false, "productName", 0.0, 3, true));
    commands.append(text(0.0 + offsetX, 10.8 + offsetY, 26.0, 1.8, labelPlan.standardText, 3.4, false, "standardText"));
    commands.append(text(0.0 + offsetX, 12.05 + offsetY, 26.0, 1.8, labelPlan.addressText, 4.0, false, "addressText"));
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

    const auto overriddenCommands = applyOverrides(commands, overrides, offsetX, offsetY);
    return NativeLabelDrawingPlan{
        labelPlan.paperSize,
        appendTemplateElements(overriddenCommands, labelPlan, templateElements, offsetX, offsetY),
        300.0,
    };
}

NativeDrawCommand NativeLabelDrawingPlanner::text(
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

NativeDrawCommand NativeLabelDrawingPlanner::qrCode(double x, double y, double size, const QString& payload, const QString& elementKey)
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

NativeDrawCommand NativeLabelDrawingPlanner::rectangle(double x, double y, double width, double height, const QString& elementKey)
{
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::Rectangle;
    command.x = x;
    command.y = y;
    command.width = width;
    command.height = height;
    command.elementKey = elementKey;
    return command;
}

void NativeLabelDrawingPlanner::addWeightCommands(QList<NativeDrawCommand>& commands, const QString& value, double x, double y, const QString& keyPrefix)
{
    const auto splitIndex = value.lastIndexOf(' ');
    if (splitIndex < 0 || splitIndex == value.size() - 1) {
        // 无法拆分标签和值时按一段文本绘制，避免异常数据中断打印。
        commands.append(text(x, y, 15.2, 1.8, value, 4.5, false, keyPrefix + "Value"));
        return;
    }

    const auto label = value.left(splitIndex + 1);
    const auto number = value.mid(splitIndex + 1);
    // 重量数字单独加粗，匹配现有小标签的可读性设计。
    commands.append(text(x, y, 7.8, 1.8, label, 4.0, false, keyPrefix + "Label"));
    commands.append(text(x + 7.9, y - 0.08, 7.3, 1.9, number, 4.5, true, keyPrefix + "Value"));
}

void NativeLabelDrawingPlanner::addPartCommands(QList<NativeDrawCommand>& commands, const LabelRenderPlan& labelPlan, double offsetX, double offsetY)
{
    for (int index = 0; index < labelPlan.parts.size(); ++index) {
        const auto& part = labelPlan.parts[index];
        // 明细区域按两列排布，行高与 .NET 版本保持一致。
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

double NativeLabelDrawingPlanner::calculateFooterY(const LabelRenderPlan& labelPlan)
{
    if (labelPlan.parts.isEmpty()) {
        // 没有明细时底部备注固定落在下方留白区域。
        return 23.1;
    }
    // 有明细时底部备注跟随最后一行明细下移，避免文本重叠。
    const auto rows = (labelPlan.parts.size() + 1) / 2;
    return 16.4 + rows * 2.05 + 0.65;
}

QList<NativeDrawCommand> NativeLabelDrawingPlanner::applyOverrides(
    const QList<NativeDrawCommand>& commands,
    const QHash<QString, TemplateElementOverride>& overrides,
    double offsetX,
    double offsetY)
{
    if (overrides.isEmpty()) {
        return commands;
    }

    QHash<QString, NativeDrawCommand> firstCommandByKey;
    for (const auto& command : commands) {
        if (!command.elementKey.isEmpty() && !firstCommandByKey.contains(command.elementKey)) {
            firstCommandByKey.insert(command.elementKey, command);
        }
    }

    QList<NativeDrawCommand> result;
    result.reserve(commands.size());
    for (auto command : commands) {
        if (!command.elementKey.isEmpty() && overrides.contains(command.elementKey)) {
            const auto overrideValue = overrides[command.elementKey];
            const auto firstCommand = firstCommandByKey.value(command.elementKey, command);
            // 重复 key 的元素按第一项作为锚点，其余项保留原始相对间距，避免明细行被压到同一个坐标。
            if (overrideValue.x.has_value()) {
                command.x = overrideValue.x.value() + offsetX + (command.x - firstCommand.x);
            }
            if (overrideValue.y.has_value()) {
                command.y = overrideValue.y.value() + offsetY + (command.y - firstCommand.y);
            }
            if (overrideValue.fontSizePt.has_value()) {
                command.fontSizePt = overrideValue.fontSizePt.value();
            }
            if (overrideValue.bold.has_value()) {
                command.bold = overrideValue.bold.value();
            }
        }
        result.append(command);
    }
    return result;
}

QList<NativeDrawCommand> NativeLabelDrawingPlanner::appendTemplateElements(
    const QList<NativeDrawCommand>& commands,
    const LabelRenderPlan& labelPlan,
    const QList<TemplateElement>& templateElements,
    double offsetX,
    double offsetY)
{
    if (templateElements.isEmpty()) {
        return commands;
    }

    QList<TemplateElement> sortedElements = templateElements;
    std::stable_sort(sortedElements.begin(), sortedElements.end(), [](const TemplateElement& left, const TemplateElement& right) {
        return left.zIndex < right.zIndex;
    });

    QList<NativeDrawCommand> result = commands;
    result.reserve(commands.size() + sortedElements.size());

    const auto interpolateTemplateText = [&labelPlan](const QString& text) {
        QString resultText;
        resultText.reserve(text.size());

        int cursor = 0;
        while (cursor < text.size()) {
            const auto placeholderStart = text.indexOf(QStringLiteral("${"), cursor);
            if (placeholderStart < 0) {
                resultText.append(text.mid(cursor));
                break;
            }

            resultText.append(text.mid(cursor, placeholderStart - cursor));
            const auto placeholderEnd = text.indexOf(QLatin1Char('}'), placeholderStart + 2);
            if (placeholderEnd < 0) {
                resultText.append(text.mid(placeholderStart));
                break;
            }

            const auto fieldKey = text.mid(placeholderStart + 2, placeholderEnd - placeholderStart - 2).trimmed();
            // 旧自定义元素路径没有请求上下文，只能从 LabelRenderPlan 中替换已有字段。
            resultText.append(NativeLabelDrawingPlanner::valueForField(labelPlan, fieldKey));
            cursor = placeholderEnd + 1;
        }

        return resultText;
    };

    const auto verticalTextValue = [](const QString& text) {
        QStringList lines;
        lines.reserve(text.size());
        for (const auto& ch : text) {
            if (ch == QLatin1Char('\r')) {
                continue;
            }
            if (ch == QLatin1Char('\n')) {
                lines.append(QString());
                continue;
            }
            lines.append(QString(ch));
        }
        return lines.join(QLatin1Char('\n'));
    };

    for (const auto& element : sortedElements) {
        if (!element.visible) {
            continue;
        }

        const auto elementKey = element.id.trimmed();
        if (elementKey.isEmpty()) {
            continue;
        }

        const double x = element.x + offsetX;
        const double y = element.y + offsetY;
        switch (element.type) {
        case TemplateElementType::FixedText: {
            const auto value = interpolateTemplateText(element.text);
            auto command = text(
                x,
                y,
                element.width,
                element.height,
                element.verticalText ? verticalTextValue(value) : value,
                element.fontSizePt,
                element.bold,
                elementKey,
                element.rotationDegrees,
                element.maxLines,
                element.ellipsis);
            command.autoFitFont = element.autoFitFont;
            command.autoFitMinFontSizePt = element.autoFitMinFontSizePt;
            command.autoFitMaxFontSizePt = element.autoFitMaxFontSizePt;
            result.append(command);
            break;
        }
        case TemplateElementType::BoundField: {
            // 绑定字段只读取渲染计划中的展示文本，避免自定义模板元素反向依赖原始业务对象。
            auto command = text(
                x,
                y,
                element.width,
                element.height,
                valueForField(labelPlan, element.fieldKey),
                element.fontSizePt,
                element.bold,
                elementKey,
                element.rotationDegrees,
                element.maxLines,
                element.ellipsis);
            command.autoFitFont = element.autoFitFont;
            command.autoFitMinFontSizePt = element.autoFitMinFontSizePt;
            command.autoFitMaxFontSizePt = element.autoFitMaxFontSizePt;
            result.append(command);
            break;
        }
        case TemplateElementType::QrCode: {
            const auto payload = element.payload.trimmed().isEmpty()
                ? valueForField(labelPlan, element.fieldKey)
                : interpolateTemplateText(element.payload);
            NativeDrawCommand command;
            command.type = NativeDrawCommandType::QrCode;
            command.x = x;
            command.y = y;
            command.width = element.width;
            command.height = element.height;
            command.text = payload;
            command.rotationDegrees = element.rotationDegrees;
            command.elementKey = elementKey;
            result.append(command);
            break;
        }
        case TemplateElementType::Rectangle: {
            NativeDrawCommand command;
            command.type = NativeDrawCommandType::Rectangle;
            command.x = x;
            command.y = y;
            command.width = element.width;
            command.height = element.height;
            command.rotationDegrees = element.rotationDegrees;
            command.elementKey = elementKey;
            result.append(command);
            break;
        }
        }
    }

    return result;
}

QString NativeLabelDrawingPlanner::valueForField(const LabelRenderPlan& labelPlan, const QString& fieldKey)
{
    const auto partText = partRowText(labelPlan, fieldKey);
    if (!partText.isEmpty()) {
        return partText;
    }
    if (fieldKey == QStringLiteral("identifierText")) {
        return labelPlan.identifierText;
    }
    if (fieldKey == QStringLiteral("productName")) {
        return labelPlan.productName;
    }
    if (fieldKey == QStringLiteral("finishedWeightLabel")) {
        return weightLabelPart(labelPlan.finishedWeightText);
    }
    if (fieldKey == QStringLiteral("finishedWeightValue")) {
        return weightValuePart(labelPlan.finishedWeightText);
    }
    if (fieldKey == QStringLiteral("finishedWeightText")) {
        return labelPlan.finishedWeightText;
    }
    if (fieldKey == QStringLiteral("roughWeightLabel")) {
        return weightLabelPart(labelPlan.roughWeightText);
    }
    if (fieldKey == QStringLiteral("roughWeightValue")) {
        return weightValuePart(labelPlan.roughWeightText);
    }
    if (fieldKey == QStringLiteral("roughWeightText")) {
        return labelPlan.roughWeightText;
    }
    if (fieldKey == QStringLiteral("salesCode")) {
        return labelPlan.salesCode;
    }
    if (fieldKey == QStringLiteral("standardText")) {
        return labelPlan.standardText;
    }
    if (fieldKey == QStringLiteral("addressText")) {
        return labelPlan.addressText;
    }
    if (fieldKey == QStringLiteral("qrPayload")) {
        return labelPlan.qrPayload;
    }
    if (fieldKey == QStringLiteral("goldPurityText")) {
        return labelPlan.goldPurityText;
    }
    if (fieldKey == QStringLiteral("priceText")) {
        return labelPlan.priceText;
    }
    if (fieldKey == QStringLiteral("additionalPriceText")) {
        return labelPlan.additionalPriceText;
    }
    if (fieldKey == QStringLiteral("tagWeightText")) {
        return labelPlan.tagWeightText;
    }
    if (fieldKey == QStringLiteral("qrNote")) {
        return QString::fromUtf8("标签约%1g").arg(labelPlan.tagWeightText);
    }
    if (fieldKey == QStringLiteral("footerText")) {
        return labelPlan.footerText;
    }
    return QString();
}

} // 命名空间 sleekpr::core
