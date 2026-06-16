#pragma once

namespace sleekpr::core {

enum class NativeDrawCommandType
{
    // 文本绘制命令。
    Text,

    // 二维码绘制命令。
    QrCode,

    // 矩形绘制命令，用于诊断边框等场景。
    Rectangle
};

} // 命名空间 sleekpr::core
