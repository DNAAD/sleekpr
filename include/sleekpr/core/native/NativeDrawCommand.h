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

    // 模板设计器文本需要按元素宽度自动换行，超出元素高度的内容由绘制后端裁切。
    bool wrapText = false;

    // 文本自适应字号只影响 Qt 原生绘制阶段，不改变模板中保存的基础字号。
    bool autoFitFont = false;
    double autoFitMinFontSizePt = 0.0;
    double autoFitMaxFontSizePt = 0.0;

    // 单元格背景色给 Qt 原生预览和打印后端使用；空值表示透明。
    QString backgroundColor;

    // 单元格文字色给 Qt 原生预览和打印后端使用；空值表示使用默认画笔。
    QString textColor;

    // 单元格边框宽度使用毫米单位，0 表示沿用当前后端默认线宽。
    double borderWidthMm = 0.0;
};

} // namespace sleekpr::core
