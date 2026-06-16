#pragma once

#include "sleekpr/core/native/NativeDrawCommandType.h"

#include <QString>

namespace sleekpr::core {

struct NativeDrawCommand
{
    // 命令类型决定渲染后端如何解释后续字段。
    NativeDrawCommandType type = NativeDrawCommandType::Text;

    // 绘制区域左上角坐标，单位为毫米。
    double x = 0.0;
    double y = 0.0;

    // 绘制区域尺寸，单位为毫米。
    double width = 0.0;
    double height = 0.0;

    // 文本或二维码载荷。
    QString text;
    double fontSizePt = 0.0;
    bool bold = false;
    double rotationDegrees = 0.0;
    int maxLines = 0;
    bool ellipsis = false;

    // 模板元素 key，用于设置页覆盖位置、字号和加粗状态。
    QString elementKey;
};

} // 命名空间 sleekpr::core
