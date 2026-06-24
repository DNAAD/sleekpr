#pragma once

#include "sleekpr/core/native/NativeDrawCommand.h"

#include <QSizeF>

namespace sleekpr::infrastructure {

class TextAutoFitSizer
{
public:
    static double fitPointSize(
        const sleekpr::core::NativeDrawCommand& command,
        QSizeF layoutSizePx,
        double dpiX,
        double dpiY);
    // 长时间编辑或测试前可清空字号适配缓存，避免旧测量结果影响后续诊断。
    static void clearCache();
    // 返回当前缓存条目数，用于确认重复渲染是否复用已有字号计算。
    static int cacheEntryCount();
};

} // 命名空间 sleekpr::infrastructure
