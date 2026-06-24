#include "sleekpr/app/TemplateDesignerPresenter.h"

#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/TableElement.h"

namespace sleekpr::app {

DesignerElementPropertyModel TemplateDesignerPresenter::elementPropertyModel(
    const sleekpr::core::TemplateElement& element,
    bool canEdit) const
{
    DesignerElementPropertyModel model;
    model.elementId = element.id;
    model.type = element.type;
    model.visible = true;
    model.canEdit = canEdit;
    model.x = element.x;
    model.y = element.y;
    model.width = element.width;
    model.height = element.height;
    model.fontSizePt = element.fontSizePt;
    model.rotationDegrees = element.rotationDegrees;
    model.bold = element.bold;
    model.autoFitFont = element.autoFitFont;
    model.autoFitMinFontSizePt = element.autoFitMinFontSizePt;
    model.autoFitMaxFontSizePt = element.autoFitMaxFontSizePt;
    model.verticalText = element.verticalText;
    model.dataPath = element.dataPath;
    model.arrayGridRows = element.arrayGridRows;
    model.arrayGridColumns = element.arrayGridColumns;
    model.arrayGridRowHeightMm = element.arrayGridRowHeightMm;
    model.arrayGridCellTemplate = element.arrayGridCellTemplate;
    model.arrayGridDrawBorders = element.arrayGridDrawBorders;

    const auto isTextElement = element.type == sleekpr::core::TemplateElementType::FixedText
        || element.type == sleekpr::core::TemplateElementType::BoundField;
    model.isArrayGrid = element.type == sleekpr::core::TemplateElementType::ArrayGrid;
    model.canEditValue = isTextElement || element.type == sleekpr::core::TemplateElementType::QrCode;
    model.supportsTextStyle = isTextElement || model.isArrayGrid;
    model.supportsAutoFitFont = isTextElement;
    model.supportsVerticalText = element.type == sleekpr::core::TemplateElementType::FixedText;

    switch (element.type) {
    case sleekpr::core::TemplateElementType::FixedText:
        model.value = element.text;
        model.valuePlaceholder = QString::fromUtf8("例如：地址:${address}");
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        model.value = element.fieldKey;
        model.valuePlaceholder = QString::fromUtf8("字段 key，例如 productName");
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        model.value = element.payload.trimmed().isEmpty() && !element.fieldKey.trimmed().isEmpty()
            ? QStringLiteral("${%1}").arg(element.fieldKey.trimmed())
            : element.payload;
        model.valuePlaceholder = QString::fromUtf8("例如：${qrPayload} 或 地址:${address}");
        break;
    case sleekpr::core::TemplateElementType::ArrayGrid:
        model.valuePlaceholder = QString::fromUtf8("数组网格请编辑下方 dataPath 和单元格模板");
        break;
    default:
        model.valuePlaceholder = QString::fromUtf8("当前元素可编辑位置和尺寸");
        break;
    }

    return model;
}

DesignerTablePropertyModel TemplateDesignerPresenter::tablePropertyModel(
    const sleekpr::core::TableElement& table,
    bool canEdit) const
{
    DesignerTablePropertyModel model;
    model.tableId = table.id;
    model.visible = true;
    model.canEdit = canEdit;
    model.displayName = table.displayName;
    model.dataPath = table.dataPath;
    model.x = table.x;
    model.y = table.y;
    model.width = table.width;
    model.height = table.height;
    model.headerRowHeightMm = table.headerRowHeightMm;
    model.detailRowHeightMm = table.detailRowHeightMm;
    model.repeatHeaderOnPage = table.repeatHeaderOnPage;
    model.drawBorders = table.drawBorders;
    model.columnsText = TablePropertiesCommand::formatColumns(table.columns);
    return model;
}

TemplateDesignerCommandResult TemplateDesignerPresenter::applyElementProperties(
    sleekpr::core::TemplateElement& element,
    const DesignerElementPropertyModel& model) const
{
    return ElementPropertiesCommand(model).apply(element);
}

TemplateDesignerCommandResult TemplateDesignerPresenter::applyTableProperties(
    sleekpr::core::TemplateDocument& document,
    const QString& tableId,
    const DesignerTablePropertyModel& model) const
{
    return TablePropertiesCommand(model).apply(document, tableId);
}

} // 命名空间 sleekpr::app
