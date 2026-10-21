#include "sleekpr/core/printing/PreviewResourceBudget.h"
#include <cmath>
#include <limits>

namespace sleekpr::core {

PreviewImageCheck checkPreviewImageRequest(const PreviewImageRequest& request, const PreviewResourceBudget& budget)
{
    const auto fail = [](PreviewImageError error, const QString& message) {
        return PreviewImageCheck{error, {}, 0, message};
    };
    for (const auto value : {request.paperSizeMm.width(), request.paperSizeMm.height(), request.dpi,
             request.zoom, request.devicePixelRatio}) {
        if (!std::isfinite(value) || value <= 0) return fail(PreviewImageError::InvalidInput,
            QString::fromUtf8("纸张、DPI、倍率与设备像素比必须为有限正数"));
    }
    const long double dpi = static_cast<long double>(request.dpi) * request.zoom * request.devicePixelRatio;
    const long double dotsPerMeter = dpi / 25.4L * 1000;
    // 图像宽高可很小，但分辨率元数据仍存储为 int，不能绕过其可表示范围。
    if (!std::isfinite(dpi) || dotsPerMeter > std::numeric_limits<int>::max()) {
        return fail(PreviewImageError::ResourceLimit, QString::fromUtf8("预览分辨率过大，请降低预览倍率或 DPI"));
    }
    if (dotsPerMeter < 1) return fail(PreviewImageError::InvalidInput, QString::fromUtf8("预览分辨率过小"));
    const long double width = request.paperSizeMm.width() / 25.4L * dpi;
    const long double height = request.paperSizeMm.height() / 25.4L * dpi;
    if (!std::isfinite(width) || !std::isfinite(height) || budget.maxSidePixels < 1
        || width > static_cast<long double>(budget.maxSidePixels) + 0.5L
        || height > static_cast<long double>(budget.maxSidePixels) + 0.5L) {
        return fail(PreviewImageError::ResourceLimit, QString::fromUtf8("预览单边超过 %1 像素，请调整纸张、倍率或 DPI").arg(budget.maxSidePixels));
    }
    if (width < 0.5L || height < 0.5L) return fail(PreviewImageError::InvalidInput, QString::fromUtf8("预览尺寸不足一个像素"));
    const auto pixelsWide = qRound64(static_cast<double>(width));
    const auto pixelsHigh = qRound64(static_cast<double>(height));
    if (pixelsWide > budget.maxSidePixels || pixelsHigh > budget.maxSidePixels) {
        return fail(PreviewImageError::ResourceLimit, QString::fromUtf8("预览单边像素数超限"));
    }
    const auto pixels = static_cast<quint64>(pixelsWide) * static_cast<quint64>(pixelsHigh);
    // ARGB32 每像素四字节；先除后比较，避免字节乘法及 qsizetype 溢出。
    if (pixels > budget.maxPixels || pixels > budget.maxImageBytes / 4
        || pixels > static_cast<quint64>(std::numeric_limits<qsizetype>::max()) / 4) {
        return fail(PreviewImageError::ResourceLimit, QString::fromUtf8("预览总像素或图像字节超限，请减小纸张、倍率或 DPI"));
    }
    PreviewImageCheck result{PreviewImageError::None, QSize(static_cast<int>(pixelsWide), static_cast<int>(pixelsHigh)), static_cast<double>(dpi), {}};
    result.dotsPerMeter = static_cast<int>(dotsPerMeter);
    return result;
}

namespace {

bool checkWork(PreviewPlanningBudget& budget, qsizetype count, qsizetype& used, qsizetype maximum,
    PreviewPlanningLimit kind, bool consume)
{
    if (!budget.ok()) return false;
    if (count < 0 || maximum < 0 || used < 0 || used > maximum || count > maximum - used) {
        budget.exceeded = kind;
        const auto label = kind == PreviewPlanningLimit::Pages ? QString::fromUtf8("页数")
            : kind == PreviewPlanningLimit::Commands ? QString::fromUtf8("绘制命令") : QString::fromUtf8("布局单元格");
        budget.message = QString::fromUtf8("预览%1超过预算（最大 %2），请减少数据或拆分批次").arg(label).arg(maximum);
        return false;
    }
    if (consume) used += count;
    return true;
}

} // 匿名命名空间

bool PreviewPlanningBudget::consumePages(qsizetype count) { return checkWork(*this, count, usedPages, maxPages, PreviewPlanningLimit::Pages, true); }
bool PreviewPlanningBudget::ensurePages(qsizetype count) { return checkWork(*this, count, usedPages, maxPages, PreviewPlanningLimit::Pages, false); }
bool PreviewPlanningBudget::consumeCommands(qsizetype count) { return checkWork(*this, count, usedCommands, maxCommands, PreviewPlanningLimit::Commands, true); }
bool PreviewPlanningBudget::consumeCells(qsizetype count) { return checkWork(*this, count, usedCells, maxCells, PreviewPlanningLimit::Cells, true); }
bool PreviewPlanningBudget::ensureCells(qsizetype count) { return checkWork(*this, count, usedCells, maxCells, PreviewPlanningLimit::Cells, false); }

} // 命名空间 sleekpr::core
