#include "sleekpr/infrastructure/printing/TemplatePrintService.h"

#include "sleekpr/core/labels/TemplateOverrideKey.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/templates/DefaultPaperSpecs.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateDocumentValidator.h"
#include "sleekpr/core/templates/TemplateInputSchema.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
#include <QSizeF>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace sleekpr::infrastructure {

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
using sleekpr::core::TemplateLibraryStore;
using sleekpr::core::TemplateRenderContext;
using sleekpr::core::TemplateRenderContextBuilder;
using sleekpr::core::templateOverrideKey;

namespace {

QString configurationPath(const QString& settingsPath, const QString& name)
{
    // 与原 HTTP 路径约定一致，模板库、纸张和字段预设始终跟随 settings 所在目录。
    return QFileInfo(settingsPath).absoluteDir().filePath(name);
}

TemplateLibraryStore templateLibraryStoreForSettings(const QString& settingsPath)
{
    return TemplateLibraryStore(configurationPath(settingsPath, QStringLiteral("templates")));
}

core::PaperSpecStore paperSpecStoreForSettings(const QString& settingsPath)
{
    return core::PaperSpecStore(configurationPath(settingsPath, QStringLiteral("paper-specs.json")));
}

core::FieldPresetStore fieldPresetStoreForSettings(const QString& settingsPath)
{
    return core::FieldPresetStore(configurationPath(settingsPath, QStringLiteral("field-presets.json")));
}

std::optional<TemplateDocument> findLegacyTemplate(
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& templateName)
{
    // 保持旧查找顺序：settings 的精确 key、内嵌别名、库文件 id，最后才是库内别名。
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

    // 纸张尺寸是打印兼容契约的一部分，必须保留模板引用的真实页面尺寸。
    labelPlan.paperSize = LabelPaperSize(paperSpec->widthMm, paperSpec->heightMm);
    return labelPlan;
}

LabelOffset labelOffsetWithPaperMargins(LabelOffset labelOffset, const std::optional<PaperSpec>& paperSpec)
{
    if (!paperSpec.has_value()) {
        return labelOffset;
    }

    // 领域坐标仍以毫米表示，纸张左上边距只在打印边界合入全局偏移。
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
        // 专属 profile 保留设备 DPI，空打印机名的通用 profile 则沿用纸张默认 DPI。
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
        // 旧 tag 路径继续使用兼容渲染，不新增必填校验或正式模板接口的错误阻断。
        return renderer.renderPrint(document, labelPlanWithSpec, labelOffset, deviceProfile, context);
    }

