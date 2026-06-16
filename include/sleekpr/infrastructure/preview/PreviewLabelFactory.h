#pragma once

#include "sleekpr/core/labels/LabelItem.h"
#include "sleekpr/core/labels/LabelTemplateKey.h"

namespace sleekpr::infrastructure {

class PreviewLabelFactory
{
public:
    // 构造本地预览窗口使用的演示标签数据，后续接入真实接口时只替换这一层数据来源。
    static sleekpr::core::LabelItem createDemoLabel(sleekpr::core::LabelTemplateKey templateKey = sleekpr::core::LabelTemplateKey::Default80x30);
};

} // 命名空间 sleekpr::infrastructure
