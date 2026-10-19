#include "sleekpr/app/TemplateCanvasEditor.h"

#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/core/templates/TemplateDocumentEditModel.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QMenu>
#include <QMimeData>
#include <QStyle>

#include <algorithm>
#include <cmath>

namespace sleekpr::app {
namespace {

constexpr auto kElementsMime = "application/x-sleekpr-label-elements+json";
constexpr qsizetype kClipboardBytesLimit = 2 * 1024 * 1024;
constexpr qsizetype kClipboardElementLimit = 500;

bool safeClipboardElements(const QList<sleekpr::core::TemplateElement>& elements)
{
    qint64 gridCells = 0;
    for (const auto& element : elements) {
        if (!std::isfinite(element.fontSizePt) || element.fontSizePt <= 0.0 || element.fontSizePt > 1000.0
            || !std::isfinite(element.rotationDegrees)
            || !std::isfinite(element.autoFitMinFontSizePt) || !std::isfinite(element.autoFitMaxFontSizePt)
            || element.autoFitMinFontSizePt <= 0.0 || element.autoFitMaxFontSizePt > 1000.0) {
            return false;
        }
        if (element.type == sleekpr::core::TemplateElementType::ArrayGrid) {
            // 专用 MIME 也可能来自外部程序；在渲染前限制行列与总格数，防止乘法溢出或界面阻塞。
            if (element.arrayGridRows < 1 || element.arrayGridRows > 20 || element.arrayGridColumns < 1
                || element.arrayGridColumns > 20 || !std::isfinite(element.arrayGridRowHeightMm)
                || element.arrayGridRowHeightMm < 0.0) {
                return false;
            }
            gridCells += qint64(element.arrayGridRows) * element.arrayGridColumns;
            if (gridCells > 10000) {
                return false;
            }
        }
    }
    return true;
}

QRectF rawRect(const sleekpr::core::TemplateElement& element)
{
    return {element.x, element.y, element.width, element.height};
}

bool sameDocument(const sleekpr::core::TemplateDocument& left, const sleekpr::core::TemplateDocument& right)
{
    return sleekpr::core::TemplateDocumentJson::toJson(left) == sleekpr::core::TemplateDocumentJson::toJson(right);
}

} // 命名空间

TemplateCanvasEditor::TemplateCanvasEditor(TemplatePreviewLabel* canvas, Host host, QObject* parent)
    : QObject(parent), m_canvas(canvas), m_host(std::move(host))
{
    m_canvas->setDragStartCallback([this](QPoint point, Qt::KeyboardModifiers modifiers) { return beginGesture(point, modifiers); });
    m_canvas->setDragDeltaCallback([this](QPoint delta) { updateGesture(delta); });
    m_canvas->setDragFinishedCallback([this](bool cancelled) { finishGesture(cancelled); });
    m_canvas->setCanvasClickCallback([this](QPoint, Qt::KeyboardModifiers) {
        if (m_clickSelection.has_value()) {
            setSelection({*m_clickSelection});
        }
    });
    m_canvas->setKeyboardNudgeCallback([this](QPoint direction, Qt::KeyboardModifiers modifiers) { nudge(direction, modifiers); });
    m_canvas->setCanvasShortcutCallback([this](int key, Qt::KeyboardModifiers modifiers) { return handleShortcut(key, modifiers); });
}

const QStringList& TemplateCanvasEditor::selection() const { return m_selection; }
bool TemplateCanvasEditor::isGestureActive() const { return m_gesture != Gesture::None; }

TemplateCanvasEditor::~TemplateCanvasEditor()
{
    // 控制器早于画布销毁时，不能让控件继续保留捕获 this 的回调。
    m_canvas->setDragStartCallback({});
    m_canvas->setDragDeltaCallback({});
    m_canvas->setDragFinishedCallback({});
    m_canvas->setCanvasClickCallback({});
    m_canvas->setKeyboardNudgeCallback({});
    m_canvas->setCanvasShortcutCallback({});
    m_canvas->setContextMenuCallback({});
}

bool TemplateCanvasEditor::editable(const sleekpr::core::TemplateLayer& layer, const sleekpr::core::TemplateElement& element) const
{
    return layer.visible && !layer.locked && element.visible && !element.locked;
}

QStringList TemplateCanvasEditor::editableIds(const QStringList& ids) const
{
    QStringList result;
    for (const auto& id : ids) {
        for (const auto& layer : m_host.document().layers) {
            for (const auto& element : layer.elements) {
                if (element.id == id && editable(layer, element) && !result.contains(id)) {
                    result.append(id);
                }
            }
        }
    }
    return result;
}

void TemplateCanvasEditor::setSelection(QStringList ids)
{
    // 列表仍可选中锁定对象查看属性，但它们不进入画布变换选区。
    ids.removeDuplicates();
    ids.removeIf([this](const QString& id) {
        for (const auto& layer : m_host.document().layers) {
            if (std::any_of(layer.elements.cbegin(), layer.elements.cend(), [&id](const auto& element) { return element.id == id; })) {
                return false;
            }
        }
        return true;
    });
    if (m_selection != ids) {
        m_selection = std::move(ids);
        publishSelection();
    } else {
        refreshOverlay();
    }
}

void TemplateCanvasEditor::publishSelection()
{
    m_host.selectionChanged(m_selection);
    refreshOverlay();
    updateActions();
}

QRectF TemplateCanvasEditor::elementRect(const sleekpr::core::TemplateElement& element) const
{
    return m_host.displayTransform().mapRect(rawRect(element));
}

void TemplateCanvasEditor::refreshOverlay()
{
    QList<QRectF> rectangles;
    const auto ids = editableIds(m_selection);
    for (const auto& layer : m_host.document().layers) {
        for (const auto& element : layer.elements) {
            if (ids.contains(element.id)) {
                rectangles.append(elementRect(element));
            }
        }
    }
    const auto bounds = sleekpr::core::TemplateSelectionEditModel::bounds(m_host.document(), ids);
    m_canvas->setEditorSelectionRectsMm(rectangles, m_host.displayTransform().mapRect(bounds), !ids.isEmpty());
    updateActions();
}

QPointF TemplateCanvasEditor::documentPoint(QPoint position) const
{
    const auto point = m_canvas->mapToPrintableImagePx(position);
    const auto size = m_canvas->printableImageSizePx();
    const auto paper = m_host.paperSize();
    if (size.isEmpty() || paper.isEmpty()) {
        return {};
    }
    // 预览已叠加打印偏移和设备校准，命中与拖拽必须先逆变换回模板毫米坐标。
    return m_host.displayTransform().inverted().map(QPointF(point.x() * paper.width() / size.width(),
        point.y() * paper.height() / size.height()));
}

bool TemplateCanvasEditor::beginGesture(QPoint position, Qt::KeyboardModifiers modifiers)
{
    finishGesture(false);
    m_host.beforeEdit();
    m_startPosition = position;
    m_accumulatedDelta = {};
    m_gestureChanged = false;
    m_duplicateOnDrag = modifiers.testFlag(Qt::AltModifier);
    m_clickSelection.reset();
    m_beforeSelection = m_selection;
    m_resizeEdges = m_canvas->editorResizeHandleAt(position);
    const auto additive = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::ShiftModifier);
    if (m_resizeEdges != Qt::Edges{} && !editableIds(m_selection).isEmpty() && !additive) {
        m_gesture = Gesture::Resize;
    } else {
        const auto hit = m_host.hitTest(position);
        const auto canHit = hit.has_value() && !editableIds({*hit}).isEmpty();
        const auto selectedBounds = sleekpr::core::TemplateSelectionEditModel::bounds(m_host.document(), editableIds(m_selection));
        if (!additive && !selectedBounds.isEmpty() && selectedBounds.contains(documentPoint(position))) {
            // 重叠处拖拽优先移动当前选区；真正的单击仍可切换到命中的较小对象。
            if (canHit && !m_selection.contains(*hit)) {
                m_clickSelection = *hit;
            }
            m_gesture = Gesture::Move;
        } else if (canHit) {
            if (additive) {
                auto ids = m_selection;
                if (ids.contains(*hit)) {
                    ids.removeAll(*hit);
                } else {
                    ids.append(*hit);
                }
                setSelection(ids);
                // 增减选区的按键点击不顺带移动元素，避免 Ctrl 点击轻微手抖产生误拖。
                m_gesture = Gesture::None;
                return true;
            }
            if (!m_selection.contains(*hit)) {
                setSelection({*hit});
            }
            m_gesture = Gesture::Move;
        } else {
            const auto imageRect = QRect(m_canvas->printableImageOriginPx(), m_canvas->printableImageSizePx());
            if (!imageRect.contains(position)) {
                return false;
            }
            m_marqueeBaseSelection = additive ? m_selection : QStringList{};
            if (!additive) {
                setSelection({});
            }
            m_gesture = Gesture::Marquee;
        }
    }
    m_beforeGesture = m_host.document();
    m_startBounds = sleekpr::core::TemplateSelectionEditModel::bounds(*m_beforeGesture, editableIds(m_selection));
    return true;
}

