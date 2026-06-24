#pragma once

#include "sleekpr/core/labels/LabelRenderPlan.h"
#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/settings/LabelOffset.h"
#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QString>

namespace sleekpr::core {

struct TemplatePrintRenderResult
{
    // 正式打印和预览需要拿到渲染错误，避免表格内容失败时仍返回成功。
    NativePrintDrawingPlan plan;
    QString errorMessage;

    bool success() const
    {
        return errorMessage.isEmpty();
    }
};

class TemplateDocumentRenderer
{
public:
    NativeLabelDrawingPlan render(
        const TemplateDocument& document,
        const LabelRenderPlan& labelPlan,
        const LabelOffset& labelOffset,
        const DeviceProfile& profile) const;

    NativeLabelDrawingPlan render(
        const TemplateDocument& document,
        const LabelRenderPlan& labelPlan,
        const LabelOffset& labelOffset,
        const DeviceProfile& profile,
        const TemplateRenderContext& context) const;

    NativePrintDrawingPlan renderPrint(
        const TemplateDocument& document,
        const LabelRenderPlan& labelPlan,
        const LabelOffset& labelOffset,
        const DeviceProfile& profile,
        const TemplateRenderContext& context) const;

    TemplatePrintRenderResult tryRenderPrint(
        const TemplateDocument& document,
        const LabelRenderPlan& labelPlan,
        const LabelOffset& labelOffset,
        const DeviceProfile& profile,
        const TemplateRenderContext& context) const;
};

} // 命名空间 sleekpr::core
