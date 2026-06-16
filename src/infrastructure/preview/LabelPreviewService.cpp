#include "sleekpr/infrastructure/preview/LabelPreviewService.h"

#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/labels/TemplateOverrideKey.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"

namespace sleekpr::infrastructure {

QImage LabelPreviewService::renderDemoPreview(
    const sleekpr::core::PrintClientSettings& settings,
    sleekpr::core::LabelTemplateKey templateKey) const
{
    return renderPreview(PreviewLabelFactory::createDemoLabel(templateKey), settings, settings.defaultPrinter);
}

QImage LabelPreviewService::renderPreview(
    const sleekpr::core::LabelItem& item,
    const sleekpr::core::PrintClientSettings& settings) const
{
    return renderPreview(item, settings, QString{});
}

QImage LabelPreviewService::renderPreview(
    const sleekpr::core::LabelItem& item,
    const sleekpr::core::PrintClientSettings& settings,
    const QString& printerName) const
{
    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(item);
    const auto templateKey = sleekpr::core::templateOverrideKey(labelPlan.templateKey);
    const auto documentIt = settings.templateDocuments.constFind(templateKey);
    if (documentIt != settings.templateDocuments.cend()) {
        // 完整模板文档已经包含图层和元素；这里只叠加设备 profile，确保预览与真实打印使用同一份毫米坐标计划。
        const auto profile = sleekpr::core::DeviceProfileResolver::resolve(documentIt.value().deviceProfiles, printerName);
        const auto drawingPlan = sleekpr::core::TemplateDocumentRenderer().render(
            documentIt.value(),
            labelPlan,
            settings.labelOffset,
            profile);
        return LabelPreviewImageRenderer().renderImage(drawingPlan, drawingPlan.renderDpi);
    }

    const auto drawingPlan = sleekpr::core::NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        settings.labelOffset,
        settings.templateOverrides.value(templateKey),
        settings.templateElements.value(templateKey));

    return LabelPreviewImageRenderer().renderImage(drawingPlan, drawingPlan.renderDpi);
}

} // 命名空间 sleekpr::infrastructure
