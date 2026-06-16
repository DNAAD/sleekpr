#include "sleekpr/core/security/PrivateNetworkAccessPolicy.h"

#include "sleekpr/core/security/LocalCorsPolicy.h"

namespace sleekpr::core {

bool PrivateNetworkAccessPolicy::shouldAllow(
    const QString& privateNetworkRequestHeader,
    const QString& origin,
    const QStringList& configuredOrigins)
{
    // PNA 响应头只在浏览器明确发起私有网络预检且 Origin 可信时返回。
    return privateNetworkRequestHeader.compare("true", Qt::CaseInsensitive) == 0
        && LocalCorsPolicy::isAllowedOrigin(origin, configuredOrigins);
}

} // 命名空间 sleekpr::core