    const auto renderResult = renderer.tryRenderPrint(
        document,
        labelPlanWithSpec,
        labelOffset,
        deviceProfile,
        context);
    if (!renderResult.success()) {
        *errorMessage = renderResult.errorMessage.isEmpty()
            ? QString::fromUtf8("表格渲染失败")
            : renderResult.errorMessage;
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

QString responseTemplateKey(const TemplateDocument& document, const QString& requestedTemplate)
{
    auto key = document.templateKey.trimmed();
    if (!key.isEmpty()) {
        return key;
    }

    key = requestedTemplate.trimmed();
    return key.isEmpty() ? QStringLiteral("default") : key;
}

} // 匿名命名空间

TemplatePrintService::TemplatePrintService(QString settingsPath)
    : m_settingsPath(std::move(settingsPath))
{
}

std::optional<TemplatePrintSnapshot> TemplatePrintService::resolveSnapshot(const PrintClientSettings& settings,
    const TemplatePrintInput& input, QString* errorCode, QString* errorMessage) const
{
    if (errorCode) errorCode->clear();
    if (errorMessage) errorMessage->clear();
    const auto fail = [errorCode, errorMessage](const QString& code, const QString& message) -> std::optional<TemplatePrintSnapshot> {
        if (errorCode) *errorCode = code;
        if (errorMessage) *errorMessage = message;
        return std::nullopt;
    };
    const auto document = findTemplate(settings, input.templateId);
    if (!document) return fail(QStringLiteral("TEMPLATE_NOT_FOUND"), QString::fromUtf8("未找到指定模板。"));
    QString presetError;
    std::optional<FieldPreset> preset;
    if (!input.fieldPresetId.trimmed().isEmpty()) {
        preset = fieldPresetStoreForSettings(m_settingsPath).loadPreset(input.fieldPresetId.trimmed(), &presetError);
        if (!preset) return fail(QStringLiteral("FIELD_PRESET_NOT_FOUND"), presetError);
    }
    const auto paper = paperSpecForTemplate(m_settingsPath, *document);
    TemplatePrintSnapshot result;
    result.templateKey = responseTemplateKey(*document, input.templateId);
    result.document = *document;
    result.labelPlan = labelPlanWithPaperSpec(templateLabelPlan(*document), paper);
    result.labelOffset = labelOffsetWithPaperMargins(settings.labelOffset, paper);
    result.deviceProfile = deviceProfileWithPaperDpi(DeviceProfileResolver::resolve(document->deviceProfiles, input.printerName), *document, paper);
    // 只在此处合并一次真实输入与预设，后台阶段不能重新读盘或注入设计期样例。
    result.context = TemplateRenderContextBuilder::build(document->fieldSchema, preset ? preset->values : QJsonObject{}, input.values);
    result.requireCompleteValues = input.requireCompleteValues;
    return result;
}

QList<TemplatePrintChoice> TemplatePrintService::templates(
    const PrintClientSettings& settings,
    QString* errorMessage) const
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return QList<TemplatePrintChoice>{};
    };

    QList<TemplatePrintChoice> result;
    QSet<QString> legacyNames;
    const auto appendChoice = [&result, &legacyNames](
                                  const QString& source,
                                  const QString& key,
                                  const TemplateDocument& document) {
        legacyNames.insert(key.trimmed().toCaseFolded());
        legacyNames.insert(document.id.trimmed().toCaseFolded());
        legacyNames.insert(document.templateKey.trimmed().toCaseFolded());
        const auto encodedKey = QString::fromLatin1(QUrl::toPercentEncoding(key));
        result.append(TemplatePrintChoice{source + encodedKey, document.name.trimmed(), document});
    };

    // 不依赖 QHash 的遍历顺序，固定先列内嵌模板，再按文件 id 列模板库。
    auto embeddedKeys = settings.templateDocuments.keys();
    std::sort(embeddedKeys.begin(), embeddedKeys.end());
    for (const auto& key : embeddedKeys) {
        appendChoice(QStringLiteral("@settings:"), key, settings.templateDocuments.value(key));
    }

    const auto libraryPath = configurationPath(m_settingsPath, QStringLiteral("templates"));
    const QFileInfo libraryInfo(libraryPath);
    if (libraryInfo.exists() && (!libraryInfo.isDir() || !libraryInfo.isReadable())) {
        return fail(QStringLiteral("读取模板目录失败：%1").arg(libraryPath));
    }
    const auto store = templateLibraryStoreForSettings(m_settingsPath);
    for (const auto& templateId : store.templateIds()) {
        QString loadError;
        const auto document = store.loadTemplate(templateId, &loadError);
        if (!document.has_value()) {
            // 不把损坏模板悄悄当成空目录，调用方应阻断依赖不完整列表的操作。
            return fail(QStringLiteral("读取模板 %1 失败：%2").arg(templateId, loadError));
        }
        appendChoice(QStringLiteral("@library:"), templateId, document.value());
    }

    for (auto& choice : result) {
        // 选择 key 避开所有旧别名；即使旧配置使用相同前缀，也保留旧接口的查找优先级。
        while (legacyNames.contains(choice.key.toCaseFolded())) {
            choice.key.prepend(QLatin1Char('@'));
        }
        const auto name = choice.title.isEmpty() ? choice.document.id : choice.title;
        choice.title = QStringLiteral("%1 [%2]").arg(name, choice.key);
    }
    return result;
}

