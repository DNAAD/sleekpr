#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QString>

namespace sleekpr::core {

class FileSettingsStore
{
public:
    explicit FileSettingsStore(QString settingsPath);

    // 读取本机操作员控制的客户端设置；文件缺失或不可读时回退到安全默认值，保持与 dotnet-print 一致。
    PrintClientSettings load() const;

    // 使用 camelCase JSON 持久化设置，确保迁移期间可以复用既有配置文件。
    bool save(const PrintClientSettings& settings) const;

private:
    QString m_settingsPath;
};

} // 命名空间 sleekpr::core
