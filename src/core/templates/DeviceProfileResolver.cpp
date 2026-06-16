#include "sleekpr/core/templates/DeviceProfileResolver.h"

namespace sleekpr::core {

DeviceProfile DeviceProfileResolver::resolve(const QList<DeviceProfile>& profiles, const QString& printerName)
{
    const auto normalizedPrinterName = printerName.trimmed();

    for (const auto& profile : profiles) {
        // 非空打印机名采用大小写不敏感匹配，避免系统返回大小写差异导致校准失效。
        if (!profile.printerName.trimmed().isEmpty()
            && profile.printerName.compare(normalizedPrinterName, Qt::CaseInsensitive) == 0) {
            return profile;
        }
    }

    for (const auto& profile : profiles) {
        // printerName 为空表示模板级默认 profile。
        if (profile.printerName.trimmed().isEmpty()) {
            return profile;
        }
    }

    return DeviceProfile{};
}

} // 命名空间 sleekpr::core