std::optional<TemplateDocument> TemplatePrintService::findTemplate(
    const PrintClientSettings& settings,
    const QString& templateId) const
{
    const auto templateName = templateId.trimmed().isEmpty() ? QStringLiteral("default") : templateId.trimmed();
    const auto legacyDocument = findLegacyTemplate(settings, m_settingsPath, templateName);
    if (legacyDocument.has_value()) {
        return legacyDocument;
    }

    // 仅在旧 key 和别名都未命中时解释目录选择 key，防止改变历史请求语义。
    if (!templateName.startsWith(QLatin1Char('@'))) {
        return std::nullopt;
    }
    auto qualifiedKey = templateName;
    while (qualifiedKey.startsWith(QLatin1Char('@'))) {
        qualifiedKey.remove(0, 1);
    }
    const auto settingsPrefix = QStringLiteral("settings:");
    if (qualifiedKey.startsWith(settingsPrefix)) {
        const auto key = QUrl::fromPercentEncoding(qualifiedKey.mid(settingsPrefix.size()).toUtf8());
        const auto it = settings.templateDocuments.constFind(key);
        return it == settings.templateDocuments.cend() ? std::nullopt : std::optional<TemplateDocument>(it.value());
    }
    const auto libraryPrefix = QStringLiteral("library:");
    if (qualifiedKey.startsWith(libraryPrefix)) {
        const auto key = QUrl::fromPercentEncoding(qualifiedKey.mid(libraryPrefix.size()).toUtf8());
        return templateLibraryStoreForSettings(m_settingsPath).loadTemplate(key);
    }
    return std::nullopt;
}

std::optional<PreparedTemplatePrint> TemplatePrintService::prepare(
    const PrintClientSettings& settings,
    const TemplatePrintInput& input,
    QString* errorCode,
    QString* errorMessage,
    core::PrintPreflightReport* preflight,
    core::PreviewPlanningBudget* budget) const
{
    if (preflight) *preflight = {};
    QString code, message;
    const auto snapshot = resolveSnapshot(settings, input, &code, &message);
    if (!snapshot) {
        if (errorCode) *errorCode = code;
        if (errorMessage) *errorMessage = message;
        if (preflight) preflight->issues.append({core::PrintPreflightSeverity::Error, code, message});
        return std::nullopt;
    }
    return prepareSnapshot(*snapshot, preflight, errorCode, errorMessage, budget);
}

