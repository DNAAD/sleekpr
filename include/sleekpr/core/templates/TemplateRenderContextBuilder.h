#pragma once

#include "sleekpr/core/templates/FieldSchema.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QJsonObject>
#include <QList>

namespace sleekpr::core {

class TemplateRenderContextBuilder
{
public:
    static TemplateRenderContext build(const QList<FieldDefinition>& fieldSchema,
                                       const QJsonObject& presetValues,
                                       const QJsonObject& requestValues);
};

} // 命名空间 sleekpr::core
