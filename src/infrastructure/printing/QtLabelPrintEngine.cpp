#include "sleekpr/infrastructure/printing/QtLabelPrintEngine.h"

#include "sleekpr/core/native/NativeDrawCommandType.h"
#include "sleekpr/infrastructure/preview/QrCodeMatrixRenderer.h"
#include "sleekpr/infrastructure/printing/QrModulePixelLayout.h"
#include "sleekpr/infrastructure/rendering/TextAutoFitSizer.h"

#include <QColor>
#include <QFont>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QPrinter>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <exception>

namespace sleekpr::infrastructure {

namespace {

struct PagePixelMetrics
{
    QRectF pageRect;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double dpiX = 203.0;
    double dpiY = 203.0;
};

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

    const auto dpi = effectiveDpi(plan, printer);
    printer.setResolution(static_cast<int>(dpi + 0.5));

    const QPageSize pageSize(QSizeF(plan.paperSize.widthMm(), plan.paperSize.heightMm()), QPageSize::Millimeter);
    printer.setPageSize(pageSize);
    printer.setFullPage(true);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
}

PagePixelMetrics pagePixelMetrics(const QPrinter& printer, const sleekpr::core::NativePrintDrawingPlan& plan)
{
    const auto dpi = effectiveDpi(plan, printer);
    PagePixelMetrics metrics;
    metrics.pageRect = QRectF(printer.pageRect(QPrinter::DevicePixel));
    if (metrics.pageRect.width() <= 0.0 || metrics.pageRect.height() <= 0.0) {
        metrics.pageRect = QRectF(
            0.0,
            0.0,
            plan.paperSize.widthMm() * dpi / 25.4,
            plan.paperSize.heightMm() * dpi / 25.4);
    }

    // 以打印机实际报告的 DevicePixel 页面为准，避免整页位图被驱动二次缩放。
    metrics.scaleX = metrics.pageRect.width() / std::max(1.0, plan.paperSize.widthMm());
    metrics.scaleY = metrics.pageRect.height() / std::max(1.0, plan.paperSize.heightMm());
    metrics.dpiX = metrics.scaleX * 25.4;
    metrics.dpiY = metrics.scaleY * 25.4;
    return metrics;
}

QRectF pixelAlignedRect(const sleekpr::core::NativeDrawCommand& command, const PagePixelMetrics& metrics)
{
    const auto left = metrics.pageRect.left() + command.x * metrics.scaleX;
    const auto top = metrics.pageRect.top() + command.y * metrics.scaleY;
    const auto width = command.width * metrics.scaleX;
    const auto height = command.height * metrics.scaleY;
    return QRectF(
        std::round(left),
        std::round(top),
        std::max(1.0, std::round(width)),
        std::max(1.0, std::round(height)));
}

QFont commandFont(const sleekpr::core::NativeDrawCommand& command, const PagePixelMetrics& metrics, double fontSizePt)
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    const auto averageDpi = (metrics.dpiX + metrics.dpiY) / 2.0;
    const auto pixelSize = std::max(1, static_cast<int>(std::round(fontSizePt * averageDpi / 72.0)));
    font.setPixelSize(pixelSize);
    font.setBold(command.bold);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void rotateAroundCenter(QPainter& painter, const QRectF& rect, double rotationDegrees)
{
    if (rotationDegrees == 0.0) {
        return;
    }
    painter.translate(rect.center());
    painter.rotate(-rotationDegrees);
    painter.translate(-rect.center());
}

bool isTopToBottomTextRotation(double rotationDegrees)
{
    auto normalized = std::fmod(rotationDegrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return std::abs(normalized - 90.0) < 0.001;
}

QColor colorOrDefault(const QString& rawColor, const QColor& fallback)
{
    const QColor color(rawColor.trimmed());
    return color.isValid() ? color : fallback;
}

double borderWidthPixels(const sleekpr::core::NativeDrawCommand& command, const PagePixelMetrics& metrics)
{
    if (command.borderWidthMm <= 0.0) {
        return 1.0;
    }
    return std::max(1.0, command.borderWidthMm * (metrics.scaleX + metrics.scaleY) / 2.0);
}

void drawRectangle(QPainter& painter, const sleekpr::core::NativeDrawCommand& command, const PagePixelMetrics& metrics)
{
    const auto rect = pixelAlignedRect(command, metrics);
    painter.save();
    rotateAroundCenter(painter, rect, command.rotationDegrees);
    if (!command.backgroundColor.trimmed().isEmpty()) {
        painter.fillRect(rect, colorOrDefault(command.backgroundColor, Qt::transparent));
    }
    painter.setPen(QPen(Qt::black, borderWidthPixels(command, metrics)));
    painter.drawRect(rect.adjusted(0.0, 0.0, -1.0, -1.0));
    painter.restore();
}

void drawQrFallback(QPainter& painter, const QRectF& rect)
{
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(rect.adjusted(0.0, 0.0, -1.0, -1.0));
    painter.drawText(rect, Qt::AlignCenter, QStringLiteral("QR"));
}

void drawQrCode(QPainter& painter, const sleekpr::core::NativeDrawCommand& command, const PagePixelMetrics& metrics)
{
    const auto rect = pixelAlignedRect(command, metrics);

    QrCodeMatrix matrix;
    try {
        matrix = QrCodeMatrixRenderer().render(command.text);
    } catch (const std::exception&) {
        painter.save();
        rotateAroundCenter(painter, rect, command.rotationDegrees);
        drawQrFallback(painter, rect);
        painter.restore();
        return;
    }

    const auto maxPixelSize = static_cast<int>(std::floor(std::min(rect.width(), rect.height())));
    const auto qrPixelSize = maxPixelSize;
    const auto moduleBounds = QrModulePixelLayout::moduleBounds(matrix.size(), qrPixelSize);
    const auto left = static_cast<int>(std::round(rect.left() + (rect.width() - qrPixelSize) / 2.0));
    const auto top = static_cast<int>(std::round(rect.top() + (rect.height() - qrPixelSize) / 2.0));

    painter.save();
    rotateAroundCenter(painter, rect, command.rotationDegrees);
    // 二维码按整数边界填充，同时保持整体尺寸等于模板设计尺寸。
    for (int row = 0; row < matrix.size(); ++row) {
        for (int column = 0; column < matrix.size(); ++column) {
            if (matrix.hasDarkModule(column, row)) {
                const auto moduleLeft = moduleBounds.at(column);
                const auto moduleTop = moduleBounds.at(row);
                const auto moduleWidth = moduleBounds.at(column + 1) - moduleLeft;
                const auto moduleHeight = moduleBounds.at(row + 1) - moduleTop;
                if (moduleWidth <= 0 || moduleHeight <= 0) {
                    continue;
                }
                painter.fillRect(
                    QRect(left + moduleLeft, top + moduleTop, moduleWidth, moduleHeight),
                    Qt::black);
            }
        }
    }
    painter.restore();
}

void drawText(QPainter& painter, const sleekpr::core::NativeDrawCommand& command, const PagePixelMetrics& metrics)
{
    const auto rect = pixelAlignedRect(command, metrics);

    QTextOption option;
    option.setWrapMode(command.wrapText || command.ellipsis ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    option.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    const auto layoutSize = command.rotationDegrees == 0.0
        ? QSizeF(rect.width(), rect.height())
        : QSizeF(rect.height(), rect.width());
    const auto effectiveFontSizePt = TextAutoFitSizer::fitPointSize(command, layoutSize, metrics.dpiX, metrics.dpiY);

    painter.save();
    painter.setFont(commandFont(command, metrics, effectiveFontSizePt));
    painter.setPen(colorOrDefault(command.textColor, Qt::black));
    if (command.rotationDegrees != 0.0) {
        if (isTopToBottomTextRotation(command.rotationDegrees)) {
            painter.translate(rect.right(), rect.top());
            painter.rotate(90.0);
        } else {
            painter.translate(rect.left(), rect.bottom());
            painter.rotate(-command.rotationDegrees);
        }
        const QRectF rotatedRect(0.0, 0.0, rect.height(), rect.width());
        // 旋转 90 度的编号按从上往下阅读的方向绘制，避免竖向编号倒读。
        painter.setClipRect(rotatedRect);
        painter.drawText(rotatedRect, command.text, option);
    } else {
        painter.setClipRect(rect);
        painter.drawText(rect, command.text, option);
    }
    painter.restore();
}

void drawCommand(QPainter& painter, const sleekpr::core::NativeDrawCommand& command, const PagePixelMetrics& metrics)
{
    if (command.type == sleekpr::core::NativeDrawCommandType::Rectangle) {
        drawRectangle(painter, command, metrics);
        return;
    }
    if (command.type == sleekpr::core::NativeDrawCommandType::QrCode) {
        drawQrCode(painter, command, metrics);
        return;
    }
    drawText(painter, command, metrics);
}

} // 匿名命名空间

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

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (qsizetype pageIndex = 0; pageIndex < plan.pages.size(); ++pageIndex) {
        if (pageIndex > 0 && !printer.newPage()) {
            return false;
        }

        const auto metrics = pagePixelMetrics(printer, plan);
        for (const auto& command : plan.pages.at(pageIndex).commands) {
            drawCommand(painter, command, metrics);
        }
    }

    return true;
}

} // 命名空间 sleekpr::infrastructure
