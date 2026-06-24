#include "sleekpr/http/TemplateRequestPlanner.h"

#include "sleekpr/core/labels/TemplateOverrideKey.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/templates/DefaultPaperSpecs.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateDocumentValidator.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"

#include <QSizeF>

namespace sleekpr::http {

using sleekpr::core::DeviceProfile;
using sleekpr::core::DeviceProfileResolver;
using sleekpr::core::FieldPreset;
using sleekpr::core::LabelOffset;
using sleekpr::core::LabelPaperSize;
using sleekpr::core::LabelRenderPlan;
using sleekpr::core::NativeLabelDrawingPlanner;
using sleekpr::core::NativePrintDrawingPlan;
using sleekpr::core::PaperSpec;
using sleekpr::core::PrintClientSettings;
using sleekpr::core::TemplateDocument;
using sleekpr::core::TemplateDocumentRenderer;
using sleekpr::core::TemplateDocumentValidator;
using sleekpr::core::TemplateRenderContext;
using sleekpr::core::TemplateRenderContextBuilder;
using sleekpr::core::templateOverrideKey;

namespace {

std::optional<TemplateDocument> findTemplateDocument(
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& requestedTemplate)
{
    // 先查旧 settings 内嵌模板，再查独立模板库，保证历史配置和新模板库可以并行过渡。
    const auto templateName = requestedTemplate.trimmed().isEmpty()
        ? QStringLiteral("default")
        : requestedTemplate.trimmed();

    const auto directIt = settings.templateDocuments.constFind(templateName);
    if (directIt != settings.templateDocuments.cend()) {
        return directIt.value();
    }

    for (auto it = settings.templateDocuments.cbegin(); it != settings.templateDocuments.cend(); ++it) {
        const auto& document = it.value();
        if (document.id.compare(templateName, Qt::CaseInsensitive) == 0
            || document.templateKey.compare(templateName, Qt::CaseInsensitive) == 0) {
            return document;
        }
    }

    const auto store = templateLibraryStoreForSettings(settingsPath);
    QString errorMessage;
    auto document = store.loadTemplate(templateName, &errorMessage);
    if (document.has_value()) {
        return document;
    }

    for (const auto& templateId : store.templateIds()) {
        document = store.loadTemplate(templateId, &errorMessage);
        if (!document.has_value()) {
            continue;
        }
        if (document->id.compare(templateName, Qt::CaseInsensitive) == 0
            || document->templateKey.compare(templateName, Qt::CaseInsensitive) == 0) {
            return document;
        }
    }

    return std::nullopt;
}

std::optional<FieldPreset> findFieldPresetForRequest(
    const QString& settingsPath,
    const QJsonObject& requestRoot,
    QString* errorMessage)
{
    const auto presetId = stringFor(requestRoot, {"fieldPresetId", "FieldPresetId", "presetId", "PresetId"}).trimmed();
    if (presetId.isEmpty()) {
        return std::nullopt;
    }

    auto preset = fieldPresetStoreForSettings(settingsPath).loadPreset(presetId, errorMessage);
    return preset;
}

LabelRenderPlan templateLabelPlan(const TemplateDocument& document)
{
    LabelRenderPlan plan;
    plan.templateKey = document.templateKey.compare(QStringLiteral("silver"), Qt::CaseInsensitive) == 0
        ? sleekpr::core::LabelTemplateKey::Silver80x30
        : sleekpr::core::LabelTemplateKey::Default80x30;
    return plan;
}

std::optional<PaperSpec> paperSpecForTemplate(const QString& settingsPath, const TemplateDocument& document)
{
    auto paperSpecId = document.paperSpecId.trimmed();
    if (paperSpecId.isEmpty()) {
        paperSpecId = QStringLiteral("label-80x30");
    }

    const auto storedSpec = paperSpecStoreForSettings(settingsPath).loadPaperSpec(paperSpecId);
    if (storedSpec.has_value()) {
        return storedSpec;
    }

    return sleekpr::core::builtInPaperSpec(paperSpecId);
}

LabelRenderPlan labelPlanWithPaperSpec(LabelRenderPlan labelPlan, const std::optional<PaperSpec>& paperSpec)
{
    if (!paperSpec.has_value()) {
        return labelPlan;
    }

    // 纸张尺寸是打印兼容契约的一部分：HTTP 打印端必须按模板引用的 paperSpecId 输出真实页面尺寸。
    labelPlan.paperSize = LabelPaperSize(paperSpec->widthMm, paperSpec->heightMm);
    return labelPlan;
}

LabelOffset labelOffsetWithPaperMargins(LabelOffset labelOffset, const std::optional<PaperSpec>& paperSpec)
{
    if (!paperSpec.has_value()) {
        return labelOffset;
    }

    // 领域坐标仍以毫米表示；纸张左/上边距在打印边界合入整体偏移，避免改写模板内每个元素坐标。
    labelOffset.x += paperSpec->marginLeftMm;
    labelOffset.y += paperSpec->marginTopMm;
    return labelOffset;
}

DeviceProfile deviceProfileWithPaperDpi(
    DeviceProfile profile,
    const TemplateDocument& document,
    const std::optional<PaperSpec>& paperSpec)
{
    if (paperSpec.has_value() && (document.deviceProfiles.isEmpty() || profile.printerName.trimmed().isEmpty())) {
        // 没有专用打印机 profile 时使用纸张规格默认 DPI；空打印机名的通用 profile 不应把 203DPI 标签机改回 300DPI。
        profile.dpi = paperSpec->defaultDpi;
    }
    return profile;
}

std::optional<NativePrintDrawingPlan> renderTemplatePrintPlan(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& printerName,
    const TemplateRenderContext& context,
    QString* errorMessage)
{
    const auto paperSpec = paperSpecForTemplate(settingsPath, document);
    const auto renderer = TemplateDocumentRenderer();
    const auto labelPlanWithSpec = labelPlanWithPaperSpec(labelPlan, paperSpec);
    const auto labelOffset = labelOffsetWithPaperMargins(settings.labelOffset, paperSpec);
    const auto deviceProfile = deviceProfileWithPaperDpi(
        DeviceProfileResolver::resolve(document.deviceProfiles, printerName),
        document,
        paperSpec);
    if (errorMessage == nullptr) {
        return renderer.renderPrint(document, labelPlanWithSpec, labelOffset, deviceProfile, context);
    }

    const auto renderResult = renderer.tryRenderPrint(
        document,
        labelPlanWithSpec,
        labelOffset,
        deviceProfile,
        context);
    if (!renderResult.success()) {
        if (errorMessage != nullptr) {
            *errorMessage = renderResult.errorMessage;
        }
        return std::nullopt;
    }
    return renderResult.plan;
}

QString missingRequiredFieldsMessage(const TemplateRenderContext& context)
{
    return QString::fromUtf8("缺少必填字段：%1").arg(context.missingRequiredFieldNames.join(QString::fromUtf8("、")));
}

QString templateValidationMessage(const sleekpr::core::TemplateValidationResult& validation)
{
    const auto messages = validation.errorMessages();
    return messages.isEmpty()
        ? QString::fromUtf8("模板校验失败")
        : messages.join(QString::fromUtf8("；"));
}

QSizeF paperSizeForTemplateValidation(const QString& settingsPath, const TemplateDocument& document)
{
    const auto paperSpec = paperSpecForTemplate(settingsPath, document);
    if (paperSpec.has_value() && paperSpec->widthMm > 0.0 && paperSpec->heightMm > 0.0) {
        return QSizeF(paperSpec->widthMm, paperSpec->heightMm);
    }

    const auto fallbackPlan = templateLabelPlan(document);
    return QSizeF(fallbackPlan.paperSize.widthMm(), fallbackPlan.paperSize.heightMm());
}

QString responseTemplateKey(const TemplateDocument& document, const QJsonObject& requestRoot)
{
    auto key = document.templateKey.trimmed();
    if (!key.isEmpty()) {
        return key;
    }

    key = stringFor(requestRoot, {"templateKey", "TemplateKey", "templateId", "TemplateId"}).trimmed();
    return key.isEmpty() ? QStringLiteral("default") : key;
}

} // 匿名命名空间

std::optional<TemplateRequestPlan> buildTemplateRequestPlan(
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QJsonObject& root,
    const QString& selectedPrinter,
    QString* errorCode,
    QString* errorMessage)
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

