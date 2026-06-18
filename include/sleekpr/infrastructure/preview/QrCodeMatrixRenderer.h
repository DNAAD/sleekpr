#pragma once

#include "sleekpr/infrastructure/preview/QrCodeMatrix.h"

#include <QString>

namespace sleekpr::infrastructure {

class QrCodeMatrixRenderer
{
public:
    // 生成真实 QR Code 矩阵；当前支持 Version 1-4 + Q 级纠错，覆盖常见标签编号和较长业务码。
    QrCodeMatrix render(const QString& payload) const;
};

} // 命名空间 sleekpr::infrastructure
