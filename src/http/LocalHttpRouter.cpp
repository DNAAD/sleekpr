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
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPrinterInfo>
#include <QUuid>

#include <utility>

namespace sleekpr::http {

using sleekpr::core::FileSettingsStore;
using sleekpr::core::LabelItem;
using sleekpr::core::LabelPartItem;
using sleekpr::core::LabelRenderPlanner;
using sleekpr::core::NativeLabelDrawingPlanner;
using sleekpr::core::NativePrintDrawingPlan;
using sleekpr::core::PrintClientSettings;
using sleekpr::core::PrintClientSettingsJson;
using sleekpr::core::PrinterSelectionResolver;
using sleekpr::core::PrivateNetworkAccessPolicy;
using sleekpr::core::DeviceProfileResolver;
using sleekpr::core::TemplateDocumentRenderer;
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
    headers.insert("Access-Control-Allow-Headers", "*");
    headers.insert("Access-Control-Allow-Methods", "GET, POST, OPTIONS");

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

const sleekpr::core::TemplateDocument* findTemplateDocument(
    const PrintClientSettings& settings,
    const QString& requestedTemplate)
{
    const auto templateName = requestedTemplate.trimmed().isEmpty()
        ? QStringLiteral("default")
        : requestedTemplate.trimmed();

    const auto directIt = settings.templateDocuments.constFind(templateName);
    if (directIt != settings.templateDocuments.cend()) {
        return &directIt.value();
    }

    for (auto it = settings.templateDocuments.cbegin(); it != settings.templateDocuments.cend(); ++it) {
        const auto& document = it.value();
        if (document.id.compare(templateName, Qt::CaseInsensitive) == 0
            || document.templateKey.compare(templateName, Qt::CaseInsensitive) == 0) {
            return &document;
        }
    }

    return nullptr;
}

sleekpr::core::LabelRenderPlan templateLabelPlan(const sleekpr::core::TemplateDocument& document)
{
    sleekpr::core::LabelRenderPlan plan;
    plan.templateKey = document.templateKey.compare(QStringLiteral("silver"), Qt::CaseInsensitive) == 0
        ? sleekpr::core::LabelTemplateKey::Silver80x30
        : sleekpr::core::LabelTemplateKey::Default80x30;
    return plan;
}

QString missingRequiredFieldsMessage(const TemplateRenderContext& context)
{
    return QString::fromUtf8("缺少必填字段：%1").arg(context.missingRequiredFieldNames.join(QString::fromUtf8("、")));
}

NativePrintDrawingPlan createPrintPlan(
    const sleekpr::core::LabelRenderPlan& labelPlan,
    const PrintClientSettings& settings,
    const QString& printerName)
{
    const auto templateKey = templateOverrideKey(labelPlan.templateKey);
    const auto documentIt = settings.templateDocuments.constFind(templateKey);
    if (documentIt != settings.templateDocuments.cend()) {
        // HTTP 打印只接收已解析出的本机打印机名；设备 profile 按该名称选择，避免请求参数绕过本机配置。
        const auto profile = DeviceProfileResolver::resolve(documentIt.value().deviceProfiles, printerName);
        return TemplateDocumentRenderer().renderPrint(documentIt.value(), labelPlan, settings.labelOffset, profile, TemplateRenderContext{});
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
            const auto printPlan = createPrintPlan(labelPlan, settings, selectedPrinter);
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
            stringFor(root, {"templateKey", "TemplateKey", "templateId", "TemplateId"}));
        if (templateDocument == nullptr) {
            return jsonResponse(failEnvelope("TEMPLATE_NOT_FOUND", QString::fromUtf8("未找到指定模板。")), 404, extraHeaders);
        }

        const auto valuesValue = valueFor(root, {"values", "Values", "fieldValues", "FieldValues"});
        if (!valuesValue.isUndefined() && !valuesValue.isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("字段值 values 必须是对象。")), 400, extraHeaders);
        }

        const auto context = TemplateRenderContextBuilder::build(
            templateDocument->fieldSchema,
            QJsonObject{},
            valuesValue.toObject());
        if (context.hasMissingRequiredFields()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", missingRequiredFieldsMessage(context)), 400, extraHeaders);
        }

        const auto executePrint = valueFor(root, {"executePrint", "ExecutePrint"}).toBool();
        const auto selectedPrinter = PrinterSelectionResolver().resolve(settings, stringFor(root, {"printerName", "PrinterName"}));
        const auto profile = DeviceProfileResolver::resolve(templateDocument->deviceProfiles, selectedPrinter);
        const auto printPlan = TemplateDocumentRenderer().renderPrint(
            *templateDocument,
            templateLabelPlan(*templateDocument),
            settings.labelOffset,
            profile,
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
