#include "sleekpr/app/TemplatePreviewLabel.h"

#include <QKeyEvent>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sleekpr::app {

namespace {

constexpr int kHorizontalRulerHeightPx = 18;
constexpr int kVerticalRulerWidthPx = 28;

struct DesignAidItem
{
    QString key;
    QRectF rectPx;
    double leftMm = 0.0;
    double topMm = 0.0;
    double rightMm = 0.0;
    double bottomMm = 0.0;
};

QString designAidElementKey(QString commandKey)
{
    // 表格和数组网格会拆成多条底层绘制命令，辅助层需要回到原始模板元素，避免尺寸框过度分散。
    if (commandKey.endsWith(QStringLiteral(".border"))) {
        commandKey.chop(QStringLiteral(".border").size());
    }

    const auto cellMarkerIndex = commandKey.indexOf(QStringLiteral(".cell"));
    if (cellMarkerIndex > 0) {
        return commandKey.left(cellMarkerIndex);
    }

    const auto headerMarkerIndex = commandKey.indexOf(QStringLiteral(".header."));
    if (headerMarkerIndex > 0) {
        return commandKey.left(headerMarkerIndex);
    }

    const auto rowMarkerIndex = commandKey.indexOf(QStringLiteral(".row"));
    if (rowMarkerIndex > 0) {
        auto cursor = rowMarkerIndex + QStringLiteral(".row").size();
        while (cursor < commandKey.size() && commandKey.at(cursor).isDigit()) {
            ++cursor;
        }
        if (cursor > rowMarkerIndex + QStringLiteral(".row").size()
            && (cursor == commandKey.size() || commandKey.at(cursor) == QChar('.'))) {
            return commandKey.left(rowMarkerIndex);
        }
    }

    return commandKey;
}

} // 匿名命名空间

