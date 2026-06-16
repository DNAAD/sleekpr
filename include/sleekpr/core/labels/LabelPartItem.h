#pragma once

#include <QString>

namespace sleekpr::core {

struct LabelPartItem
{
    // 明细品类名称。
    QString categoryName;

    // 明细重量，单位为克。
    double partWeight = 0.0;
};

} // 命名空间 sleekpr::core
