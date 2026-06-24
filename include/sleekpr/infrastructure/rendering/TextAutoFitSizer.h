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
};

} // 命名空间 sleekpr::infrastructure
