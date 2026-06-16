#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QString>

namespace sleekpr::core {

class SettingsEditModel
{
public:
    static void resetElement(PrintClientSettings& settings, const QString& templateKey, const QString& elementKey);
    static void resetAllElements(PrintClientSettings& settings, const QString& templateKey);
};

} // 命名空间 sleekpr::core
