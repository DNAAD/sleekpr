#include "sleekpr/http/PrintRoutes.h"

#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/settings/PrinterSelectionResolver.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"
#include "sleekpr/http/TemplateRequestPlanner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace sleekpr::http {

using sleekpr::core::LabelItem;
using sleekpr::core::LabelPartItem;
using sleekpr::core::LabelRenderPlanner;
using sleekpr::core::PrinterSelectionResolver;

namespace {

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

} // 匿名命名空间

PrintRoutes::PrintRoutes(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine)
    : m_settingsPath(std::move(settingsPath))
    , m_printEngine(printEngine)
{
}

std::optional<LocalHttpResponse> PrintRoutes::route(
    const LocalHttpRequest& request,
    const sleekpr::core::PrintClientSettings& settings,
    const QHash<QByteArray, QByteArray>& extraHeaders) const
{
    const auto method = request.method.toUpper();
    const auto path = request.path;

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
        const auto executePrint = valueFor(root, {"executePrint", "ExecutePrint"}).toBool();
        const auto selectedPrinter = PrinterSelectionResolver().resolve(settings, stringFor(root, {"printerName", "PrinterName"}));

        QString errorCode;
        QString errorMessage;
        const auto requestPlan = buildTemplateRequestPlan(
            settings,
            m_settingsPath,
            root,
            selectedPrinter,
            &errorCode,
            &errorMessage);
        if (!requestPlan.has_value()) {
            const auto statusCode = errorCode == QStringLiteral("TEMPLATE_NOT_FOUND")
                || errorCode == QStringLiteral("FIELD_PRESET_NOT_FOUND")
                ? 404
                : 400;
            return jsonResponse(failEnvelope(errorCode, errorMessage), statusCode, extraHeaders);
        }

        int printed = 0;
        int failed = 0;
        if (executePrint) {
            if (m_printEngine != nullptr && m_printEngine->print(requestPlan->printPlan, selectedPrinter)) {
                printed = 1;
            } else {
                failed = 1;
            }
        }

        return jsonResponse(okEnvelope(printResultJson(requestPlan->requestId, 1, printed, failed, executePrint)), 200, extraHeaders);
    }

    return std::nullopt;
}

} // 命名空间 sleekpr::http
