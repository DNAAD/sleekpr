#include "sleekpr/app/TemplateDesignerCommand.h"

#include "sleekpr/app/TableColumnTextCodec.h"
#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateDocumentEditModel.h"

#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace sleekpr::app {

namespace {

bool sameDouble(double left, double right)
{
    return qFuzzyCompare(left + 1.0, right + 1.0);
}

bool sameTableColumn(const sleekpr::core::TableColumn& left, const sleekpr::core::TableColumn& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.fieldKey == right.fieldKey
        && sameDouble(left.widthMm, right.widthMm)
        && left.widthMode == right.widthMode
        && sameDouble(left.flexWeight, right.flexWeight)
        && left.alignment == right.alignment
        && sameDouble(left.fontSizePt, right.fontSizePt)
        && left.bold == right.bold
        && left.ellipsis == right.ellipsis;
}

bool sameTableColumns(const QList<sleekpr::core::TableColumn>& left, const QList<sleekpr::core::TableColumn>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameTableColumn(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool sameTableCellStyle(const sleekpr::core::TableCellStyle& left, const sleekpr::core::TableCellStyle& right)
{
    return left.id == right.id
        && sameDouble(left.fontSizePt, right.fontSizePt)
        && left.bold == right.bold
        && left.alignment == right.alignment
        && left.verticalAlignment == right.verticalAlignment
        && sameDouble(left.paddingLeftMm, right.paddingLeftMm)
        && sameDouble(left.paddingTopMm, right.paddingTopMm)
        && sameDouble(left.paddingRightMm, right.paddingRightMm)
        && sameDouble(left.paddingBottomMm, right.paddingBottomMm)
        && left.drawBorder == right.drawBorder
        && sameDouble(left.borderWidthMm, right.borderWidthMm)
        && left.backgroundColor == right.backgroundColor
        && left.textColor == right.textColor
        && left.wrapText == right.wrapText
        && left.ellipsis == right.ellipsis;
}

bool sameTableCellStyles(const QList<sleekpr::core::TableCellStyle>& left, const QList<sleekpr::core::TableCellStyle>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameTableCellStyle(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool sameTableRowBand(const sleekpr::core::TableRowBand& left, const sleekpr::core::TableRowBand& right)
{
    return left.id == right.id
        && left.kind == right.kind
        && left.title == right.title
        && left.dataPath == right.dataPath
        && left.heightMode == right.heightMode
        && sameDouble(left.heightMm, right.heightMm)
        && sameDouble(left.minHeightMm, right.minHeightMm)
        && left.repeatOnPage == right.repeatOnPage
        && left.printOn == right.printOn;
}

bool sameTableRowBands(const QList<sleekpr::core::TableRowBand>& left, const QList<sleekpr::core::TableRowBand>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameTableRowBand(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool sameTableCellTemplate(const sleekpr::core::TableCellTemplate& left, const sleekpr::core::TableCellTemplate& right)
{
    return left.id == right.id
        && left.rowBandId == right.rowBandId
        && left.columnId == right.columnId
        && left.textTemplate == right.textTemplate
        && left.fieldKey == right.fieldKey
        && left.styleId == right.styleId
        && left.overflowPolicy == right.overflowPolicy
        && left.maxLines == right.maxLines
        && left.colSpan == right.colSpan
        && left.rowSpan == right.rowSpan
        && left.visible == right.visible;
}

bool sameTableCellTemplates(const QList<sleekpr::core::TableCellTemplate>& left, const QList<sleekpr::core::TableCellTemplate>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameTableCellTemplate(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool sameTableMergeRegion(const sleekpr::core::TableMergeRegion& left, const sleekpr::core::TableMergeRegion& right)
{
    return left.id == right.id
        && left.rowBandId == right.rowBandId
        && left.startRowOffset == right.startRowOffset
        && left.startColumnId == right.startColumnId
        && left.rowSpan == right.rowSpan
        && left.colSpan == right.colSpan;
}

bool sameTableMergeRegions(const QList<sleekpr::core::TableMergeRegion>& left, const QList<sleekpr::core::TableMergeRegion>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameTableMergeRegion(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool sameTablePagination(const sleekpr::core::TablePaginationPolicy& left, const sleekpr::core::TablePaginationPolicy& right)
{
    return left.repeatHeaderOnPage == right.repeatHeaderOnPage
        && left.keepGroupTogether == right.keepGroupTogether
        && left.allowRowSplit == right.allowRowSplit
        && left.maxPages == right.maxPages
        && left.overflowPolicy == right.overflowPolicy
        && left.orphanDetailRows == right.orphanDetailRows
        && left.groupKeyField == right.groupKeyField;
}

bool sameTemplateElement(const sleekpr::core::TemplateElement& left, const sleekpr::core::TemplateElement& right)
{
    return left.id == right.id
        && left.layerId == right.layerId
        && left.zIndex == right.zIndex
        && left.visible == right.visible
        && left.locked == right.locked
        && left.type == right.type
        && left.displayName == right.displayName
        && sameDouble(left.x, right.x)
        && sameDouble(left.y, right.y)
        && sameDouble(left.width, right.width)
        && sameDouble(left.height, right.height)
        && left.text == right.text
        && left.fieldKey == right.fieldKey
        && left.payload == right.payload
        && sameDouble(left.fontSizePt, right.fontSizePt)
        && left.bold == right.bold
        && sameDouble(left.rotationDegrees, right.rotationDegrees)
        && left.verticalText == right.verticalText
        && left.dataPath == right.dataPath
        && left.arrayGridRows == right.arrayGridRows
        && left.arrayGridColumns == right.arrayGridColumns
        && sameDouble(left.arrayGridRowHeightMm, right.arrayGridRowHeightMm)
        && left.arrayGridCellTemplate == right.arrayGridCellTemplate
        && left.arrayGridDrawBorders == right.arrayGridDrawBorders
        && left.maxLines == right.maxLines
        && left.ellipsis == right.ellipsis
        && left.autoFitFont == right.autoFitFont
        && sameDouble(left.autoFitMinFontSizePt, right.autoFitMinFontSizePt)
        && sameDouble(left.autoFitMaxFontSizePt, right.autoFitMaxFontSizePt);
}

bool sameTableElement(const sleekpr::core::TableElement& left, const sleekpr::core::TableElement& right)
{
    return left.id == right.id
        && left.layerId == right.layerId
        && left.zIndex == right.zIndex
        && left.visible == right.visible
        && left.locked == right.locked
        && left.displayName == right.displayName
        && sameDouble(left.x, right.x)
        && sameDouble(left.y, right.y)
        && sameDouble(left.width, right.width)
        && sameDouble(left.height, right.height)
        && left.dataPath == right.dataPath
        && sameDouble(left.headerRowHeightMm, right.headerRowHeightMm)
        && sameDouble(left.detailRowHeightMm, right.detailRowHeightMm)
        && left.drawBorders == right.drawBorders
        && left.repeatHeaderOnPage == right.repeatHeaderOnPage
        && sameTableColumns(left.columns, right.columns)
        && sameTableCellStyles(left.cellStyles, right.cellStyles)
        && sameTableRowBands(left.rowBands, right.rowBands)
        && sameTableCellTemplates(left.cellTemplates, right.cellTemplates)
        && sameTableMergeRegions(left.mergeRegions, right.mergeRegions)
        && sameTablePagination(left.pagination, right.pagination);
}

sleekpr::core::TableElement* findTable(sleekpr::core::TemplateDocument& document, const QString& tableId)
{
    for (auto& layer : document.layers) {
        for (auto& table : layer.tables) {
            if (table.id == tableId) {
                return &table;
            }
        }
    }
    return nullptr;
}

} // 匿名命名空间

static sleekpr::core::TableColumn columnFromModel(const DesignerTableColumnModel& model)
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

static QString normalizedId(const QString& value, const QString& prefix, int index)
{
    const auto trimmed = value.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("%1%2").arg(prefix).arg(index + 1) : trimmed;
}

static QList<sleekpr::core::TableCellStyle> cellStylesFromModels(const QList<DesignerTableCellStyleModel>& models)
{
    QList<sleekpr::core::TableCellStyle> styles;
    styles.reserve(models.size());
    for (int index = 0; index < models.size(); ++index) {
        const auto& model = models[index];
        sleekpr::core::TableCellStyle style;
        style.id = normalizedId(model.styleId, QStringLiteral("style"), index);
        style.fontSizePt = std::max(1.0, model.fontSizePt);
        style.bold = model.bold;
        style.alignment = model.alignment;
        style.verticalAlignment = model.verticalAlignment;
        style.paddingLeftMm = std::max(0.0, model.paddingLeftMm);
        style.paddingTopMm = std::max(0.0, model.paddingTopMm);
        style.paddingRightMm = std::max(0.0, model.paddingRightMm);
        style.paddingBottomMm = std::max(0.0, model.paddingBottomMm);
        style.drawBorder = model.drawBorder;
        style.borderWidthMm = std::max(0.0, model.borderWidthMm);
        style.backgroundColor = model.backgroundColor.trimmed();
        style.textColor = model.textColor.trimmed();
        style.wrapText = model.wrapText;
        style.ellipsis = model.ellipsis;
        styles.append(style);
    }
    return styles;
}

static QList<sleekpr::core::TableRowBand> rowBandsFromModels(const QList<DesignerTableRowBandModel>& models)
{
    QList<sleekpr::core::TableRowBand> rowBands;
    rowBands.reserve(models.size());
    for (int index = 0; index < models.size(); ++index) {
        const auto& model = models[index];
        sleekpr::core::TableRowBand rowBand;
        rowBand.id = normalizedId(model.rowBandId, QStringLiteral("band"), index);
        rowBand.kind = model.kind;
        rowBand.title = model.title.trimmed();
        rowBand.dataPath = model.dataPath.trimmed();
        rowBand.heightMode = model.heightMode;
        rowBand.heightMm = std::max(0.1, model.heightMm);
        rowBand.minHeightMm = std::max(0.1, model.minHeightMm);
        rowBand.repeatOnPage = model.repeatOnPage;
        rowBand.printOn = model.printOn;
        rowBands.append(rowBand);
    }
    return rowBands;
}

static QList<sleekpr::core::TableCellTemplate> cellTemplatesFromModels(const QList<DesignerTableCellTemplateModel>& models)
{
    QList<sleekpr::core::TableCellTemplate> cells;
    cells.reserve(models.size());
    for (int index = 0; index < models.size(); ++index) {
        const auto& model = models[index];
        sleekpr::core::TableCellTemplate cell;
        cell.id = normalizedId(model.templateId, QStringLiteral("cell"), index);
        cell.rowBandId = model.rowBandId.trimmed();
        cell.columnId = model.columnId.trimmed();
        cell.textTemplate = model.textTemplate;
        cell.fieldKey = model.fieldKey.trimmed();
        cell.styleId = model.styleId.trimmed();
        cell.overflowPolicy = model.overflowPolicy;
        cell.maxLines = std::max(1, model.maxLines);
        cell.colSpan = std::max(1, model.colSpan);
        cell.rowSpan = std::max(1, model.rowSpan);
        cell.visible = model.visible;
        cells.append(cell);
    }
    return cells;
}

static QList<sleekpr::core::TableMergeRegion> mergeRegionsFromModels(const QList<DesignerTableMergeRegionModel>& models)
{
    QList<sleekpr::core::TableMergeRegion> merges;
    merges.reserve(models.size());
    for (int index = 0; index < models.size(); ++index) {
        const auto& model = models[index];
        sleekpr::core::TableMergeRegion merge;
        merge.id = normalizedId(model.mergeId, QStringLiteral("merge"), index);
        merge.rowBandId = model.rowBandId.trimmed();
        merge.startRowOffset = std::max(0, model.startRowOffset);
        merge.startColumnId = model.startColumnId.trimmed();
        merge.rowSpan = std::max(1, model.rowSpan);
        merge.colSpan = std::max(1, model.colSpan);
        merges.append(merge);
    }
    return merges;
}

ElementPropertiesCommand::ElementPropertiesCommand(DesignerElementPropertyModel model)
    : m_model(std::move(model))
{
}

TemplateDesignerCommandResult ElementPropertiesCommand::apply(sleekpr::core::TemplateElement& element) const
{
    TemplateDesignerCommandResult result;
    result.targetId = element.id;
    if (!m_model.canEdit) {
        result.errorMessage = QString::fromUtf8("当前元素不可编辑");
        return result;
    }

    auto updated = element;
    const auto value = m_model.value;
    switch (updated.type) {
    case sleekpr::core::TemplateElementType::FixedText:
        // 固定文本保存模板原文，${字段名} 在 Qt 原生渲染链路中替换。
        updated.text = value;
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        updated.fieldKey = value.trimmed();
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        // 二维码保存模板表达式，真正矩阵内容在打印/预览前解析。
        updated.payload = value;
        break;
    case sleekpr::core::TemplateElementType::ArrayGrid:
        updated.dataPath = m_model.dataPath.trimmed();
        updated.arrayGridRows = m_model.arrayGridRows;
        updated.arrayGridColumns = m_model.arrayGridColumns;
        updated.arrayGridRowHeightMm = m_model.arrayGridRowHeightMm;
        updated.arrayGridCellTemplate = m_model.arrayGridCellTemplate;
        updated.arrayGridDrawBorders = m_model.arrayGridDrawBorders;
        break;
    default:
        break;
    }

    updated.x = m_model.x;
    updated.y = m_model.y;
    updated.width = m_model.width;
    updated.height = m_model.height;
    updated.fontSizePt = m_model.fontSizePt;
    updated.rotationDegrees = m_model.rotationDegrees;
    updated.bold = m_model.bold;
    updated.autoFitFont = m_model.supportsAutoFitFont && m_model.autoFitFont;
    updated.autoFitMinFontSizePt = m_model.autoFitMinFontSizePt;
    updated.autoFitMaxFontSizePt = std::max(updated.autoFitMinFontSizePt, m_model.autoFitMaxFontSizePt);
    updated.verticalText = m_model.supportsVerticalText && m_model.verticalText;

    if (sameTemplateElement(element, updated)) {
        return result;
    }

    element = updated;
    result.changed = true;
    return result;
}

TablePropertiesCommand::TablePropertiesCommand(DesignerTablePropertyModel model)
    : m_model(std::move(model))
{
}

TemplateDesignerCommandResult TablePropertiesCommand::apply(sleekpr::core::TemplateDocument& document, const QString& tableId) const
{
    TemplateDesignerCommandResult result;
    result.targetId = tableId;
    if (!m_model.canEdit) {
        result.errorMessage = QString::fromUtf8("当前表格不可编辑");
        return result;
    }

    auto* table = findTable(document, tableId);
    if (table == nullptr) {
        result.errorMessage = QString::fromUtf8("请先选择表格");
        return result;
    }

    auto updated = *table;
    updated.displayName = m_model.displayName.trimmed();
    updated.dataPath = m_model.dataPath.trimmed();
    updated.x = m_model.x;
    updated.y = m_model.y;
    updated.width = m_model.width;
    updated.height = m_model.height;
    updated.headerRowHeightMm = m_model.headerRowHeightMm;
    updated.detailRowHeightMm = m_model.detailRowHeightMm;
    updated.repeatHeaderOnPage = m_model.repeatHeaderOnPage;
    updated.pagination.repeatHeaderOnPage = m_model.repeatHeaderOnPage;
    updated.pagination.keepGroupTogether = m_model.keepGroupTogether;
    updated.pagination.allowRowSplit = m_model.allowRowSplit;
    updated.pagination.maxPages = std::max(1, m_model.maxPages);
    updated.pagination.orphanDetailRows = std::max(0, m_model.orphanDetailRows);
    updated.pagination.groupKeyField = m_model.groupKeyField.trimmed();
    updated.pagination.overflowPolicy = m_model.tableOverflowPolicy;
    updated.drawBorders = m_model.drawBorders;

    QList<sleekpr::core::TableColumn> nextColumns;
    if (m_model.preferStructuredColumns && !m_model.columns.isEmpty()) {
        nextColumns.reserve(m_model.columns.size());
        for (const auto& columnModel : m_model.columns) {
            nextColumns.append(columnFromModel(columnModel));
        }
    } else {
        nextColumns = parseColumns(m_model.columnsText);
    }
    if (nextColumns.isEmpty()) {
        result.errorMessage = QString::fromUtf8("表格列配置无效");
        return result;
    }
    updated.columns = nextColumns;
    updated.cellStyles = cellStylesFromModels(m_model.cellStyles);
    updated.rowBands = rowBandsFromModels(m_model.rowBands);
    updated.cellTemplates = cellTemplatesFromModels(m_model.cellTemplates);
    updated.mergeRegions = mergeRegionsFromModels(m_model.mergeRegions);

    if (sameTableElement(*table, updated)) {
        return result;
    }

    if (!sleekpr::core::TemplateDocumentEditModel::updateTable(document, tableId, updated)) {
        result.errorMessage = QString::fromUtf8("表格属性保存失败");
        return result;
    }

    result.changed = true;
    return result;
}

QString TablePropertiesCommand::formatColumns(const QList<sleekpr::core::TableColumn>& columns)
{
    // 当前 UI 仍使用单行列定义文本，命令层统一负责和结构化列模型互转。
    return TableColumnTextCodec::format(columns);
}

QList<sleekpr::core::TableColumn> TablePropertiesCommand::parseColumns(const QString& text)
{
    // 列配置语法：列标题=字段key:列宽，多列用英文逗号分隔。
    QString errorMessage;
    auto columns = TableColumnTextCodec::parse(text, &errorMessage);
    Q_UNUSED(errorMessage);
    return columns;
}

} // 命名空间 sleekpr::app