QPointF TemplateCanvasEditor::snappedDelta(QPointF delta)
{
    QList<QLineF> guides;
    if (!m_snapEnabled) {
        m_canvas->setEditorSnapGuidesMm(guides);
        return delta;
    }
    const auto paper = m_host.paperSize();
    const auto inverse = m_host.displayTransform().inverted();
    const auto paperRect = inverse.mapRect(QRectF(QPointF(), paper));
    QList<double> targetsX{paperRect.left(), paperRect.center().x(), paperRect.right()};
    QList<double> targetsY{paperRect.top(), paperRect.center().y(), paperRect.bottom()};
    for (const auto& layer : m_beforeGesture->layers) {
        if (!layer.visible) {
            continue;
        }
        for (const auto& element : layer.elements) {
            if (element.visible && !m_selection.contains(element.id)) {
                const auto rect = rawRect(element);
                targetsX.append({rect.left(), rect.center().x(), rect.right()});
                targetsY.append({rect.top(), rect.center().y(), rect.bottom()});
            }
        }
    }
    const auto moved = m_startBounds.translated(delta);
    const auto tolerance = documentPoint(m_startPosition + QPoint(5, 5)) - documentPoint(m_startPosition);
    const auto snapAxis = [](const QList<double>& targets, const QList<double>& sources, double limit) -> std::optional<QPair<double, double>> {
        double best = std::abs(limit);
        std::optional<QPair<double, double>> result;
        for (const auto target : targets) {
            for (const auto source : sources) {
                const auto offset = target - source;
                if (std::abs(offset) <= best) {
                    best = std::abs(offset);
                    result = qMakePair(offset, target);
                }
            }
        }
        return result;
    };
    const auto xSnap = snapAxis(targetsX, {moved.left(), moved.center().x(), moved.right()}, tolerance.x());
    const auto ySnap = snapAxis(targetsY, {moved.top(), moved.center().y(), moved.bottom()}, tolerance.y());
    const auto transform = m_host.displayTransform();
    if (xSnap.has_value()) {
        delta.rx() += xSnap->first;
        guides.append(QLineF(transform.map(QPointF(xSnap->second, paperRect.top())), transform.map(QPointF(xSnap->second, paperRect.bottom()))));
    }
    if (ySnap.has_value()) {
        delta.ry() += ySnap->first;
        guides.append(QLineF(transform.map(QPointF(paperRect.left(), ySnap->second)), transform.map(QPointF(paperRect.right(), ySnap->second))));
    }
    m_canvas->setEditorSnapGuidesMm(guides);
    return delta;
}

