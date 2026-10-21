#include "sleekpr/http/TemplateRequestPlanner.h"

#include "sleekpr/http/LocalHttpRouteSupport.h"
#include "sleekpr/infrastructure/printing/TemplatePrintService.h"

namespace sleekpr::http {

using sleekpr::core::LabelRenderPlan;
using sleekpr::core::NativePrintDrawingPlan;
using sleekpr::core::PrintClientSettings;
using sleekpr::infrastructure::TemplatePrintInput;
using sleekpr::infrastructure::TemplatePrintService;

std::optional<TemplateRequestPlan> buildTemplateRequestPlan(
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QJsonObject& root,
    const QString& selectedPrinter,
    QString* errorCode,
    QString* errorMessage,
    core::PreviewPlanningBudget* budget)
{
    const auto fail = [errorCode, errorMessage](const QString& code, const QString& message) {
        if (errorCode != nullptr) {
            *errorCode = code;
        }
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return std::optional<TemplateRequestPlan>{};
    };

    TemplatePrintInput input;
    input.templateId = stringFor(root, {"templateKey", "TemplateKey", "templateId", "TemplateId"});
    input.fieldPresetId = stringFor(root, {"fieldPresetId", "FieldPresetId", "presetId", "PresetId"});
    input.printerName = selectedPrinter;
    const TemplatePrintService service(settingsPath);
    const auto valuesValue = valueFor(root, {"values", "Values", "fieldValues", "FieldValues"});
    if (!valuesValue.isUndefined() && !valuesValue.isObject()) {
        // 保留旧接口的错误优先级：模板不存在先报模板错误，再报告 values 的 JSON 类型。
        if (!service.findTemplate(settings, input.templateId).has_value()) {
            return fail("TEMPLATE_NOT_FOUND", QString::fromUtf8("未找到指定模板。"));
        }
        return fail("BAD_REQUEST", QString::fromUtf8("字段值 values 必须是对象。"));
    }
    input.values = valuesValue.toObject();

    const auto prepared = service.prepare(settings, input, errorCode, errorMessage, nullptr, budget);
    if (!prepared.has_value()) {
        return std::nullopt;
    }

    // 请求标识与 JSON 别名属于传输契约，服务只接收类型化的打印输入。
    TemplateRequestPlan result;
    result.requestId = requestIdOrFallback(stringFor(root, {"requestId", "RequestId"}));
    result.templateKey = prepared->templateKey;
    result.printPlan = prepared->printPlan;
    return result;
}

NativePrintDrawingPlan createPrintPlan(
    const LabelRenderPlan& labelPlan,
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& printerName)
{
    return TemplatePrintService(settingsPath).createLegacyPlan(labelPlan, settings, printerName);
}

} // 命名空间 sleekpr::http
