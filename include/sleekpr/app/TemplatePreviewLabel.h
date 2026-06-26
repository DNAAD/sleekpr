#pragma once

#include "sleekpr/core/native/NativeDrawCommand.h"

#include <QLabel>
#include <QList>
#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <Qt>

#include <functional>

class QPainter;
class QContextMenuEvent;

namespace sleekpr::app {

class TemplatePreviewLabel final : public QLabel
{
public:
    using DragDeltaCallback = std::function<void(QPoint)>;
    using DragStartCallback = std::function<bool(QPoint)>;
    using DoubleClickCallback = std::function<void(QPoint)>;
    using ContextMenuCallback = std::function<void(QPoint, QPoint)>;
    using KeyboardNudgeCallback = std::function<void(QPoint, Qt::KeyboardModifiers)>;

    explicit TemplatePreviewLabel(QWidget* parent = nullptr);

    void setDraggingEnabled(bool enabled);
    void setDragStartCallback(DragStartCallback callback);
    void setDragDeltaCallback(DragDeltaCallback callback);
    void setDoubleClickCallback(DoubleClickCallback callback);
    void setContextMenuCallback(ContextMenuCallback callback);
    void setKeyboardNudgeCallback(KeyboardNudgeCallback callback);
    void setRulerVisible(bool visible);
    bool isRulerVisible() const;
    void setRulerPrecisionMm(double precisionMm);
    double rulerPrecisionMm() const;
    void setRulerPaperSizeMm(QSizeF paperSizeMm);
    void setDesignAidVisible(bool visible);
    bool isDesignAidVisible() const;
    void setDesignAidCommands(QList<sleekpr::core::NativeDrawCommand> commands);
    int designAidCommandCount() const;
    void setDesignAidSelectedElementKey(QString elementKey);
    QString designAidSelectedElementKey() const;
    QPoint printableImageOriginPx() const;
    QSize printableImageSizePx() const;
    QPoint mapToPrintableImagePx(QPoint widgetPosition) const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF commandRectPx(const sleekpr::core::NativeDrawCommand& command, QPoint imageOrigin, QSize imageSize) const;
    void drawDesignAids(QPainter& painter, QPoint imageOrigin, QSize imageSize) const;

    bool m_draggingEnabled = false;
    bool m_dragging = false;
    bool m_rulerVisible = false;
    bool m_designAidVisible = false;
    double m_rulerPrecisionMm = 1.0;
    QSizeF m_rulerPaperSizeMm;
    QList<sleekpr::core::NativeDrawCommand> m_designAidCommands;
    QString m_designAidSelectedElementKey;
    QPoint m_lastMousePosition;
    DragStartCallback m_dragStartCallback;
    DragDeltaCallback m_dragDeltaCallback;
    DoubleClickCallback m_doubleClickCallback;
    ContextMenuCallback m_contextMenuCallback;
    KeyboardNudgeCallback m_keyboardNudgeCallback;
};

} // 命名空间 sleekpr::app
