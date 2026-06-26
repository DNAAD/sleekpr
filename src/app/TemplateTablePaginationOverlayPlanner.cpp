#include "sleekpr/app/TemplateTablePaginationOverlayPlanner.h"

#include <algorithm>

namespace sleekpr::app {

namespace {

constexpr double kMinimumOverlayHeightMm = 1.6;
constexpr double kPageBreakOverlayHeightMm = 2.0;
constexpr double kOverlayStackGapMm = 2.2;

bool tableHasPaginationSignals(const sleekpr::core::TableElement& table, const sleekpr::core::TableLayoutResult& layout)
{
    return table.visible
        && table.width > 0.0
        && table.height > 0.0
        && (layout.pages.size() > 1 || !layout.diagnostics.isEmpty() || !layout.errorMessage.trimmed().isEmpty());
}

QRectF clampedOverlayRect(const sleekpr::core::TableElement& table, double yOffsetMm, double heightMm)
{
    const auto safeHeight = std::clamp(heightMm, kMinimumOverlayHeightMm, std::max(kMinimumOverlayHeightMm, table.height));
    const auto maxOffset = std::max(0.0, table.height - safeHeight);
    const auto top = table.y + std::clamp(yOffsetMm, 0.0, maxOffset);
    return QRectF(table.x, top, table.width, safeHeight);
}

QString diagnosticText(const sleekpr::core::TableLayoutDiagnostic& diagnostic)
{
    const auto rowNumber = diagnostic.sourceRowIndex >= 0 ? diagnostic.sourceRowIndex + 1 : 0;
    switch (diagnostic.code) {
    case sleekpr::core::TableLayoutDiagnosticCode::OrphanRowsMoved:
        return rowNumber > 0
            ? QString::fromUtf8("孤行保护：第%1行起换页").arg(rowNumber)
            : QString::fromUtf8("孤行保护");
    case sleekpr::core::TableLayoutDiagnosticCode::GroupMovedToNextPage:
        return rowNumber > 0
            ? QString::fromUtf8("分组不拆分：第%1行起换页").arg(rowNumber)
            : QString::fromUtf8("分组不拆分");
    case sleekpr::core::TableLayoutDiagnosticCode::MergeRegionCrossesPage:
        return diagnostic.mergeId.trimmed().isEmpty()
            ? QString::fromUtf8("跨页合并")
            : QString::fromUtf8("跨页合并：%1").arg(diagnostic.mergeId);
    }
    return QString::fromUtf8("分页提示");
}

bool diagnosticIsSevere(sleekpr::core::TableLayoutDiagnosticCode code)
{
    return code == sleekpr::core::TableLayoutDiagnosticCode::MergeRegionCrossesPage;
}

double sourceRowOffsetForDiagnostic(
    const sleekpr::core::TableElement& table,
    const sleekpr::core::TableLayoutResult& layout,
    const sleekpr::core::TableLayoutDiagnostic& diagnostic)
{
    if (diagnostic.sourceRowIndex < 0) {
        return 0.0;
    }

    for (const auto& page : layout.pages) {
        for (const auto& cell : page.cells) {
            if (cell.sourceRowIndex == diagnostic.sourceRowIndex) {
                return cell.rect.y();
            }
        }
    }
    return std::min(table.height * 0.5, std::max(0.0, table.height - kMinimumOverlayHeightMm));
}

void appendPageBreakOverlays(
    QList<TablePaginationOverlay>* overlays,
    const sleekpr::core::TableElement& table,
    const sleekpr::core::TableLayoutResult& layout)
{
    if (overlays == nullptr || layout.pages.size() <= 1) {
        return;
    }

    for (int pageIndex = 1; pageIndex < layout.pages.size(); ++pageIndex) {
        const auto& page = layout.pages[pageIndex];
        const auto yOffset = std::max(0.0, table.height - kPageBreakOverlayHeightMm - (pageIndex - 1) * kOverlayStackGapMm);

        TablePaginationOverlay overlay;
        overlay.tableId = table.id;
        overlay.kind = TablePaginationOverlayKind::PageBreak;
        overlay.rectMm = clampedOverlayRect(table, yOffset, kPageBreakOverlayHeightMm);
        overlay.text = QString::fromUtf8("分页到第%1页：第%2行起")
                           .arg(page.pageNumber)
                           .arg(page.firstRowIndex + 1);
        overlay.pageNumber = page.pageNumber;
        overlays->append(overlay);
    }
}

void appendRepeatedHeaderOverlay(
    QList<TablePaginationOverlay>* overlays,
    const sleekpr::core::TableElement& table,
    const sleekpr::core::TableLayoutResult& layout)
{
    if (overlays == nullptr || layout.pages.size() <= 1 || !table.pagination.repeatHeaderOnPage) {
        return;
    }

    const auto repeatedPageCount = layout.pages.size() - 1;
    TablePaginationOverlay overlay;
    overlay.tableId = table.id;
    overlay.kind = TablePaginationOverlayKind::RepeatedHeader;
    overlay.rectMm = clampedOverlayRect(table, 0.0, std::max(kMinimumOverlayHeightMm, std::min(table.headerRowHeightMm, 3.2)));
    overlay.text = QString::fromUtf8("重复表头 x%1").arg(repeatedPageCount);
    overlay.pageNumber = 2;
    overlays->append(overlay);
}

void appendDiagnosticOverlays(
    QList<TablePaginationOverlay>* overlays,
    const sleekpr::core::TableElement& table,
    const sleekpr::core::TableLayoutResult& layout)
{
    if (overlays == nullptr) {
        return;
    }

    for (int index = 0; index < layout.diagnostics.size(); ++index) {
        const auto& diagnostic = layout.diagnostics[index];
        const auto yOffset = sourceRowOffsetForDiagnostic(table, layout, diagnostic) + index * 0.6;

        TablePaginationOverlay overlay;
        overlay.tableId = table.id;
        overlay.kind = TablePaginationOverlayKind::Warning;
        overlay.rectMm = clampedOverlayRect(table, yOffset, 2.8);
        overlay.text = diagnosticText(diagnostic);
        overlay.pageNumber = diagnostic.pageNumber;
        overlay.severe = diagnosticIsSevere(diagnostic.code);
        overlays->append(overlay);
    }

    if (layout.diagnostics.isEmpty() && !layout.errorMessage.trimmed().isEmpty()) {
        TablePaginationOverlay overlay;
        overlay.tableId = table.id;
        overlay.kind = TablePaginationOverlayKind::Warning;
        overlay.rectMm = clampedOverlayRect(table, table.height * 0.5, 3.2);
        overlay.text = layout.errorMessage.trimmed();
        overlay.severe = true;
        overlays->append(overlay);
    }
}

} // namespace

QList<TablePaginationOverlay> TemplateTablePaginationOverlayPlanner::plan(
    const sleekpr::core::TemplateDocument& document,
    const sleekpr::core::TemplateRenderContext& renderContext)
{
    QList<TablePaginationOverlay> overlays;
    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }
        for (const auto& table : layer.tables) {
            const auto layout = sleekpr::core::TableElementLayout::layout(table, renderContext);
            overlays.append(planForTable(table, layout));
        }
    }
    return overlays;
}

QList<TablePaginationOverlay> TemplateTablePaginationOverlayPlanner::planForTable(
    const sleekpr::core::TableElement& table,
    const sleekpr::core::TableLayoutResult& layout)
{
    QList<TablePaginationOverlay> overlays;
    if (!tableHasPaginationSignals(table, layout)) {
        return overlays;
    }

    appendPageBreakOverlays(&overlays, table, layout);
    appendRepeatedHeaderOverlay(&overlays, table, layout);
    appendDiagnosticOverlays(&overlays, table, layout);
    return overlays;
}

} // namespace sleekpr::app
