#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateSelectionEditModel.h"

#include <QObject>
#include <QRectF>
#include <QStringList>
#include <QTransform>

#include <functional>
#include <optional>

class QAction;
class QMenu;
class QWidget;

namespace sleekpr::app {

class TemplatePreviewLabel;

// 画布编辑会话只编排选区和命令；几何规则属于核心模型，图像与打印仍走原生渲染链路。
class TemplateCanvasEditor final : public QObject
{
public:
    struct Host {
        std::function<sleekpr::core::TemplateDocument&()> document;
        std::function<QSizeF()> paperSize;
        std::function<QString()> targetLayerId;
        std::function<QTransform()> displayTransform;
        std::function<std::optional<QString>(QPoint)> hitTest;
        std::function<void()> beforeEdit;
        std::function<void(const QStringList&)> selectionChanged;
        std::function<void(bool committed)> documentChanged;
        std::function<void()> undo;
        std::function<void()> redo;
    };

    TemplateCanvasEditor(TemplatePreviewLabel* canvas, Host host, QObject* parent);
    ~TemplateCanvasEditor() override;
    const QStringList& selection() const;
    void setSelection(QStringList ids);
    void refreshOverlay();
    void installActions(QWidget* toolParent, QMenu* menu);
    bool isGestureActive() const;
    void finishGesture(bool cancelled = false);
    void deleteSelection();

private:
    enum class Gesture { None, Move, Resize, Marquee };
    bool beginGesture(QPoint position, Qt::KeyboardModifiers modifiers);
    void updateGesture(QPoint delta);
    void nudge(QPoint direction, Qt::KeyboardModifiers modifiers);
    bool handleShortcut(int key, Qt::KeyboardModifiers modifiers);
    void copySelection();
    void pasteSelection();
    void duplicateSelection();
    void alignSelection(sleekpr::core::SelectionAlignment alignment);
    void commitCommand(bool changed);
    void updateActions();
    QPointF documentPoint(QPoint position) const;
    QRectF elementRect(const sleekpr::core::TemplateElement& element) const;
    bool editable(const sleekpr::core::TemplateLayer& layer, const sleekpr::core::TemplateElement& element) const;
    QStringList editableIds(const QStringList& ids) const;
    QPointF snappedDelta(QPointF delta);
    QRectF resizedBounds(QPointF delta) const;
    void publishSelection();

    TemplatePreviewLabel* m_canvas;
    Host m_host;
    QStringList m_selection;
    Gesture m_gesture = Gesture::None;
    std::optional<sleekpr::core::TemplateDocument> m_beforeGesture;
    std::optional<sleekpr::core::TemplateDocument> m_duplicateDocument;
    QStringList m_beforeSelection;
    QStringList m_marqueeBaseSelection;
    QPoint m_startPosition;
    QPoint m_accumulatedDelta;
    QRectF m_startBounds;
    Qt::Edges m_resizeEdges;
    bool m_gestureChanged = false;
    bool m_duplicateOnDrag = false;
    std::optional<QString> m_clickSelection;
    bool m_snapEnabled = true;
    QList<QAction*> m_selectionActions;
    QAction* m_pasteAction = nullptr;
    QAction* m_distributeHorizontalAction = nullptr;
    QAction* m_distributeVerticalAction = nullptr;
};

} // 命名空间 sleekpr::app