std::optional<PreparedTemplatePrint> TemplatePrintService::prepareSnapshot(const TemplatePrintSnapshot& snapshot,
    core::PrintPreflightReport* preflight, QString* errorCode, QString* errorMessage, core::PreviewPlanningBudget* budget)
{
    if (errorCode) errorCode->clear();
    if (errorMessage) errorMessage->clear();
    if (preflight != nullptr) *preflight = {};
    const auto fail = [errorCode, errorMessage, preflight](const QString& code, const QString& message) {
        if (errorCode != nullptr) {
            *errorCode = code;
        }
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        if (preflight != nullptr && std::none_of(preflight->issues.cbegin(), preflight->issues.cend(), [](const auto& issue) {
                return issue.severity == core::PrintPreflightSeverity::Error;
            })) {
            preflight->issues.append({core::PrintPreflightSeverity::Error, code, message});
        }
        return std::optional<PreparedTemplatePrint>{};
    };

    if (budget && !budget->ensurePages(1)) return fail(QStringLiteral("REQUEST_TOO_LARGE"), budget->message);
    const auto& document = snapshot.document;
    const auto& context = snapshot.context;
    const auto paperSize = QSizeF(snapshot.labelPlan.paperSize.widthMm(), snapshot.labelPlan.paperSize.heightMm());
    if (preflight != nullptr) {
        preflight->document = document;
        const auto& profile = snapshot.deviceProfile;
        preflight->paperSizeMm = paperSize;
        preflight->offsetXmm = snapshot.labelOffset.x + profile.offsetX;
        preflight->offsetYmm = snapshot.labelOffset.y + profile.offsetY;
        preflight->scaleX = profile.scaleX > 0 ? profile.scaleX : 1;
        preflight->scaleY = profile.scaleY > 0 ? profile.scaleY : 1;
    }

    auto validationDocument = document;
    QList<core::TemplateInputIssue> inputIssues;
    if (snapshot.requireCompleteValues) {
        // 先用原 schema 合并后的真实值校验，绝不让推断字段被 Builder 自动补成空值。
        inputIssues = core::TemplateInputSchema::validate(document, context.values);
        if (preflight != nullptr) {
            for (const auto& issue : inputIssues) {
                preflight->issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("TEMPLATE_INPUT_INVALID"),
                    issue.message, core::PrintPreflightTarget::Field, issue.fieldKey, issue.elementIds, 0, 0, issue});
            }
        }
        // 只给结构校验的副本补充声明；返回结果和 Renderer 继续接收原始文档。
        for (const auto& field : core::TemplateInputSchema::fields(document)) {
            if (field.inferred) {
                validationDocument.fieldSchema.append(field.definition);
            }
        }
        // 正式检查不能退回设计期样例；只检查本次请求、预设与实际默认值。
        for (auto& field : validationDocument.fieldSchema) field.sampleValue.clear();
    } else if (context.hasMissingRequiredFields()) {
        return fail("BAD_REQUEST", missingRequiredFieldsMessage(context));
    }

    const auto templateValidation = TemplateDocumentValidator().validate(
        validationDocument,
        paperSize,
        context.values);
    if (preflight != nullptr) {
        for (const auto& issue : templateValidation.issues) {
            preflight->issues.append({issue.severity == core::TemplateValidationSeverity::Error
                    ? core::PrintPreflightSeverity::Error : core::PrintPreflightSeverity::Warning,
                issue.code, issue.message, issue.elementId.isEmpty() ? core::PrintPreflightTarget::General : core::PrintPreflightTarget::Element,
                {}, issue.elementId.isEmpty() ? QStringList{} : QStringList{issue.elementId}});
        }
    }
    // 桌面报告一次收齐数据与版式问题；对外错误码仍保持原来数据错误优先的契约。
    if (!inputIssues.isEmpty()) {
        QStringList messages;
        for (const auto& issue : inputIssues) messages.append(issue.message);
        return fail("MISSING_TEMPLATE_FIELDS", QStringLiteral("模板输入不完整或类型不正确：%1")
            .arg(messages.join(QStringLiteral("；"))));
    }
    if (templateValidation.hasErrors()) {
        return fail("TEMPLATE_VALIDATION_FAILED", templateValidationMessage(templateValidation));
    }

    if (preflight != nullptr && snapshot.requireCompleteValues) {
        for (const auto& field : core::TemplateInputSchema::fields(document)) {
            const auto value = context.values.value(field.definition.key);
            const bool empty = (value.isString() && value.toString().trimmed().isEmpty())
                || (value.isArray() && value.toArray().isEmpty()) || (value.isObject() && value.toObject().isEmpty());
            if (field.inferred && empty) {
                preflight->issues.append({core::PrintPreflightSeverity::Warning, QStringLiteral("FIELD_INTENTIONALLY_EMPTY"),
                    QStringLiteral("字段 %1 已明确留空").arg(field.definition.key), core::PrintPreflightTarget::Field,
                    field.definition.key, field.elementIds});
            }
        }
    }

    const auto rendered = TemplateDocumentRenderer().tryRenderPrint(document, snapshot.labelPlan,
        snapshot.labelOffset, snapshot.deviceProfile, context, budget);
    if (!rendered.success()) {
        return fail(budget && !budget->ok() ? QStringLiteral("REQUEST_TOO_LARGE") : QStringLiteral("TEMPLATE_RENDER_FAILED"),
            rendered.errorMessage.isEmpty() ? QString::fromUtf8("表格渲染失败") : rendered.errorMessage);
    }

    PreparedTemplatePrint result;
    result.templateKey = snapshot.templateKey;
    result.document = document;
    result.printPlan = rendered.plan;
    result.context = context;
    return result;
}

NativePrintDrawingPlan TemplatePrintService::createLegacyPlan(
    const LabelRenderPlan& labelPlan,
    const PrintClientSettings& settings,
    const QString& printerName) const
{
    const auto templateKey = templateOverrideKey(labelPlan.templateKey);
    const auto documentIt = settings.templateDocuments.constFind(templateKey);
    if (documentIt != settings.templateDocuments.cend()) {
        const auto printPlan = renderTemplatePrintPlan(
            documentIt.value(),
            labelPlan,
            settings,
            m_settingsPath,
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

QList<FieldPreset> TemplatePrintService::fieldPresets(QString* errorMessage) const
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return fieldPresetStoreForSettings(m_settingsPath).presets(errorMessage);
}

bool TemplatePrintService::saveFieldPreset(const FieldPreset& preset, QString* errorMessage) const
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return fieldPresetStoreForSettings(m_settingsPath).savePreset(preset, errorMessage);
}

} // 命名空间 sleekpr::infrastructure
