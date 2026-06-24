#include "sleekpr/http/LocalHttpRouteSupport.h"

#include "sleekpr/core/security/LocalCorsPolicy.h"
#include "sleekpr/core/security/PrivateNetworkAccessPolicy.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>

namespace sleekpr::http {

using sleekpr::core::PrivateNetworkAccessPolicy;

QJsonObject okEnvelope(const QJsonObject& data)
{
    // 响应包络保持 success/code/message/data 形状，前端无需区分 .NET 与原生客户端。
    return QJsonObject{
        {"success", true},
        {"code", "OK"},
        {"message", ""},
        {"data", data},
    };
}

QJsonObject failEnvelope(const QString& code, const QString& message)
{
    // 错误响应同样使用兼容包络，便于前端统一展示和日志记录。
    return QJsonObject{
        {"success", false},
        {"code", code},
        {"message", message},
        {"data", QJsonObject{}},
    };
}

LocalHttpResponse jsonResponse(
    const QJsonObject& payload,
    int statusCode,
    const QHash<QByteArray, QByteArray>& extraHeaders)
{
    QHash<QByteArray, QByteArray> headers = extraHeaders;
    headers.insert("Content-Type", "application/json; charset=utf-8");
    return LocalHttpResponse{
        statusCode,
        headers,
        QJsonDocument(payload).toJson(QJsonDocument::Compact),
    };
}

LocalHttpResponse emptyResponse(int statusCode, const QHash<QByteArray, QByteArray>& extraHeaders)
{
    return LocalHttpResponse{statusCode, extraHeaders, QByteArray{}};
}

LocalHttpResponse requestTooLargeResponse(const QString& message, const QHash<QByteArray, QByteArray>& extraHeaders)
{
    return jsonResponse(failEnvelope(QStringLiteral("REQUEST_TOO_LARGE"), message), 413, extraHeaders);
}

QString headerValue(const LocalHttpRequest& request, const QString& name)
{
    return request.headers.value(name.toLower()).trimmed();
}

QHash<QByteArray, QByteArray> corsHeaders(
    const LocalHttpRequest& request,
    const sleekpr::core::PrintClientSettings& settings)
{
    QHash<QByteArray, QByteArray> headers;
    const auto origin = headerValue(request, "origin");
    if (origin.isEmpty()) {
        return headers;
    }

    headers.insert("Access-Control-Allow-Origin", origin.toUtf8());
    headers.insert("Vary", "Origin");
    headers.insert("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Sleekpr-Token");
    headers.insert("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");

    if (PrivateNetworkAccessPolicy::shouldAllow(
            headerValue(request, "access-control-request-private-network"),
            origin,
            settings.allowedOrigins)) {
        headers.insert(PrivateNetworkAccessPolicy::ResponseHeaderName, "true");
    }

    return headers;
}

bool hasRejectedOrigin(const LocalHttpRequest& request, const sleekpr::core::PrintClientSettings& settings)
{
    const auto origin = headerValue(request, "origin");
    return !origin.isEmpty() && !sleekpr::core::LocalCorsPolicy::isAllowedOrigin(origin, settings.allowedOrigins);
}

QJsonValue valueFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    for (const auto* name : names) {
        if (object.contains(name)) {
            return object.value(name);
        }
    }
    // 路由层需要区分“字段不存在”和“字段存在但值为 null”，否则可选字段会被误判为非法输入。
    return QJsonValue(QJsonValue::Undefined);
}

QString stringFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    return valueFor(object, names).toString();
}

double doubleFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    const auto value = valueFor(object, names);
    if (value.isString()) {
        bool ok = false;
        const auto parsed = value.toString().toDouble(&ok);
        return ok ? parsed : 0.0;
    }
    return value.toDouble();
}

int intFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    const auto value = valueFor(object, names);
    if (value.isString()) {
        bool ok = false;
        const auto parsed = value.toString().toInt(&ok);
        return ok ? parsed : 0;
    }
    return value.toInt();
}

