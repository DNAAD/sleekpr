#pragma once

#include <QLabel>
#include <QPoint>
#include <Qt>

#include <functional>

namespace sleekpr::app {

class TemplatePreviewLabel final : public QLabel
{
public:
    using DragDeltaCallback = std::function<void(QPoint)>;
    using DragStartCallback = std::function<bool(QPoint)>;
    using KeyboardNudgeCallback = std::function<void(QPoint, Qt::KeyboardModifiers)>;

    explicit TemplatePreviewLabel(QWidget* parent = nullptr);

    void setDraggingEnabled(bool enabled);
    void setDragStartCallback(DragStartCallback callback);
    void setDragDeltaCallback(DragDeltaCallback callback);
    void setKeyboardNudgeCallback(KeyboardNudgeCallback callback);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool m_draggingEnabled = false;
    bool m_dragging = false;
    QPoint m_lastMousePosition;
    DragStartCallback m_dragStartCallback;
    DragDeltaCallback m_dragDeltaCallback;
    KeyboardNudgeCallback m_keyboardNudgeCallback;
};

} // 命名空间 sleekpr::app
