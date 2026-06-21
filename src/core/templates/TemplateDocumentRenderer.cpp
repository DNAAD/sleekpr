#include "sleekpr/core/templates/TemplateDocumentRenderer.h"

#include "sleekpr/core/templates/TableElementRenderer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>

namespace sleekpr::core {
namespace {

double usableScale(double value)
{
    return value > 0.0 ? value : 1.0;
}

double usableRenderDpi(double value)
{
    return value > 0.0 ? value : 300.0;
}

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

    const auto& part = labelPlan.parts[index];
    return QStringLiteral("%1:%2g").arg(part.categoryName, part.partWeightText);
}

QString legacyValueForField(const LabelRenderPlan& labelPlan, const QString& fieldKey)
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

QList<TemplateElement> visibleElementsInRenderOrder(const TemplateDocument& document)
{
    QList<TemplateElement> result;
    int renderZIndex = 0;

    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }

        QList<TemplateElement> layerElements;
        for (const auto& element : layer.elements) {
            if (element.visible) {
                layerElements.append(element);
            }
        }

        // 图层内按 zIndex 稳定排序；随后重写渲染序号，避免跨图层再次排序时打乱图层顺序。
        std::stable_sort(layerElements.begin(), layerElements.end(), [](const TemplateElement& left, const TemplateElement& right) {
            return left.zIndex < right.zIndex;
        });

        for (auto element : layerElements) {
            element.zIndex = renderZIndex++;
            result.append(element);
        }
    }

    return result;
}

QList<TableElement> visibleTablesInRenderOrder(const TemplateDocument& document)
{
    QList<TableElement> result;
    int renderZIndex = 0;

    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }

        QList<TableElement> layerTables;
        for (const auto& table : layer.tables) {
            if (table.visible) {
                layerTables.append(table);
            }
        }

        // 表格和普通元素分开追加，但表格内部仍保持图层顺序与 zIndex 稳定排序。
        std::stable_sort(layerTables.begin(), layerTables.end(), [](const TableElement& left, const TableElement& right) {
            return left.zIndex < right.zIndex;
        });

        for (auto table : layerTables) {
            table.zIndex = renderZIndex++;
            result.append(table);
        }
    }

    return result;
}

QString jsonValueToText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        const auto number = value.toDouble();
        const auto integer = static_cast<qint64>(number);
        if (static_cast<double>(integer) == number) {
            return QString::number(integer);
        }
        return QString::number(number, 'g', 15);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

QString valueForTemplateField(const LabelRenderPlan& labelPlan, const TemplateRenderContext& context, const QString& fieldKey)
{
    const auto key = fieldKey.trimmed();
    if (key.isEmpty()) {
        return QString();
    }
    if (context.values.contains(key)) {
        return jsonValueToText(context.values.value(key));
    }
    // 兼容旧设计器中保存的标签字段 key；自定义字段仍由上面的上下文分支处理。
    return legacyValueForField(labelPlan, key);
}

QString interpolateTemplateText(const LabelRenderPlan& labelPlan, const TemplateRenderContext& context, const QString& text)
{
    QString result;
    result.reserve(text.size());

    int cursor = 0;
    while (cursor < text.size()) {
        const auto placeholderStart = text.indexOf(QStringLiteral("${"), cursor);
        if (placeholderStart < 0) {
            result.append(text.mid(cursor));
            break;
        }

        result.append(text.mid(cursor, placeholderStart - cursor));
        const auto placeholderEnd = text.indexOf(QLatin1Char('}'), placeholderStart + 2);
        if (placeholderEnd < 0) {
            result.append(text.mid(placeholderStart));
            break;
        }

        const auto fieldKey = text.mid(placeholderStart + 2, placeholderEnd - placeholderStart - 2).trimmed();
        // 固定文本中的 ${字段名} 在渲染阶段替换；缺失字段按产品约定输出空字符串。
        result.append(valueForTemplateField(labelPlan, context, fieldKey));
        cursor = placeholderEnd + 1;
    }

    return result;
}

