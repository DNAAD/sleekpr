#pragma once

#include "sleekpr/app/TemplateTablePaginationOverlay.h"
#include "sleekpr/core/native/NativeDrawCommand.h"

#include <QLabel>
#include <QList>
#include <QPair>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <Qt>

#include <functional>

class QPainter;
class QContextMenuEvent;

namespace sleekpr::app {

struct CanvasFloatingAction
{
    QString id;
    QString text;
    bool enabled = true;
};

class TemplatePreviewLabel final : public QLabel
{
public:
    using DragDeltaCallback = std::function<void(QPoint)>;
    using DragStartCallback = std::function<bool(QPoint)>;
    using DoubleClickCallback = std::function<void(QPoint)>;
    using ContextMenuCallback = std::function<void(QPoint, QPoint)>;
    using KeyboardNudgeCallback = std::function<void(QPoint, Qt::KeyboardModifiers)>;
    using CanvasFloatingActionCallback = std::function<void(QString)>;

    explicit TemplatePreviewLabel(QWidget* parent = nullptr);

    void setDraggingEnabled(bool enabled);
    void setDragStartCallback(DragStartCallback callback);
    void setDragDeltaCallback(DragDeltaCallback callback);
    void setDoubleClickCallback(DoubleClickCallback callback);
    void setContextMenuCallback(ContextMenuCallback callback);
    void setKeyboardNudgeCallback(KeyboardNudgeCallback callback);
    void setCanvasFloatingActionCallback(CanvasFloatingActionCallback callback);
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
    void setCanvasFocusRectMm(QRectF rectMm);
    QRectF canvasFocusRectMm() const;
    void setCanvasFloatingActions(QList<CanvasFloatingAction> actions);
    int canvasFloatingActionCount() const;
    QRect canvasFloatingActionRect(const QString& actionId) const;
    void setTablePaginationOverlays(QList<TablePaginationOverlay> overlays);
    int tablePaginationOverlayCount() const;
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
    void drawTablePaginationOverlays(QPainter& painter, QPoint imageOrigin, QSize imageSize) const;
    void drawCanvasFocusRect(QPainter& painter, QPoint imageOrigin, QSize imageSize) const;
    void drawCanvasFloatingActions(QPainter& painter, QPoint imageOrigin, QSize imageSize) const;
    QRectF canvasFocusRectPx(QPoint imageOrigin, QSize imageSize) const;
    QList<QPair<CanvasFloatingAction, QRect>> canvasFloatingActionLayouts() const;
    QString canvasFloatingActionAt(QPoint position) const;

    bool m_draggingEnabled = false;
    bool m_dragging = false;
    bool m_rulerVisible = false;
    bool m_designAidVisible = false;
    double m_rulerPrecisionMm = 1.0;
    QSizeF m_rulerPaperSizeMm;
    QList<sleekpr::core::NativeDrawCommand> m_designAidCommands;
    QString m_designAidSelectedElementKey;
    QRectF m_canvasFocusRectMm;
    QList<CanvasFloatingAction> m_canvasFloatingActions;
    QList<TablePaginationOverlay> m_tablePaginationOverlays;
    QString m_hoveredCanvasFloatingActionId;
    QPoint m_lastMousePosition;
    DragStartCallback m_dragStartCallback;
    DragDeltaCallback m_dragDeltaCallback;
    DoubleClickCallback m_doubleClickCallback;
    ContextMenuCallback m_contextMenuCallback;
    KeyboardNudgeCallback m_keyboardNudgeCallback;
    CanvasFloatingActionCallback m_canvasFloatingActionCallback;
};

} // 命名空间 sleekpr::app
