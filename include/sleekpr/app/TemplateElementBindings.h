#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"
#include "sleekpr/core/templates/TemplateDocument.h"

namespace sleekpr::app {

class TemplateElementBindings final
{
public:
    static QList<DesignerFieldBinding> describe(const core::TemplateElement& element, const core::TemplateDocument& document);
    static QString apply(core::TemplateElement& updated, const core::TemplateElement& original, const QList<DesignerFieldBinding>& bindings,
        bool deferMerge = false);
    static bool references(const core::TemplateElement& element, DesignerBindingSource source, const QString& key);
};

} // 命名空间 sleekpr::app