QString verticalTextValue(const QString& text)
{
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
}

NativeDrawCommand textCommand(
    const TemplateElement& element,
    const QString& value,
    double offsetX,
    double offsetY)
{
    NativeDrawCommand command{
        NativeDrawCommandType::Text,
        element.x + offsetX,
        element.y + offsetY,
        element.width,
        element.height,
        value,
        element.fontSizePt,
        element.bold,
        element.rotationDegrees,
        element.maxLines,
        element.ellipsis,
        element.id.trimmed(),
    };
    // 完整模板的文本元素以设计器矩形为排版边界：宽度内自动换行，高度外交给后端裁切。
    command.wrapText = true;
    return command;
}

NativeDrawCommand qrCommand(
    const TemplateElement& element,
    const QString& payload,
    double offsetX,
    double offsetY)
{
    return NativeDrawCommand{
        NativeDrawCommandType::QrCode,
        element.x + offsetX,
        element.y + offsetY,
        element.width,
        element.height,
        payload,
        0.0,
        false,
        element.rotationDegrees,
        0,
        false,
        element.id.trimmed(),
    };
}

NativeDrawCommand rectangleCommand(const TemplateElement& element, double offsetX, double offsetY)
{
    return NativeDrawCommand{
        NativeDrawCommandType::Rectangle,
        element.x + offsetX,
        element.y + offsetY,
        element.width,
        element.height,
        QString(),
        0.0,
        false,
        element.rotationDegrees,
        0,
        false,
        element.id.trimmed(),
    };
}

QJsonValue valueAtPath(const QJsonObject& object, const QString& path)
{
    const auto normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return QJsonValue();
    }
    if (object.contains(normalizedPath)) {
        return object.value(normalizedPath);
    }

    // 数组网格允许 dataPath 或单元格模板字段使用 order.items、product.name 这样的嵌套路径。
    QJsonValue current = object;
    for (const auto& part : normalizedPath.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        if (!current.isObject()) {
            return QJsonValue();
        }
        const auto currentObject = current.toObject();
        if (!currentObject.contains(part)) {
            return QJsonValue();
        }
        current = currentObject.value(part);
    }
    return current;
}

QString interpolateArrayGridCellText(const TemplateElement& element, const QJsonValue& rowValue)
{
    const auto cellTemplate = element.arrayGridCellTemplate.trimmed().isEmpty()
        ? QStringLiteral("${value}")
        : element.arrayGridCellTemplate;
    const auto rowObject = rowValue.isObject() ? rowValue.toObject() : QJsonObject{{QStringLiteral("value"), rowValue}};

    QString result;
    result.reserve(cellTemplate.size());

    int cursor = 0;
    while (cursor < cellTemplate.size()) {
        const auto placeholderStart = cellTemplate.indexOf(QStringLiteral("${"), cursor);
        if (placeholderStart < 0) {
            result.append(cellTemplate.mid(cursor));
            break;
        }

        result.append(cellTemplate.mid(cursor, placeholderStart - cursor));
        const auto placeholderEnd = cellTemplate.indexOf(QLatin1Char('}'), placeholderStart + 2);
        if (placeholderEnd < 0) {
            result.append(cellTemplate.mid(placeholderStart));
            break;
        }

        const auto fieldKey = cellTemplate.mid(placeholderStart + 2, placeholderEnd - placeholderStart - 2).trimmed();
        result.append(jsonValueToText(valueAtPath(rowObject, fieldKey)));
        cursor = placeholderEnd + 1;
    }

    return result;
}

NativeDrawCommand arrayGridBorderCommand(
    const TemplateElement& element,
    int cellIndex,
    double x,
    double y,
    double width,
    double height)
{
    return NativeDrawCommand{
        NativeDrawCommandType::Rectangle,
        x,
        y,
        width,
        height,
        QString(),
        0.0,
        false,
        element.rotationDegrees,
        0,
        false,
        QStringLiteral("%1.cell%2.border").arg(element.id.trimmed()).arg(cellIndex),
    };
}

