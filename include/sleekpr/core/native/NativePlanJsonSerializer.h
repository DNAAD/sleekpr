#pragma once

#include "sleekpr/core/native/NativeLabelDrawingPlan.h"

#include <QJsonObject>

namespace sleekpr::core {

class NativePlanJsonSerializer
{
public:
    // 将毫米坐标绘制计划导出为 JSON，供后续诊断工具和自动化比对查看。
    QJsonObject toJson(const NativeLabelDrawingPlan& plan) const;

private:
    static QString commandTypeName(NativeDrawCommandType type);
};

} // 命名空间 sleekpr::core
