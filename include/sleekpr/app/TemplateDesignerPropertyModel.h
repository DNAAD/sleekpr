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
    bool drawBorders = true;
    QString columnsText;
    QList<DesignerTableColumnModel> columns;
    // 结构化列编辑器发出的模型才优先使用 columns；高级文本框编辑仍走旧文本解析。
    bool preferStructuredColumns = false;
};

} // 命名空间 sleekpr::app
