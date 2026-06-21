#include "sleekpr/core/templates/DefaultPaperSpecs.h"

namespace sleekpr::core {

PaperSpec defaultLabel80x30PaperSpec()
{
    PaperSpec spec;
    spec.id = QStringLiteral("label-80x30");
    spec.name = QString::fromUtf8("80x30 标签");
    spec.widthMm = 80.0;
    spec.heightMm = 30.0;
    // 常见 80x30 热敏标签机为 203DPI，内置兜底必须避免按 300DPI 输出后交给驱动缩放。
    spec.defaultDpi = 203.0;
    return spec;
}

std::optional<PaperSpec> builtInPaperSpec(const QString& paperSpecId)
{
    const auto normalizedId = paperSpecId.trimmed();
    if (normalizedId == QStringLiteral("label-80x30")) {
        return defaultLabel80x30PaperSpec();
    }
    return std::nullopt;
}

} // namespace sleekpr::core