QRectF TemplateCanvasEditor::resizedBounds(QPointF delta) const
{
    auto result = m_startBounds;
    constexpr double minimum = 0.5;
    const auto bounded = [](double value, double lower, double upper) {
        // 旧模板可能小于最小边，先保证区间合法，再由核心模型统一补齐最小尺寸。
        return std::clamp(value, std::min(lower, upper), std::max(lower, upper));
    };
    if (m_resizeEdges.testFlag(Qt::LeftEdge)) {
        result.setLeft(bounded(result.left() + delta.x(), 0.0, result.right() - minimum));
    }
    if (m_resizeEdges.testFlag(Qt::RightEdge)) {
        result.setRight(bounded(result.right() + delta.x(), result.left() + minimum, m_host.paperSize().width()));
    }
    if (m_resizeEdges.testFlag(Qt::TopEdge)) {
        result.setTop(bounded(result.top() + delta.y(), 0.0, result.bottom() - minimum));
    }
    if (m_resizeEdges.testFlag(Qt::BottomEdge)) {
        result.setBottom(bounded(result.bottom() + delta.y(), result.top() + minimum, m_host.paperSize().height()));
    }
    return result;
}

void TemplateCanvasEditor::updateGesture(QPoint delta)
{
    if (!m_beforeGesture.has_value() || m_gesture == Gesture::None) {
        return;
    }
    m_accumulatedDelta += delta;
    const auto currentPoint = m_startPosition + m_accumulatedDelta;
    auto deltaMm = documentPoint(currentPoint) - documentPoint(m_startPosition);
    if (m_gesture == Gesture::Marquee) {
        const auto rect = QRectF(documentPoint(m_startPosition), documentPoint(currentPoint)).normalized();
        auto ids = m_marqueeBaseSelection;
        for (const auto& layer : m_host.document().layers) {
            for (const auto& element : layer.elements) {
                if (editable(layer, element) && rect.intersects(rawRect(element)) && !ids.contains(element.id)) {
                    ids.append(element.id);
                }
            }
        }
        m_canvas->setEditorMarqueeRectMm(m_host.displayTransform().mapRect(rect));
        setSelection(ids);
        return;
    }
    // 始终从手势起始快照计算，避免吸附后的微小增量累计造成粘滞和尺寸漂移。
    auto next = m_duplicateDocument.has_value() ? *m_duplicateDocument : *m_beforeGesture;
    auto ids = editableIds(m_selection);
    if (m_duplicateOnDrag && m_gesture == Gesture::Move) {
        ids = sleekpr::core::TemplateSelectionEditModel::duplicate(next, ids, m_host.targetLayerId(), {}, m_host.paperSize());
        if (ids.isEmpty()) {
            return;
        }
        m_duplicateOnDrag = false;
        m_duplicateDocument = next;
        // 复制只生成一次，起始快照保留给取消和撤销，后续像素事件复用同一组 id。
        m_selection = ids;
        m_gestureChanged = true;
    }
    bool changed = false;
    if (m_gesture == Gesture::Move) {
        deltaMm = snappedDelta(deltaMm);
        changed = sleekpr::core::TemplateSelectionEditModel::move(next, ids, deltaMm, m_host.paperSize());
    } else if (m_gesture == Gesture::Resize) {
        changed = sleekpr::core::TemplateSelectionEditModel::resize(next, ids, resizedBounds(deltaMm), m_host.paperSize());
    }
    if (changed || m_gestureChanged) {
        m_host.document() = std::move(next);
        m_gestureChanged = true;
        refreshOverlay();
        m_host.documentChanged(false);
    }
}

