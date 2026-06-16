#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QString>

namespace sleekpr::core {

class TemplateDocumentEditModel
{
public:
    static bool addLayer(TemplateDocument& document, const QString& id, const QString& name);
    static bool deleteLayer(TemplateDocument& document, const QString& layerId);
    static bool moveLayerUp(TemplateDocument& document, const QString& layerId);
    static bool moveLayerDown(TemplateDocument& document, const QString& layerId);
    static bool setLayerVisible(TemplateDocument& document, const QString& layerId, bool visible);
    static bool setLayerLocked(TemplateDocument& document, const QString& layerId, bool locked);
    static bool addElement(TemplateDocument& document, const QString& layerId, const TemplateElement& element);
    static bool deleteElement(TemplateDocument& document, const QString& elementId);
    static bool moveElement(TemplateDocument& document, const QString& elementId, double x, double y);
    static bool moveElementUp(TemplateDocument& document, const QString& elementId);
    static bool moveElementDown(TemplateDocument& document, const QString& elementId);
    static bool setElementVisible(TemplateDocument& document, const QString& elementId, bool visible);
    static bool setElementLocked(TemplateDocument& document, const QString& elementId, bool locked);
    static bool addTable(TemplateDocument& document, const QString& layerId, const TableElement& table);
    static bool updateTable(TemplateDocument& document, const QString& tableId, const TableElement& table);
    static bool deleteTable(TemplateDocument& document, const QString& tableId);
    static bool moveTable(TemplateDocument& document, const QString& tableId, double x, double y);
    static bool setTableVisible(TemplateDocument& document, const QString& tableId, bool visible);
    static bool setTableLocked(TemplateDocument& document, const QString& tableId, bool locked);
    static bool saveVersion(TemplateDocument& document, const QString& versionId, const QString& name, const QString& createdAt, const QString& note);
    static bool restoreVersion(TemplateDocument& document, const QString& versionId);
};

} // 命名空间 sleekpr::core
