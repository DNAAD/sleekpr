#include "sleekpr/infrastructure/printing/QtLabelPrintEngine.h"

#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrinter>

namespace sleekpr::infrastructure {

bool QtLabelPrintEngine::print(const sleekpr::core::NativeLabelDrawingPlan& plan, const QString& printerName)
{
    QPrinter printer(QPrinter::HighResolution);
    if (!printerName.trimmed().isEmpty()) {
        printer.setPrinterName(printerName.trimmed());
    }

    const auto printerDpi = printer.resolution() > 0 ? static_cast<double>(printer.resolution()) : 300.0;
    const auto dpi = plan.renderDpi > 0.0 ? plan.renderDpi : printerDpi;
    // 设备 profile 的 DPI 同时用于图片生成和 QPrinter 分辨率，避免毫米坐标在真实设备上被二次缩放。
    printer.setResolution(static_cast<int>(dpi + 0.5));

    // 真实打印保持 80x30mm 页面和零边距，渲染图像仍由统一预览渲染器生成，避免两套绘制逻辑。
    const QPageSize pageSize(QSizeF(plan.paperSize.widthMm(), plan.paperSize.heightMm()), QPageSize::Millimeter);
    printer.setPageSize(pageSize);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

    QPainter painter(&printer);
    if (!painter.isActive()) {
        return false;
    }

    const QImage image = LabelPreviewImageRenderer().renderImage(plan, dpi);
    if (image.isNull()) {
        return false;
    }

    painter.drawImage(printer.pageRect(QPrinter::DevicePixel), image);
    return true;
}

} // 命名空间 sleekpr::infrastructure
