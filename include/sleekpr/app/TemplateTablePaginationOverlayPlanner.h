#pragma once

#include "sleekpr/app/TemplateTablePaginationOverlay.h"
#include "sleekpr/core/templates/TableElementLayout.h"
#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QList>

namespace sleekpr::app {

class TemplateTablePaginationOverlayPlanner
{
public:
    // 规划器只产出设计器画布提示，不参与正式预览图片和真实打印排版。
    static QList<TablePaginationOverlay> plan(
        const sleekpr::core::TemplateDocument& document,
        const sleekpr::core::TemplateRenderContext& renderContext);
    static QList<TablePaginationOverlay> planForTable(
        const sleekpr::core::TableElement& table,
        const sleekpr::core::TableLayoutResult& layout);
};

} // namespace sleekpr::app
