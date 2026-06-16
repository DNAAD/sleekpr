#pragma once

#include "sleekpr/core/native/NativeDrawCommand.h"
#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/printing/LabelPaperSize.h"

#include <QList>

namespace sleekpr::core {

struct NativePrintPage
{
    // pageNumber 使用从 1 开始的页码，便于后续日志和用户提示直接展示。
    int pageNumber = 1;
    QList<NativeDrawCommand> commands;
};

struct NativePrintDrawingPlan
{
    // 多页计划沿用单页计划的纸张和 DPI 语义，页面内容通过 pages 拆分。
    LabelPaperSize paperSize = LabelPaperSize::create80x30();
    double renderDpi = 300.0;
    QList<NativePrintPage> pages;

    static NativePrintDrawingPlan fromSinglePage(const NativeLabelDrawingPlan& plan)
    {
        NativePrintDrawingPlan result;
        result.paperSize = plan.paperSize;
        result.renderDpi = plan.renderDpi;
        result.pages.append(NativePrintPage{1, plan.commands});
        return result;
    }

    NativeLabelDrawingPlan firstPageAsLabelPlan() const
    {
        NativeLabelDrawingPlan result;
        result.paperSize = paperSize;
        result.renderDpi = renderDpi;
        if (!pages.isEmpty()) {
            result.commands = pages.first().commands;
        }
        return result;
    }
};

} // 命名空间 sleekpr::core
