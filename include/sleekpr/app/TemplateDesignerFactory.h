#pragma once

#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/TableElement.h"
#include "sleekpr/core/templates/TemplateDocument.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace sleekpr::app {

class TemplateDesignerFactory final
{
public:
    static QString createTemplateId();
    static QString createDeviceProfileId();
    static QString createLayerId();
    static QString nextLayerName(int layerCount);

    static QString elementTypeKey(sleekpr::core::TemplateElementType type);
    static QString elementTypeName(sleekpr::core::TemplateElementType type);
    static QJsonObject createDefaultSampleData();

    static sleekpr::core::TemplateDocument createBlankDocument(
        const QString& id,
        const QString& name,
        const QString& templateKey,
        const QString& paperSpecId,
        const QList<sleekpr::core::DeviceProfile>& deviceProfiles);
    static sleekpr::core::TemplateElement createElement(sleekpr::core::TemplateElementType type);
    static sleekpr::core::TableElement createTable();

private:
    static QString createElementId(const QString& prefix);
};

} // 命名空间 sleekpr::app
