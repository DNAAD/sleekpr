#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QtGlobal>

#include <algorithm>
#include <optional>

namespace sleekpr::http {

struct LocalHttpLimits
{
    static constexpr qsizetype kDefaultMaxHeaderBytes = 16 * 1024;
    static constexpr qsizetype kDefaultMaxContentLengthBytes = 8 * 1024 * 1024;
    static constexpr int kDefaultMaxPreviewBatchItems = 50;
    static constexpr int kDefaultMaxPreviewPages = 100;
    static constexpr qsizetype kDefaultMaxPreviewResponseBytes = 4 * 1024 * 1024;

    static constexpr qsizetype kMinMaxHeaderBytes = 4 * 1024;
    static constexpr qsizetype kMaxMaxHeaderBytes = 256 * 1024;
    static constexpr qsizetype kMinMaxContentLengthBytes = 1 * 1024 * 1024;
    static constexpr qsizetype kMaxMaxContentLengthBytes = 64 * 1024 * 1024;
    static constexpr int kMinMaxPreviewBatchItems = 1;
    static constexpr int kMaxMaxPreviewBatchItems = 500;
    static constexpr int kMinMaxPreviewPages = 1;
    static constexpr int kMaxMaxPreviewPages = 1000;
    static constexpr qsizetype kMinMaxPreviewResponseBytes = 1 * 1024 * 1024;
    static constexpr qsizetype kMaxMaxPreviewResponseBytes = 64 * 1024 * 1024;

    // 请求头只承载路由和少量 CORS 信息，过大时直接拒绝，避免无界缓存。
    qsizetype maxHeaderBytes = kDefaultMaxHeaderBytes;
    // 本地接口主要接收模板、设置和打印数据；8MB 足够常规模板，同时能挡住误发大文件。
    qsizetype maxContentLengthBytes = kDefaultMaxContentLengthBytes;
    // 批量预览面向 Web 端交互缩略图，默认限制在可快速返回的规模。
    int maxPreviewBatchItems = kDefaultMaxPreviewBatchItems;
    // 表格分页预览可能一次生成多页，必须限制总页数避免连续渲染拖垮客户端。
    int maxPreviewPages = kDefaultMaxPreviewPages;
    // PNG Base64 会放大响应体，返回前按最终 JSON 字节数兜底。
    qsizetype maxPreviewResponseBytes = kDefaultMaxPreviewResponseBytes;

    static LocalHttpLimits fromSettings(const sleekpr::core::PrintClientSettings& settings);
};

template <typename T>
T limitValueOrDefault(const std::optional<T>& value, T defaultValue, T minimum, T maximum)
{
    return std::clamp(value.value_or(defaultValue), minimum, maximum);
}

inline LocalHttpLimits LocalHttpLimits::fromSettings(const sleekpr::core::PrintClientSettings& settings)
{
    LocalHttpLimits limits;
    const auto& configured = settings.localHttpLimits;
    // settings.json 可能由外部接口写入，运行时必须再次夹紧，避免配置绕过 UI 上限。
    limits.maxHeaderBytes = limitValueOrDefault(
        configured.maxHeaderBytes,
        kDefaultMaxHeaderBytes,
        kMinMaxHeaderBytes,
        kMaxMaxHeaderBytes);
    limits.maxContentLengthBytes = limitValueOrDefault(
        configured.maxContentLengthBytes,
        kDefaultMaxContentLengthBytes,
        kMinMaxContentLengthBytes,
        kMaxMaxContentLengthBytes);
    limits.maxPreviewBatchItems = limitValueOrDefault(
        configured.maxPreviewBatchItems,
        kDefaultMaxPreviewBatchItems,
        kMinMaxPreviewBatchItems,
        kMaxMaxPreviewBatchItems);
    limits.maxPreviewPages = limitValueOrDefault(
        configured.maxPreviewPages,
        kDefaultMaxPreviewPages,
        kMinMaxPreviewPages,
        kMaxMaxPreviewPages);
    limits.maxPreviewResponseBytes = limitValueOrDefault(
        configured.maxPreviewResponseBytes,
        kDefaultMaxPreviewResponseBytes,
        kMinMaxPreviewResponseBytes,
        kMaxMaxPreviewResponseBytes);
    return limits;
}

} // 命名空间 sleekpr::http
