#pragma once

#include "sleekpr/core/native/NativeLabelDrawingPlan.h"

#include <QString>

namespace sleekpr::infrastructure {

class LabelPrintEngine
{
public:
    virtual ~LabelPrintEngine() = default;

    // 打印边界只接收已经规划好的毫米坐标命令，避免设备适配层重新理解业务字段。
    virtual bool print(const sleekpr::core::NativeLabelDrawingPlan& plan, const QString& printerName) = 0;
};

} // 命名空间 sleekpr::infrastructure
