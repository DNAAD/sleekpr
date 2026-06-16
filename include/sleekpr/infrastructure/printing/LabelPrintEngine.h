#pragma once

#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/native/NativePrintDrawingPlan.h"

#include <QString>

namespace sleekpr::infrastructure {

class LabelPrintEngine
{
public:
    virtual ~LabelPrintEngine() = default;

    // 打印边界只接收已经规划好的毫米坐标命令，避免设备适配层重新理解业务字段。
    virtual bool print(const sleekpr::core::NativeLabelDrawingPlan& plan, const QString& printerName) = 0;

    // 多页入口用于单据和复杂表格；默认实现按页落回旧单页入口，便于设备适配器逐步升级。
    virtual bool print(const sleekpr::core::NativePrintDrawingPlan& plan, const QString& printerName)
    {
        if (plan.pages.isEmpty()) {
            return false;
        }

        for (const auto& page : plan.pages) {
            sleekpr::core::NativeLabelDrawingPlan pagePlan;
            pagePlan.paperSize = plan.paperSize;
            pagePlan.renderDpi = plan.renderDpi;
            pagePlan.commands = page.commands;
            if (!print(pagePlan, printerName)) {
                return false;
            }
        }

        return true;
    }
};

} // 命名空间 sleekpr::infrastructure
