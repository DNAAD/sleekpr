#pragma once

#include <QtGlobal>

#include <optional>

namespace sleekpr::core {

struct LocalHttpLimitSettings
{
    // 这些字段是 settings.json 中的用户覆盖值；缺失时由 HTTP 层的 LocalHttpLimits 默认值兜底。
    std::optional<qsizetype> maxHeaderBytes;
    std::optional<qsizetype> maxContentLengthBytes;
    std::optional<int> maxPreviewBatchItems;
    std::optional<int> maxPreviewPages;
    std::optional<qsizetype> maxPreviewResponseBytes;
};

} // 命名空间 sleekpr::core
