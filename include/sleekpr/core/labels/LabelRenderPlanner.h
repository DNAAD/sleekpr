#pragma once

#include "sleekpr/core/labels/LabelItem.h"
#include "sleekpr/core/labels/LabelRenderPlan.h"

namespace sleekpr::core {

class LabelRenderPlanner
{
public:
    // 将接口传入的原始标签数据整理成可展示文本；这里包含业务文案和字段兜底规则，但不包含绘制坐标。
    LabelRenderPlan createPlan(const LabelItem& item) const;

private:
    static QString formatWeight(double value);
    static QString createStandardText(LabelTemplateKey templateKey, const QString& purity);
    static QString createAdditionalPriceText(double additionalPrice);
    static QList<LabelPartRenderPlan> createParts(const LabelItem& item);
    static QString createFooterText(const LabelItem& item);
};

} // 命名空间 sleekpr::core