void TemplateCanvasEditor::finishGesture(bool cancelled)
{
    if (m_gesture == Gesture::None) {
        return;
    }
    const auto changed = m_gestureChanged;
    m_gesture = Gesture::None;
    m_canvas->setEditorMarqueeRectMm({});
    m_canvas->setEditorSnapGuidesMm({});
    if (cancelled && m_beforeGesture.has_value()) {
        m_host.document() = *m_beforeGesture;
        m_selection = m_beforeSelection;
    }
    // 序列化比较只在释放时做一次，单击和拖回原位不增加撤销记录。
    const auto commit = changed && !cancelled && m_beforeGesture.has_value()
        && !sameDocument(*m_beforeGesture, m_host.document());
    m_beforeGesture.reset();
    m_duplicateDocument.reset();
    m_gestureChanged = false;
    if (changed) {
        m_host.documentChanged(commit);
    }
    publishSelection();
}

void TemplateCanvasEditor::nudge(QPoint direction, Qt::KeyboardModifiers modifiers)
{
    m_host.beforeEdit();
    const auto step = modifiers.testFlag(Qt::ShiftModifier) ? 1.0 : 0.1;
    commitCommand(sleekpr::core::TemplateSelectionEditModel::move(m_host.document(), editableIds(m_selection),
        QPointF(direction) * step, m_host.paperSize()));
}

void TemplateCanvasEditor::commitCommand(bool changed)
{
    if (changed) {
        m_host.documentChanged(true);
        publishSelection();
    }
}

void TemplateCanvasEditor::duplicateSelection()
{
    m_host.beforeEdit();
    const auto ids = sleekpr::core::TemplateSelectionEditModel::duplicate(m_host.document(), editableIds(m_selection),
        m_host.targetLayerId(), {2.0, 2.0}, m_host.paperSize());
    if (!ids.isEmpty()) {
        m_selection = ids;
        commitCommand(true);
    }
}

