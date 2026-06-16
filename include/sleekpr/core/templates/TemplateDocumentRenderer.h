#pragma once

#include "sleekpr/core/labels/LabelRenderPlan.h"
#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/settings/LabelOffset.h"
#include "sleekpr/core/templates/TemplateDocument.h"

namespace sleekpr::core {

class TemplateDocumentRenderer
{
public:
    NativeLabelDrawingPlan render(
        const TemplateDocument& document,
        const LabelRenderPlan& labelPlan,
        const LabelOffset& labelOffset,
        const DeviceProfile& profile) const;
};

} // 命名空间 sleekpr::core
