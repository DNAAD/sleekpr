#pragma once

#include "sleekpr/app/TemplateTablePaginationOverlay.h"
#include "sleekpr/core/native/NativeDrawCommand.h"

#include <QLabel>
#include <QLineF>
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
    using DragStartCallback = std::function<bool(QPoint, Qt::KeyboardModifiers)>;
    using DragFinishedCallback = std::function<void(bool cancelled)>;
    using CanvasClickCallback = std::function<void(QPoint, Qt::KeyboardModifiers)>;
    using DoubleClickCallback = std::function<void(QPoint)>;
    using ContextMenuCallback = std::function<void(QPoint, QPoint)>;
    using KeyboardNudgeCallback = std::function<void(QPoint, Qt::KeyboardModifiers)>;
    using CanvasShortcutCallback = std::function<bool(int, Qt::KeyboardModifiers)>;
    using CanvasFloatingActionCallback = std::function<void(QString)>;

    explicit TemplatePreviewLabel(QWidget* parent = nullptr);

    void setDraggingEnabled(bool enabled);
    void setDragStartCallback(DragStartCallback callback);
    void setDragDeltaCallback(DragDeltaCallback callback);
    void setDragFinishedCallback(DragFinishedCallback callback);
    void setCanvasClickCallback(CanvasClickCallback callback);
    void setDoubleClickCallback(DoubleClickCallback callback);
    void setContextMenuCallback(ContextMenuCallback callback);
    void setKeyboardNudgeCallback(KeyboardNudgeCallback callback);
    void setCanvasShortcutCallback(CanvasShortcutCallback callback);
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
    // 此处接收已完成设备变换的预览毫米坐标，只维护交互覆盖层，不修改原生预览图。
    void setEditorSelectionRectsMm(const QList<QRectF>& rects, QRectF bounds, bool handlesEnabled = true);
    void setEditorMarqueeRectMm(QRectF rectMm);
    void setEditorSnapGuidesMm(const QList<QLineF>& guides);
    Qt::Edges editorResizeHandleAt(QPoint localPosition) const;
    QRect editorResizeHandleRect(Qt::Edges edges) const;
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
    bool event(QEvent* event) override;
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
    void drawEditorOverlays(QPainter& painter, QPoint imageOrigin, QSize imageSize) const;
    QRectF editorRectPx(QRectF rectMm, QPoint imageOrigin, QSize imageSize) const;
    QRectF editorInteractionFramePx() const;
    void updateEditorCursor(QPoint position);
    void finishDrag(bool cancelled, QMouseEvent* releaseEvent = nullptr);
    QRectF canvasFocusRectPx(QPoint imageOrigin, QSize imageSize) const;
    QList<QPair<CanvasFloatingAction, QRect>> canvasFloatingActionLayouts() const;
    QString canvasFloatingActionAt(QPoint position) const;

    bool m_draggingEnabled = false;
    bool m_dragging = false;
    bool m_dragMoved = false;
    bool m_rulerVisible = false;
    bool m_designAidVisible = false;
    double m_rulerPrecisionMm = 1.0;
    QSizeF m_rulerPaperSizeMm;
    QList<sleekpr::core::NativeDrawCommand> m_designAidCommands;
    QString m_designAidSelectedElementKey;
    QList<QRectF> m_editorSelectionRectsMm;
    QRectF m_editorSelectionBoundsMm;
    bool m_editorResizeHandlesEnabled = true;
    QRectF m_editorMarqueeRectMm;
    QList<QLineF> m_editorSnapGuidesMm;
    Qt::Edges m_dragResizeEdges;
    QRectF m_canvasFocusRectMm;
    QList<CanvasFloatingAction> m_canvasFloatingActions;
    QList<TablePaginationOverlay> m_tablePaginationOverlays;
    QString m_hoveredCanvasFloatingActionId;
    QPoint m_dragStartPosition;
    QPoint m_lastMousePosition;
    DragStartCallback m_dragStartCallback;
    DragDeltaCallback m_dragDeltaCallback;
    DragFinishedCallback m_dragFinishedCallback;
    CanvasClickCallback m_canvasClickCallback;
    DoubleClickCallback m_doubleClickCallback;
    ContextMenuCallback m_contextMenuCallback;
    KeyboardNudgeCallback m_keyboardNudgeCallback;
    CanvasShortcutCallback m_canvasShortcutCallback;
    CanvasFloatingActionCallback m_canvasFloatingActionCallback;
};

} // 命名空间 sleekpr::app