void TemplateCanvasEditor::copySelection()
{
    m_host.beforeEdit();
    auto document = m_host.document();
    const auto ids = editableIds(m_selection);
    if (ids.isEmpty()) {
        return;
    }
    sleekpr::core::TemplateLayer clipboardLayer;
    clipboardLayer.id = QStringLiteral("clipboard");
    for (const auto& layer : document.layers) {
        for (auto element : layer.elements) {
            if (ids.contains(element.id)) {
                element.layerId = clipboardLayer.id;
                clipboardLayer.elements.append(element);
            }
        }
    }
    if (clipboardLayer.elements.size() > kClipboardElementLimit) {
        return;
    }
    document.layers = {clipboardLayer};
    document.versions.clear();
    document.sampleData = {};
    document.fieldSchema = {};
    document.deviceProfiles.clear();
    const auto bytes = QJsonDocument(sleekpr::core::TemplateDocumentJson::toJson(document)).toJson(QJsonDocument::Compact);
    if (bytes.size() > kClipboardBytesLimit) {
        return;
    }
    auto* mime = new QMimeData();
    mime->setData(kElementsMime, bytes);
    QApplication::clipboard()->setMimeData(mime);
}

void TemplateCanvasEditor::pasteSelection()
{
    const auto* mime = QApplication::clipboard()->mimeData();
    if (mime == nullptr || !mime->hasFormat(kElementsMime)) {
        return;
    }
    const auto bytes = mime->data(kElementsMime);
    if (bytes.size() > kClipboardBytesLimit) {
        return;
    }
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(bytes, &error);
    QString message;
    if (error.error != QJsonParseError::NoError || !json.isObject()
        || !sleekpr::core::TemplateDocumentJson::validateForImport(json.object(), &message)) {
        return;
    }
    const auto copied = sleekpr::core::TemplateDocumentJson::fromJson(json.object());
    QList<sleekpr::core::TemplateElement> elements;
    for (const auto& layer : copied.layers) {
        elements.append(layer.elements);
    }
    if (elements.isEmpty() || elements.size() > kClipboardElementLimit || !safeClipboardElements(elements)) {
        return;
    }
    m_host.beforeEdit();
    const auto ids = sleekpr::core::TemplateSelectionEditModel::paste(m_host.document(), elements,
        m_host.targetLayerId(), {2.0, 2.0}, m_host.paperSize());
    if (!ids.isEmpty()) {
        m_selection = ids;
        commitCommand(true);
    }
}

void TemplateCanvasEditor::deleteSelection()
{
    m_host.beforeEdit();
    const auto ids = editableIds(m_selection);
    bool changed = false;
    for (const auto& id : ids) {
        changed |= sleekpr::core::TemplateDocumentEditModel::deleteElement(m_host.document(), id);
    }
    if (changed) {
        m_selection.clear();
        commitCommand(true);
    }
}

void TemplateCanvasEditor::alignSelection(sleekpr::core::SelectionAlignment alignment)
{
    m_host.beforeEdit();
    commitCommand(sleekpr::core::TemplateSelectionEditModel::align(m_host.document(), editableIds(m_selection), alignment, m_host.paperSize()));
}

bool TemplateCanvasEditor::handleShortcut(int key, Qt::KeyboardModifiers modifiers)
{
    const auto relevant = modifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    if (relevant == Qt::ControlModifier) {
        switch (key) {
        case Qt::Key_A: {
            QStringList ids;
            for (const auto& layer : m_host.document().layers) {
                for (const auto& element : layer.elements) {
                    if (editable(layer, element)) {
                        ids.append(element.id);
                    }
                }
            }
            setSelection(ids);
            return true;
        }
        case Qt::Key_C: copySelection(); return true;
        case Qt::Key_V: pasteSelection(); return true;
        case Qt::Key_D: duplicateSelection(); return true;
        case Qt::Key_Z: m_host.beforeEdit(); m_host.undo(); return true;
        case Qt::Key_Y: m_host.beforeEdit(); m_host.redo(); return true;
        default: break;
        }
    }
    if (relevant == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_Z) {
        m_host.beforeEdit();
        m_host.redo();
        return true;
    }
    if (relevant == Qt::NoModifier && (key == Qt::Key_Delete || key == Qt::Key_Backspace)) {
        deleteSelection();
        return true;
    }
    if (relevant == Qt::NoModifier && key == Qt::Key_Escape) {
        setSelection({});
        return true;
    }
    return false;
}

