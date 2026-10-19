#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/DesignerFeatures.h"

#include <QApplication>
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
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace sleekpr::app {

namespace {

// 标签版使用紧凑标尺；显式启用表格设计器时再为浮动工具条预留空间。
constexpr int kHorizontalRulerHeightPx = kTableDesignerEnabled ? 44 : 18;
constexpr int kVerticalRulerWidthPx = 28;
constexpr int kEditorResizeHandleSizePx = 9;
constexpr int kEditorResizeHandleHitMarginPx = 3;
// 相邻手柄的命中区之间至少留一个逻辑像素，同时保留中央移动区，尺寸不随 DPR 改变。
constexpr int kEditorMinimumInteractionSidePx = 2 * (kEditorResizeHandleSizePx + 2 * kEditorResizeHandleHitMarginPx + 1);

const std::array<Qt::Edges, 8> kEditorResizeEdges = {
    Qt::TopEdge | Qt::LeftEdge, Qt::TopEdge | Qt::RightEdge,
    Qt::BottomEdge | Qt::RightEdge, Qt::BottomEdge | Qt::LeftEdge,
    Qt::TopEdge, Qt::RightEdge, Qt::BottomEdge, Qt::LeftEdge};

bool finitePoint(QPointF point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

QRectF normalizedEditorRect(QRectF rect)
{
    if (!finitePoint(rect.topLeft()) || !finitePoint(rect.bottomRight())) {
        return {};
    }
    rect = rect.normalized();
    return rect.isEmpty() ? QRectF{} : rect;
}

Qt::CursorShape resizeCursor(Qt::Edges edges)
{
    if (edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge)) {
        return Qt::SizeFDiagCursor;
    }
    if (edges == (Qt::TopEdge | Qt::RightEdge) || edges == (Qt::BottomEdge | Qt::LeftEdge)) {
        return Qt::SizeBDiagCursor;
    }
    return edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge) ? Qt::SizeHorCursor : Qt::SizeVerCursor;
}

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
    if (!enabled) {
        finishDrag(true);
    }
    updateEditorCursor(m_lastMousePosition);
}

void TemplatePreviewLabel::setDragStartCallback(DragStartCallback callback)
{
    m_dragStartCallback = std::move(callback);
}

void TemplatePreviewLabel::setDragDeltaCallback(DragDeltaCallback callback)
{
    m_dragDeltaCallback = std::move(callback);
}

void TemplatePreviewLabel::setDragFinishedCallback(DragFinishedCallback callback)
{
    m_dragFinishedCallback = std::move(callback);
}

