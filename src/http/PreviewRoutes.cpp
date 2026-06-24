#include "sleekpr/http/PreviewRoutes.h"

#include "sleekpr/core/settings/PrinterSelectionResolver.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"
#include "sleekpr/http/TemplateRequestPlanner.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace sleekpr::http {

using sleekpr::core::NativeLabelDrawingPlan;
using sleekpr::core::NativePrintDrawingPlan;
using sleekpr::core::PrinterSelectionResolver;
using sleekpr::infrastructure::LabelPreviewImageRenderer;

namespace {

QJsonObject printPlanPaperJson(const NativePrintDrawingPlan& plan)
{
    return QJsonObject{
        {"widthMm", plan.paperSize.widthMm()},
        {"heightMm", plan.paperSize.heightMm()},
        {"dpi", plan.renderDpi},
    };
}

NativeLabelDrawingPlan labelPlanForPreviewPage(const NativePrintDrawingPlan& printPlan, const sleekpr::core::NativePrintPage& page)
{
    NativeLabelDrawingPlan labelPlan;
    labelPlan.paperSize = printPlan.paperSize;
    labelPlan.renderDpi = printPlan.renderDpi;
    labelPlan.commands = page.commands;
    return labelPlan;
}

QJsonObject previewPageJson(
    const NativePrintDrawingPlan& printPlan,
    const sleekpr::core::NativePrintPage& page,
    int globalPageNumber,
    int itemIndex,
    const QString& itemRequestId,
    const QString& templateKey,
    const LabelPreviewImageRenderer& renderer)
{
    const auto png = renderer.renderPng(labelPlanForPreviewPage(printPlan, page), printPlan.renderDpi);
    return QJsonObject{
        {"pageNumber", globalPageNumber},
        {"jobPageNumber", page.pageNumber},
        {"itemIndex", itemIndex},
        {"requestId", itemRequestId},
        {"templateKey", templateKey},
        {"contentType", "image/png"},
        {"imageBase64", QString::fromLatin1(png.toBase64())},
        {"paper", printPlanPaperJson(printPlan)},
    };
}

QJsonObject mergedPreviewItemRequest(const QJsonObject& root, const QJsonObject& item, const QString& fallbackRequestId)
{
    QJsonObject merged = root;
    for (auto it = item.constBegin(); it != item.constEnd(); ++it) {
        merged.insert(it.key(), it.value());
    }

    // 批量预览中每条数据都需要稳定 requestId，便于 Web 端把缩略图和原始业务数据对应起来。
    if (stringFor(merged, {"requestId", "RequestId"}).trimmed().isEmpty()) {
        merged.insert(QStringLiteral("requestId"), fallbackRequestId);
    }
    return merged;
}

qsizetype previewResponseBodySize(
    const QString& requestId,
    int totalItems,
    const QJsonObject& firstPaper,
    const QJsonArray& pages)
{
    const auto payload = okEnvelope(QJsonObject{
        {"requestId", requestId},
        {"totalItems", totalItems},
        {"totalPages", pages.size()},
        {"paper", firstPaper},
        {"pages", pages},
    });
    return QJsonDocument(payload).toJson(QJsonDocument::Compact).size();
}

} // 匿名命名空间

PreviewRoutes::PreviewRoutes(QString settingsPath, LocalHttpLimits limits)
    : m_settingsPath(std::move(settingsPath))
    , m_limits(limits)
{
}

