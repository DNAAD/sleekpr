#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include "sleekpr/core/printing/PrintUnitConverter.h"
#include "sleekpr/infrastructure/preview/QrCodeMatrixRenderer.h"

#include <QBuffer>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QTextOption>

#include <cmath>
#include <exception>

namespace sleekpr::infrastructure {

namespace {

bool isTopToBottomTextRotation(double rotationDegrees)
{
    auto normalized = std::fmod(rotationDegrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return std::abs(normalized - 90.0) < 0.001;
}

void drawRotatedText(QPainter& painter, const QRectF& rect, const sleekpr::core::NativeDrawCommand& command, const QTextOption& option)
{
    if (isTopToBottomTextRotation(command.rotationDegrees)) {
        painter.translate(rect.right(), rect.top());
        painter.rotate(90.0);
        const QRectF rotatedRect(0, 0, rect.height(), rect.width());
        // 90 度旋转后的文本从元素顶部向下排列，符合竖向编号的阅读方向。
        painter.setClipRect(rotatedRect);
        painter.drawText(rotatedRect, command.text, option);
        return;
    }

    painter.translate(rect.left(), rect.bottom());
    painter.rotate(-command.rotationDegrees);
    const QRectF rotatedRect(0, 0, rect.height(), rect.width());
    painter.setClipRect(rotatedRect);
    painter.drawText(rotatedRect, command.text, option);
}

} // namespace

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
            painter.save();
            if (command.rotationDegrees != 0.0) {
                // 非文本元素围绕自身中心旋转，避免二维码和矩形在预览/打印时发生拉伸。
                painter.translate(rect.center());
                painter.rotate(-command.rotationDegrees);
                painter.translate(-rect.center());
            }
            painter.drawRect(rect);
            painter.restore();
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
        option.setWrapMode(command.wrapText || command.ellipsis ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        option.setAlignment(Qt::AlignLeft | Qt::AlignTop);

        painter.save();
        if (command.rotationDegrees != 0.0) {
            drawRotatedText(painter, rect, command, option);
        } else {
            // 文本按元素区域裁切：宽度不足时可换行，高度不足时不向外溢出。
            painter.setClipRect(rect);
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

    painter.save();
    if (command.rotationDegrees != 0.0) {
        // 二维码模块按原始正方形网格绘制后整体旋转，保持模块比例不变。
        painter.translate(rect.center());
        painter.rotate(-command.rotationDegrees);
        painter.translate(-rect.center());
    }

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
    painter.restore();
}

} // 命名空间 sleekpr::infrastructure
