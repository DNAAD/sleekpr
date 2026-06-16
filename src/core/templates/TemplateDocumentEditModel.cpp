#include "sleekpr/core/templates/TemplateDocumentEditModel.h"

namespace sleekpr::core {
namespace {

bool hasCleanId(const QString& id)
{
    return !id.isEmpty() && id == id.trimmed();
}

int layerIndexOf(const TemplateDocument& document, const QString& layerId)
{
    for (int index = 0; index < document.layers.size(); ++index) {
        if (document.layers[index].id == layerId) {
            return index;
        }
    }
    return -1;
}

int versionIndexOf(const TemplateDocument& document, const QString& versionId)
{
    for (int index = 0; index < document.versions.size(); ++index) {
        if (document.versions[index].id == versionId) {
            return index;
        }
    }
    return -1;
}

void normalizeZIndex(TemplateLayer& layer)
{
    // 图层内顺序是唯一来源，每次重排后都把 zIndex 压成连续值。
    for (int index = 0; index < layer.elements.size(); ++index) {
        layer.elements[index].zIndex = index;
    }
}

void normalizeTableZIndex(TemplateLayer& layer)
{
    // 表格和普通元素分别排序，避免旧版普通元素的 zIndex 被表格改动扰动。
    for (int index = 0; index < layer.tables.size(); ++index) {
        layer.tables[index].zIndex = index;
    }
}

struct ElementLocation
{
    int layerIndex = -1;
    int elementIndex = -1;
};

ElementLocation elementLocationOf(const TemplateDocument& document, const QString& elementId)
{
    for (int layerIndex = 0; layerIndex < document.layers.size(); ++layerIndex) {
        const auto& elements = document.layers[layerIndex].elements;
        for (int elementIndex = 0; elementIndex < elements.size(); ++elementIndex) {
            if (elements[elementIndex].id == elementId) {
                return ElementLocation{layerIndex, elementIndex};
            }
        }
    }
    return {};
}

bool elementExists(const TemplateDocument& document, const QString& elementId)
{
    return elementLocationOf(document, elementId).elementIndex >= 0;
}

struct TableLocation
{
    int layerIndex = -1;
    int tableIndex = -1;
};

TableLocation tableLocationOf(const TemplateDocument& document, const QString& tableId)
{
    for (int layerIndex = 0; layerIndex < document.layers.size(); ++layerIndex) {
        const auto& tables = document.layers[layerIndex].tables;
        for (int tableIndex = 0; tableIndex < tables.size(); ++tableIndex) {
            if (tables[tableIndex].id == tableId) {
                return TableLocation{layerIndex, tableIndex};
            }
        }
    }
    return {};
}

bool tableExists(const TemplateDocument& document, const QString& tableId)
{
    return tableLocationOf(document, tableId).tableIndex >= 0;
}

bool itemIdExists(const TemplateDocument& document, const QString& itemId)
{
    return elementExists(document, itemId) || tableExists(document, itemId);
}

bool canEditElement(const TemplateDocument& document, const ElementLocation& location)
{
    if (location.layerIndex < 0 || location.elementIndex < 0) {
        return false;
    }

    const auto& layer = document.layers[location.layerIndex];
    return !layer.locked && !layer.elements[location.elementIndex].locked;
}

bool canEditElementLayer(const TemplateDocument& document, const ElementLocation& location)
{
    if (location.layerIndex < 0 || location.elementIndex < 0) {
        return false;
    }

    return !document.layers[location.layerIndex].locked;
}

bool canEditTable(const TemplateDocument& document, const TableLocation& location)
{
    if (location.layerIndex < 0 || location.tableIndex < 0) {
        return false;
    }

    const auto& layer = document.layers[location.layerIndex];
    return !layer.locked && !layer.tables[location.tableIndex].locked;
}

bool canEditTableLayer(const TemplateDocument& document, const TableLocation& location)
{
    if (location.layerIndex < 0 || location.tableIndex < 0) {
        return false;
    }

    return !document.layers[location.layerIndex].locked;
}

} // 匿名命名空间

bool TemplateDocumentEditModel::addLayer(TemplateDocument& document, const QString& id, const QString& name)
{
    if (!hasCleanId(id) || layerIndexOf(document, id) >= 0) {
        return false;
    }

    TemplateLayer layer;
    layer.id = id;
    layer.name = name;
    document.layers.append(layer);
    return true;
}

bool TemplateDocumentEditModel::deleteLayer(TemplateDocument& document, const QString& layerId)
{
    const auto index = layerIndexOf(document, layerId);
    if (index < 0 || document.layers[index].locked) {
        return false;
    }

    document.layers.removeAt(index);
    return true;
}

bool TemplateDocumentEditModel::moveLayerUp(TemplateDocument& document, const QString& layerId)
{
    const auto index = layerIndexOf(document, layerId);
    if (index <= 0 || document.layers[index].locked) {
        return false;
    }

    document.layers.swapItemsAt(index, index - 1);
    return true;
}

bool TemplateDocumentEditModel::moveLayerDown(TemplateDocument& document, const QString& layerId)
{
    const auto index = layerIndexOf(document, layerId);
    if (index < 0 || document.layers[index].locked || index >= document.layers.size() - 1) {
        return false;
    }

    document.layers.swapItemsAt(index, index + 1);
    return true;
}

bool TemplateDocumentEditModel::setLayerVisible(TemplateDocument& document, const QString& layerId, bool visible)
{
    const auto index = layerIndexOf(document, layerId);
    if (index < 0) {
        return false;
    }

    document.layers[index].visible = visible;
    return true;
}

bool TemplateDocumentEditModel::setLayerLocked(TemplateDocument& document, const QString& layerId, bool locked)
{
    const auto index = layerIndexOf(document, layerId);
    if (index < 0) {
        return false;
    }

    document.layers[index].locked = locked;
    return true;
}

bool TemplateDocumentEditModel::addElement(TemplateDocument& document, const QString& layerId, const TemplateElement& element)
{
    const auto index = layerIndexOf(document, layerId);
    if (index < 0 || document.layers[index].locked || !hasCleanId(element.id) || itemIdExists(document, element.id)) {
        return false;
    }

    TemplateElement elementToAdd = element;
    elementToAdd.layerId = layerId;
    document.layers[index].elements.append(elementToAdd);
    normalizeZIndex(document.layers[index]);
    return true;
}

bool TemplateDocumentEditModel::deleteElement(TemplateDocument& document, const QString& elementId)
{
    const auto location = elementLocationOf(document, elementId);
    if (!canEditElement(document, location)) {
        return false;
    }

    auto& layer = document.layers[location.layerIndex];
    layer.elements.removeAt(location.elementIndex);
    normalizeZIndex(layer);
    return true;
}

bool TemplateDocumentEditModel::moveElement(TemplateDocument& document, const QString& elementId, double x, double y)
{
    const auto location = elementLocationOf(document, elementId);
    if (!canEditElement(document, location)) {
        return false;
    }

    auto& element = document.layers[location.layerIndex].elements[location.elementIndex];
    element.x = x;
    element.y = y;
    return true;
}

bool TemplateDocumentEditModel::moveElementUp(TemplateDocument& document, const QString& elementId)
{
    const auto location = elementLocationOf(document, elementId);
    if (!canEditElement(document, location) || location.elementIndex == 0) {
        return false;
    }

    auto& layer = document.layers[location.layerIndex];
    layer.elements.swapItemsAt(location.elementIndex, location.elementIndex - 1);
    normalizeZIndex(layer);
    return true;
}

bool TemplateDocumentEditModel::moveElementDown(TemplateDocument& document, const QString& elementId)
{
    const auto location = elementLocationOf(document, elementId);
    if (!canEditElement(document, location)) {
        return false;
    }

    auto& layer = document.layers[location.layerIndex];
    if (location.elementIndex >= layer.elements.size() - 1) {
        return false;
    }

    layer.elements.swapItemsAt(location.elementIndex, location.elementIndex + 1);
    normalizeZIndex(layer);
    return true;
}

bool TemplateDocumentEditModel::setElementVisible(TemplateDocument& document, const QString& elementId, bool visible)
{
    const auto location = elementLocationOf(document, elementId);
    if (!canEditElementLayer(document, location)) {
        return false;
    }

    document.layers[location.layerIndex].elements[location.elementIndex].visible = visible;
    return true;
}

bool TemplateDocumentEditModel::setElementLocked(TemplateDocument& document, const QString& elementId, bool locked)
{
    const auto location = elementLocationOf(document, elementId);
    if (!canEditElementLayer(document, location)) {
        return false;
    }

    document.layers[location.layerIndex].elements[location.elementIndex].locked = locked;
    return true;
}

bool TemplateDocumentEditModel::addTable(TemplateDocument& document, const QString& layerId, const TableElement& table)
{
    const auto index = layerIndexOf(document, layerId);
    if (index < 0 || document.layers[index].locked || !hasCleanId(table.id) || itemIdExists(document, table.id)) {
        return false;
    }

    TableElement tableToAdd = table;
    tableToAdd.layerId = layerId;
    document.layers[index].tables.append(tableToAdd);
    normalizeTableZIndex(document.layers[index]);
    return true;
}

bool TemplateDocumentEditModel::updateTable(TemplateDocument& document, const QString& tableId, const TableElement& table)
{
    const auto location = tableLocationOf(document, tableId);
    if (!canEditTable(document, location) || !hasCleanId(table.id) || table.id != tableId) {
        return false;
    }

    auto& layer = document.layers[location.layerIndex];
    if (!table.layerId.isEmpty() && table.layerId != layer.id) {
        return false;
    }

    const auto zIndex = layer.tables[location.tableIndex].zIndex;
    TableElement tableToUpdate = table;
    tableToUpdate.layerId = layer.id;
    tableToUpdate.zIndex = zIndex;
    layer.tables[location.tableIndex] = tableToUpdate;
    normalizeTableZIndex(layer);
    return true;
}

bool TemplateDocumentEditModel::deleteTable(TemplateDocument& document, const QString& tableId)
{
    const auto location = tableLocationOf(document, tableId);
    if (!canEditTable(document, location)) {
        return false;
    }

    auto& layer = document.layers[location.layerIndex];
    layer.tables.removeAt(location.tableIndex);
    normalizeTableZIndex(layer);
    return true;
}

bool TemplateDocumentEditModel::moveTable(TemplateDocument& document, const QString& tableId, double x, double y)
{
    const auto location = tableLocationOf(document, tableId);
    if (!canEditTable(document, location)) {
        return false;
    }

    auto& table = document.layers[location.layerIndex].tables[location.tableIndex];
    table.x = x;
    table.y = y;
    return true;
}

bool TemplateDocumentEditModel::setTableVisible(TemplateDocument& document, const QString& tableId, bool visible)
{
    const auto location = tableLocationOf(document, tableId);
    if (!canEditTableLayer(document, location)) {
        return false;
    }

    document.layers[location.layerIndex].tables[location.tableIndex].visible = visible;
    return true;
}

bool TemplateDocumentEditModel::setTableLocked(TemplateDocument& document, const QString& tableId, bool locked)
{
    const auto location = tableLocationOf(document, tableId);
    if (!canEditTableLayer(document, location)) {
        return false;
    }

    document.layers[location.layerIndex].tables[location.tableIndex].locked = locked;
    return true;
}

bool TemplateDocumentEditModel::saveVersion(
    TemplateDocument& document,
    const QString& versionId,
    const QString& name,
    const QString& createdAt,
    const QString& note)
{
    if (!hasCleanId(versionId) || versionIndexOf(document, versionId) >= 0) {
        return false;
    }

    TemplateVersion version;
    version.id = versionId;
    version.name = name;
    version.createdAt = createdAt;
    version.note = note;
    version.layersSnapshot = document.layers;
    document.versions.append(version);
    document.activeVersionId = versionId;
    return true;
}

bool TemplateDocumentEditModel::restoreVersion(TemplateDocument& document, const QString& versionId)
{
    const auto index = versionIndexOf(document, versionId);
    if (index < 0) {
        return false;
    }

    document.layers = document.versions[index].layersSnapshot;
    document.activeVersionId = versionId;
    return true;
}

} // 命名空间 sleekpr::core
