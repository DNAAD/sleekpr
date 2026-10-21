#pragma once

#include "sleekpr/core/native/NativePrintDrawingPlan.h"

#include <QString>
#include <optional>

namespace sleekpr::core {

class PrintCopyPlanner
{
public:
    static constexpr int MaxCopies = 100;
    static constexpr int MaxJobPages = 1000;
    static std::optional<int> validateCopies(const NativePrintDrawingPlan& source, int copies,
        QString* errorMessage = nullptr);

    // 按整份顺序展开到同一个原生打印任务，不让驱动份数差异改变页面顺序。
    static std::optional<NativePrintDrawingPlan> repeat(const NativePrintDrawingPlan& source, int copies,
        QString* errorMessage = nullptr);
};

} // 命名空间 sleekpr::core
