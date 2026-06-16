#pragma once

#include <optional>

namespace sleekpr::core {

struct TemplateElementOverride
{
    // 单个模板元素的可选横向覆盖坐标，单位为毫米。
    std::optional<double> x;

    // 单个模板元素的可选纵向覆盖坐标，单位为毫米。
    std::optional<double> y;

    // 单个模板元素的可选字号覆盖，单位为 pt。
    std::optional<double> fontSizePt;

    // 单个模板元素的可选加粗覆盖。
    std::optional<bool> bold;
};

} // 命名空间 sleekpr::core