TemplatePreviewLabel::TemplatePreviewLabel(QWidget* parent)
    : QLabel(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void TemplatePreviewLabel::setDraggingEnabled(bool enabled)
{
    m_draggingEnabled = enabled;
    setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void TemplatePreviewLabel::setDragStartCallback(DragStartCallback callback)
{
    m_dragStartCallback = std::move(callback);
}

void TemplatePreviewLabel::setDragDeltaCallback(DragDeltaCallback callback)
{
    m_dragDeltaCallback = std::move(callback);
}

void TemplatePreviewLabel::setKeyboardNudgeCallback(KeyboardNudgeCallback callback)
{
    m_keyboardNudgeCallback = std::move(callback);
}

void TemplatePreviewLabel::setRulerVisible(bool visible)
{
    if (m_rulerVisible == visible) {
        return;
    }
    m_rulerVisible = visible;
    update();
}

bool TemplatePreviewLabel::isRulerVisible() const
{
    return m_rulerVisible;
}

void TemplatePreviewLabel::setRulerPrecisionMm(double precisionMm)
{
    const auto normalized = std::max(0.1, precisionMm);
    if (std::abs(m_rulerPrecisionMm - normalized) < 0.0001) {
        return;
    }
    m_rulerPrecisionMm = normalized;
    update();
}

double TemplatePreviewLabel::rulerPrecisionMm() const
{
    return m_rulerPrecisionMm;
}

void TemplatePreviewLabel::setRulerPaperSizeMm(QSizeF paperSizeMm)
{
    if (m_rulerPaperSizeMm == paperSizeMm) {
        return;
    }
    m_rulerPaperSizeMm = paperSizeMm;
    update();
}

void TemplatePreviewLabel::setDesignAidVisible(bool visible)
{
    if (m_designAidVisible == visible) {
        return;
    }
    m_designAidVisible = visible;
    update();
}

bool TemplatePreviewLabel::isDesignAidVisible() const
{
    return m_designAidVisible;
}

void TemplatePreviewLabel::setDesignAidCommands(QList<sleekpr::core::NativeDrawCommand> commands)
{
    m_designAidCommands = std::move(commands);
    update();
}

int TemplatePreviewLabel::designAidCommandCount() const
{
    return m_designAidCommands.size();
}

void TemplatePreviewLabel::setDesignAidSelectedElementKey(QString elementKey)
{
    elementKey = designAidElementKey(elementKey.trimmed());
    if (m_designAidSelectedElementKey == elementKey) {
        return;
    }
    m_designAidSelectedElementKey = std::move(elementKey);
    update();
}

QString TemplatePreviewLabel::designAidSelectedElementKey() const
{
    return m_designAidSelectedElementKey;
}

QPoint TemplatePreviewLabel::printableImageOriginPx() const
{
    return m_rulerVisible ? QPoint(kVerticalRulerWidthPx, kHorizontalRulerHeightPx) : QPoint(0, 0);
}

QSize TemplatePreviewLabel::printableImageSizePx() const
{
    if (!pixmap().isNull()) {
        return pixmap().size();
    }
    const auto origin = printableImageOriginPx();
    return QSize(std::max(0, width() - origin.x()), std::max(0, height() - origin.y()));
}

QPoint TemplatePreviewLabel::mapToPrintableImagePx(QPoint widgetPosition) const
{
    return widgetPosition - printableImageOriginPx();
}

void TemplatePreviewLabel::paintEvent(QPaintEvent* event)
{
    if (!m_rulerVisible || m_rulerPaperSizeMm.isEmpty() || pixmap().isNull()) {
        QLabel::paintEvent(event);
        return;
    }

    Q_UNUSED(event);
    const auto origin = printableImageOriginPx();
    const auto imageSize = printableImageSizePx();
    const auto scaleX = imageSize.width() / m_rulerPaperSizeMm.width();
    const auto scaleY = imageSize.height() / m_rulerPaperSizeMm.height();
    const auto precision = std::max(0.1, m_rulerPrecisionMm);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), palette().window());
    painter.drawPixmap(origin, pixmap());

    painter.setPen(QColor(60, 64, 67, 180));
    painter.fillRect(QRect(origin.x(), 0, imageSize.width(), origin.y()), QColor(255, 255, 255, 240));
    painter.fillRect(QRect(0, origin.y(), origin.x(), imageSize.height()), QColor(255, 255, 255, 240));
    painter.fillRect(QRect(0, 0, origin.x(), origin.y()), QColor(245, 246, 248, 255));
    painter.setFont(QFont(QStringLiteral("Arial"), 7));

    const auto drawVerticalTick = [&](double mm) {
        const int x = origin.x() + static_cast<int>(std::round(mm * scaleX));
        const bool major = std::fmod(mm, 10.0) < 0.0001 || std::abs(std::fmod(mm, 10.0) - 10.0) < 0.0001;
        const bool middle = std::fmod(mm, 5.0) < 0.0001 || std::abs(std::fmod(mm, 5.0) - 5.0) < 0.0001;
        const int tickHeight = major ? 14 : (middle ? 10 : 6);
        painter.drawLine(x, 0, x, tickHeight);
        if (major) {
            painter.drawText(x + 2, 16, QString::number(static_cast<int>(mm)));
        }
    };

    const auto drawHorizontalTick = [&](double mm) {
        const int y = origin.y() + static_cast<int>(std::round(mm * scaleY));
        const bool major = std::fmod(mm, 10.0) < 0.0001 || std::abs(std::fmod(mm, 10.0) - 10.0) < 0.0001;
        const bool middle = std::fmod(mm, 5.0) < 0.0001 || std::abs(std::fmod(mm, 5.0) - 5.0) < 0.0001;
        const int tickWidth = major ? 24 : (middle ? 18 : 10);
        painter.drawLine(0, y, tickWidth, y);
        if (major && y > 8) {
            painter.drawText(2, y - 2, QString::number(static_cast<int>(mm)));
        }
    };

    // 标尺只画在打印图层外侧，避免遮挡模板内容；真实打印仍只使用原始 pixmap/绘制计划。
    painter.drawRect(QRect(origin, imageSize).adjusted(0, 0, -1, -1));
    for (double mm = 0.0; mm <= m_rulerPaperSizeMm.width() + 0.0001; mm += precision) {
        drawVerticalTick(mm);
    }
    for (double mm = 0.0; mm <= m_rulerPaperSizeMm.height() + 0.0001; mm += precision) {
        drawHorizontalTick(mm);
    }
    if (m_designAidVisible && !m_designAidCommands.isEmpty()) {
        drawDesignAids(painter, origin, imageSize);
    }
}

