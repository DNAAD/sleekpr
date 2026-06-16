#include "sleekpr/core/printing/LabelPaperSize.h"

#include "sleekpr/core/printing/PrintUnitConverter.h"

namespace sleekpr::core {

LabelPaperSize::LabelPaperSize(double widthMm, double heightMm)
    : m_widthMm(widthMm)
    , m_heightMm(heightMm)
{
}

LabelPaperSize LabelPaperSize::create80x30()
{
    // 当前标签打印链路以 80mm x 30mm 为主规格。
    return LabelPaperSize(80.0, 30.0);
}

double LabelPaperSize::widthMm() const
{
    return m_widthMm;
}

double LabelPaperSize::heightMm() const
{
    return m_heightMm;
}

int LabelPaperSize::widthHundredthsInch() const
{
    return PrintUnitConverter().millimetersToHundredthsInch(m_widthMm);
}

int LabelPaperSize::heightHundredthsInch() const
{
    return PrintUnitConverter().millimetersToHundredthsInch(m_heightMm);
}

} // 命名空间 sleekpr::core
