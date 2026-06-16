#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include "sleekpr/core/printing/PrintUnitConverter.h"
#include "sleekpr/infrastructure/preview/QrCodeMatrixRenderer.h"

#include <QBuffer>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QTextOption>

#include <exception>

namespace sleekpr::infrastructure {

QImage LabelPreviewImageRenderer::renderImage(const sleekpr::core::NativeLabelDrawingPlan& plan, double dpi) const
{
    const sleekpr::core::PrintUnitConverter converter(dpi);
    const auto width = converter.millimetersToPixels(plan.paperSize.widthMm());
    const auto height = converter.millimetersToPixels(plan.paperSize.heightMm());
    const auto scale = dpi / 25.4;

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.setDotsPerMeterX(static_cast<int>((dpi / 25.4) * 1000.0));
    image.setDotsPerMeterY(static_cast<int>((dpi / 25.4) * 1000.0));
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setPen(Qt::black);

    for (const auto& command : plan.commands) {
        const QRectF rect(command.x * scale, command.y * scale, command.width * scale, command.height * scale);
        if (command.type == sleekpr::core::NativeDrawCommandType::Rectangle) {
            painter.drawRect(rect);
            continue;
        }
        if (command.type == sleekpr::core::NativeDrawCommandType::QrCode) {
            drawQrCode(painter, command, scale);
            continue;
        }

        // 字号仍使用 pt，由 Qt 根据目标设备换算；坐标和区域大小保持毫米计划换算后的像素值。
        QFont font("Microsoft YaHei");
        font.setPointSizeF(command.fontSizePt);
        font.setBold(command.bold);
        painter.setFont(font);

        QTextOption option;
        option.setWrapMode(command.ellipsis ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        option.setAlignment(Qt::AlignLeft | Qt::AlignTop);

        painter.save();
        if (command.rotationDegrees != 0.0) {
            painter.translate(rect.left(), rect.bottom());
            painter.rotate(-command.rotationDegrees);
            painter.drawText(QRectF(0, 0, rect.height(), rect.width()), command.text, option);
        } else {
            painter.drawText(rect, command.text, option);
        }
        painter.restore();
    }

    return image;
}

QByteArray LabelPreviewImageRenderer::renderPng(const sleekpr::core::NativeLabelDrawingPlan& plan, double dpi) const
{
    const auto image = renderImage(plan, dpi);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

void LabelPreviewImageRenderer::drawQrCode(QPainter& painter, const sleekpr::core::NativeDrawCommand& command, double scale)
{
    const QRectF rect(command.x * scale, command.y * scale, command.width * scale, command.height * scale);
    QrCodeMatrix matrix;
    try {
        matrix = QrCodeMatrixRenderer().render(command.text);
    } catch (const std::exception&) {
        // 二维码内容异常时仍画出占位框，避免预览窗口或打印任务因为单个字段直接崩溃。
        painter.drawRect(rect);
        painter.drawText(rect, Qt::AlignCenter, QStringLiteral("QR"));
        return;
    }
    const auto moduleSize = rect.width() / matrix.size();

    // 二维码模块必须逐格填充，禁止缩放位图；这样预览和后续真实打印都能保持清晰边缘。
    for (int row = 0; row < matrix.size(); ++row) {
        for (int column = 0; column < matrix.size(); ++column) {
            if (matrix.hasDarkModule(column, row)) {
                painter.fillRect(
                    QRectF(rect.left() + column * moduleSize, rect.top() + row * moduleSize, moduleSize, moduleSize),
                    Qt::black);
            }
        }
    }
}

} // 命名空间 sleekpr::infrastructure
