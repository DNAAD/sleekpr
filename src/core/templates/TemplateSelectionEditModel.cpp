#include "sleekpr/core/templates/TemplateSelectionEditModel.h"

#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace sleekpr::core {
namespace {

constexpr double minimumSideMm = 0.5;
constexpr double geometryToleranceMm = 1e-9;

struct ElementLocation
{
    qsizetype layerIndex;
    qsizetype elementIndex;
    QRectF rectMm;
};

bool validPaper(QSizeF paper)
{
    return std::isfinite(paper.width()) && std::isfinite(paper.height())
        && paper.width() > 0.0 && paper.height() > 0.0;
}

bool validPoint(QPointF point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool validRect(const QRectF& rect)
{
    // 同时检查端点，防止有限坐标相加溢出，或极端坐标吞掉正尺寸后出现退化矩形。
    return validPoint(rect.topLeft()) && std::isfinite(rect.width()) && std::isfinite(rect.height())
        && rect.width() > 0.0 && rect.height() > 0.0 && validPoint(rect.bottomRight())
        && rect.right() > rect.left() && rect.bottom() > rect.top();
}

QRectF elementRect(const TemplateElement& element)
{
    return {element.x, element.y, element.width, element.height};
}

bool collectSelection(const TemplateDocument& document, const QStringList& ids, QList<ElementLocation>& selection)
{
    QSet<QString> requested(ids.cbegin(), ids.cend());
    requested.remove(QString());
    for (qsizetype layerIndex = 0; layerIndex < document.layers.size(); ++layerIndex) {
        const auto& layer = document.layers[layerIndex];
        if (!layer.visible || layer.locked) {
            continue;
        }
        for (qsizetype elementIndex = 0; elementIndex < layer.elements.size(); ++elementIndex) {
            const auto& element = layer.elements[elementIndex];
            if (!element.visible || element.locked || !requested.contains(element.id)) {
                continue;
            }
            const auto rect = elementRect(element);
            if (!validRect(rect)) {
                return false;
            }
            requested.remove(element.id);
            selection.append({layerIndex, elementIndex, rect});
        }
    }
    return !selection.isEmpty();
}

QList<QRectF> selectionRects(const QList<ElementLocation>& selection)
{
    QList<QRectF> rects;
    rects.reserve(selection.size());
    for (const auto& location : selection) {
        rects.append(location.rectMm);
    }
    return rects;
}

QRectF combinedBounds(const QList<QRectF>& rects)
{
    if (rects.isEmpty()) {
        return {};
    }
    auto result = rects.first();
    for (const auto& rect : rects) {
        if (!validRect(rect)) {
            return {};
        }
        result = result.united(rect);
    }
    return validRect(result) ? result : QRectF();
}

bool constrainToPaper(QList<QRectF>& rects, QPointF delta, QSizeF paper)
{
    if (!validPaper(paper) || !validPoint(delta)) {
        return false;
    }
    const auto bounds = combinedBounds(rects);
    if (!validRect(bounds) || bounds.width() - paper.width() > geometryToleranceMm
        || bounds.height() - paper.height() > geometryToleranceMm) {
        return false;
    }
    const auto minX = -bounds.left();
    const auto minY = -bounds.top();
    const auto maxX = paper.width() - bounds.right();
    const auto maxY = paper.height() - bounds.bottom();
    if (!validPoint({minX, minY}) || !validPoint({maxX, maxY})) {
        return false;
    }
    // 先约束整组选区的同一个位移，不逐元素裁剪，避免破坏相对位置和均匀间距。
    // 浮点运算可能产生极小边界误差，容差仅处理舍入，不允许实际超出纸张。
    const QPointF adjusted(std::clamp(delta.x(), minX, std::max(minX, maxX)),
        std::clamp(delta.y(), minY, std::max(minY, maxY)));
    for (auto& rect : rects) {
        rect.translate(adjusted);
        if (!validRect(rect) || rect.left() < -geometryToleranceMm || rect.top() < -geometryToleranceMm
            || rect.right() - paper.width() > geometryToleranceMm
            || rect.bottom() - paper.height() > geometryToleranceMm) {
            return false;
        }
    }
    return true;
}

bool applyRects(TemplateDocument& document, const QList<ElementLocation>& selection, const QList<QRectF>& rects)
{
    const auto differs = [](const QRectF& left, const QRectF& right) {
        return std::abs(left.x() - right.x()) > geometryToleranceMm
            || std::abs(left.y() - right.y()) > geometryToleranceMm
            || std::abs(left.width() - right.width()) > geometryToleranceMm
            || std::abs(left.height() - right.height()) > geometryToleranceMm;
    };
    bool changed = false;
    for (qsizetype index = 0; index < selection.size(); ++index) {
        changed = changed || differs(selection[index].rectMm, rects[index]);
    }
    if (!changed) {
        return false;
    }
    // 所有几何验证先在副本完成，校验成功后才写入，保证失败不会留下半次编辑。
    for (qsizetype index = 0; index < selection.size(); ++index) {
        const auto& location = selection[index];
        auto& element = document.layers[location.layerIndex].elements[location.elementIndex];
        const auto& rect = rects[index];
        element.x = rect.x();
        element.y = rect.y();
        element.width = rect.width();
        element.height = rect.height();
    }
    return true;
}

bool distribute(QList<QRectF>& rects, const QRectF& bounds, bool horizontal)
{
    if (rects.size() < 3) {
        return false;
    }
    QList<qsizetype> order(rects.size());
    std::iota(order.begin(), order.end(), qsizetype(0));
    std::stable_sort(order.begin(), order.end(), [&rects, horizontal](qsizetype left, qsizetype right) {
        return horizontal ? rects[left].left() < rects[right].left() : rects[left].top() < rects[right].top();
    });
    double occupied = 0.0;
    for (const auto& rect : rects) {
        occupied += horizontal ? rect.width() : rect.height();
    }
    // 分布的是边缘间距而非中心距离；原有元素重叠时允许负间距，保持原选区跨度。
    const auto span = horizontal ? bounds.width() : bounds.height();
    const auto gap = (span - occupied) / double(rects.size() - 1);
    if (!std::isfinite(gap)) {
        return false;
    }
    double cursor = horizontal ? bounds.left() : bounds.top();
    double previous = cursor;
    for (const auto index : order) {
        auto& rect = rects[index];
        if (horizontal) {
            rect.moveLeft(cursor);
        } else {
            rect.moveTop(cursor);
        }
        const auto start = horizontal ? rect.left() : rect.top();
        const auto end = horizontal ? rect.right() : rect.bottom();
        const auto limit = horizontal ? bounds.right() : bounds.bottom();
        // 极端重叠无法同时保持原跨度与顺序时拒绝，不扩大选区或交换元素次序。
        if (!validRect(rect) || start < previous - geometryToleranceMm || end - limit > geometryToleranceMm) {
            return false;
        }
        previous = start;
        cursor += (horizontal ? rect.width() : rect.height()) + gap;
    }
    return true;
}

qsizetype writableLayerIndex(const TemplateDocument& document, const QString& id)
{
    if (id.isEmpty()) {
        return -1;
    }
    for (qsizetype index = 0; index < document.layers.size(); ++index) {
        const auto& layer = document.layers[index];
        if (layer.id == id) {
            return layer.visible && !layer.locked ? index : -1;
        }
    }
    return -1;
}

} // 匿名命名空间

QRectF TemplateSelectionEditModel::bounds(const TemplateDocument& document, const QStringList& ids)
{
    QList<ElementLocation> selection;
    return collectSelection(document, ids, selection) ? combinedBounds(selectionRects(selection)) : QRectF();
}

bool TemplateSelectionEditModel::move(TemplateDocument& document, const QStringList& ids, QPointF deltaMm, QSizeF paperSizeMm)
{
    QList<ElementLocation> selection;
    if (!collectSelection(document, ids, selection)) {
        return false;
    }
    auto rects = selectionRects(selection);
    return constrainToPaper(rects, deltaMm, paperSizeMm) && applyRects(document, selection, rects);
}

bool TemplateSelectionEditModel::resize(TemplateDocument& document, const QStringList& ids, QRectF targetBoundsMm, QSizeF paperSizeMm)
{
    QList<ElementLocation> selection;
    if (!validPaper(paperSizeMm) || !validRect(targetBoundsMm) || !collectSelection(document, ids, selection)) {
        return false;
    }
    auto rects = selectionRects(selection);
    const auto source = combinedBounds(rects);
    if (!validRect(source)) {
        return false;
    }
    double minimumScaleX = 0.0;
    double minimumScaleY = 0.0;
    for (const auto& rect : rects) {
        minimumScaleX = std::max(minimumScaleX, minimumSideMm / rect.width());
        minimumScaleY = std::max(minimumScaleY, minimumSideMm / rect.height());
    }
    // 最小边约束作用于整个选区比例，不能单独放大某个小元素而破坏相对布局。
    const auto minimumWidth = std::max(minimumSideMm, source.width() * minimumScaleX);
    const auto minimumHeight = std::max(minimumSideMm, source.height() * minimumScaleY);
    if (!std::isfinite(minimumWidth) || !std::isfinite(minimumHeight)
        || minimumWidth > paperSizeMm.width() || minimumHeight > paperSizeMm.height()) {
        return false;
    }
    const auto width = std::clamp(targetBoundsMm.width(), minimumWidth, paperSizeMm.width());
    const auto height = std::clamp(targetBoundsMm.height(), minimumHeight, paperSizeMm.height());
    const QRectF target(std::clamp(targetBoundsMm.x(), 0.0, paperSizeMm.width() - width),
        std::clamp(targetBoundsMm.y(), 0.0, paperSizeMm.height() - height), width, height);
    const auto scaleX = target.width() / source.width();
    const auto scaleY = target.height() / source.height();
    for (auto& rect : rects) {
        rect = {target.x() + (rect.x() - source.x()) * scaleX,
            target.y() + (rect.y() - source.y()) * scaleY,
            std::max(minimumSideMm, rect.width() * scaleX), std::max(minimumSideMm, rect.height() * scaleY)};
    }
    return constrainToPaper(rects, {}, paperSizeMm) && applyRects(document, selection, rects);
}

bool TemplateSelectionEditModel::align(TemplateDocument& document, const QStringList& ids,
    SelectionAlignment alignment, QSizeF paperSizeMm)
{
    QList<ElementLocation> selection;
    if (!validPaper(paperSizeMm) || !collectSelection(document, ids, selection)) {
        return false;
    }
    auto rects = selectionRects(selection);
    const auto source = combinedBounds(rects);
    if (!validRect(source)) {
        return false;
    }
    // 过滤之后只有一个可编辑元素时，以纸张为参照；多选以自身的设计矩形并集为参照。
    const auto reference = rects.size() == 1 ? QRectF(QPointF(), paperSizeMm) : source;
    if (alignment == SelectionAlignment::DistributeHorizontally || alignment == SelectionAlignment::DistributeVertically) {
        if (!distribute(rects, source, alignment == SelectionAlignment::DistributeHorizontally)) {
            return false;
        }
    } else {
        for (auto& rect : rects) {
            switch (alignment) {
            case SelectionAlignment::Left:
                rect.moveLeft(reference.left());
                break;
            case SelectionAlignment::HorizontalCenter:
                rect.moveLeft(reference.center().x() - rect.width() / 2.0);
                break;
            case SelectionAlignment::Right:
                rect.moveRight(reference.right());
                break;
            case SelectionAlignment::Top:
                rect.moveTop(reference.top());
                break;
            case SelectionAlignment::VerticalCenter:
                rect.moveTop(reference.center().y() - rect.height() / 2.0);
                break;
            case SelectionAlignment::Bottom:
                rect.moveBottom(reference.bottom());
                break;
            default:
                return false;
            }
        }
    }
    return constrainToPaper(rects, {}, paperSizeMm) && applyRects(document, selection, rects);
}

QStringList TemplateSelectionEditModel::duplicate(TemplateDocument& document, const QStringList& ids,
    const QString& targetLayerId, QPointF offsetMm, QSizeF paperSizeMm)
{
    QList<ElementLocation> selection;
    if (!collectSelection(document, ids, selection)) {
        return {};
    }
    std::stable_sort(selection.begin(), selection.end(), [&document](const ElementLocation& left, const ElementLocation& right) {
        if (left.layerIndex != right.layerIndex) {
            return left.layerIndex < right.layerIndex;
        }
        return document.layers[left.layerIndex].elements[left.elementIndex].zIndex
            < document.layers[right.layerIndex].elements[right.elementIndex].zIndex;
    });
    QList<TemplateElement> elements;
    elements.reserve(selection.size());
    for (const auto& location : selection) {
        elements.append(document.layers[location.layerIndex].elements[location.elementIndex]);
    }
    return paste(document, elements, targetLayerId, offsetMm, paperSizeMm);
}

QStringList TemplateSelectionEditModel::paste(TemplateDocument& document, const QList<TemplateElement>& elements,
    const QString& targetLayerId, QPointF offsetMm, QSizeF paperSizeMm)
{
    const auto layerIndex = writableLayerIndex(document, targetLayerId);
    if (layerIndex < 0 || !validPaper(paperSizeMm) || !validPoint(offsetMm)) {
        return {};
    }
    QList<TemplateElement> copies;
    QList<QRectF> rects;
    copies.reserve(elements.size());
    rects.reserve(elements.size());
    for (const auto& element : elements) {
        if (!element.visible || element.locked) {
            continue;
        }
        // 直接复制结构体，避免新加样式或绑定属性后遗漏；来源图层不参与跨模板粘贴校验。
        copies.append(element);
        rects.append(elementRect(element));
    }
    if (!constrainToPaper(rects, offsetMm, paperSizeMm)) {
        return {};
    }
    int highestZIndex = -1;
    for (const auto& element : document.layers[layerIndex].elements) {
        highestZIndex = std::max(highestZIndex, element.zIndex);
    }
    for (const auto& table : document.layers[layerIndex].tables) {
        highestZIndex = std::max(highestZIndex, table.zIndex);
    }
    if (copies.size() > qint64(std::numeric_limits<int>::max()) - highestZIndex) {
        return {};
    }
    QSet<QString> usedIds;
    for (const auto& layer : document.layers) {
        usedIds.insert(layer.id);
        for (const auto& element : layer.elements) {
            usedIds.insert(element.id);
        }
        for (const auto& table : layer.tables) {
            usedIds.insert(table.id);
        }
    }
    for (const auto& copy : copies) {
        usedIds.insert(copy.id);
    }
    QStringList result;
    result.reserve(copies.size());
    for (qsizetype index = 0; index < copies.size(); ++index) {
        auto& copy = copies[index];
        do {
            copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        } while (usedIds.contains(copy.id));
        usedIds.insert(copy.id);
        copy.layerId = targetLayerId;
        copy.zIndex = ++highestZIndex;
        copy.x = rects[index].x();
        copy.y = rects[index].y();
        result.append(copy.id);
    }
    // 输入可能正是目标图层的元素列表；全部取副本后再追加，避免迭代过程中失效或无限复制。
    document.layers[layerIndex].elements.append(copies);
    return result;
}

} // 命名空间 sleekpr::core
