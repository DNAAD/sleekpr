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
        && sameTableColumns(left.columns, right.columns);
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
