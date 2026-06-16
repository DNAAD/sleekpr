#include "sleekpr/app/TemplatePreviewLabel.h"

#include <QKeyEvent>
#include <QMouseEvent>

#include <utility>

namespace sleekpr::app {

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
