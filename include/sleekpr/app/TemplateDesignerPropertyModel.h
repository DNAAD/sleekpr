#pragma once

#include "sleekpr/core/settings/TemplateElement.h"

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
};

} // 命名空间 sleekpr::app
