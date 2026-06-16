#pragma once

#include <QString>
#include <QStringList>

namespace sleekpr::core {

class PrivateNetworkAccessPolicy
{
public:
    static constexpr auto RequestHeaderName = "Access-Control-Request-Private-Network";
    static constexpr auto ResponseHeaderName = "Access-Control-Allow-Private-Network";

    // 只有可信 Origin 发起的私有网络预检请求才允许返回 PNA 放行响应头。
    static bool shouldAllow(
        const QString& privateNetworkRequestHeader,
        const QString& origin,
        const QStringList& configuredOrigins);
};

} // 命名空间 sleekpr::core
