#include "sleekpr/core/settings/PrinterSelectionResolver.h"

namespace sleekpr::core {

QString PrinterSelectionResolver::resolve(const PrintClientSettings& settings, const QString&) const
{
    // 当前 .NET 客户端有意忽略请求级打印机名，确保本机操作员通过设置控制实际设备。
    return settings.defaultPrinter.trimmed();
}

} // 命名空间 sleekpr::core
