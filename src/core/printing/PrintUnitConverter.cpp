#include "sleekpr/core/printing/PrintUnitConverter.h"

#include <cmath>

namespace sleekpr::core {

PrintUnitConverter::PrintUnitConverter(double dpi)
    : m_dpi(dpi)
{
}

int PrintUnitConverter::millimetersToHundredthsInch(double millimeters) const
{
    // Windows 打印纸张尺寸 API 使用百分之一英寸，必须四舍五入成整数。
    return static_cast<int>(std::lround((millimeters / 25.4) * 100.0));
}

int PrintUnitConverter::millimetersToPixels(double millimeters) const
{
    // 预览位图按 DPI 换算像素，300 DPI 下 80mm 应接近 945 像素。
    return static_cast<int>(std::lround((millimeters / 25.4) * m_dpi));
}

} // 命名空间 sleekpr::core
