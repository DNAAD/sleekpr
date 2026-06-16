#include "sleekpr/infrastructure/preview/LabelPreviewService.h"

#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/labels/TemplateOverrideKey.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"

namespace sleekpr::infrastructure {

QImage LabelPreviewService::renderDemoPreview(
    const sleekpr::core::PrintClientSettings& settings,
    sleekpr::core::LabelTemplateKey templateKey) const
{
    return renderPreview(PreviewLabelFactory::createDemoLabel(templateKey), settings);
}

QImage LabelPreviewService::renderPreview(
    const sleekpr::core::LabelItem& item,
    const sleekpr::core::PrintClientSettings& settings) const
{
    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(item);
    const auto drawingPlan = sleekpr::core::NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        settings.labelOffset,
        settings.templateOverrides.value(sleekpr::core::templateOverrideKey(labelPlan.templateKey)),
        settings.templateElements.value(sleekpr::core::templateOverrideKey(labelPlan.templateKey)));

    return LabelPreviewImageRenderer().renderImage(drawingPlan);
}

} // 命名空间 sleekpr::infrastructure
