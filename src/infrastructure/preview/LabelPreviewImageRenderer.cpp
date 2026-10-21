#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include "sleekpr/infrastructure/preview/QrCodeMatrixRenderer.h"
#include "sleekpr/infrastructure/rendering/TextAutoFitSizer.h"

#include <QIODevice>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <new>
#include <utility>

namespace sleekpr::infrastructure {

namespace {

PreviewRenderResult failure(core::PreviewImageError error, const QString& message)
{
    return {error, message, {}, {}};
}

class BoundedPngDevice final : public QIODevice
{
public:
    explicit BoundedPngDevice(qsizetype maximum) : m_maximum(maximum) { open(QIODevice::WriteOnly | QIODevice::Unbuffered); }
    bool isSequential() const override { return true; }
    const QByteArray& bytes() const { return m_bytes; }
    bool exceeded() const { return m_exceeded; }
protected:
    qint64 readData(char*, qint64) override { return -1; }
    qint64 writeData(const char* data, qint64 length) override
    {
        // 编码器输出必须在追加到 QByteArray 前检查，不能等完整 PNG 占用内存后才计量。
        if (length < 0 || length > m_maximum - m_bytes.size()) {
            m_exceeded = true;
            setErrorString(QString::fromUtf8("PNG 输出超过编码预算"));
            return -1;
        }
        m_bytes.append(data, static_cast<qsizetype>(length));
        return length;
    }
private:
    qsizetype m_maximum;
    QByteArray m_bytes;
    bool m_exceeded = false;
};

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

} // 匿名命名空间

LabelPreviewImageRenderer::LabelPreviewImageRenderer(PngEncoder encoder, ImageAllocator allocator)
    : m_encoder(std::move(encoder)), m_allocator(std::move(allocator)) {}

PreviewRenderResult LabelPreviewImageRenderer::tryRenderImage(const core::NativeLabelDrawingPlan& plan,
    const core::PreviewImageRequest& request, const core::PreviewResourceBudget& budget) const
{
    const auto checked = core::checkPreviewImageRequest(request, budget);
    if (!checked.ok()) return failure(checked.error, checked.message);
    try {
        return renderCheckedImage(plan, checked, request.devicePixelRatio);
    } catch (const std::bad_alloc&) {
        return failure(core::PreviewImageError::AllocationFailed, QString::fromUtf8("预览内存分配失败"));
    } catch (...) {
        return failure(core::PreviewImageError::PaintFailed, QString::fromUtf8("Qt 预览绘制失败"));
    }
}

PreviewRenderResult LabelPreviewImageRenderer::tryRenderPng(const core::NativeLabelDrawingPlan& plan,
    const core::PreviewImageRequest& request, const core::PreviewResourceBudget& budget, qsizetype maximum) const
{
    if (maximum <= 0) return failure(core::PreviewImageError::ResourceLimit, QString::fromUtf8("PNG 编码预算不足"));
    auto result = tryRenderImage(plan, request, budget);
    if (!result.ok()) return result;
    try {
        BoundedPngDevice device(maximum);
        const bool encoded = m_encoder ? m_encoder(result.image, &device) : result.image.save(&device, "PNG");
        if (device.exceeded()) return failure(core::PreviewImageError::ResourceLimit,
            QString::fromUtf8("预览 PNG 超过响应预算，请减少内容或拆分批次"));
        if (!encoded || device.bytes().isEmpty()) return failure(core::PreviewImageError::EncodeFailed,
            QString::fromUtf8("PNG 编码失败，未返回空白成功结果"));
        result.png = device.bytes();
        return result;
    } catch (const std::bad_alloc&) {
        return failure(core::PreviewImageError::AllocationFailed, QString::fromUtf8("PNG 编码内存分配失败"));
    } catch (...) {
        return failure(core::PreviewImageError::EncodeFailed, QString::fromUtf8("PNG 编码失败"));
    }
}

PreviewRenderResult LabelPreviewImageRenderer::renderCheckedImage(const core::NativeLabelDrawingPlan& plan,
    const core::PreviewImageCheck& checked, double devicePixelRatio) const
{
    const auto dpi = checked.rasterDpi;
    const auto scale = dpi / 25.4;

    auto image = m_allocator ? m_allocator(checked.pixelSize) : QImage(checked.pixelSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull() || image.size() != checked.pixelSize || image.format() != QImage::Format_ARGB32_Premultiplied
        || image.sizeInBytes() != static_cast<qsizetype>(checked.pixelSize.width()) * checked.pixelSize.height() * 4) {
        return failure(core::PreviewImageError::AllocationFailed, QString::fromUtf8("预览图像分配失败或格式不正确"));
    }
    image.setDotsPerMeterX(checked.dotsPerMeter);
    image.setDotsPerMeterY(checked.dotsPerMeter);
    image.fill(Qt::white);

    QPainter painter;
    if (!painter.begin(&image)) return failure(core::PreviewImageError::PaintFailed, QString::fromUtf8("无法启动 Qt 图像绘制"));
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
            if (!command.backgroundColor.trimmed().isEmpty()) {
                painter.fillRect(rect, colorOrDefault(command.backgroundColor, Qt::transparent));
            }
            QPen pen(colorOrDefault(command.textColor, Qt::black));
            if (command.borderWidthMm > 0.0) {
                pen.setWidthF(std::max(1.0, command.borderWidthMm * scale));
            }
            painter.setPen(pen);
            painter.drawRect(rect);
            painter.restore();
            continue;
        }
        if (command.type == sleekpr::core::NativeDrawCommandType::QrCode) {
            drawQrCode(painter, command, scale);
            continue;
        }

