#include "sleekpr/core/templates/TemplateDocumentFactory.h"

#include "sleekpr/core/native/NativeDrawCommandType.h"

#include <algorithm>
#include <QHash>
#include <QSet>

namespace sleekpr::core {
namespace {

constexpr int MaximumTemplatePartRows = 9;

QString fieldKeyForCommand(const QString& elementKey)
{
    static const QHash<QString, QString> fieldKeys{
        {QStringLiteral("identifier"), QStringLiteral("identifierText")},
        {QStringLiteral("verticalIdentifier"), QStringLiteral("identifierText")},
        {QStringLiteral("productName"), QStringLiteral("productName")},
        {QStringLiteral("finishedWeightLabel"), QStringLiteral("finishedWeightLabel")},
        {QStringLiteral("finishedWeightValue"), QStringLiteral("finishedWeightValue")},
        {QStringLiteral("roughWeightLabel"), QStringLiteral("roughWeightLabel")},
        {QStringLiteral("roughWeightValue"), QStringLiteral("roughWeightValue")},
        {QStringLiteral("standardText"), QStringLiteral("standardText")},
        {QStringLiteral("addressText"), QStringLiteral("addressText")},
        {QStringLiteral("salesCode"), QStringLiteral("salesCode")},
        {QStringLiteral("priceText"), QStringLiteral("priceText")},
        {QStringLiteral("additionalPrice"), QStringLiteral("additionalPriceText")},
        {QStringLiteral("qrNote"), QStringLiteral("qrNote")},
        {QStringLiteral("footerText"), QStringLiteral("footerText")},
    };
    return fieldKeys.value(elementKey);
}

QString uniqueElementId(const QString& preferredId, QSet<QString>& usedIds)
{
    const auto baseId = preferredId.trimmed().isEmpty() ? QStringLiteral("element") : preferredId.trimmed();
    if (!usedIds.contains(baseId)) {
        usedIds.insert(baseId);
        return baseId;
    }

    int suffix = 1;
    while (usedIds.contains(QStringLiteral("%1-%2").arg(baseId).arg(suffix))) {
        ++suffix;
    }

    const auto id = QStringLiteral("%1-%2").arg(baseId).arg(suffix);
    usedIds.insert(id);
    return id;
}

TemplateElement elementFromCommand(
    const NativeDrawCommand& command,
    int index,
    const QString& layerId,
    QSet<QString>& usedIds,
    QHash<QString, int>& commandOccurrences)
{
    const auto occurrenceIndex = commandOccurrences.value(command.elementKey, 0);
    commandOccurrences[command.elementKey] = occurrenceIndex + 1;

    TemplateElement element;
    element.id = uniqueElementId(command.elementKey.isEmpty() ? QStringLiteral("element-%1").arg(index) : command.elementKey, usedIds);
    element.layerId = layerId;
    element.zIndex = index;
    element.x = command.x;
    element.y = command.y;
    element.width = command.width;
    element.height = command.height;
    element.text = command.text;
    element.fontSizePt = command.fontSizePt;
    element.bold = command.bold;
    element.rotationDegrees = command.rotationDegrees;
    element.maxLines = command.maxLines;
    element.ellipsis = command.ellipsis;

    switch (command.type) {
    case NativeDrawCommandType::Text: {
        const auto fieldKey = command.elementKey == QStringLiteral("partRow")
            ? QStringLiteral("partRow:%1").arg(occurrenceIndex)
            : fieldKeyForCommand(command.elementKey);
        if (fieldKey.isEmpty()) {
            element.type = TemplateElementType::FixedText;
        } else {
            // 已知业务字段转为动态绑定，模板渲染时再从 LabelRenderPlan 取最新文本。
            element.type = TemplateElementType::BoundField;
            element.fieldKey = fieldKey;
        }
        break;
    }
    case NativeDrawCommandType::QrCode:
        element.type = TemplateElementType::QrCode;
        if (command.elementKey == QStringLiteral("qrCode")) {
            // 默认二维码保持动态载荷，避免启动模板时固化演示数据。
            element.fieldKey = QStringLiteral("qrPayload");
            element.payload = QString();
        } else {
            element.payload = command.text;
        }
        break;
    case NativeDrawCommandType::Rectangle:
        element.type = TemplateElementType::Rectangle;
        break;
    }

    return element;
}

bool isPartRowElement(const TemplateElement& element)
{
    return element.type == TemplateElementType::BoundField
        && element.fieldKey.startsWith(QStringLiteral("partRow:"));
}

void renumberZIndexes(QList<TemplateElement>& elements)
{
    for (int index = 0; index < elements.size(); ++index) {
        elements[index].zIndex = index;
    }
}

void appendMissingPartRows(QList<TemplateElement>& elements, const QString& layerId, QSet<QString>& usedIds)
{
    QList<TemplateElement> partRows;
    int insertIndex = -1;
    for (int index = 0; index < elements.size(); ++index) {
        if (isPartRowElement(elements[index])) {
            partRows.append(elements[index]);
            insertIndex = index;
        }
    }

    if (partRows.isEmpty() || partRows.size() >= MaximumTemplatePartRows) {
        return;
    }

    const auto firstRow = partRows.first();
    const auto leftX = firstRow.x;
    const auto rightX = partRows.size() > 1 ? partRows[1].x : firstRow.x + 11.5;
    const auto detectedRowSpacing = partRows.size() > 2 ? partRows[2].y - firstRow.y : 2.05;
    const auto rowSpacing = detectedRowSpacing > 0.0 ? detectedRowSpacing : 2.05;

    for (int rowIndex = partRows.size(); rowIndex < MaximumTemplatePartRows; ++rowIndex) {
        auto element = firstRow;
        element.id = uniqueElementId(QStringLiteral("partRow"), usedIds);
        element.layerId = layerId;
        element.fieldKey = QStringLiteral("partRow:%1").arg(rowIndex);
        // 补齐的明细槽只保留动态绑定，显示内容在渲染当前业务数据时再生成。
        element.text = QString();
        element.x = rowIndex % 2 == 0 ? leftX : rightX;
        element.y = firstRow.y + (rowIndex / 2) * rowSpacing;
        elements.insert(++insertIndex, element);
    }
}

} // 匿名命名空间

TemplateDocument TemplateDocumentFactory::fromDrawingPlan(
    const QString& templateKey,
    const QString& name,
    const NativeLabelDrawingPlan& drawingPlan,
    const QList<TemplateElement>& customElements)
{
    const QString baseLayerId = QStringLiteral("base");
    QSet<QString> usedIds;
    QHash<QString, int> commandOccurrences;

    TemplateLayer baseLayer;
    baseLayer.id = baseLayerId;
    baseLayer.name = QString::fromUtf8("基础图层");
    baseLayer.visible = true;
    baseLayer.locked = false;
    baseLayer.elements.reserve(drawingPlan.commands.size() + customElements.size());

    for (int index = 0; index < drawingPlan.commands.size(); ++index) {
        baseLayer.elements.append(elementFromCommand(drawingPlan.commands[index], index, baseLayerId, usedIds, commandOccurrences));
    }
    appendMissingPartRows(baseLayer.elements, baseLayerId, usedIds);
    renumberZIndexes(baseLayer.elements);

    QList<TemplateElement> sortedCustomElements = customElements;
    std::stable_sort(sortedCustomElements.begin(), sortedCustomElements.end(), [](const TemplateElement& left, const TemplateElement& right) {
        return left.zIndex < right.zIndex;
    });

    int nextZIndex = baseLayer.elements.size();
    for (auto customElement : sortedCustomElements) {
        customElement.id = uniqueElementId(customElement.id, usedIds);
        customElement.layerId = baseLayerId;
        // 自定义元素追加到基础命令之后，避免覆盖启动模板的原有绘制顺序。
        customElement.zIndex = nextZIndex++;
        baseLayer.elements.append(customElement);
    }

    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("template-") + templateKey;
    document.name = name;
    document.templateKey = templateKey;
    document.layers = {baseLayer};
    return document;
}

} // 命名空间 sleekpr::core
