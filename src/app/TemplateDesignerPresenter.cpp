#include "sleekpr/app/TemplateDesignerPresenter.h"

#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/TableElement.h"

#include <algorithm>

namespace sleekpr::app {

namespace {

DesignerTableRowBandModel rowBandModel(const sleekpr::core::TableRowBand& rowBand)
{
    DesignerTableRowBandModel model;
    model.rowBandId = rowBand.id;
    model.kind = rowBand.kind;
    model.title = rowBand.title;
    model.dataPath = rowBand.dataPath;
    model.heightMode = rowBand.heightMode;
    model.heightMm = rowBand.heightMm;
    model.minHeightMm = rowBand.minHeightMm;
    model.repeatOnPage = rowBand.repeatOnPage;
    model.printOn = rowBand.printOn;
    return model;
}

DesignerTableCellStyleModel cellStyleModel(const sleekpr::core::TableCellStyle& style)
{
    DesignerTableCellStyleModel model;
    model.styleId = style.id;
    model.fontSizePt = style.fontSizePt;
    model.bold = style.bold;
    model.alignment = style.alignment;
    model.verticalAlignment = style.verticalAlignment;
    model.paddingLeftMm = style.paddingLeftMm;
    model.paddingTopMm = style.paddingTopMm;
    model.paddingRightMm = style.paddingRightMm;
    model.paddingBottomMm = style.paddingBottomMm;
    model.drawBorder = style.drawBorder;
    model.borderWidthMm = style.borderWidthMm;
    model.backgroundColor = style.backgroundColor;
    model.textColor = style.textColor;
    model.wrapText = style.wrapText;
    model.ellipsis = style.ellipsis;
    return model;
}

DesignerTableCellTemplateModel cellTemplateModel(const sleekpr::core::TableCellTemplate& cell)
{
    DesignerTableCellTemplateModel model;
    model.templateId = cell.id;
    model.rowBandId = cell.rowBandId;
    model.columnId = cell.columnId;
    model.textTemplate = cell.textTemplate;
    model.fieldKey = cell.fieldKey;
    model.styleId = cell.styleId;
    model.overflowPolicy = cell.overflowPolicy;
    model.maxLines = cell.maxLines;
    model.colSpan = cell.colSpan;
    model.rowSpan = cell.rowSpan;
    model.visible = cell.visible;
    return model;
}

DesignerTableMergeRegionModel mergeRegionModel(const sleekpr::core::TableMergeRegion& merge)
{
    DesignerTableMergeRegionModel model;
    model.mergeId = merge.id;
    model.rowBandId = merge.rowBandId;
    model.startRowOffset = merge.startRowOffset;
    model.startColumnId = merge.startColumnId;
    model.rowSpan = merge.rowSpan;
    model.colSpan = merge.colSpan;
    return model;
}

} // 匿名命名空间

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
    model.repeatHeaderOnPage = table.pagination.repeatHeaderOnPage;
    model.keepGroupTogether = table.pagination.keepGroupTogether;
    model.allowRowSplit = table.pagination.allowRowSplit;
    model.maxPages = table.pagination.maxPages;
    model.orphanDetailRows = table.pagination.orphanDetailRows;
    model.groupKeyField = table.pagination.groupKeyField;
    model.tableOverflowPolicy = table.pagination.overflowPolicy;
    model.drawBorders = table.drawBorders;
    model.columnsText = TablePropertiesCommand::formatColumns(table.columns);
    for (const auto& column : table.columns) {
        model.columns.append(tableColumnModel(column));
    }
    for (const auto& rowBand : table.rowBands) {
        model.rowBands.append(rowBandModel(rowBand));
    }
    for (const auto& style : table.cellStyles) {
        model.cellStyles.append(cellStyleModel(style));
    }
    for (const auto& cell : table.cellTemplates) {
        model.cellTemplates.append(cellTemplateModel(cell));
    }
    for (const auto& merge : table.mergeRegions) {
        model.mergeRegions.append(mergeRegionModel(merge));
    }
    return model;
}

DesignerTableColumnModel TemplateDesignerPresenter::tableColumnModel(const sleekpr::core::TableColumn& column) const
{
    DesignerTableColumnModel model;
    model.columnId = column.id;
    model.title = column.title;
    model.fieldKey = column.fieldKey;
    model.widthMode = column.widthMode;
    model.widthMm = column.widthMm;
    model.flexWeight = column.flexWeight;
    model.alignment = column.alignment;
    model.fontSizePt = column.fontSizePt;
    model.bold = column.bold;
    model.ellipsis = column.ellipsis;
    return model;
}

sleekpr::core::TableColumn TemplateDesignerPresenter::tableColumnFromModel(const DesignerTableColumnModel& model) const
{
    sleekpr::core::TableColumn column;
    column.id = model.columnId.trimmed().isEmpty() ? model.fieldKey.trimmed() : model.columnId.trimmed();
    column.title = model.title;
    column.fieldKey = model.fieldKey.trimmed();
    column.widthMode = model.widthMode;
    column.widthMm = std::max(0.1, model.widthMm);
    column.flexWeight = std::max(0.1, model.flexWeight);
    column.alignment = model.alignment;
    column.fontSizePt = std::max(1.0, model.fontSizePt);
    column.bold = model.bold;
    column.ellipsis = model.ellipsis;
    return column;
}

QList<sleekpr::core::TableColumn> TemplateDesignerPresenter::tableColumnsFromModels(
    const QList<DesignerTableColumnModel>& models) const
{
    QList<sleekpr::core::TableColumn> columns;
    columns.reserve(models.size());
    for (const auto& model : models) {
        columns.append(tableColumnFromModel(model));
    }
    return columns;
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
