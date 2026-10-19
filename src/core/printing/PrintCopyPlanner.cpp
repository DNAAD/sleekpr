#include "sleekpr/core/printing/PrintCopyPlanner.h"

#include <cmath>

namespace sleekpr::core {

std::optional<NativePrintDrawingPlan> PrintCopyPlanner::repeat(const NativePrintDrawingPlan& source,
    int copies, QString* errorMessage)
{
    if (errorMessage != nullptr) errorMessage->clear();
    const auto fail = [errorMessage](const QString& message) -> std::optional<NativePrintDrawingPlan> {
        if (errorMessage != nullptr) *errorMessage = message;
        return std::nullopt;
    };
    if (copies < 1 || copies > MaxCopies) {
        return fail(QString::fromUtf8("打印份数必须在 1 到 %1 之间").arg(MaxCopies));
    }
    if (source.pages.isEmpty() || !std::isfinite(source.renderDpi) || source.renderDpi <= 0
        || !std::isfinite(source.paperSize.widthMm()) || source.paperSize.widthMm() <= 0
        || !std::isfinite(source.paperSize.heightMm()) || source.paperSize.heightMm() <= 0) {
        return fail(QString::fromUtf8("打印计划为空或纸张参数无效"));
    }
    // 先除后乘，避免巨大计划的页数乘法溢出；失败时不生成半份任务。
    if (source.pages.size() > MaxJobPages / copies) {
        return fail(QString::fromUtf8("单次打印不能超过 %1 页").arg(MaxJobPages));
    }
    auto result = source;
    result.pages.clear();
    result.pages.reserve(source.pages.size() * copies);
    for (int copy = 0; copy < copies; ++copy) {
        for (auto page : source.pages) {
            page.pageNumber = result.pages.size() + 1;
            result.pages.append(std::move(page));
        }
    }
    return result;
}

} // 命名空间 sleekpr::core