    const auto templateDocument = findTemplateDocument(
        settings,
        settingsPath,
        stringFor(root, {"templateKey", "TemplateKey", "templateId", "TemplateId"}));
    if (!templateDocument.has_value()) {
        return fail("TEMPLATE_NOT_FOUND", QString::fromUtf8("未找到指定模板。"));
    }

    const auto valuesValue = valueFor(root, {"values", "Values", "fieldValues", "FieldValues"});
    if (!valuesValue.isUndefined() && !valuesValue.isObject()) {
        return fail("BAD_REQUEST", QString::fromUtf8("字段值 values 必须是对象。"));
    }

    QString presetErrorMessage;
    const auto fieldPreset = findFieldPresetForRequest(settingsPath, root, &presetErrorMessage);
    if (!presetErrorMessage.isEmpty() && !fieldPreset.has_value()) {
        return fail("FIELD_PRESET_NOT_FOUND", presetErrorMessage);
    }

    const auto context = TemplateRenderContextBuilder::build(
        templateDocument->fieldSchema,
        fieldPreset.has_value() ? fieldPreset->values : QJsonObject{},
        valuesValue.toObject());
    if (context.hasMissingRequiredFields()) {
        return fail("BAD_REQUEST", missingRequiredFieldsMessage(context));
    }

    const auto templateValidation = TemplateDocumentValidator().validate(
        templateDocument.value(),
        paperSizeForTemplateValidation(settingsPath, templateDocument.value()),
        context.values);
    if (templateValidation.hasErrors()) {
        return fail("TEMPLATE_VALIDATION_FAILED", templateValidationMessage(templateValidation));
    }

    TemplateRequestPlan result;
    result.requestId = requestIdOrFallback(stringFor(root, {"requestId", "RequestId"}));
    result.templateKey = responseTemplateKey(templateDocument.value(), root);
    QString renderErrorMessage;
    const auto printPlan = renderTemplatePrintPlan(
        templateDocument.value(),
        templateLabelPlan(templateDocument.value()),
        settings,
        settingsPath,
        selectedPrinter,
        context,
        &renderErrorMessage);
    if (!printPlan.has_value()) {
        return fail("TEMPLATE_RENDER_FAILED", renderErrorMessage);
    }
    result.printPlan = printPlan.value();
    return result;
}

NativePrintDrawingPlan createPrintPlan(
    const LabelRenderPlan& labelPlan,
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& printerName)
{
    const auto templateKey = templateOverrideKey(labelPlan.templateKey);
    const auto documentIt = settings.templateDocuments.constFind(templateKey);
    if (documentIt != settings.templateDocuments.cend()) {
        const auto printPlan = renderTemplatePrintPlan(
            documentIt.value(),
            labelPlan,
            settings,
            settingsPath,
            printerName,
            TemplateRenderContext{},
            nullptr);
        if (printPlan.has_value()) {
            return printPlan.value();
        }
    }

    return NativePrintDrawingPlan::fromSinglePage(NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        settings.labelOffset,
        settings.templateOverrides.value(templateKey),
        settings.templateElements.value(templateKey)));
}

} // 命名空间 sleekpr::http
