#pragma once

#include "sleekpr/core/native/NativeLabelDrawingPlan.h"

#include <QByteArray>
#include <QImage>

class QPainter;

namespace sleekpr::infrastructure {

class LabelPreviewImageRenderer
{
public:
    // 使用 Qt 绘制原生图像；预览窗口和 PNG 调试输出都必须复用同一份毫米坐标计划。
    QImage renderImage(const sleekpr::core::NativeLabelDrawingPlan& plan, double dpi = 300.0) const;

    // 输出 PNG 字节仅作为调试和测试辅助，正式预览由 Qt 窗口直接显示 QImage。
    QByteArray renderPng(const sleekpr::core::NativeLabelDrawingPlan& plan, double dpi = 300.0) const;

private:
    static void drawQrCode(QPainter& painter, const sleekpr::core::NativeDrawCommand& command, double scale);
};

} // 命名空间 sleekpr::infrastructure
