#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

namespace sleekpr::core {

class PrinterSelectionResolver
{
public:
    // 统一打印机选择策略，当前迁移阶段只信任本机设置中的默认打印机。
    QString resolve(const PrintClientSettings& settings, const QString& requestPrinterName = {}) const;
};

} // 命名空间 sleekpr::core
