#pragma once

#include "sleekpr/core/native/NativeDrawCommand.h"
#include "sleekpr/core/printing/LabelPaperSize.h"

#include <QList>

namespace sleekpr::core {

struct NativeLabelDrawingPlan
{
    // 绘制计划对应的纸张尺寸。
    LabelPaperSize paperSize = LabelPaperSize::create80x30();

    // 按顺序执行的毫米坐标绘制命令。
    QList<NativeDrawCommand> commands;
};

} // 命名空间 sleekpr::core