void TemplateCanvasEditor::installActions(QWidget* toolParent, QMenu* menu)
{
    const auto add = [this, toolParent, menu](const QString& text, const QString& name, QStyle::StandardPixmap icon,
                         const QString& shortcut, const std::function<void()>& callback) {
        auto* action = new QAction(toolParent->style()->standardIcon(icon), text, this);
        action->setObjectName(name);
        action->setToolTip(text);
        action->setShortcut(QKeySequence(shortcut));
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(action, &QAction::triggered, this, callback);
        toolParent->addAction(action);
        m_canvas->addAction(action);
        if (menu != nullptr) {
            menu->addAction(action);
        }
        return action;
    };
    if (menu != nullptr) {
        menu->addSeparator();
    }
    m_selectionActions.append(add(QString::fromUtf8("复制"), QStringLiteral("designerCopyAction"), QStyle::SP_FileIcon,
        QStringLiteral("Ctrl+C"), [this] { copySelection(); }));
    m_pasteAction = add(QString::fromUtf8("粘贴"), QStringLiteral("designerPasteAction"), QStyle::SP_DialogOpenButton,
        QStringLiteral("Ctrl+V"), [this] { pasteSelection(); });
    m_selectionActions.append(add(QString::fromUtf8("创建副本"), QStringLiteral("designerDuplicateAction"), QStyle::SP_FileLinkIcon,
        QStringLiteral("Ctrl+D"), [this] { duplicateSelection(); }));
    auto* alignMenu = new QMenu(QString::fromUtf8("对齐与分布"), toolParent);
    const std::pair<const char*, sleekpr::core::SelectionAlignment> alignments[] = {
        {"左对齐", sleekpr::core::SelectionAlignment::Left}, {"水平居中", sleekpr::core::SelectionAlignment::HorizontalCenter},
        {"右对齐", sleekpr::core::SelectionAlignment::Right}, {"顶端对齐", sleekpr::core::SelectionAlignment::Top},
        {"垂直居中", sleekpr::core::SelectionAlignment::VerticalCenter}, {"底端对齐", sleekpr::core::SelectionAlignment::Bottom},
        {"水平等距分布", sleekpr::core::SelectionAlignment::DistributeHorizontally},
        {"垂直等距分布", sleekpr::core::SelectionAlignment::DistributeVertically}};
    for (const auto& [name, alignment] : alignments) {
        auto* action = alignMenu->addAction(QString::fromUtf8(name));
        action->setObjectName(QStringLiteral("designerAlign%1Action").arg(static_cast<int>(alignment)));
        connect(action, &QAction::triggered, this, [this, alignment] { alignSelection(alignment); });
        m_selectionActions.append(action);
        if (alignment == sleekpr::core::SelectionAlignment::DistributeHorizontally) m_distributeHorizontalAction = action;
        if (alignment == sleekpr::core::SelectionAlignment::DistributeVertically) m_distributeVerticalAction = action;
    }
    toolParent->addAction(alignMenu->menuAction());
    if (menu != nullptr) menu->addMenu(alignMenu);
    auto* snap = new QAction(QString::fromUtf8("智能吸附"), this);
    snap->setObjectName(QStringLiteral("designerSnapAction"));
    snap->setCheckable(true);
    snap->setChecked(true);
    connect(snap, &QAction::toggled, this, [this](bool enabled) { m_snapEnabled = enabled; });
    toolParent->addAction(snap);
    if (menu != nullptr) menu->addAction(snap);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] { updateActions(); });
    m_canvas->setContextMenuCallback([this, toolParent](QPoint position, QPoint globalPosition) {
        if (const auto hit = m_host.hitTest(position); hit.has_value() && !m_selection.contains(*hit)) {
            setSelection({*hit});
        }
        QMenu context(m_canvas);
        for (auto* action : toolParent->actions()) context.addAction(action);
        context.exec(globalPosition);
    });
    updateActions();
}

void TemplateCanvasEditor::updateActions()
{
    const auto count = editableIds(m_selection).size();
    for (auto* action : m_selectionActions) action->setEnabled(count > 0);
    if (m_distributeHorizontalAction != nullptr) m_distributeHorizontalAction->setEnabled(count >= 3);
    if (m_distributeVerticalAction != nullptr) m_distributeVerticalAction->setEnabled(count >= 3);
    if (m_pasteAction != nullptr) {
        const auto* mime = QApplication::clipboard()->mimeData();
        m_pasteAction->setEnabled(mime != nullptr && mime->hasFormat(kElementsMime));
    }
}

} // 命名空间 sleekpr::app