std::optional<LocalHttpResponse> PreviewRoutes::route(
    const LocalHttpRequest& request,
    const sleekpr::core::PrintClientSettings& settings,
    const QHash<QByteArray, QByteArray>& extraHeaders) const
{
    const auto method = request.method.toUpper();
    const auto path = request.path;

    if (path != "/preview/template" || method != "POST") {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const auto parsedDocument = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsedDocument.isObject()) {
        return jsonResponse(failEnvelope("BAD_REQUEST", "Invalid template preview request JSON"), 400, extraHeaders);
    }

    const auto root = parsedDocument.object();
    const auto itemsValue = valueFor(root, {"items", "Items", "jobs", "Jobs"});
    const auto hasBatchItems = !itemsValue.isUndefined();
    if (hasBatchItems && !itemsValue.isArray()) {
        return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("批量预览 items 必须是数组。")), 400, extraHeaders);
    }

    const auto batchRequestId = requestIdOrFallback(stringFor(root, {"requestId", "RequestId"}));
    QJsonArray requestItems;
    if (hasBatchItems) {
        requestItems = itemsValue.toArray();
        if (requestItems.isEmpty()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("批量预览至少需要一条模板数据。")), 400, extraHeaders);
        }
    } else {
        requestItems.append(root);
    }
    if (m_limits.maxPreviewBatchItems >= 0 && requestItems.size() > m_limits.maxPreviewBatchItems) {
        return requestTooLargeResponse(
            QString::fromUtf8("批量预览条数超过本地接口限制。"),
            extraHeaders);
    }

    LabelPreviewImageRenderer renderer;
    QJsonArray pages;
    QJsonObject firstPaper;
    int globalPageNumber = 1;

    for (int itemIndex = 0; itemIndex < requestItems.size(); ++itemIndex) {
        if (!requestItems.at(itemIndex).isObject()) {
            return jsonResponse(failEnvelope("BAD_REQUEST", QString::fromUtf8("批量预览 items 每一项必须是对象。")), 400, extraHeaders);
        }

        const auto itemRoot = hasBatchItems
            ? mergedPreviewItemRequest(
                  root,
                  requestItems.at(itemIndex).toObject(),
                  QStringLiteral("%1-%2").arg(batchRequestId).arg(itemIndex + 1))
            : root;
        const auto selectedPrinter = PrinterSelectionResolver().resolve(
            settings,
            stringFor(itemRoot, {"printerName", "PrinterName"}));

        QString errorCode;
        QString errorMessage;
        const auto itemPlan = buildTemplateRequestPlan(
            settings,
            m_settingsPath,
            itemRoot,
            selectedPrinter,
            &errorCode,
            &errorMessage);
        if (!itemPlan.has_value()) {
            const auto message = hasBatchItems
                ? QString::fromUtf8("第 %1 条预览数据失败：%2").arg(itemIndex + 1).arg(errorMessage)
                : errorMessage;
            return jsonResponse(failEnvelope(errorCode, message), 400, extraHeaders);
        }

        if (firstPaper.isEmpty()) {
            firstPaper = printPlanPaperJson(itemPlan->printPlan);
        }
        const auto nextTotalPages = (globalPageNumber - 1) + itemPlan->printPlan.pages.size();
        if (m_limits.maxPreviewPages >= 0 && nextTotalPages > m_limits.maxPreviewPages) {
            return requestTooLargeResponse(
                QString::fromUtf8("预览页数超过本地接口限制。"),
                extraHeaders);
        }

        for (const auto& page : itemPlan->printPlan.pages) {
            pages.append(previewPageJson(
                itemPlan->printPlan,
                page,
                globalPageNumber,
                itemIndex,
                itemPlan->requestId,
                itemPlan->templateKey,
                renderer));
            if (m_limits.maxPreviewResponseBytes >= 0
                && previewResponseBodySize(batchRequestId, requestItems.size(), firstPaper, pages)
                    > m_limits.maxPreviewResponseBytes) {
                return requestTooLargeResponse(
                    QString::fromUtf8("预览响应体超过本地接口限制。"),
                    extraHeaders);
            }
            ++globalPageNumber;
        }
    }

    return jsonResponse(okEnvelope(QJsonObject{
        {"requestId", batchRequestId},
        {"totalItems", requestItems.size()},
        {"totalPages", pages.size()},
        {"paper", firstPaper},
        {"pages", pages},
    }), 200, extraHeaders);
}

} // 命名空间 sleekpr::http
