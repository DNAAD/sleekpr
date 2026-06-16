#include "sleekpr/core/security/LocalCorsPolicy.h"

#include <QSet>
#include <QUrl>

namespace sleekpr::core {

namespace {

bool isLoopbackHost(const QString& host)
{
    const auto normalized = host.toLower();
    // 浏览器端调试和本机前端都应允许访问本地打印服务。
    return normalized == "localhost"
        || normalized == "127.0.0.1"
        || normalized == "::1";
}

} // 匿名命名空间

bool LocalCorsPolicy::isAllowedOrigin(const QString& origin, const QStringList& configuredOrigins)
{
    const QUrl url(origin);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        return false;
    }

    if (isLoopbackHost(url.host())) {
        return true;
    }

    // 该生产前端 IP 已在 .NET 客户端内置，迁移期间继续信任以保持兼容。
    if (url.host() == "114.132.160.27") {
        return true;
    }

    return configuredOrigins.contains(origin, Qt::CaseInsensitive);
}

} // 命名空间 sleekpr::core
