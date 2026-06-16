#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QJsonObject>

namespace sleekpr::core {

class PrintClientSettingsJson
{
public:
    // 设置 JSON 是 HTTP 接口和本地文件的共同契约，集中维护可避免两边字段逐渐漂移。
    static QJsonObject toJson(const PrintClientSettings& settings);
    static PrintClientSettings fromJson(const QJsonObject& json);
};

} // 命名空间 sleekpr::core
