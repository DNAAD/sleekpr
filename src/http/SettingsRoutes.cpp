#include "sleekpr/http/SettingsRoutes.h"

#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPrinterInfo>

#include <utility>

namespace sleekpr::http {

namespace {

QJsonObject printersToJson(const sleekpr::core::PrintClientSettings& settings)
{
    QJsonArray printers;
    const auto defaultPrinterName = settings.defaultPrinter.trimmed();

    for (const auto& printer : QPrinterInfo::availablePrinters()) {
        const auto name = printer.printerName();
        // 如果用户已配置默认打印机，以配置为准；否则退回 Qt 识别到的系统默认打印机。
        printers.append(QJsonObject{
            {"name", name},
            {"displayName", name},
            {"isDefault", !defaultPrinterName.isEmpty()
                ? name.compare(defaultPrinterName, Qt::CaseInsensitive) == 0
                : printer.isDefault()},
            {"status", "unknown"},
        });
    }

    return QJsonObject{{"printers", printers}};
}

} // 匿名命名空间

SettingsRoutes::SettingsRoutes(QString settingsPath)
    : m_settingsPath(std::move(settingsPath))
{
}

std::optional<LocalHttpResponse> SettingsRoutes::route(
    const LocalHttpRequest& request,
    const sleekpr::core::PrintClientSettings& settings,
    const QHash<QByteArray, QByteArray>& extraHeaders) const
{
    const auto method = request.method.toUpper();
    const auto path = request.path;

    if ((path == "/" || path == "/health") && method == "GET") {
        // 健康检查是前端发现本地客户端是否在线的最小接口。
        return jsonResponse(okEnvelope(QJsonObject{
            {"ready", true},
            {"service", "sleekpr"},
            {"protocol", "http"},
        }), 200, extraHeaders);
    }

    if (path == "/settings" && method == "GET") {
        return jsonResponse(okEnvelope(sleekpr::core::PrintClientSettingsJson::toJson(settings)), 200, extraHeaders);
    }

    if (path == "/settings" && method == "POST") {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid settings JSON"), 400, extraHeaders);
        }

        const auto nextSettings = sleekpr::core::PrintClientSettingsJson::fromJson(document.object());
        if (!sleekpr::core::FileSettingsStore(m_settingsPath).save(nextSettings)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", "Unable to save settings"), 500, extraHeaders);
        }
        return jsonResponse(
            okEnvelope(sleekpr::core::PrintClientSettingsJson::toJson(
                sleekpr::core::FileSettingsStore(m_settingsPath).load())),
            200,
            extraHeaders);
    }

    if (path == "/printers" && method == "GET") {
        // 打印机发现依赖本机系统环境，必须留在 HTTP/应用边界；核心层只接收已选择的打印机名。
        return jsonResponse(okEnvelope(printersToJson(settings)), 200, extraHeaders);
    }

    return std::nullopt;
}

} // 命名空间 sleekpr::http
