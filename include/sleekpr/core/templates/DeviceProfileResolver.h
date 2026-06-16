#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

class DeviceProfileResolver
{
public:
    // 优先选择打印机专属 profile，找不到时回退到 printerName 为空的默认 profile。
    static DeviceProfile resolve(const QList<DeviceProfile>& profiles, const QString& printerName);
};

} // 命名空间 sleekpr::core
