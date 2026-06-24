#include "sleekpr/http/LocalHttpRouter.h"

#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"
#include "sleekpr/http/PreviewRoutes.h"
#include "sleekpr/http/PrintRoutes.h"
#include "sleekpr/http/SettingsRoutes.h"
#include "sleekpr/http/TemplateRoutes.h"

#include <utility>

namespace sleekpr::http {

using sleekpr::core::FileSettingsStore;

LocalHttpRouter::LocalHttpRouter(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine)
    : LocalHttpRouter(std::move(settingsPath), printEngine, LocalHttpLimits{})
{
}

LocalHttpRouter::LocalHttpRouter(
    QString settingsPath,
    sleekpr::infrastructure::LabelPrintEngine* printEngine,
    LocalHttpLimits limits)
    : m_settingsPath(std::move(settingsPath))
    , m_printEngine(printEngine)
    , m_limits(limits)
{
}

void LocalHttpRouter::setLimits(LocalHttpLimits limits)
{
    m_limits = limits;
}

LocalHttpResponse LocalHttpRouter::route(const LocalHttpRequest& request) const
{
    if (m_limits.maxContentLengthBytes >= 0 && request.body.size() > m_limits.maxContentLengthBytes) {
        return requestTooLargeResponse(QString::fromUtf8("请求体超过本地接口限制。"));
    }

    const auto settings = FileSettingsStore(m_settingsPath).load();
    const auto method = request.method.toUpper();

    if (hasRejectedOrigin(request, settings)) {
        return jsonResponse(failEnvelope("FORBIDDEN_ORIGIN", "Origin is not allowed"), 403);
    }

    const auto extraHeaders = corsHeaders(request, settings);
    if (method == "OPTIONS") {
        return emptyResponse(204, extraHeaders);
    }

    if (const auto response = SettingsRoutes(m_settingsPath).route(request, settings, extraHeaders)) {
        // 设置类接口先交给独立路由模块，主路由只负责公共拦截和后续模块分发。
        return response.value();
    }

    if (const auto response = TemplateRoutes(m_settingsPath).route(request, extraHeaders)) {
        // 模板库、纸张规格和字段预设都属于模板管理接口，交给专门路由维护。
        return response.value();
    }

    if (const auto response = PreviewRoutes(m_settingsPath, m_limits).route(request, settings, extraHeaders)) {
        // 预览接口保持 Qt 原生渲染，HTTP 层只返回 PNG 数据。
        return response.value();
    }

    if (const auto response = PrintRoutes(m_settingsPath, m_printEngine).route(request, settings, extraHeaders)) {
        // 打印接口集中处理执行开关和打印引擎调用，避免主路由继续膨胀。
        return response.value();
    }

    return jsonResponse(failEnvelope("NOT_FOUND", "Route not implemented in native migration yet"), 404, extraHeaders);
}

} // 命名空间 sleekpr::http
