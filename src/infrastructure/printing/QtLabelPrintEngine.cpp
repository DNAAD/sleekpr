#include "sleekpr/infrastructure/printing/QtLabelPrintEngine.h"

#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include <QImage>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrinter>

namespace sleekpr::infrastructure {

namespace {

sleekpr::core::NativeLabelDrawingPlan labelPlanForPage(
    const sleekpr::core::NativePrintDrawingPlan& plan,
    const sleekpr::core::NativePrintPage& page)
{
    sleekpr::core::NativeLabelDrawingPlan result;
    result.paperSize = plan.paperSize;
    result.renderDpi = plan.renderDpi;
    result.commands = page.commands;
    return result;
}

double effectiveDpi(const sleekpr::core::NativePrintDrawingPlan& plan, const QPrinter& printer)
{
    const auto printerDpi = printer.resolution() > 0 ? static_cast<double>(printer.resolution()) : 300.0;
    return plan.renderDpi > 0.0 ? plan.renderDpi : printerDpi;
}

void configurePrinter(QPrinter& printer, const sleekpr::core::NativePrintDrawingPlan& plan, const QString& printerName)
{
    if (!printerName.trimmed().isEmpty()) {
        printer.setPrinterName(printerName.trimmed());
    }

    // 设备 profile 的 DPI 同时用于图片生成和 QPrinter 分辨率，避免毫米坐标在真实设备上被二次缩放。
    const auto dpi = effectiveDpi(plan, printer);
    printer.setResolution(static_cast<int>(dpi + 0.5));

    // 页面尺寸由打印计划提供，既兼容 80x30 标签，也兼容后续单据和复杂表格纸张。
    const QPageSize pageSize(QSizeF(plan.paperSize.widthMm(), plan.paperSize.heightMm()), QPageSize::Millimeter);
    printer.setPageSize(pageSize);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
}

} // 命名空间

bool QtLabelPrintEngine::print(const sleekpr::core::NativeLabelDrawingPlan& plan, const QString& printerName)
{
    return print(sleekpr::core::NativePrintDrawingPlan::fromSinglePage(plan), printerName);
}

bool QtLabelPrintEngine::print(const sleekpr::core::NativePrintDrawingPlan& plan, const QString& printerName)
{
    if (plan.pages.isEmpty()) {
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);
    configurePrinter(printer, plan, printerName);

    QPainter painter(&printer);
    if (!painter.isActive()) {
        return false;
    }

    const auto dpi = effectiveDpi(plan, printer);
    LabelPreviewImageRenderer renderer;
    for (qsizetype pageIndex = 0; pageIndex < plan.pages.size(); ++pageIndex) {
        if (pageIndex > 0 && !printer.newPage()) {
            return false;
        }

        const QImage image = renderer.renderImage(labelPlanForPage(plan, plan.pages[pageIndex]), dpi);
        if (image.isNull()) {
            return false;
        }

        painter.drawImage(printer.pageRect(QPrinter::DevicePixel), image);
    }

    return true;
}

} // 命名空间 sleekpr::infrastructure