QRectF TemplatePreviewLabel::commandRectPx(
    const sleekpr::core::NativeDrawCommand& command,
    QPoint imageOrigin,
    QSize imageSize) const
{
    const double scaleX = imageSize.width() / m_rulerPaperSizeMm.width();
    const double scaleY = imageSize.height() / m_rulerPaperSizeMm.height();
    return QRectF(
        imageOrigin.x() + command.x * scaleX,
        imageOrigin.y() + command.y * scaleY,
        command.width * scaleX,
        command.height * scaleY);
}

void TemplatePreviewLabel::drawDesignAids(QPainter& painter, QPoint imageOrigin, QSize imageSize) const
{
    QList<DesignAidItem> items;
    QHash<QString, int> itemIndexByKey;
    // 先按模板元素聚合命令区域，尺寸标注显示的是元素整体占用空间。
    for (const auto& command : m_designAidCommands) {
        const auto key = designAidElementKey(command.elementKey.trimmed());
        if (key.isEmpty() || command.width <= 0.0 || command.height <= 0.0) {
            continue;
        }

        const auto rectPx = commandRectPx(command, imageOrigin, imageSize);
        const auto indexIt = itemIndexByKey.constFind(key);
        if (indexIt == itemIndexByKey.cend()) {
            DesignAidItem item;
            item.key = key;
            item.rectPx = rectPx;
            item.leftMm = command.x;
            item.topMm = command.y;
            item.rightMm = command.x + command.width;
            item.bottomMm = command.y + command.height;
            itemIndexByKey.insert(key, items.size());
            items.append(item);
            continue;
        }

        auto& item = items[*indexIt];
        item.rectPx = item.rectPx.united(rectPx);
        item.leftMm = std::min(item.leftMm, command.x);
        item.topMm = std::min(item.topMm, command.y);
        item.rightMm = std::max(item.rightMm, command.x + command.width);
        item.bottomMm = std::max(item.bottomMm, command.y + command.height);
    }

    if (items.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setClipRect(QRect(imageOrigin, imageSize));

    QPen guidePen(QColor(30, 136, 229, 70));
    guidePen.setStyle(Qt::DashLine);
    guidePen.setWidth(1);
    painter.setPen(guidePen);
    const auto paperRect = QRectF(imageOrigin, imageSize);
    // 对齐参考线只画在设计器预览层，帮助观察各元素上下左右边缘是否齐平。
    for (const auto& item : items) {
        painter.drawLine(QPointF(item.rectPx.left(), paperRect.top()), QPointF(item.rectPx.left(), paperRect.bottom()));
        painter.drawLine(QPointF(item.rectPx.right(), paperRect.top()), QPointF(item.rectPx.right(), paperRect.bottom()));
        painter.drawLine(QPointF(paperRect.left(), item.rectPx.top()), QPointF(paperRect.right(), item.rectPx.top()));
        painter.drawLine(QPointF(paperRect.left(), item.rectPx.bottom()), QPointF(paperRect.right(), item.rectPx.bottom()));
    }

    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 7));
    for (const auto& item : items) {
        const bool selected = !m_designAidSelectedElementKey.isEmpty() && item.key == m_designAidSelectedElementKey;
        QPen boxPen(selected ? QColor(255, 128, 0, 230) : QColor(0, 104, 183, 180));
        boxPen.setStyle(Qt::DashLine);
        boxPen.setWidth(selected ? 2 : 1);
        painter.setPen(boxPen);
        painter.drawRect(item.rectPx);

        if (selected) {
            QPen selectedGuidePen(QColor(255, 128, 0, 170));
            selectedGuidePen.setStyle(Qt::DashLine);
            selectedGuidePen.setWidth(1);
            painter.setPen(selectedGuidePen);
            painter.drawLine(QPointF(item.rectPx.left(), paperRect.top()), QPointF(item.rectPx.left(), paperRect.bottom()));
            painter.drawLine(QPointF(item.rectPx.right(), paperRect.top()), QPointF(item.rectPx.right(), paperRect.bottom()));
            painter.drawLine(QPointF(paperRect.left(), item.rectPx.top()), QPointF(paperRect.right(), item.rectPx.top()));
            painter.drawLine(QPointF(paperRect.left(), item.rectPx.bottom()), QPointF(paperRect.right(), item.rectPx.bottom()));
        }

        const auto sizeText = QStringLiteral("%1 x %2 mm")
                                  .arg(item.rightMm - item.leftMm, 0, 'f', 1)
                                  .arg(item.bottomMm - item.topMm, 0, 'f', 1);
        // 尺寸标签贴近元素边框并限制在纸张内部，避免覆盖外置标尺。
        auto textRect = QRectF(painter.fontMetrics().boundingRect(sizeText)).adjusted(-3, -2, 3, 2);
        double textX = item.rectPx.left();
        double textY = item.rectPx.top() - textRect.height() - 2.0;
        if (textY < paperRect.top()) {
            textY = item.rectPx.top() + 2.0;
        }
        if (textX + textRect.width() > paperRect.right()) {
            textX = paperRect.right() - textRect.width();
        }
        textX = std::max(paperRect.left(), textX);
        textY = std::min(std::max(paperRect.top(), textY), paperRect.bottom() - textRect.height());
        textRect.moveTopLeft(QPointF(textX, textY));

        painter.fillRect(textRect, QColor(255, 255, 255, 220));
        painter.setPen(selected ? QColor(196, 72, 0, 230) : QColor(0, 82, 145, 210));
        painter.drawRect(textRect);
        painter.drawText(textRect, Qt::AlignCenter, sizeText);
    }
    painter.restore();
}