        const auto layoutSize = command.rotationDegrees == 0.0
            ? QSizeF(rect.width(), rect.height())
            : QSizeF(rect.height(), rect.width());
        const auto effectiveFontSizePt = TextAutoFitSizer::fitPointSize(command, layoutSize, dpi, dpi);

        // 字号仍使用 pt，由 Qt 根据目标设备换算；坐标和区域大小保持毫米计划换算后的像素值。
        QFont font("Microsoft YaHei");
        font.setPointSizeF(effectiveFontSizePt);
        font.setBold(command.bold);
        painter.setFont(font);

        QTextOption option;
        option.setWrapMode(command.wrapText || command.ellipsis ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        option.setAlignment(Qt::AlignLeft | Qt::AlignTop);

        painter.save();
        painter.setPen(colorOrDefault(command.textColor, Qt::black));
        if (command.rotationDegrees != 0.0) {
            drawRotatedText(painter, rect, command, option);
        } else {
            // 文本按元素区域裁切：宽度不足时可换行，高度不足时不向外溢出。
            painter.setClipRect(rect);
            painter.drawText(rect, command.text, option);
        }
        painter.restore();
    }

    if (!painter.end()) return failure(core::PreviewImageError::PaintFailed, QString::fromUtf8("Qt 图像绘制未能完成"));
    // DPR 只在绘制结束后设置，避免 QPainter 再隐式乘一次设备倍率。
    image.setDevicePixelRatio(devicePixelRatio);
    return {core::PreviewImageError::None, {}, image, {}};
}

QImage LabelPreviewImageRenderer::renderImage(const core::NativeLabelDrawingPlan& plan, double dpi) const
{
    return tryRenderImage(plan, {QSizeF(plan.paperSize.widthMm(), plan.paperSize.heightMm()), dpi}).image;
}

QByteArray LabelPreviewImageRenderer::renderPng(const sleekpr::core::NativeLabelDrawingPlan& plan, double dpi) const
{
    return tryRenderPng(plan, {QSizeF(plan.paperSize.widthMm(), plan.paperSize.heightMm()), dpi}, {},
        std::numeric_limits<qsizetype>::max()).png;
}

void LabelPreviewImageRenderer::drawQrCode(QPainter& painter, const sleekpr::core::NativeDrawCommand& command, double scale)
{
    const QRectF rect(command.x * scale, command.y * scale, command.width * scale, command.height * scale);
    QrCodeMatrix matrix;
    try {
        matrix = QrCodeMatrixRenderer().render(command.text);
    } catch (const std::bad_alloc&) {
        throw;
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
