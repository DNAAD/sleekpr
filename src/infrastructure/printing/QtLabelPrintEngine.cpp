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

    // 真实打印保持 80x30mm 页面和零边距，渲染图像仍由统一预览渲染器生成，避免两套绘制逻辑。
    const QPageSize pageSize(QSizeF(plan.paperSize.widthMm(), plan.paperSize.heightMm()), QPageSize::Millimeter);
    printer.setPageSize(pageSize);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

    QPainter painter(&printer);
    if (!painter.isActive()) {
        return false;
    }

    const auto dpi = printer.resolution() > 0 ? static_cast<double>(printer.resolution()) : 300.0;
    const QImage image = LabelPreviewImageRenderer().renderImage(plan, dpi);
    if (image.isNull()) {
        return false;
    }

    painter.drawImage(printer.pageRect(QPrinter::DevicePixel), image);
    return true;
}

} // 命名空间 sleekpr::infrastructure
