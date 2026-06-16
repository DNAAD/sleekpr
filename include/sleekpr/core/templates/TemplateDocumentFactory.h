#pragma once

#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/templates/TemplateDocument.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

class TemplateDocumentFactory
{
public:
    static TemplateDocument fromDrawingPlan(
        const QString& templateKey,
        const QString& name,
        const NativeLabelDrawingPlan& drawingPlan,
        const QList<TemplateElement>& customElements);
};

} // 命名空间 sleekpr::core
