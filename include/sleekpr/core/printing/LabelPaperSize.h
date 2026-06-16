#pragma once

namespace sleekpr::core {

class LabelPaperSize
{
public:
    LabelPaperSize(double widthMm, double heightMm);

    // 当前业务固定使用 80mm x 30mm 标签纸，后续新增纸型应从这里扩展。
    static LabelPaperSize create80x30();

    double widthMm() const;
    double heightMm() const;
    int widthHundredthsInch() const;
    int heightHundredthsInch() const;

private:
    double m_widthMm;
    double m_heightMm;
};

} // 命名空间 sleekpr::core
