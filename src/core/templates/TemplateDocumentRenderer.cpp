#include "sleekpr/core/templates/TemplateDocumentRenderer.h"

#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/templates/TableElementRenderer.h"

#include <algorithm>

namespace sleekpr::core {
namespace {

double usableScale(double value)
{
    return value > 0.0 ? value : 1.0;
}

double usableRenderDpi(double value)
{
    return value > 0.0 ? value : 300.0;
}

QList<TemplateElement> visibleElementsInRenderOrder(const TemplateDocument& document)
{
    QList<TemplateElement> result;
    int renderZIndex = 0;

    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }

        QList<TemplateElement> layerElements;
        for (const auto& element : layer.elements) {
            if (element.visible) {
                layerElements.append(element);
            }
        }

        // 图层内按 zIndex 稳定排序；随后重写渲染序号，避免跨图层再次排序时打乱图层顺序。
        std::stable_sort(layerElements.begin(), layerElements.end(), [](const TemplateElement& left, const TemplateElement& right) {
            return left.zIndex < right.zIndex;
        });

        for (auto element : layerElements) {
            element.zIndex = renderZIndex++;
            result.append(element);
        }
    }

    return result;
}

QList<TableElement> visibleTablesInRenderOrder(const TemplateDocument& document)
{
    QList<TableElement> result;
    int renderZIndex = 0;

    for (const auto& layer : document.layers) {
        if (!layer.visible) {
            continue;
        }

        QList<TableElement> layerTables;
        for (const auto& table : layer.tables) {
            if (table.visible) {
                layerTables.append(table);
            }
        }

        // 表格和普通元素分开追加，但表格内部仍保持图层顺序与 zIndex 稳定排序。
        std::stable_sort(layerTables.begin(), layerTables.end(), [](const TableElement& left, const TableElement& right) {
            return left.zIndex < right.zIndex;
        });

        for (auto table : layerTables) {
            table.zIndex = renderZIndex++;
            result.append(table);
        }
    }

    return result;
}

void applyDeviceScale(NativeLabelDrawingPlan& drawingPlan, const DeviceProfile& profile)
{
    const auto scaleX = usableScale(profile.scaleX);
    const auto scaleY = usableScale(profile.scaleY);

    for (auto& command : drawingPlan.commands) {
        // profile 缩放只处理毫米坐标和区域尺寸，不触碰文本、二维码载荷或字体语义。
        command.x *= scaleX;
        command.y *= scaleY;
        command.width *= scaleX;
        command.height *= scaleY;
    }
}

} // 匿名命名空间

NativeLabelDrawingPlan TemplateDocumentRenderer::render(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const LabelOffset& labelOffset,
    const DeviceProfile& profile) const
{
    return render(document, labelPlan, labelOffset, profile, TemplateRenderContext{});
}

NativeLabelDrawingPlan TemplateDocumentRenderer::render(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const LabelOffset& labelOffset,
    const DeviceProfile& profile,
    const TemplateRenderContext& context) const
{
    NativeLabelDrawingPlan drawingPlan;
    drawingPlan.paperSize = labelPlan.paperSize;
    drawingPlan.renderDpi = usableRenderDpi(profile.dpi);

    const auto renderElements = visibleElementsInRenderOrder(document);
    const auto offsetX = labelOffset.x + profile.offsetX;
    const auto offsetY = labelOffset.y + profile.offsetY;
    drawingPlan.commands = NativeLabelDrawingPlanner::appendTemplateElements({}, labelPlan, renderElements, offsetX, offsetY);

    const auto renderTables = visibleTablesInRenderOrder(document);
    for (const auto& table : renderTables) {
        const auto tableResult = TableElementRenderer::renderSinglePage(table, context, offsetX, offsetY);
        if (!tableResult.success()) {
            // 当前渲染计划没有错误通道；表格入口校验会在后续打印流程统一阻止异常模板。
            continue;
        }
        drawingPlan.commands.append(tableResult.commands);
    }

    applyDeviceScale(drawingPlan, profile);
    return drawingPlan;
}

} // 命名空间 sleekpr::core