QString requestIdOrFallback(const QString& requestId)
{
    if (!requestId.trimmed().isEmpty()) {
        return requestId.trimmed();
    }
    return QString("print-%1").arg(QDateTime::currentMSecsSinceEpoch());
}

QJsonObject printResultJson(const QString& requestId, int total, int printed, int failed, bool executePrint)
{
    const auto pending = executePrint ? 0 : total;
    const auto status = failed > 0 ? QStringLiteral("partial_failed") : QStringLiteral("queued");
    return QJsonObject{
        {"jobId", QString("job-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))},
        {"requestId", requestId},
        {"status", status},
        {"accepted", total},
        {"total", total},
        {"printed", printed},
        {"failed", failed},
        {"pending", pending},
    };
}

QString templateLibraryDirectoryForSettings(const QString& settingsPath)
{
    // 模板库跟随 settings.json 放在同级目录，便于整套客户端配置备份和迁移。
    const QFileInfo settingsFile(settingsPath);
    return settingsFile.absoluteDir().filePath(QStringLiteral("templates"));
}

sleekpr::core::TemplateLibraryStore templateLibraryStoreForSettings(const QString& settingsPath)
{
    return sleekpr::core::TemplateLibraryStore(templateLibraryDirectoryForSettings(settingsPath));
}

QString paperSpecFilePathForSettings(const QString& settingsPath)
{
    // 纸张规格和 settings.json 同级，便于把整套客户端配置一起备份或迁移。
    const QFileInfo settingsFile(settingsPath);
    return settingsFile.absoluteDir().filePath(QStringLiteral("paper-specs.json"));
}

sleekpr::core::PaperSpecStore paperSpecStoreForSettings(const QString& settingsPath)
{
    return sleekpr::core::PaperSpecStore(paperSpecFilePathForSettings(settingsPath));
}

QString fieldPresetFilePathForSettings(const QString& settingsPath)
{
    // 字段预设和 settings.json 同级，便于整套客户端配置一起备份或迁移。
    const QFileInfo settingsFile(settingsPath);
    return settingsFile.absoluteDir().filePath(QStringLiteral("field-presets.json"));
}

sleekpr::core::FieldPresetStore fieldPresetStoreForSettings(const QString& settingsPath)
{
    return sleekpr::core::FieldPresetStore(fieldPresetFilePathForSettings(settingsPath));
}

QString templateIdFromPath(const QString& path)
{
    // 模板 id 直接映射到文件名，路由层先挡掉路径分隔符，避免把路径语义泄漏给存储层。
    const auto prefix = QStringLiteral("/templates/");
    if (!path.startsWith(prefix)) {
        return {};
    }

    const auto templateId = path.mid(prefix.size()).trimmed();
    if (templateId.isEmpty() || templateId.contains(QLatin1Char('/')) || templateId.contains(QLatin1Char('\\'))) {
        return {};
    }
    return templateId;
}

QString paperSpecIdFromPath(const QString& path)
{
    // 路由层只处理 URL 形状，id 的完整安全校验由 PaperSpecStore 统一负责。
    const auto prefix = QStringLiteral("/paper-specs/");
    if (!path.startsWith(prefix)) {
        return {};
    }

    const auto paperSpecId = path.mid(prefix.size()).trimmed();
    if (paperSpecId.isEmpty() || paperSpecId.contains(QLatin1Char('/')) || paperSpecId.contains(QLatin1Char('\\'))) {
        return {};
    }
    return paperSpecId;
}

QString fieldPresetIdFromPath(const QString& path)
{
    // 路由层只处理 URL 形状，完整安全校验由 FieldPresetStore 统一负责。
    const auto prefix = QStringLiteral("/field-presets/");
    if (!path.startsWith(prefix)) {
        return {};
    }

    const auto presetId = path.mid(prefix.size()).trimmed();
    if (presetId.isEmpty() || presetId.contains(QLatin1Char('/')) || presetId.contains(QLatin1Char('\\'))) {
        return {};
    }
    return presetId;
}

} // 命名空间 sleekpr::http
