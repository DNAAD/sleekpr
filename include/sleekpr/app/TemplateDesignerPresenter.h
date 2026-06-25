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

    // 表格列编辑器只处理 app 层模型，Presenter 统一负责和核心模板结构互转。
    DesignerTableColumnModel tableColumnModel(const sleekpr::core::TableColumn& column) const;
    sleekpr::core::TableColumn tableColumnFromModel(const DesignerTableColumnModel& model) const;
    QList<sleekpr::core::TableColumn> tableColumnsFromModels(const QList<DesignerTableColumnModel>& models) const;

    TemplateDesignerCommandResult applyElementProperties(
        sleekpr::core::TemplateElement& element,
        const DesignerElementPropertyModel& model) const;
    TemplateDesignerCommandResult applyTableProperties(
        sleekpr::core::TemplateDocument& document,
        const QString& tableId,
        const DesignerTablePropertyModel& model) const;
};

} // 命名空间 sleekpr::app
