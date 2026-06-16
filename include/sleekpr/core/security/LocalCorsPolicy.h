#pragma once

#include <QString>
#include <QStringList>

namespace sleekpr::core {

class LocalCorsPolicy
{
public:
    // 保持 dotnet-print 的信任模型：回环前端始终允许，已知生产主机内置允许，其他来源由操作员配置。
    static bool isAllowedOrigin(const QString& origin, const QStringList& configuredOrigins = {});
};

} // 命名空间 sleekpr::core
