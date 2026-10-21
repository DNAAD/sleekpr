#include "sleekpr/infrastructure/preview/TemplatePreviewJob.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateDocumentValidator.h"
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

namespace sleekpr::infrastructure {

PreviewWorkResult TemplatePreviewJob::run(const TemplatePreviewSnapshot& snapshot)
{
    PreviewWorkResult result;
    QElapsedTimer elapsed;
    elapsed.start();
    const auto& resolved = snapshot.resolved;
    auto budget = snapshot.planningBudget;
    const auto previousPages = budget.usedPages;
    const auto failBudget = [&] {
        result.plan.reset();
        result.errorCode = QStringLiteral("REQUEST_TOO_LARGE");
        result.errorMessage = budget.message;
    };
    if (!budget.ensurePages(1)) {
        failBudget();
    } else if (snapshot.preparedPlan) {
        result.plan = snapshot.preparedPlan;
        // 缓存计划不再展开；本次栅格化任务只处理所选的一页。
        if (!budget.consumePages(1) || (snapshot.pageIndex >= 0 && snapshot.pageIndex < result.plan->pages.size()
                && !budget.consumeCommands(result.plan->pages.at(snapshot.pageIndex).commands.size()))) failBudget();
    } else if (snapshot.legacySettings) {
        const auto& settings = *snapshot.legacySettings;
        result.plan = core::NativePrintDrawingPlan::fromSinglePage(core::NativeLabelDrawingPlanner().createPlan(
            resolved.labelPlan, resolved.labelOffset, settings.templateOverrides.value(resolved.templateKey),
            settings.templateElements.value(resolved.templateKey)));
        if (!budget.consumePages(1) || !budget.consumeCommands(result.plan->pages.first().commands.size())) failBudget();
    } else if (snapshot.designMode) {
        const auto rendered = core::TemplateDocumentRenderer().tryRenderPrint(resolved.document, resolved.labelPlan,
            resolved.labelOffset, resolved.deviceProfile, resolved.context, &budget);
        if (rendered.success()) result.plan = rendered.plan;
        else {
            result.errorCode = budget.ok() ? QStringLiteral("TEMPLATE_RENDER_FAILED") : QStringLiteral("REQUEST_TOO_LARGE");
            result.errorMessage = rendered.errorMessage;
        }
        // 设计期仍展示可修正的版式，校验结果只作提示；正式工作台通过 prepareSnapshot 阻断非法输入。
        const auto validation = core::TemplateDocumentValidator().validate(resolved.document,
            QSizeF(resolved.labelPlan.paperSize.widthMm(), resolved.labelPlan.paperSize.heightMm()));
        for (const auto& issue : validation.issues) {
            result.preflight.issues.append({issue.severity == core::TemplateValidationSeverity::Error
                    ? core::PrintPreflightSeverity::Error : core::PrintPreflightSeverity::Warning,
                issue.code, issue.message});
        }
    } else {
        const auto prepared = TemplatePrintService::prepareSnapshot(resolved, &result.preflight,
            &result.errorCode, &result.errorMessage, &budget);
        if (prepared) result.plan = prepared->printPlan;
    }
    if (snapshot.selectedTable && result.plan) {
        result.tableId = snapshot.selectedTable->id;
        const auto generatedPages = budget.usedPages;
        // 分页提示复用同一文档的页额度，布局工作量仍累计；它不额外产生打印页面。
        budget.usedPages = previousPages;
        result.tableLayout = core::TableElementLayout::layout(*snapshot.selectedTable, resolved.context, &budget);
        budget.usedPages = generatedPages;
        if (!budget.ok()) failBudget();
        else if (!result.tableLayout->success()) {
            result.plan.reset();
            result.errorCode = QStringLiteral("TEMPLATE_RENDER_FAILED");
            result.errorMessage = result.tableLayout->errorMessage;
        }
    }
    result.planningNanoseconds = elapsed.nsecsElapsed();
    ClientPerformanceMetrics::applicationMetrics().record(PerformanceStage::Planning, result.planningNanoseconds);
    if (!result.plan) return result;
    if (!snapshot.renderImage) return result;
    if (snapshot.pageIndex < 0 || snapshot.pageIndex >= result.plan->pages.size()) {
        result.errorCode = QStringLiteral("PREVIEW_PAGE_INVALID");
        result.errorMessage = QString::fromUtf8("预览页码无效");
        return result;
    }
    result.baseDpi = snapshot.maxPreviewDpi > 0 ? std::min(result.plan->renderDpi, snapshot.maxPreviewDpi) : result.plan->renderDpi;
    auto page = result.plan->firstPageAsLabelPlan();
    page.commands = result.plan->pages.at(snapshot.pageIndex).commands;
    elapsed.restart();
    const auto rendered = LabelPreviewImageRenderer().tryRenderImage(page,
        {QSizeF(page.paperSize.widthMm(), page.paperSize.heightMm()), result.baseDpi, snapshot.zoom, snapshot.devicePixelRatio}, snapshot.imageBudget);
    result.image = rendered.image;
    result.rasterNanoseconds = elapsed.nsecsElapsed();
    ClientPerformanceMetrics::applicationMetrics().record(PerformanceStage::Rasterization, result.rasterNanoseconds);
    if (!rendered.ok()) {
        result.errorCode = rendered.error == core::PreviewImageError::ResourceLimit
            ? QStringLiteral("REQUEST_TOO_LARGE") : QStringLiteral("PREVIEW_RENDER_FAILED");
        result.errorMessage = rendered.errorMessage;
    }
    return result;
}

} // 命名空间 sleekpr::infrastructure
