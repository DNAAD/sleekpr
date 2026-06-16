#pragma once

namespace sleekpr::core {

class PrintUnitConverter
{
public:
    explicit PrintUnitConverter(double dpi = 300.0);

    // Windows 打印纸张尺寸使用百分之一英寸，领域层毫米值在这里集中换算。
    int millimetersToHundredthsInch(double millimeters) const;

    // 预览图片按 DPI 转成像素，避免渲染层重复散落单位换算。
    int millimetersToPixels(double millimeters) const;

private:
    double m_dpi;
};

} // 命名空间 sleekpr::core