NativeDrawCommand arrayGridTextCommand(
    const TemplateElement& element,
    int cellIndex,
    double x,
    double y,
    double width,
    double height,
    const QString& text)
{
    constexpr double paddingX = 0.6;
    constexpr double paddingY = 0.4;
    NativeDrawCommand command{
        NativeDrawCommandType::Text,
        x + paddingX,
        y + paddingY,
        std::max(0.1, width - paddingX * 2.0),
        std::max(0.1, height - paddingY * 2.0),
        text,
        element.fontSizePt,
        element.bold,
        element.rotationDegrees,
        element.maxLines,
        element.ellipsis,
        QStringLiteral("%1.cell%2").arg(element.id.trimmed()).arg(cellIndex),
    };
    // 数组网格单元格同样按单元格宽度换行，避免动态数据被横向截断。
    command.wrapText = true;
    return command;
}

QList<NativeDrawCommand> arrayGridCommands(
    const TemplateElement& element,
    const TemplateRenderContext& context,
    double offsetX,
    double offsetY)
{
    QList<NativeDrawCommand> commands;
    const auto rows = std::max(1, element.arrayGridRows);
    const auto columns = std::max(1, element.arrayGridColumns);
    const auto totalCells = rows * columns;
    commands.reserve(totalCells * 2);

    const auto arrayValue = valueAtPath(context.values, element.dataPath);
    const auto items = arrayValue.isArray() ? arrayValue.toArray() : QJsonArray{};
    const auto cellWidth = element.width / columns;
    const auto defaultCellHeight = element.height / rows;
    const auto rowStep = element.arrayGridRowHeightMm > 0.0
        ? element.arrayGridRowHeightMm
        : defaultCellHeight;

    for (int cellIndex = 0; cellIndex < totalCells; ++cellIndex) {
        const auto rowIndex = cellIndex / columns;
        const auto columnIndex = cellIndex % columns;
        const auto cellX = element.x + offsetX + columnIndex * cellWidth;
        // 行高控制相邻两行起点之间的距离，便于压缩或拉开数组明细行。
        const auto cellY = element.y + offsetY + rowIndex * rowStep;

        if (element.arrayGridDrawBorders) {
            commands.append(arrayGridBorderCommand(element, cellIndex, cellX, cellY, cellWidth, defaultCellHeight));
        }
        if (cellIndex >= items.size()) {
            continue;
        }

        const auto text = interpolateArrayGridCellText(element, items.at(cellIndex));
        if (!text.trimmed().isEmpty()) {
            commands.append(arrayGridTextCommand(element, cellIndex, cellX, cellY, cellWidth, defaultCellHeight, text));
        }
    }

    return commands;
}

QList<NativeDrawCommand> renderTemplateElements(
    const LabelRenderPlan& labelPlan,
    const QList<TemplateElement>& elements,
    const TemplateRenderContext& context,
    double offsetX,
    double offsetY)
{
    QList<NativeDrawCommand> result;
    result.reserve(elements.size());

    for (const auto& element : elements) {
        const auto elementKey = element.id.trimmed();
        if (elementKey.isEmpty() || !element.visible) {
            continue;
        }

        switch (element.type) {
        case TemplateElementType::FixedText: {
            const auto text = interpolateTemplateText(labelPlan, context, element.text);
            // 竖排固定文本只改变绘制命令中的文本换行，不改变元素坐标和旋转语义。
            result.append(textCommand(element, element.verticalText ? verticalTextValue(text) : text, offsetX, offsetY));
            break;
        }
        case TemplateElementType::BoundField:
            // 完整模板优先使用本次打印上下文，实现任意自定义字段；没有上下文值时回退旧标签字段。
            result.append(textCommand(element, valueForTemplateField(labelPlan, context, element.fieldKey), offsetX, offsetY));
            break;
        case TemplateElementType::QrCode: {
            const auto payload = element.payload.trimmed().isEmpty()
                ? valueForTemplateField(labelPlan, context, element.fieldKey)
                : interpolateTemplateText(labelPlan, context, element.payload);
            result.append(qrCommand(element, payload, offsetX, offsetY));
            break;
        }
        case TemplateElementType::Rectangle:
            result.append(rectangleCommand(element, offsetX, offsetY));
            break;
        case TemplateElementType::ArrayGrid:
            result.append(arrayGridCommands(element, context, offsetX, offsetY));
            break;
        }
    }

    return result;
}

