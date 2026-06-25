#pragma once

#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/TableElement.h"

#include <QList>
#include <QString>

namespace sleekpr::app {

struct DesignerElementPropertyModel
{
    QString elementId;
    sleekpr::core::TemplateElementType type = sleekpr::core::TemplateElementType::FixedText;
    bool visible = false;
    bool canEdit = false;
    bool canEditValue = false;
    bool supportsTextStyle = false;
    bool supportsAutoFitFont = false;
    bool supportsVerticalText = false;
    bool isArrayGrid = false;
    QString value;
    QString valuePlaceholder;
    double x = 0.0;
    double y = 0.0;
    double width = 10.0;
    double height = 3.0;
    double fontSizePt = 4.0;
    double rotationDegrees = 0.0;
    bool bold = false;
    bool autoFitFont = false;
    double autoFitMinFontSizePt = 3.0;
    double autoFitMaxFontSizePt = 12.0;
    bool verticalText = false;
    QString dataPath;
    int arrayGridRows = 2;
    int arrayGridColumns = 3;
    double arrayGridRowHeightMm = 0.0;
    QString arrayGridCellTemplate = QStringLiteral("${text}:${value}");
    bool arrayGridDrawBorders = true;
};

struct DesignerTableColumnModel
{
    // 右侧属性面板使用结构化列模型，避免复杂表格继续依赖单行文本拼接。
    QString columnId;
    QString title;
    QString fieldKey;
    sleekpr::core::TableColumnWidthMode widthMode = sleekpr::core::TableColumnWidthMode::Fixed;
    double widthMm = 20.0;
    double flexWeight = 1.0;
    sleekpr::core::TableCellAlignment alignment = sleekpr::core::TableCellAlignment::Left;
    double fontSizePt = 8.0;
    bool bold = false;
    bool ellipsis = false;
};

struct DesignerTableRowBandModel
{
    // 行带是复杂表格的结构边界，设计器用它区分表头、明细、小计、汇总和页脚等可编辑区域。
    QString rowBandId;
    sleekpr::core::TableRowBandKind kind = sleekpr::core::TableRowBandKind::Detail;
    QString title;
    QString dataPath;
    sleekpr::core::TableRowHeightMode heightMode = sleekpr::core::TableRowHeightMode::Fixed;
    double heightMm = 5.0;
    double minHeightMm = 4.0;
    bool repeatOnPage = false;
    sleekpr::core::TableRowPrintScope printOn = sleekpr::core::TableRowPrintScope::EveryPage;
};

struct DesignerTableCellStyleModel
{
    // 单元格样式独立成模型，避免每个单元格模板重复保存字体、边距和边框设置。
    QString styleId;
    double fontSizePt = 8.0;
    bool bold = false;
    sleekpr::core::TableCellAlignment alignment = sleekpr::core::TableCellAlignment::Left;
    sleekpr::core::TableVerticalAlignment verticalAlignment = sleekpr::core::TableVerticalAlignment::Top;
    double paddingLeftMm = 0.6;
    double paddingTopMm = 0.4;
    double paddingRightMm = 0.6;
    double paddingBottomMm = 0.4;
    bool drawBorder = true;
    double borderWidthMm = 0.1;
    QString backgroundColor;
    QString textColor;
    bool wrapText = false;
    bool ellipsis = false;
};

struct DesignerTableCellTemplateModel
{
    // 单元格模板决定某个行带和列交叉位置显示什么内容，并可覆盖默认样式和溢出策略。
    QString templateId;
    QString rowBandId;
    QString columnId;
    QString textTemplate;
    QString fieldKey;
    QString styleId;
    sleekpr::core::TableCellOverflowPolicy overflowPolicy = sleekpr::core::TableCellOverflowPolicy::Clip;
    int maxLines = 1;
    int colSpan = 1;
    int rowSpan = 1;
    bool visible = true;
};

struct DesignerTableMergeRegionModel
{
    // 合并区域单独保存，便于设计器提供类似 Excel 的合并/拆分入口。
    QString mergeId;
    QString rowBandId;
    int startRowOffset = 0;
    QString startColumnId;
    int rowSpan = 1;
    int colSpan = 1;
};

struct DesignerTablePagePreviewModel
{
    // 分页预览只展示布局结果摘要，真实渲染仍由 Qt 原生链路完成。
    int pageNumber = 1;
    int firstRowIndex = 0;
    int rowCount = 0;
    QString note;
};

struct DesignerTablePropertyModel
{
    QString tableId;
    bool visible = false;
    bool canEdit = false;
    QString displayName;
    QString dataPath;
    double x = 0.0;
    double y = 0.0;
    double width = 80.0;
    double height = 30.0;
    double headerRowHeightMm = 6.0;
    double detailRowHeightMm = 5.0;
    bool repeatHeaderOnPage = true;
    bool keepGroupTogether = false;
    bool allowRowSplit = false;
    int maxPages = 100;
    int orphanDetailRows = 0;
    QString groupKeyField;
    sleekpr::core::TableTableOverflowPolicy tableOverflowPolicy = sleekpr::core::TableTableOverflowPolicy::Error;
    bool drawBorders = true;
    QString columnsText;
    QList<DesignerTableColumnModel> columns;
    QList<DesignerTableRowBandModel> rowBands;
    QList<DesignerTableCellStyleModel> cellStyles;
    QList<DesignerTableCellTemplateModel> cellTemplates;
    QList<DesignerTableMergeRegionModel> mergeRegions;
    QList<DesignerTablePagePreviewModel> pagePreviews;
    // 结构化列编辑器发出的模型才优先使用 columns；高级文本框编辑仍走旧文本解析。
    bool preferStructuredColumns = false;
};

} // 命名空间 sleekpr::app
