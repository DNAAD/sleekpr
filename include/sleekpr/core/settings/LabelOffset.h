#pragma once

namespace sleekpr::core {

struct LabelOffset
{
    // 整张标签的横向校准偏移，单位为毫米。
    double x = 0.0;

    // 整张标签的纵向校准偏移，单位为毫米。
    double y = 0.0;
};

} // 命名空间 sleekpr::core