void TemplatePreviewLabel::keyPressEvent(QKeyEvent* event)
{
    if (!m_draggingEnabled || !m_keyboardNudgeCallback) {
        QLabel::keyPressEvent(event);
        return;
    }

    QPoint direction;
    switch (event->key()) {
    case Qt::Key_Left:
        direction = QPoint(-1, 0);
        break;
    case Qt::Key_Right:
        direction = QPoint(1, 0);
        break;
    case Qt::Key_Up:
        direction = QPoint(0, -1);
        break;
    case Qt::Key_Down:
        direction = QPoint(0, 1);
        break;
    default:
        QLabel::keyPressEvent(event);
        return;
    }

    // 预览控件只识别方向和组合键，毫米步长由设置窗口统一决定。
    m_keyboardNudgeCallback(direction, event->modifiers());
    event->accept();
}

void TemplatePreviewLabel::mousePressEvent(QMouseEvent* event)
{
    if (m_draggingEnabled && event->button() == Qt::LeftButton) {
        if (m_dragStartCallback && !m_dragStartCallback(event->pos())) {
            QLabel::mousePressEvent(event);
            return;
        }
        setFocus(Qt::MouseFocusReason);
        // 这里只记录像素起点，毫米换算由设置窗口结合当前纸张和元素尺寸完成。
        m_dragging = true;
        m_lastMousePosition = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void TemplatePreviewLabel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->pos() - m_lastMousePosition;
        m_lastMousePosition = event->pos();
        if (!delta.isNull() && m_dragDeltaCallback) {
            m_dragDeltaCallback(delta);
        }
        event->accept();
        return;
    }
    QLabel::mouseMoveEvent(event);
}

void TemplatePreviewLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(m_draggingEnabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

} // 命名空间 sleekpr::app
