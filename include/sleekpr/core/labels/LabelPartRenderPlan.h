#pragma once

#include <QString>

namespace sleekpr::core {

struct LabelPartRenderPlan
{
    // 已清洗的明细品类名称。
    QString categoryName;

    // 已格式化成两位小数的明细重量文本。
    QString partWeightText;
};

} // 命名空间 sleekpr::core
