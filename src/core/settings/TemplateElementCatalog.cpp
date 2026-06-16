#include "sleekpr/core/settings/TemplateElementCatalog.h"

#include <array>

namespace sleekpr::core {

namespace {

struct ElementDisplayName
{
    const char* key;
    const char* displayName;
};

constexpr std::array<ElementDisplayName, 17> editableElements{{
    {"identifier", "编号"},
    {"qualityMark", "合格证"},
    {"qrCode", "二维码"},
    {"qrNote", "二维码说明"},
    {"productName", "商品名称"},
    {"standardText", "执行标准"},
    {"addressText", "地址"},
    {"salesCode", "销售码"},
    {"finishedWeightLabel", "成品重标题"},
    {"finishedWeightValue", "成品重数值"},
    {"roughWeightLabel", "总件重标题"},
    {"roughWeightValue", "总件重数值"},
    {"priceText", "银标价格"},
    {"additionalPrice", "附加金额"},
    {"partRow", "明细行"},
    {"footerText", "底部备注"},
    {"verticalIdentifier", "竖排编号"},
}};

std::optional<NativeDrawCommand> firstCommandForKey(const NativeLabelDrawingPlan& drawingPlan, const QString& key)
{
    for (const auto& command : drawingPlan.commands) {
        if (command.elementKey == key) {
            return command;
        }
    }
    return std::nullopt;
}

TemplateElementOverride defaultValueFromCommand(const NativeDrawCommand& command)
{
    TemplateElementOverride value;
    value.x = command.x;
    value.y = command.y;

    // 二维码没有字体属性，保留空值可以让界面禁用相关输入。
    if (command.type == NativeDrawCommandType::Text) {
        value.fontSizePt = command.fontSizePt;
        value.bold = command.bold;
    }

    return value;
}

} // 匿名命名空间

QList<TemplateElementDefinition> TemplateElementCatalog::fromDrawingPlan(const NativeLabelDrawingPlan& drawingPlan)
{
    QList<TemplateElementDefinition> result;
    result.reserve(static_cast<int>(editableElements.size()));

    for (const auto& element : editableElements) {
        const QString key = QString::fromUtf8(element.key);
        const auto command = firstCommandForKey(drawingPlan, key);
        if (!command.has_value()) {
            continue;
        }

        result.append(TemplateElementDefinition{
            key,
            QString::fromUtf8(element.displayName),
            defaultValueFromCommand(command.value()),
        });
    }

    return result;
}

std::optional<TemplateElementDefinition> TemplateElementCatalog::find(const QList<TemplateElementDefinition>& elements, const QString& key)
{
    for (const auto& element : elements) {
        if (element.key == key) {
            return element;
        }
    }
    return std::nullopt;
}

} // 命名空间 sleekpr::core
