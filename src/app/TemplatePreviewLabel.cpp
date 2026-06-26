#include "sleekpr/app/TemplatePreviewLabel.h"

#include <QKeyEvent>
#include <QColor>
#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetrics>
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
    setMouseTracking(true);
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

void TemplatePreviewLabel::setDoubleClickCallback(DoubleClickCallback callback)
{
    m_doubleClickCallback = std::move(callback);
}

void TemplatePreviewLabel::setContextMenuCallback(ContextMenuCallback callback)
{
    m_contextMenuCallback = std::move(callback);
}

void TemplatePreviewLabel::setKeyboardNudgeCallback(KeyboardNudgeCallback callback)
{
    m_keyboardNudgeCallback = std::move(callback);
}

void TemplatePreviewLabel::setCanvasFloatingActionCallback(CanvasFloatingActionCallback callback)
{
    m_canvasFloatingActionCallback = std::move(callback);
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

void TemplatePreviewLabel::setCanvasFocusRectMm(QRectF rectMm)
{
    if (m_canvasFocusRectMm == rectMm) {
        return;
    }
    m_canvasFocusRectMm = rectMm;
    update();
}

QRectF TemplatePreviewLabel::canvasFocusRectMm() const
{
    return m_canvasFocusRectMm;
}

void TemplatePreviewLabel::setCanvasFloatingActions(QList<CanvasFloatingAction> actions)
{
    if (m_canvasFloatingActions.size() == actions.size()) {
        auto sameActions = true;
        for (auto index = 0; index < actions.size(); ++index) {
            if (m_canvasFloatingActions[index].id != actions[index].id
                || m_canvasFloatingActions[index].text != actions[index].text
                || m_canvasFloatingActions[index].enabled != actions[index].enabled) {
                sameActions = false;
                break;
            }
        }
        if (sameActions) {
            return;
        }
    }

    m_canvasFloatingActions = std::move(actions);
    if (!m_hoveredCanvasFloatingActionId.isEmpty()
        && std::none_of(
            m_canvasFloatingActions.cbegin(),
            m_canvasFloatingActions.cend(),
            [this](const CanvasFloatingAction& action) {
                return action.id == m_hoveredCanvasFloatingActionId;
            })) {
        m_hoveredCanvasFloatingActionId.clear();
    }
    update();
}

int TemplatePreviewLabel::canvasFloatingActionCount() const
{
    return m_canvasFloatingActions.size();
}

QRect TemplatePreviewLabel::canvasFloatingActionRect(const QString& actionId) const
{
    for (const auto& layout : canvasFloatingActionLayouts()) {
        if (layout.first.id == actionId) {
            return layout.second;
        }
    }
    return QRect();
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
    drawCanvasFocusRect(painter, origin, imageSize);
    drawCanvasFloatingActions(painter, origin, imageSize);
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

void TemplatePreviewLabel::drawCanvasFocusRect(QPainter& painter, QPoint imageOrigin, QSize imageSize) const
{
    if (m_canvasFocusRectMm.isNull() || m_rulerPaperSizeMm.isEmpty() || imageSize.isEmpty()) {
        return;
    }

    const auto scaleX = imageSize.width() / m_rulerPaperSizeMm.width();
    const auto scaleY = imageSize.height() / m_rulerPaperSizeMm.height();
    QRectF rectPx(
        imageOrigin.x() + m_canvasFocusRectMm.x() * scaleX,
        imageOrigin.y() + m_canvasFocusRectMm.y() * scaleY,
        m_canvasFocusRectMm.width() * scaleX,
        m_canvasFocusRectMm.height() * scaleY);
    rectPx = rectPx.normalized().adjusted(0.5, 0.5, -0.5, -0.5);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    // 焦点框只标记当前画布编辑目标，不参与真实预览图和打印输出。
    painter.fillRect(rectPx, QColor(0, 132, 255, 34));
    QPen focusPen(QColor(0, 120, 215, 230));
    focusPen.setWidth(2);
    painter.setPen(focusPen);
    painter.drawRect(rectPx);
    painter.restore();
}

QRectF TemplatePreviewLabel::canvasFocusRectPx(QPoint imageOrigin, QSize imageSize) const
{
    if (m_canvasFocusRectMm.isNull() || m_rulerPaperSizeMm.isEmpty() || imageSize.isEmpty()) {
        return QRectF();
    }

    const auto scaleX = imageSize.width() / m_rulerPaperSizeMm.width();
    const auto scaleY = imageSize.height() / m_rulerPaperSizeMm.height();
    return QRectF(
        imageOrigin.x() + m_canvasFocusRectMm.x() * scaleX,
        imageOrigin.y() + m_canvasFocusRectMm.y() * scaleY,
        m_canvasFocusRectMm.width() * scaleX,
        m_canvasFocusRectMm.height() * scaleY)
        .normalized();
}

QList<QPair<CanvasFloatingAction, QRect>> TemplatePreviewLabel::canvasFloatingActionLayouts() const
{
    QList<QPair<CanvasFloatingAction, QRect>> layouts;
    if (m_canvasFloatingActions.isEmpty() || m_canvasFocusRectMm.isNull() || m_rulerPaperSizeMm.isEmpty()) {
        return layouts;
    }

    const auto imageOrigin = printableImageOriginPx();
    const auto imageSize = printableImageSizePx();
    const auto focusRect = canvasFocusRectPx(imageOrigin, imageSize);
    if (focusRect.isNull() || imageSize.isEmpty()) {
        return layouts;
    }

    const QFont actionFont(QStringLiteral("Arial"), 8, QFont::DemiBold);
    const QFontMetrics metrics(actionFont);
    constexpr int actionHeight = 24;
    constexpr int horizontalPadding = 10;
    constexpr int actionSpacing = 4;
    QList<int> actionWidths;
    auto totalWidth = 0;
    const auto actionCount = static_cast<int>(m_canvasFloatingActions.size());
    for (const auto& action : m_canvasFloatingActions) {
        const auto width = std::max(28, metrics.horizontalAdvance(action.text) + horizontalPadding * 2);
        actionWidths.append(width);
        totalWidth += width;
    }
    totalWidth += std::max(0, actionCount - 1) * actionSpacing;

    const auto minX = imageOrigin.x() + 4;
    const auto maxX = imageOrigin.x() + imageSize.width() - totalWidth - 4;
    auto left = static_cast<int>(std::round(focusRect.center().x() - totalWidth / 2.0));
    left = maxX >= minX ? std::clamp(left, minX, maxX) : minX;

    auto top = static_cast<int>(std::round(focusRect.top() - actionHeight - 6));
    if (top < 2) {
        top = static_cast<int>(std::round(focusRect.bottom() + 6));
    }
    if (top + actionHeight > height() - 2) {
        top = std::max(2, height() - actionHeight - 2);
    }

    auto cursorX = left;
    for (auto index = 0; index < actionCount; ++index) {
        const QRect rect(cursorX, top, actionWidths[index], actionHeight);
        layouts.append(qMakePair(m_canvasFloatingActions[index], rect));
        cursorX += actionWidths[index] + actionSpacing;
    }
    return layouts;
}

QString TemplatePreviewLabel::canvasFloatingActionAt(QPoint position) const
{
    for (const auto& layout : canvasFloatingActionLayouts()) {
        if (layout.first.enabled && layout.second.contains(position)) {
            return layout.first.id;
        }
    }
    return QString();
}

void TemplatePreviewLabel::drawCanvasFloatingActions(QPainter& painter, QPoint imageOrigin, QSize imageSize) const
{
    Q_UNUSED(imageOrigin);
    Q_UNUSED(imageSize);
    const auto layouts = canvasFloatingActionLayouts();
    if (layouts.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(QFont(QStringLiteral("Arial"), 8, QFont::DemiBold));
    for (const auto& layout : layouts) {
        const auto& action = layout.first;
        const auto rect = layout.second;
        const auto hovered = action.id == m_hoveredCanvasFloatingActionId;
        const QColor background = !action.enabled
            ? QColor(105, 112, 122, 210)
            : (hovered ? QColor(0, 120, 215, 245) : QColor(33, 43, 54, 238));
        const QColor border = hovered ? QColor(140, 205, 255, 255) : QColor(255, 255, 255, 180);
        painter.setPen(border);
        painter.setBrush(background);
        // 操作条是设计器辅助控件，不参与原生标签预览图和真实打印。
        painter.drawRoundedRect(rect.adjusted(0, 0, -1, -1), 4, 4);
        painter.setPen(action.enabled ? QColor(255, 255, 255) : QColor(220, 224, 230));
        painter.drawText(rect, Qt::AlignCenter, action.text);
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
        const auto actionId = canvasFloatingActionAt(event->pos());
        if (!actionId.isEmpty() && m_canvasFloatingActionCallback) {
            setFocus(Qt::MouseFocusReason);
            m_canvasFloatingActionCallback(actionId);
            event->accept();
            return;
        }

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

void TemplatePreviewLabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_draggingEnabled && event->button() == Qt::LeftButton && m_doubleClickCallback) {
        setFocus(Qt::MouseFocusReason);
        // 双击只上报画布坐标，具体是表头、字段还是普通元素由设计器窗口判断。
        m_doubleClickCallback(event->pos());
        event->accept();
        return;
    }
    QLabel::mouseDoubleClickEvent(event);
}

void TemplatePreviewLabel::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_draggingEnabled && m_contextMenuCallback) {
        // 右键菜单需要本地坐标命中元素，也需要全局坐标定位弹出位置。
        m_contextMenuCallback(event->pos(), event->globalPos());
        event->accept();
        return;
    }
    QLabel::contextMenuEvent(event);
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

    const auto hoveredActionId = canvasFloatingActionAt(event->pos());
    if (m_hoveredCanvasFloatingActionId != hoveredActionId) {
        m_hoveredCanvasFloatingActionId = hoveredActionId;
        setCursor(!hoveredActionId.isEmpty() ? Qt::PointingHandCursor : (m_draggingEnabled ? Qt::OpenHandCursor : Qt::ArrowCursor));
        update();
    }
    if (!hoveredActionId.isEmpty()) {
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
