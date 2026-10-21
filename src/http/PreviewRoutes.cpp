#include "sleekpr/http/PreviewRoutes.h"
#include "sleekpr/http/PreviewResponseBudget.h"

#include "sleekpr/core/settings/PrinterSelectionResolver.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"
#include "sleekpr/http/TemplateRequestPlanner.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>
#include <limits>
#include <new>

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
    const QByteArray& png)
{
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

} // 匿名命名空间

PreviewRoutes::PreviewRoutes(QString settingsPath, LocalHttpLimits limits, LabelPreviewImageRenderer::PngEncoder encoder)
    : m_settingsPath(std::move(settingsPath))
    , m_limits(limits)
    , m_encoder(std::move(encoder))
{
}

std::optional<LocalHttpResponse> PreviewRoutes::route(
    const LocalHttpRequest& request,
    const sleekpr::core::PrintClientSettings& settings,
    const QHash<QByteArray, QByteArray>& extraHeaders) const
try {
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

    LabelPreviewImageRenderer renderer(m_encoder);
    const auto maximumResponse = m_limits.maxPreviewResponseBytes < 0
        ? std::numeric_limits<qsizetype>::max() : m_limits.maxPreviewResponseBytes;
    PreviewResponseBudget responseBudget(maximumResponse);
    core::PreviewPlanningBudget planningBudget;
    planningBudget.maxPages = m_limits.maxPreviewPages < 0
        ? std::numeric_limits<qsizetype>::max() : m_limits.maxPreviewPages;
    QJsonArray pages;
    QJsonObject firstPaper;
    int globalPageNumber = 1;
    const auto initialBytes = responseBudget.bodyBytes(batchRequestId, static_cast<int>(requestItems.size()), 0, {});
    if (initialBytes < 0 || initialBytes > maximumResponse) {
        return requestTooLargeResponse(QString::fromUtf8("预览响应包络超过本地接口限制。"), extraHeaders);
    }

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
            &errorMessage,
            &planningBudget);
        if (!itemPlan.has_value()) {
            const auto message = hasBatchItems
                ? QString::fromUtf8("第 %1 条预览数据失败：%2").arg(itemIndex + 1).arg(errorMessage)
                : errorMessage;
            if (errorCode == QStringLiteral("REQUEST_TOO_LARGE")) return requestTooLargeResponse(message, extraHeaders);
            return jsonResponse(failEnvelope(errorCode, message), 400, extraHeaders);
        }

        if (firstPaper.isEmpty()) {
            firstPaper = printPlanPaperJson(itemPlan->printPlan);
        }
        if (itemPlan->printPlan.pages.isEmpty()) {
            return jsonResponse(failEnvelope("PREVIEW_RENDER_FAILED", QString::fromUtf8("预览未生成有效页面")), 500, extraHeaders);
        }
        const auto nextTotalPages = (globalPageNumber - 1) + itemPlan->printPlan.pages.size();
        if (m_limits.maxPreviewPages >= 0 && nextTotalPages > m_limits.maxPreviewPages) {
            return requestTooLargeResponse(
                QString::fromUtf8("预览页数超过本地接口限制。"),
                extraHeaders);
        }

        for (const auto& page : itemPlan->printPlan.pages) {
            auto pageJson = previewPageJson(itemPlan->printPlan, page, globalPageNumber, itemIndex,
                itemPlan->requestId, itemPlan->templateKey, {});
            const auto baseBytes = responseBudget.bodyBytes(batchRequestId, static_cast<int>(requestItems.size()),
                static_cast<int>(pages.size()) + 1, firstPaper);
            const auto metadataBytes = QJsonDocument(pageJson).toJson(QJsonDocument::Compact).size();
            const qsizetype comma = pages.isEmpty() ? 0 : 1;
            if (baseBytes < 0 || baseBytes > maximumResponse || metadataBytes > maximumResponse - baseBytes
                || comma > maximumResponse - baseBytes - metadataBytes) {
                return requestTooLargeResponse(QString::fromUtf8("预览页面元数据超过响应预算。"), extraHeaders);
            }
            const auto availableBase64 = maximumResponse - baseBytes - metadataBytes - comma;
            const auto maximumPng = availableBase64 / 4 * 3;
            const auto label = labelPlanForPreviewPage(itemPlan->printPlan, page);
            const auto rendered = renderer.tryRenderPng(label,
                {QSizeF(label.paperSize.widthMm(), label.paperSize.heightMm()), itemPlan->printPlan.renderDpi}, {}, maximumPng);
            if (!rendered.ok()) {
                if (rendered.error == core::PreviewImageError::ResourceLimit) {
                    return requestTooLargeResponse(rendered.errorMessage, extraHeaders);
                }
                return jsonResponse(failEnvelope("PREVIEW_RENDER_FAILED", rendered.errorMessage), 500, extraHeaders);
            }
            pageJson.insert(QStringLiteral("imageBase64"), QString::fromLatin1(rendered.png.toBase64()));
            if (!responseBudget.appendPage(pageJson)) {
                return requestTooLargeResponse(
                    QString::fromUtf8("预览响应体超过本地接口限制。"),
                    extraHeaders);
            }
            const auto bytes = responseBudget.bodyBytes(batchRequestId, static_cast<int>(requestItems.size()),
                static_cast<int>(pages.size()) + 1, firstPaper);
            if (bytes < 0 || bytes > maximumResponse) return requestTooLargeResponse(QString::fromUtf8("预览响应体超过本地接口限制。"), extraHeaders);
            pages.append(pageJson);
            ++globalPageNumber;
        }
    }

    const auto response = jsonResponse(okEnvelope(QJsonObject{
        {"requestId", batchRequestId},
        {"totalItems", requestItems.size()},
        {"totalPages", pages.size()},
        {"paper", firstPaper},
        {"pages", pages},
    }), 200, extraHeaders);
    // 完整序列化仅在成功前执行一次精确复核，不能用估计值取代最后的字节上限。
    if (response.body.size() > maximumResponse) return requestTooLargeResponse(QString::fromUtf8("预览响应体超过本地接口限制。"), extraHeaders);
    return response;
} catch (const std::bad_alloc&) {
    return jsonResponse(failEnvelope("PREVIEW_RENDER_FAILED", QString::fromUtf8("预览内存分配失败")), 500, extraHeaders);
} catch (...) {
    return jsonResponse(failEnvelope("PREVIEW_RENDER_FAILED", QString::fromUtf8("预览计算失败")), 500, extraHeaders);
}

} // 命名空间 sleekpr::http