void TemplatePreviewLabel::setCanvasClickCallback(CanvasClickCallback callback)
{
    m_canvasClickCallback = std::move(callback);
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

void TemplatePreviewLabel::setCanvasShortcutCallback(CanvasShortcutCallback callback)
{
    m_canvasShortcutCallback = std::move(callback);
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

void TemplatePreviewLabel::setEditorSelectionRectsMm(const QList<QRectF>& rects, QRectF bounds, bool handlesEnabled)
{
    QList<QRectF> selection;
    selection.reserve(rects.size());
    QRectF combined;
    for (const auto& rect : rects) {
        const auto normalized = normalizedEditorRect(rect);
        if (!normalized.isEmpty()) {
            selection.append(normalized);
            combined = combined.united(normalized);
        }
    }
    bounds = selection.isEmpty() ? QRectF{} : normalizedEditorRect(bounds);
    if (bounds.isEmpty()) {
        bounds = combined;
    }
    if (m_editorSelectionRectsMm == selection && m_editorSelectionBoundsMm == bounds
        && m_editorResizeHandlesEnabled == handlesEnabled) {
        return;
    }
    m_editorSelectionRectsMm = std::move(selection);
    m_editorSelectionBoundsMm = bounds;
    m_editorResizeHandlesEnabled = handlesEnabled;
    updateEditorCursor(m_lastMousePosition);
    update();
}

void TemplatePreviewLabel::setEditorMarqueeRectMm(QRectF rectMm)
{
    rectMm = normalizedEditorRect(rectMm);
    if (m_editorMarqueeRectMm == rectMm) {
        return;
    }
    m_editorMarqueeRectMm = rectMm;
    update();
}

void TemplatePreviewLabel::setEditorSnapGuidesMm(const QList<QLineF>& guides)
{
    QList<QLineF> validGuides;
    validGuides.reserve(guides.size());
    for (const auto& guide : guides) {
        if (finitePoint(guide.p1()) && finitePoint(guide.p2()) && !guide.isNull()) {
            validGuides.append(guide);
        }
    }
    if (m_editorSnapGuidesMm == validGuides) {
        return;
    }
    m_editorSnapGuidesMm = std::move(validGuides);
    update();
}

QRectF TemplatePreviewLabel::editorInteractionFramePx() const
{
    auto frame = editorRectPx(m_editorSelectionBoundsMm, printableImageOriginPx(), printableImageSizePx());
    if (frame.isEmpty()) {
        return {};
    }
    const QSizeF minimumSize(std::max(frame.width(), double(kEditorMinimumInteractionSidePx)),
        std::max(frame.height(), double(kEditorMinimumInteractionSidePx)));
    if (frame.size() != minimumSize) {
        // 只把手柄外置到最小交互框；真实选区毫米范围和绘制轮廓保持不变。
        const auto center = frame.center();
        frame.setSize(minimumSize);
        frame.moveCenter(center);
    }
    return frame;
}

QRect TemplatePreviewLabel::editorResizeHandleRect(Qt::Edges edges) const
{
    if (!m_editorResizeHandlesEnabled || m_editorSelectionRectsMm.isEmpty()
        || std::find(kEditorResizeEdges.cbegin(), kEditorResizeEdges.cend(), edges) == kEditorResizeEdges.cend()) {
        return {};
    }
    const auto bounds = editorInteractionFramePx();
    if (bounds.isEmpty()) {
        return {};
    }
    QPointF anchor = bounds.center();
    if (edges.testFlag(Qt::LeftEdge)) {
        anchor.setX(bounds.left());
    } else if (edges.testFlag(Qt::RightEdge)) {
        anchor.setX(bounds.right());
    }
    if (edges.testFlag(Qt::TopEdge)) {
        anchor.setY(bounds.top());
    } else if (edges.testFlag(Qt::BottomEdge)) {
        anchor.setY(bounds.bottom());
    }
    // 手柄保持固定逻辑像素大小，不能随原生预览图的 DPR 再放大一遍。
    const QPoint center(qRound(anchor.x()), qRound(anchor.y()));
    return QRect(center - QPoint(kEditorResizeHandleSizePx / 2, kEditorResizeHandleSizePx / 2),
        QSize(kEditorResizeHandleSizePx, kEditorResizeHandleSizePx));
}

Qt::Edges TemplatePreviewLabel::editorResizeHandleAt(QPoint localPosition) const
{
    Qt::Edges nearest;
    auto nearestDistance = std::numeric_limits<double>::max();
    for (const auto edges : kEditorResizeEdges) {
        const auto handle = editorResizeHandleRect(edges);
        if (handle.isEmpty() || !handle.adjusted(-kEditorResizeHandleHitMarginPx, -kEditorResizeHandleHitMarginPx,
                                    kEditorResizeHandleHitMarginPx, kEditorResizeHandleHitMarginPx).contains(localPosition)) {
            continue;
        }
        // 命中边距也使用逻辑像素；若边缘舍入后出现重叠，取最近手柄，等距时优先四角。
        const auto delta = QPointF(localPosition) - QPointF(handle.center());
        const auto distance = delta.x() * delta.x() + delta.y() * delta.y();
        if (distance < nearestDistance) {
            nearest = edges;
            nearestDistance = distance;
        }
    }
    return nearest;
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

void TemplatePreviewLabel::setTablePaginationOverlays(QList<TablePaginationOverlay> overlays)
{
    auto sameOverlays = m_tablePaginationOverlays.size() == overlays.size();
    for (auto index = 0; sameOverlays && index < overlays.size(); ++index) {
        const auto& current = m_tablePaginationOverlays[index];
        const auto& next = overlays[index];
        sameOverlays = current.tableId == next.tableId
            && current.kind == next.kind
            && current.rectMm == next.rectMm
            && current.text == next.text
            && current.pageNumber == next.pageNumber
            && current.severe == next.severe;
    }
    if (sameOverlays) {
        return;
    }

    m_tablePaginationOverlays = std::move(overlays);
    update();
}

int TemplatePreviewLabel::tablePaginationOverlayCount() const
{
    return m_tablePaginationOverlays.size();
}

QPoint TemplatePreviewLabel::printableImageOriginPx() const
{
    return m_rulerVisible ? QPoint(kVerticalRulerWidthPx, kHorizontalRulerHeightPx) : QPoint(0, 0);
}

QSize TemplatePreviewLabel::printableImageSizePx() const
{
    if (!pixmap().isNull()) {
        return pixmap().deviceIndependentSize().toSize();
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
        if (!pixmap().isNull()) {
            QPainter painter(this);
            drawEditorOverlays(painter, printableImageOriginPx(), printableImageSizePx());
        }
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
    } else if (m_editorSelectionRectsMm.isEmpty() && !m_designAidSelectedElementKey.isEmpty()) {
        QRectF selection;
        for (const auto& command : m_designAidCommands) {
            if (designAidElementKey(command.elementKey) == m_designAidSelectedElementKey) {
                selection = selection.united(commandRectPx(command, origin, imageSize));
            }
        }
        if (!selection.isEmpty()) {
            // 常规编辑仅描出选中对象，辅助尺寸关闭时也能明确当前操作目标。
            QPen selectionPen(palette().highlight().color(), 1, Qt::DashLine);
            painter.setPen(selectionPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(selection);
        }
    }
    drawTablePaginationOverlays(painter, origin, imageSize);
    if (m_editorSelectionRectsMm.isEmpty()) {
        drawCanvasFocusRect(painter, origin, imageSize);
    }
    drawEditorOverlays(painter, origin, imageSize);
    drawCanvasFloatingActions(painter, origin, imageSize);
}

QRectF TemplatePreviewLabel::editorRectPx(QRectF rectMm, QPoint imageOrigin, QSize imageSize) const
{
    rectMm = normalizedEditorRect(rectMm);
    if (rectMm.isEmpty() || m_rulerPaperSizeMm.isEmpty() || imageSize.isEmpty()
        || !std::isfinite(m_rulerPaperSizeMm.width()) || !std::isfinite(m_rulerPaperSizeMm.height())) {
        return {};
    }
    // 输入已经是预览毫米坐标；这里只按逻辑图像尺寸缩放并加上标尺偏移，不再应用设备变换或 DPR。
    const auto scaleX = imageSize.width() / m_rulerPaperSizeMm.width();
    const auto scaleY = imageSize.height() / m_rulerPaperSizeMm.height();
    return QRectF(imageOrigin.x() + rectMm.x() * scaleX, imageOrigin.y() + rectMm.y() * scaleY,
        rectMm.width() * scaleX, rectMm.height() * scaleY);
}

void TemplatePreviewLabel::drawEditorOverlays(QPainter& painter, QPoint imageOrigin, QSize imageSize) const
{
    if (m_rulerPaperSizeMm.isEmpty() || imageSize.isEmpty()
        || !std::isfinite(m_rulerPaperSizeMm.width()) || !std::isfinite(m_rulerPaperSizeMm.height())
        || (m_editorSelectionRectsMm.isEmpty() && m_editorMarqueeRectMm.isEmpty() && m_editorSnapGuidesMm.isEmpty())) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setClipRect(QRect(imageOrigin, imageSize));
    painter.setBrush(Qt::NoBrush);

    const auto selectionColor = palette().highlight().color();
    painter.setPen(QPen(selectionColor, 1, Qt::DashLine));
    for (const auto& rect : m_editorSelectionRectsMm) {
        painter.drawRect(editorRectPx(rect, imageOrigin, imageSize));
    }
    if (m_editorSelectionRectsMm.size() > 1) {
        painter.setPen(QPen(selectionColor, 1));
        painter.drawRect(editorRectPx(m_editorSelectionBoundsMm, imageOrigin, imageSize));
    }

    if (!m_editorMarqueeRectMm.isEmpty()) {
        auto fillColor = selectionColor;
        fillColor.setAlpha(30);
        painter.setPen(QPen(selectionColor, 1, Qt::DashLine));
        painter.setBrush(fillColor);
        painter.drawRect(editorRectPx(m_editorMarqueeRectMm, imageOrigin, imageSize));
        painter.setBrush(Qt::NoBrush);
    }

    const auto scaleX = imageSize.width() / m_rulerPaperSizeMm.width();
    const auto scaleY = imageSize.height() / m_rulerPaperSizeMm.height();
    painter.setPen(QPen(QColor(208, 54, 126), 1, Qt::DashLine));
    for (const auto& guide : m_editorSnapGuidesMm) {
        painter.drawLine(QPointF(imageOrigin.x() + guide.x1() * scaleX, imageOrigin.y() + guide.y1() * scaleY),
            QPointF(imageOrigin.x() + guide.x2() * scaleX, imageOrigin.y() + guide.y2() * scaleY));
    }
    painter.restore();

    // 手柄可跨出纸张边缘半个尺寸，便于选择贴边元素；引导线和框选填充仍严格限制在纸张内。
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(selectionColor, 1));
    painter.setBrush(Qt::white);
    for (const auto edges : kEditorResizeEdges) {
        const auto handle = editorResizeHandleRect(edges);
        if (!handle.isEmpty()) {
            painter.drawRect(handle.adjusted(0, 0, -1, -1));
        }
    }
    painter.restore();
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
        const bool legacySelected = !m_designAidSelectedElementKey.isEmpty() && item.key == m_designAidSelectedElementKey;
        const bool selected = legacySelected && m_editorSelectionRectsMm.isEmpty();
        QPen boxPen(selected ? QColor(255, 128, 0, 230) : QColor(0, 104, 183, 180));
        boxPen.setStyle(Qt::DashLine);
        boxPen.setWidth(selected ? 2 : 1);
        painter.setPen(boxPen);
        if (!legacySelected || m_editorSelectionRectsMm.isEmpty()) {
            painter.drawRect(item.rectPx);
        }

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

void TemplatePreviewLabel::drawTablePaginationOverlays(QPainter& painter, QPoint imageOrigin, QSize imageSize) const
{
    if (m_tablePaginationOverlays.isEmpty() || m_rulerPaperSizeMm.isEmpty() || imageSize.isEmpty()) {
        return;
    }

    const auto scaleX = imageSize.width() / m_rulerPaperSizeMm.width();
    const auto scaleY = imageSize.height() / m_rulerPaperSizeMm.height();
    const auto paperRect = QRectF(imageOrigin, imageSize);

    auto overlayRectPx = [imageOrigin, scaleX, scaleY](const QRectF& rectMm) {
        return QRectF(
            imageOrigin.x() + rectMm.x() * scaleX,
            imageOrigin.y() + rectMm.y() * scaleY,
            rectMm.width() * scaleX,
            rectMm.height() * scaleY)
            .normalized();
    };

    auto drawBadge = [&painter, &paperRect](QRectF anchorRect, const QString& text, const QColor& background, const QColor& foreground) {
        if (text.trimmed().isEmpty()) {
            return;
        }
        const QFontMetrics metrics(painter.font());
        const auto maxTextWidth = std::max(24, static_cast<int>(paperRect.width()) - 16);
        const auto labelText = metrics.elidedText(text, Qt::ElideRight, maxTextWidth);
        QRectF labelRect = QRectF(metrics.boundingRect(labelText)).adjusted(-6, -3, 6, 3);
        labelRect.moveTopLeft(QPointF(anchorRect.left() + 4.0, anchorRect.top() + 2.0));
        if (labelRect.right() > paperRect.right() - 4.0) {
            labelRect.moveRight(paperRect.right() - 4.0);
        }
        if (labelRect.bottom() > paperRect.bottom() - 4.0) {
            labelRect.moveBottom(paperRect.bottom() - 4.0);
        }
        labelRect.moveLeft(std::max(paperRect.left() + 4.0, labelRect.left()));
        labelRect.moveTop(std::max(paperRect.top() + 4.0, labelRect.top()));

        painter.setPen(QColor(255, 255, 255, 180));
        painter.setBrush(background);
        painter.drawRoundedRect(labelRect, 3, 3);
        painter.setPen(foreground);
        painter.drawText(labelRect, Qt::AlignCenter, labelText);
    };

    painter.save();
    painter.setClipRect(paperRect);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 7, QFont::DemiBold));

    for (const auto& overlay : m_tablePaginationOverlays) {
        const auto rectPx = overlayRectPx(overlay.rectMm).adjusted(0.5, 0.5, -0.5, -0.5);
        if (rectPx.isEmpty()) {
            continue;
        }

        if (overlay.kind == TablePaginationOverlayKind::PageBreak) {
            // 分页线只提示表格跨页边界，不改变真实标签预览和打印内容。
            painter.fillRect(rectPx, QColor(255, 152, 0, 40));
            QPen pageBreakPen(QColor(245, 124, 0, 230));
            pageBreakPen.setStyle(Qt::DashLine);
            pageBreakPen.setWidth(2);
            painter.setPen(pageBreakPen);
            painter.drawLine(QPointF(rectPx.left(), rectPx.center().y()), QPointF(rectPx.right(), rectPx.center().y()));
            drawBadge(rectPx, overlay.text, QColor(245, 124, 0, 230), QColor(255, 255, 255));
            continue;
        }

        if (overlay.kind == TablePaginationOverlayKind::RepeatedHeader) {
            painter.fillRect(rectPx, QColor(0, 150, 136, 45));
            QPen headerPen(QColor(0, 121, 107, 210));
            headerPen.setStyle(Qt::DashLine);
            headerPen.setWidth(1);
            painter.setPen(headerPen);
            painter.drawRect(rectPx);
            drawBadge(rectPx, overlay.text, QColor(0, 121, 107, 220), QColor(255, 255, 255));
            continue;
        }

        const auto warningColor = overlay.severe ? QColor(198, 40, 40, 230) : QColor(251, 140, 0, 230);
        const auto fillColor = overlay.severe ? QColor(198, 40, 40, 45) : QColor(251, 140, 0, 45);
        painter.fillRect(rectPx, fillColor);
        QPen warningPen(warningColor);
        warningPen.setWidth(2);
        painter.setPen(warningPen);
        painter.drawRoundedRect(rectPx, 2, 2);
        drawBadge(rectPx, overlay.text, warningColor, QColor(255, 255, 255));
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

    int top = 2;
    if (m_rulerVisible && imageOrigin.y() >= actionHeight) {
        // 有顶部标尺时把画布工具条放在标尺带里，避免遮住表格单元格导致难以继续点选。
        top = std::max(2, imageOrigin.y() - actionHeight);
    } else {
        top = static_cast<int>(std::round(focusRect.top() - actionHeight - 6));
        if (top < 2) {
            top = static_cast<int>(std::round(focusRect.bottom() + 6));
        }
        if (top + actionHeight > height() - 2) {
            top = std::max(2, height() - actionHeight - 2);
        }
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

bool TemplatePreviewLabel::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::UngrabMouse:
    case QEvent::Hide:
    case QEvent::WindowDeactivate:
        finishDrag(true);
        break;
    case QEvent::EnabledChange:
        if (!isEnabled()) {
            finishDrag(true);
        }
        updateEditorCursor(m_lastMousePosition);
        break;
    case QEvent::Leave:
        if (!m_dragging) {
            m_lastMousePosition = QPoint(-1, -1);
            m_hoveredCanvasFloatingActionId.clear();
            updateEditorCursor(m_lastMousePosition);
            update();
        }
        break;
    default:
        break;
    }
    return QLabel::event(event);
}

void TemplatePreviewLabel::updateEditorCursor(QPoint position)
{
    if (!m_draggingEnabled || !isEnabled()) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    if (m_dragging) {
        setCursor(m_dragResizeEdges != Qt::Edges{} ? resizeCursor(m_dragResizeEdges) : Qt::ClosedHandCursor);
        return;
    }
    if (!canvasFloatingActionAt(position).isEmpty()) {
        setCursor(Qt::PointingHandCursor);
        return;
    }
    const auto edges = editorResizeHandleAt(position);
    setCursor(edges != Qt::Edges{} ? resizeCursor(edges) : Qt::OpenHandCursor);
}

void TemplatePreviewLabel::finishDrag(bool cancelled, QMouseEvent* releaseEvent)
{
    if (!m_dragging) {
        return;
    }
    const auto wasClick = !m_dragMoved;
    const auto finishedCallback = m_dragFinishedCallback;
    // 先清除活动状态，避免单击回调隐藏窗口、释放抓取或禁用编辑时再次结束同一事务。
    m_dragging = false;
    m_dragMoved = false;
    m_dragResizeEdges = {};
    if (cancelled && QWidget::mouseGrabber() == this) {
        releaseMouse();
    }
    if (!cancelled && releaseEvent && wasClick && m_canvasClickCallback) {
        // 单击选择先落定，再通知控制器提交事务，保持选择和撤销记录的一致性。
        m_canvasClickCallback(releaseEvent->pos(), releaseEvent->modifiers());
    }
    if (finishedCallback) {
        finishedCallback(cancelled);
    }
    updateEditorCursor(m_lastMousePosition);
}

void TemplatePreviewLabel::keyPressEvent(QKeyEvent* event)
{
    if (m_dragging && event->key() == Qt::Key_Escape) {
        finishDrag(true);
        event->accept();
        return;
    }
    if (m_draggingEnabled && m_canvasShortcutCallback && m_canvasShortcutCallback(event->key(), event->modifiers())) {
        event->accept();
        return;
    }

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
        finishDrag(true);
        const auto actionId = canvasFloatingActionAt(event->pos());
        if (!actionId.isEmpty() && m_canvasFloatingActionCallback) {
            setFocus(Qt::MouseFocusReason);
            m_canvasFloatingActionCallback(actionId);
            event->accept();
            return;
        }

        const auto resizeEdges = editorResizeHandleAt(event->pos());
        if (m_dragStartCallback && !m_dragStartCallback(event->pos(), event->modifiers())) {
            QLabel::mousePressEvent(event);
            return;
        }
        setFocus(Qt::MouseFocusReason);
        // 这里只记录像素起点，毫米换算由设置窗口结合当前纸张和元素尺寸完成。
        m_dragging = true;
        m_dragMoved = false;
        m_dragStartPosition = event->pos();
        m_lastMousePosition = event->pos();
        m_dragResizeEdges = resizeEdges;
        updateEditorCursor(event->pos());
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void TemplatePreviewLabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // 双击可转入单元格或文本编辑，旧的按下手势必须先取消，随后释放不能误提交它。
        finishDrag(true);
    }
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
    if (m_dragging && !(event->buttons() & Qt::LeftButton)) {
        // 系统或其他控件可能吞掉释放事件；再次移动时也要解除旧手势。
        finishDrag(true);
    }
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragMoved
            && (event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
            event->accept();
            return;
        }

        const QPoint delta = event->pos() - m_lastMousePosition;
        m_lastMousePosition = event->pos();
        if (!delta.isNull()) {
            m_dragMoved = true;
        }
        if (!delta.isNull() && m_dragDeltaCallback) {
            m_dragDeltaCallback(delta);
        }
        event->accept();
        return;
    }

    m_lastMousePosition = event->pos();
    const auto hoveredActionId = canvasFloatingActionAt(event->pos());
    if (m_hoveredCanvasFloatingActionId != hoveredActionId) {
        m_hoveredCanvasFloatingActionId = hoveredActionId;
        update();
    }
    updateEditorCursor(event->pos());
    if (!hoveredActionId.isEmpty()) {
        event->accept();
        return;
    }

    QLabel::mouseMoveEvent(event);
}

void TemplatePreviewLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_lastMousePosition = event->pos();
        finishDrag(false, event);
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

} // 命名空间 sleekpr::app
