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

    // 来自设备 profile 的渲染 DPI，用于同一毫米坐标计划映射到不同打印机分辨率。
    double renderDpi = 300.0;
};

} // 命名空间 sleekpr::core
