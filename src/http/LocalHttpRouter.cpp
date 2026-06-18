#include "sleekpr/http/LocalHttpRouter.h"

#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/labels/TemplateOverrideKey.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/security/LocalCorsPolicy.h"
#include "sleekpr/core/security/PrivateNetworkAccessPolicy.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include "sleekpr/core/settings/PrinterSelectionResolver.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/FieldPresetJson.h"
#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/PaperSpecJson.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateDocumentValidator.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPrinterInfo>
#include <QSizeF>
#include <QUuid>

#include <optional>
#include <utility>

namespace sleekpr::http {

using sleekpr::core::FileSettingsStore;
using sleekpr::core::LabelItem;
using sleekpr::core::LabelOffset;
using sleekpr::core::LabelPartItem;
using sleekpr::core::LabelRenderPlanner;
using sleekpr::core::LabelRenderPlan;
using sleekpr::core::LabelPaperSize;
using sleekpr::core::NativeLabelDrawingPlanner;
using sleekpr::core::NativePrintDrawingPlan;
using sleekpr::core::PrintClientSettings;
using sleekpr::core::PrintClientSettingsJson;
using sleekpr::core::PrinterSelectionResolver;
using sleekpr::core::PrivateNetworkAccessPolicy;
using sleekpr::core::DeviceProfile;
using sleekpr::core::DeviceProfileResolver;
using sleekpr::core::FieldPreset;
using sleekpr::core::FieldPresetJson;
using sleekpr::core::FieldPresetStore;
using sleekpr::core::PaperSpec;
using sleekpr::core::PaperSpecJson;
using sleekpr::core::PaperSpecStore;
using sleekpr::core::TemplateDocument;
using sleekpr::core::TemplateDocumentJson;
using sleekpr::core::TemplateDocumentRenderer;
using sleekpr::core::TemplateDocumentValidator;
using sleekpr::core::TemplateLibraryStore;
using sleekpr::core::TemplateRenderContext;
using sleekpr::core::TemplateRenderContextBuilder;
using sleekpr::core::templateOverrideKey;

namespace {

QJsonObject okEnvelope(const QJsonObject& data)
{
    // 响应包络保持 success/code/message/data 形状，前端无需区分 .NET 与原生客户端。
    return QJsonObject{
        {"success", true},
        {"code", "OK"},
        {"message", ""},
        {"data", data},
    };
}

QJsonObject failEnvelope(const QString& code, const QString& message)
{
    // 错误响应同样使用兼容包络，便于前端统一展示和日志记录。
    return QJsonObject{
        {"success", false},
        {"code", code},
        {"message", message},
        {"data", QJsonObject{}},
    };
}

LocalHttpResponse jsonResponse(
    const QJsonObject& payload,
    int statusCode,
    const QHash<QByteArray, QByteArray>& extraHeaders = {})
{
    QHash<QByteArray, QByteArray> headers = extraHeaders;
    headers.insert("Content-Type", "application/json; charset=utf-8");
    return LocalHttpResponse{
        statusCode,
        headers,
        QJsonDocument(payload).toJson(QJsonDocument::Compact),
    };
}

LocalHttpResponse emptyResponse(int statusCode, const QHash<QByteArray, QByteArray>& extraHeaders)
{
    return LocalHttpResponse{statusCode, extraHeaders, QByteArray{}};
}

QString headerValue(const LocalHttpRequest& request, const QString& name)
{
    return request.headers.value(name.toLower()).trimmed();
}

QHash<QByteArray, QByteArray> corsHeaders(const LocalHttpRequest& request, const PrintClientSettings& settings)
{
    QHash<QByteArray, QByteArray> headers;
    const auto origin = headerValue(request, "origin");
    if (origin.isEmpty()) {
        return headers;
    }

    headers.insert("Access-Control-Allow-Origin", origin.toUtf8());
    headers.insert("Vary", "Origin");
    headers.insert("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Sleekpr-Token");
    headers.insert("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");

    if (PrivateNetworkAccessPolicy::shouldAllow(
            headerValue(request, "access-control-request-private-network"),
            origin,
            settings.allowedOrigins)) {
        headers.insert(PrivateNetworkAccessPolicy::ResponseHeaderName, "true");
    }

    return headers;
}

bool hasRejectedOrigin(const LocalHttpRequest& request, const PrintClientSettings& settings)
{
    const auto origin = headerValue(request, "origin");
    return !origin.isEmpty() && !sleekpr::core::LocalCorsPolicy::isAllowedOrigin(origin, settings.allowedOrigins);
}

QJsonObject printersToJson(const PrintClientSettings& settings)
{
    QJsonArray printers;
    const auto defaultPrinterName = settings.defaultPrinter.trimmed();

    for (const auto& printer : QPrinterInfo::availablePrinters()) {
        const auto name = printer.printerName();
        // 如果用户已配置默认打印机，以配置为准；否则退回 Qt 识别到的系统默认打印机。
        printers.append(QJsonObject{
            {"name", name},
            {"displayName", name},
            {"isDefault", !defaultPrinterName.isEmpty()
                ? name.compare(defaultPrinterName, Qt::CaseInsensitive) == 0
                : printer.isDefault()},
            {"status", "unknown"},
        });
    }

    return QJsonObject{{"printers", printers}};
}

QJsonValue valueFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    for (const auto* name : names) {
        if (object.contains(name)) {
            return object.value(name);
        }
    }
    return QJsonValue{};
}

QString stringFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    return valueFor(object, names).toString();
}

double doubleFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    const auto value = valueFor(object, names);
    if (value.isString()) {
        bool ok = false;
        const auto parsed = value.toString().toDouble(&ok);
        return ok ? parsed : 0.0;
    }
    return value.toDouble();
}

int intFor(const QJsonObject& object, std::initializer_list<const char*> names)
{
    const auto value = valueFor(object, names);
    if (value.isString()) {
        bool ok = false;
        const auto parsed = value.toString().toInt(&ok);
        return ok ? parsed : 0;
    }
    return value.toInt();
}

QList<LabelPartItem> partsFromJson(const QJsonObject& item)
{
    QList<LabelPartItem> parts;
    const auto partArray = valueFor(item, {"finishedProductPartVO", "FinishedProductPartVO", "finishedProductParts"}).toArray();
    for (const auto& partValue : partArray) {
        const auto partObject = partValue.toObject();
        parts.append(LabelPartItem{
            stringFor(partObject, {"categoryName", "CategoryName"}),
            doubleFor(partObject, {"partWeight", "PartWeight"}),
        });
    }
    return parts;
}

LabelItem labelItemFromJson(const QJsonObject& item)
{
    LabelItem label;
    label.factoryNo = intFor(item, {"factoryNo", "FactoryNo"});
    label.identifierCode = stringFor(item, {"identifierCode", "IdentifierCode"});
    label.productName = stringFor(item, {"productName", "ProductName"});
    label.weightCategory = stringFor(item, {"weightCategory", "WeightCategory"});
    label.finishedProductWeight = doubleFor(item, {"finishedProductWeight", "FinishedProductWeight"});
    label.roughWeight = doubleFor(item, {"roughWeight", "RoughWeight"});
    label.salesCode = stringFor(item, {"salesCode", "SalesCode"});
    label.goldPurity = stringFor(item, {"goldPurity", "GoldPurity"});
    label.address = stringFor(item, {"address", "Address"});
    label.price = doubleFor(item, {"price", "Price"});
    label.additionalPrice = doubleFor(item, {"additionalPrice", "AdditionalPrice"});
    label.tagWeight = doubleFor(item, {"tagWeight", "TagWeight"});
    label.categoryName = stringFor(item, {"categoryName", "CategoryName"});
    label.finishedProductParts = partsFromJson(item);
    label.additionalRemark = stringFor(item, {"additionalRemark", "AdditionalRemark"});
    label.inlayWeight = doubleFor(item, {"inlayWeight", "InlayWeight"});
    label.ropeWeight = doubleFor(item, {"ropeWeight", "RopeWeight"});
    label.finishedProductNote = stringFor(item, {"finishedProductNote", "FinishedProductNote"});
    return label;
}

