#pragma once

#include "sleekpr/app/TemplateDesignerCommand.h"

namespace sleekpr::core {
struct TableElement;
struct TemplateDocument;
struct TemplateElement;
}

namespace sleekpr::app {

class TemplateDesignerPresenter final
{
public:
    DesignerElementPropertyModel elementPropertyModel(
        const sleekpr::core::TemplateElement& element,
        bool canEdit) const;
    DesignerTablePropertyModel tablePropertyModel(
        const sleekpr::core::TableElement& table,
        bool canEdit) const;

    TemplateDesignerCommandResult applyElementProperties(
        sleekpr::core::TemplateElement& element,
        const DesignerElementPropertyModel& model) const;
    TemplateDesignerCommandResult applyTableProperties(
        sleekpr::core::TemplateDocument& document,
        const QString& tableId,
        const DesignerTablePropertyModel& model) const;
};

} // 命名空间 sleekpr::app