void applyDeviceScaleToCommands(QList<NativeDrawCommand>& commands, const DeviceProfile& profile)
{
    const auto scaleX = usableScale(profile.scaleX);
    const auto scaleY = usableScale(profile.scaleY);

    for (auto& command : commands) {
        // profile 缩放只处理毫米坐标和区域尺寸，不触碰文本、二维码载荷或字体语义。
        command.x *= scaleX;
        command.y *= scaleY;
        command.width *= scaleX;
        command.height *= scaleY;
    }
}

void applyDeviceScale(NativeLabelDrawingPlan& drawingPlan, const DeviceProfile& profile)
{
    applyDeviceScaleToCommands(drawingPlan.commands, profile);
}

void applyDeviceScale(NativePrintDrawingPlan& printPlan, const DeviceProfile& profile)
{
    for (auto& page : printPlan.pages) {
        applyDeviceScaleToCommands(page.commands, profile);
    }
}

} // 匿名命名空间

NativeLabelDrawingPlan TemplateDocumentRenderer::render(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const LabelOffset& labelOffset,
    const DeviceProfile& profile) const
{
    return renderPrint(document, labelPlan, labelOffset, profile, TemplateRenderContext{}).firstPageAsLabelPlan();
}

NativeLabelDrawingPlan TemplateDocumentRenderer::render(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const LabelOffset& labelOffset,
    const DeviceProfile& profile,
    const TemplateRenderContext& context) const
{
    return renderPrint(document, labelPlan, labelOffset, profile, context).firstPageAsLabelPlan();
}

NativePrintDrawingPlan TemplateDocumentRenderer::renderPrint(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const LabelOffset& labelOffset,
    const DeviceProfile& profile,
    const TemplateRenderContext& context) const
{
    NativePrintDrawingPlan printPlan;
    printPlan.paperSize = labelPlan.paperSize;
    printPlan.renderDpi = usableRenderDpi(profile.dpi);

    const auto renderElements = visibleElementsInRenderOrder(document);
    const auto offsetX = labelOffset.x + profile.offsetX;
    const auto offsetY = labelOffset.y + profile.offsetY;
    const auto baseCommands = renderTemplateElements(labelPlan, renderElements, context, offsetX, offsetY);

    NativePrintPage firstPage;
    firstPage.pageNumber = 1;
    firstPage.commands = baseCommands;
    printPlan.pages.append(firstPage);

    const auto renderTables = visibleTablesInRenderOrder(document);
    for (const auto& table : renderTables) {
        const auto tableResult = TableElementRenderer::renderPages(table, context, offsetX, offsetY);
        if (!tableResult.success()) {
            // 当前渲染计划没有错误通道；表格入口校验会在后续打印流程统一阻止异常模板。
            continue;
        }

        while (printPlan.pages.size() < tableResult.pages.size()) {
            NativePrintPage page;
            page.pageNumber = printPlan.pages.size() + 1;
            // 当前模型还没有元素的首页/每页/末页作用域配置，非表格元素先按每页固定内容重复。
            page.commands = baseCommands;
            printPlan.pages.append(page);
        }

        for (int pageIndex = 0; pageIndex < tableResult.pages.size(); ++pageIndex) {
            printPlan.pages[pageIndex].commands.append(tableResult.pages[pageIndex].commands);
        }
    }

    applyDeviceScale(printPlan, profile);
    return printPlan;
}

} // 命名空间 sleekpr::core