QList<LabelItem> labelItemsFromJson(const QJsonArray& items)
{
    QList<LabelItem> result;
    for (const auto& itemValue : items) {
        result.append(labelItemFromJson(itemValue.toObject()));
    }
    return result;
}

QString requestIdOrFallback(const QString& requestId)
{
    if (!requestId.trimmed().isEmpty()) {
        return requestId.trimmed();
    }
    return QString("print-%1").arg(QDateTime::currentMSecsSinceEpoch());
}

QJsonObject printResultJson(const QString& requestId, int total, int printed, int failed, bool executePrint)
{
    const auto pending = executePrint ? 0 : total;
    const auto status = failed > 0 ? QStringLiteral("partial_failed") : QStringLiteral("queued");
    return QJsonObject{
        {"jobId", QString("job-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))},
        {"requestId", requestId},
        {"status", status},
        {"accepted", total},
        {"total", total},
        {"printed", printed},
        {"failed", failed},
        {"pending", pending},
    };
}

QString templateLibraryDirectoryForSettings(const QString& settingsPath)
{
    // 模板库跟随 settings.json 放在同级目录，便于整套客户端配置备份和迁移。
    const QFileInfo settingsFile(settingsPath);
    return settingsFile.absoluteDir().filePath(QStringLiteral("templates"));
}

TemplateLibraryStore templateLibraryStoreForSettings(const QString& settingsPath)
{
    return TemplateLibraryStore(templateLibraryDirectoryForSettings(settingsPath));
}

QString paperSpecFilePathForSettings(const QString& settingsPath)
{
    // 纸张规格和 settings.json 同级，便于把整套客户端配置一起备份或迁移。
    const QFileInfo settingsFile(settingsPath);
    return settingsFile.absoluteDir().filePath(QStringLiteral("paper-specs.json"));
}

PaperSpecStore paperSpecStoreForSettings(const QString& settingsPath)
{
    return PaperSpecStore(paperSpecFilePathForSettings(settingsPath));
}

QString fieldPresetFilePathForSettings(const QString& settingsPath)
{
    // 字段预设和 settings.json 同级，便于整套客户端配置一起备份或迁移。
    const QFileInfo settingsFile(settingsPath);
    return settingsFile.absoluteDir().filePath(QStringLiteral("field-presets.json"));
}

FieldPresetStore fieldPresetStoreForSettings(const QString& settingsPath)
{
    return FieldPresetStore(fieldPresetFilePathForSettings(settingsPath));
}

QString templateIdFromPath(const QString& path)
{
    // 模板 id 直接映射到文件名，路由层先挡掉路径分隔符，避免把路径语义泄漏给存储层。
    const auto prefix = QStringLiteral("/templates/");
    if (!path.startsWith(prefix)) {
        return {};
    }

    const auto templateId = path.mid(prefix.size()).trimmed();
    if (templateId.isEmpty() || templateId.contains(QLatin1Char('/')) || templateId.contains(QLatin1Char('\\'))) {
        return {};
    }
    return templateId;
}

QString paperSpecIdFromPath(const QString& path)
{
    // 路由层只处理 URL 形状，id 的完整安全校验由 PaperSpecStore 统一负责。
    const auto prefix = QStringLiteral("/paper-specs/");
    if (!path.startsWith(prefix)) {
        return {};
    }

    const auto paperSpecId = path.mid(prefix.size()).trimmed();
    if (paperSpecId.isEmpty() || paperSpecId.contains(QLatin1Char('/')) || paperSpecId.contains(QLatin1Char('\\'))) {
        return {};
    }
    return paperSpecId;
}

QString fieldPresetIdFromPath(const QString& path)
{
    // 路由层只处理 URL 形状，完整安全校验由 FieldPresetStore 统一负责。
    const auto prefix = QStringLiteral("/field-presets/");
    if (!path.startsWith(prefix)) {
        return {};
    }

    const auto presetId = path.mid(prefix.size()).trimmed();
    if (presetId.isEmpty() || presetId.contains(QLatin1Char('/')) || presetId.contains(QLatin1Char('\\'))) {
        return {};
    }
    return presetId;
}

QJsonObject templateSummaryJson(const TemplateDocument& document)
{
    return QJsonObject{
        {"id", document.id},
        {"name", document.name},
        {"templateKey", document.templateKey},
        {"category", document.category},
        {"paperSpecId", document.paperSpecId},
        {"schemaVersion", document.schemaVersion},
        {"activeVersionId", document.activeVersionId},
    };
}

QJsonArray templateSummaryListJson(const TemplateLibraryStore& store)
{
    QJsonArray templates;
    for (const auto& templateId : store.templateIds()) {
        QString errorMessage;
        const auto document = store.loadTemplate(templateId, &errorMessage);
        if (!document.has_value()) {
            continue;
        }
        templates.append(templateSummaryJson(document.value()));
    }
    return templates;
}

std::optional<QJsonArray> paperSpecListJson(const PaperSpecStore& store, QString* errorMessage)
{
    QJsonArray paperSpecs;
    QString storeError;
    const auto specs = store.paperSpecs(&storeError);
    if (!storeError.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        return std::nullopt;
    }

    for (const auto& spec : specs) {
        paperSpecs.append(PaperSpecJson::toJson(spec));
    }
    return paperSpecs;
}

std::optional<QJsonArray> fieldPresetListJson(const FieldPresetStore& store, QString* errorMessage)
{
    QJsonArray presets;
    QString storeError;
    const auto loadedPresets = store.presets(&storeError);
    if (!storeError.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = storeError;
        }
        return std::nullopt;
    }

    for (const auto& preset : loadedPresets) {
        presets.append(FieldPresetJson::toJson(preset));
    }
    return presets;
}

QJsonObject paperSpecObjectFromRequest(const QJsonObject& root)
{
    // 管理界面可提交裸纸张规格对象，也可提交 { paperSpec: ... } 包装对象。
    if (root.contains("paperSpec") && root["paperSpec"].isObject()) {
        return root["paperSpec"].toObject();
    }
    return root;
}

QJsonObject fieldPresetObjectFromRequest(const QJsonObject& root)
{
    // 管理界面可提交裸字段预设对象，也可提交 { fieldPreset: ... } 包装对象。
    if (root.contains("fieldPreset") && root["fieldPreset"].isObject()) {
        return root["fieldPreset"].toObject();
    }
    return root;
}

std::optional<PaperSpec> validatedPaperSpecFromRequest(
    const QJsonObject& root,
    QString* errorMessage)
{
    const auto paperSpecObject = paperSpecObjectFromRequest(root);
    QString validationError;
    if (!PaperSpecJson::validate(paperSpecObject, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return std::nullopt;
    }
    return PaperSpecJson::fromJson(paperSpecObject);
}

std::optional<FieldPreset> validatedFieldPresetFromRequest(
    const QJsonObject& root,
    QString* errorMessage)
{
    const auto presetObject = fieldPresetObjectFromRequest(root);
    QString validationError;
    if (!FieldPresetJson::validate(presetObject, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return std::nullopt;
    }
    return FieldPresetJson::fromJson(presetObject);
}

QJsonObject templateObjectFromRequest(const QJsonObject& root)
{
    // 前端既可以提交裸模板，也可以提交带 template 包装的对象，便于复用表单状态。
    if (root.contains("template") && root["template"].isObject()) {
        return root["template"].toObject();
    }
    return root;
}

std::optional<TemplateDocument> validatedTemplateFromRequest(
    const QJsonObject& root,
    QString* errorMessage)
{
    const auto templateObject = templateObjectFromRequest(root);
    QString validationError;
    if (!TemplateDocumentJson::validateForImport(templateObject, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return std::nullopt;
    }
    return TemplateDocumentJson::fromJson(templateObject);
}

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

sleekpr::core::LabelRenderPlan templateLabelPlan(const sleekpr::core::TemplateDocument& document)
{
    sleekpr::core::LabelRenderPlan plan;
    plan.templateKey = document.templateKey.compare(QStringLiteral("silver"), Qt::CaseInsensitive) == 0
        ? sleekpr::core::LabelTemplateKey::Silver80x30
        : sleekpr::core::LabelTemplateKey::Default80x30;
    return plan;
}

std::optional<PaperSpec> paperSpecForTemplate(const QString& settingsPath, const TemplateDocument& document)
{
    const auto paperSpecId = document.paperSpecId.trimmed();
    if (paperSpecId.isEmpty()) {
        return std::nullopt;
    }

    return paperSpecStoreForSettings(settingsPath).loadPaperSpec(paperSpecId);
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
    if (paperSpec.has_value() && document.deviceProfiles.isEmpty()) {
        // 没有打印机 profile 时使用纸张规格的默认 DPI；一旦存在设备 profile，则以设备校准优先。
        profile.dpi = paperSpec->defaultDpi;
    }
    return profile;
}

NativePrintDrawingPlan renderTemplatePrintPlan(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& printerName,
    const TemplateRenderContext& context)
{
    const auto paperSpec = paperSpecForTemplate(settingsPath, document);
    return TemplateDocumentRenderer().renderPrint(
        document,
        labelPlanWithPaperSpec(labelPlan, paperSpec),
        labelOffsetWithPaperMargins(settings.labelOffset, paperSpec),
        deviceProfileWithPaperDpi(DeviceProfileResolver::resolve(document.deviceProfiles, printerName), document, paperSpec),
        context);
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

NativePrintDrawingPlan createPrintPlan(
    const sleekpr::core::LabelRenderPlan& labelPlan,
    const PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& printerName)
{
    const auto templateKey = templateOverrideKey(labelPlan.templateKey);
    const auto documentIt = settings.templateDocuments.constFind(templateKey);
    if (documentIt != settings.templateDocuments.cend()) {
        return renderTemplatePrintPlan(documentIt.value(), labelPlan, settings, settingsPath, printerName, TemplateRenderContext{});
    }

    return NativePrintDrawingPlan::fromSinglePage(NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        settings.labelOffset,
        settings.templateOverrides.value(templateKey),
        settings.templateElements.value(templateKey)));
}

} // 匿名命名空间

LocalHttpRouter::LocalHttpRouter(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine)
    : m_settingsPath(std::move(settingsPath))
    , m_printEngine(printEngine)
{
}

LocalHttpResponse LocalHttpRouter::route(const LocalHttpRequest& request) const
{
    const auto settings = FileSettingsStore(m_settingsPath).load();
    const auto method = request.method.toUpper();
    const auto path = request.path;

    if (hasRejectedOrigin(request, settings)) {
        return jsonResponse(failEnvelope("FORBIDDEN_ORIGIN", "Origin is not allowed"), 403);
    }

    const auto extraHeaders = corsHeaders(request, settings);
    if (method == "OPTIONS") {
        return emptyResponse(204, extraHeaders);
    }

    if ((path == "/" || path == "/health") && method == "GET") {
        // 健康检查是前端发现本地客户端是否在线的最小接口。
        return jsonResponse(okEnvelope(QJsonObject{
            {"ready", true},
            {"service", "sleekpr"},
            {"protocol", "http"},
        }), 200, extraHeaders);
    }

    if (path == "/settings" && method == "GET") {
        return jsonResponse(okEnvelope(PrintClientSettingsJson::toJson(settings)), 200, extraHeaders);
    }

    if (path == "/settings" && method == "POST") {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid settings JSON"), 400, extraHeaders);
        }

        const auto nextSettings = PrintClientSettingsJson::fromJson(document.object());
        if (!FileSettingsStore(m_settingsPath).save(nextSettings)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", "Unable to save settings"), 500, extraHeaders);
        }
        return jsonResponse(okEnvelope(PrintClientSettingsJson::toJson(FileSettingsStore(m_settingsPath).load())), 200, extraHeaders);
    }

    if (path == "/printers" && method == "GET") {
        // 打印机发现依赖本机系统环境，必须留在 HTTP/应用边界；核心层只接收已选择的打印机名。
        return jsonResponse(okEnvelope(printersToJson(settings)), 200, extraHeaders);
    }

    const auto routePaperSpecId = paperSpecIdFromPath(path);
    if (path == "/paper-specs" && method == "GET") {
        const auto store = paperSpecStoreForSettings(m_settingsPath);
        QString errorMessage;
        const auto paperSpecs = paperSpecListJson(store, &errorMessage);
        if (!paperSpecs.has_value()) {
            return jsonResponse(failEnvelope("PAPER_SPEC_STORE_ERROR", errorMessage), 500, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"paperSpecs", paperSpecs.value()},
        }), 200, extraHeaders);
    }

    if (!routePaperSpecId.isEmpty() && method == "GET") {
        const auto store = paperSpecStoreForSettings(m_settingsPath);
        QString errorMessage;
        const auto spec = store.loadPaperSpec(routePaperSpecId, &errorMessage);
        if (!spec.has_value()) {
            return jsonResponse(failEnvelope("PAPER_SPEC_NOT_FOUND", QString::fromUtf8("未找到指定纸张规格。")), 404, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"paperSpec", PaperSpecJson::toJson(spec.value())},
        }), 200, extraHeaders);
    }

    if (!routePaperSpecId.isEmpty() && method == "PUT") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid paper spec JSON"), 400, extraHeaders);
        }

        QString errorMessage;
        const auto spec = validatedPaperSpecFromRequest(parsedDocument.object(), &errorMessage);
        if (!spec.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }
        if (spec->id != routePaperSpecId) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("纸张规格 id 与路径不一致。")), 400, extraHeaders);
        }

        const auto store = paperSpecStoreForSettings(m_settingsPath);
        if (!store.savePaperSpec(spec.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"paperSpec", PaperSpecJson::toJson(spec.value())},
        }), 200, extraHeaders);
    }

    const auto routeFieldPresetId = fieldPresetIdFromPath(path);
    if (path == "/field-presets" && method == "GET") {
        QString errorMessage;
        const auto presets = fieldPresetListJson(fieldPresetStoreForSettings(m_settingsPath), &errorMessage);
        if (!presets.has_value()) {
            return jsonResponse(failEnvelope("FIELD_PRESET_STORE_ERROR", errorMessage), 500, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"fieldPresets", presets.value()},
        }), 200, extraHeaders);
    }

    if (!routeFieldPresetId.isEmpty() && method == "GET") {
        QString errorMessage;
        const auto preset = fieldPresetStoreForSettings(m_settingsPath).loadPreset(routeFieldPresetId, &errorMessage);
        if (!preset.has_value()) {
            return jsonResponse(failEnvelope("FIELD_PRESET_NOT_FOUND", errorMessage), 404, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"fieldPreset", FieldPresetJson::toJson(preset.value())},
        }), 200, extraHeaders);
    }

    if (!routeFieldPresetId.isEmpty() && method == "PUT") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid field preset JSON"), 400, extraHeaders);
        }

        QString errorMessage;
        const auto preset = validatedFieldPresetFromRequest(parsedDocument.object(), &errorMessage);
        if (!preset.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }
        if (preset->id != routeFieldPresetId) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("字段预设 id 与路径不一致。")), 400, extraHeaders);
        }

        const auto store = fieldPresetStoreForSettings(m_settingsPath);
        if (!store.savePreset(preset.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"fieldPreset", FieldPresetJson::toJson(preset.value())},
        }), 200, extraHeaders);
    }

    if (!routeFieldPresetId.isEmpty() && method == "DELETE") {
        QString errorMessage;
        const auto store = fieldPresetStoreForSettings(m_settingsPath);
        if (!store.removePreset(routeFieldPresetId, &errorMessage)) {
            return jsonResponse(failEnvelope("DELETE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"id", routeFieldPresetId},
            {"removed", true},
        }), 200, extraHeaders);
    }

    const auto routeTemplateId = templateIdFromPath(path);
    if (path == "/templates" && method == "GET") {
        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        return jsonResponse(okEnvelope(QJsonObject{
            {"templates", templateSummaryListJson(store)},
        }), 200, extraHeaders);
    }

    if (path == "/templates" && method == "POST") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid template JSON"), 400, extraHeaders);
        }

        // 保存入口接受裸模板对象，也接受 { "template": ... } 包装，方便前端表单提交。
        QString errorMessage;
        const auto templateDocument = validatedTemplateFromRequest(parsedDocument.object(), &errorMessage);
        if (!templateDocument.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }

        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        if (!store.saveTemplate(templateDocument.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"template", TemplateDocumentJson::toJson(templateDocument.value())},
        }), 200, extraHeaders);
    }

    if (!routeTemplateId.isEmpty() && method == "GET") {
        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        QString errorMessage;
        const auto templateDocument = store.loadTemplate(routeTemplateId, &errorMessage);
        if (!templateDocument.has_value()) {
            return jsonResponse(failEnvelope("TEMPLATE_NOT_FOUND", QString::fromUtf8("未找到指定模板。")), 404, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"template", TemplateDocumentJson::toJson(templateDocument.value())},
        }), 200, extraHeaders);
    }

    if (!routeTemplateId.isEmpty() && method == "PUT") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid template JSON"), 400, extraHeaders);
        }

        QString errorMessage;
        const auto templateDocument = validatedTemplateFromRequest(parsedDocument.object(), &errorMessage);
        if (!templateDocument.has_value()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", errorMessage), 400, extraHeaders);
        }
        if (templateDocument->id != routeTemplateId) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("模板 id 与路径不一致。")), 400, extraHeaders);
        }

        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        if (!store.saveTemplate(templateDocument.value(), &errorMessage)) {
            return jsonResponse(failEnvelope("SAVE_FAILED", errorMessage), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"template", TemplateDocumentJson::toJson(templateDocument.value())},
        }), 200, extraHeaders);
    }

    if (!routeTemplateId.isEmpty() && method == "DELETE") {
        const auto store = templateLibraryStoreForSettings(m_settingsPath);
        if (!store.removeTemplate(routeTemplateId)) {
            return jsonResponse(failEnvelope("DELETE_FAILED", QString::fromUtf8("删除模板失败。")), 400, extraHeaders);
        }

        return jsonResponse(okEnvelope(QJsonObject{
            {"id", routeTemplateId},
            {"removed", true},
        }), 200, extraHeaders);
    }

    if (path == "/print/tag" && method == "POST") {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid print request JSON"), 400, extraHeaders);
        }

        const auto root = document.object();
        const auto items = labelItemsFromJson(valueFor(root, {"items", "Items"}).toArray());
        if (items.isEmpty()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("至少需要一条标签数据。")), 400, extraHeaders);
        }

        const auto executePrint = valueFor(root, {"executePrint", "ExecutePrint"}).toBool();
        const auto selectedPrinter = PrinterSelectionResolver().resolve(settings, stringFor(root, {"printerName", "PrinterName"}));
        int printed = 0;
        int failed = 0;

        for (const auto& item : items) {
            const auto labelPlan = LabelRenderPlanner().createPlan(item);
            const auto printPlan = createPrintPlan(labelPlan, settings, m_settingsPath, selectedPrinter);
            if (!executePrint) {
                continue;
            }
            if (m_printEngine != nullptr && m_printEngine->print(printPlan, selectedPrinter)) {
                ++printed;
            } else {
                ++failed;
            }
        }

        const auto requestId = requestIdOrFallback(stringFor(root, {"requestId", "RequestId"}));
        return jsonResponse(okEnvelope(printResultJson(requestId, items.size(), printed, failed, executePrint)), 200, extraHeaders);
    }

    if (path == "/print/template" && method == "POST") {
        QJsonParseError parseError;
        const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid template print request JSON"), 400, extraHeaders);
        }

        const auto root = parsedDocument.object();
        const auto templateDocument = findTemplateDocument(
            settings,
            m_settingsPath,
            stringFor(root, {"templateKey", "TemplateKey", "templateId", "TemplateId"}));
        if (!templateDocument.has_value()) {
            return jsonResponse(failEnvelope("TEMPLATE_NOT_FOUND", QString::fromUtf8("未找到指定模板。")), 404, extraHeaders);
        }

        const auto valuesValue = valueFor(root, {"values", "Values", "fieldValues", "FieldValues"});
        if (!valuesValue.isUndefined() && !valuesValue.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("字段值 values 必须是对象。")), 400, extraHeaders);
        }

        QString presetErrorMessage;
        const auto fieldPreset = findFieldPresetForRequest(m_settingsPath, root, &presetErrorMessage);
        if (!presetErrorMessage.isEmpty() && !fieldPreset.has_value()) {
            return jsonResponse(failEnvelope("FIELD_PRESET_NOT_FOUND", presetErrorMessage), 404, extraHeaders);
        }

        const auto context = TemplateRenderContextBuilder::build(
            templateDocument->fieldSchema,
            fieldPreset.has_value() ? fieldPreset->values : QJsonObject{},
            valuesValue.toObject());
        if (context.hasMissingRequiredFields()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", missingRequiredFieldsMessage(context)), 400, extraHeaders);
        }

        const auto templateValidation = TemplateDocumentValidator().validate(
            templateDocument.value(),
            paperSizeForTemplateValidation(m_settingsPath, templateDocument.value()),
            context.values);
        if (templateValidation.hasErrors()) {
            return jsonResponse(
                failEnvelope("TEMPLATE_VALIDATION_FAILED", templateValidationMessage(templateValidation)),
                400,
                extraHeaders);
        }

        const auto executePrint = valueFor(root, {"executePrint", "ExecutePrint"}).toBool();
        const auto selectedPrinter = PrinterSelectionResolver().resolve(settings, stringFor(root, {"printerName", "PrinterName"}));
        const auto printPlan = renderTemplatePrintPlan(
            templateDocument.value(),
            templateLabelPlan(templateDocument.value()),
            settings,
            m_settingsPath,
            selectedPrinter,
            context);

        int printed = 0;
        int failed = 0;
        if (executePrint) {
            if (m_printEngine != nullptr && m_printEngine->print(printPlan, selectedPrinter)) {
                printed = 1;
            } else {
                failed = 1;
            }
        }

        const auto requestId = requestIdOrFallback(stringFor(root, {"requestId", "RequestId"}));
        return jsonResponse(okEnvelope(printResultJson(requestId, 1, printed, failed, executePrint)), 200, extraHeaders);
    }

    return jsonResponse(failEnvelope("NOT_FOUND", "Route not implemented in native migration yet"), 404, extraHeaders);
}

} // 命名空间 sleekpr::http
