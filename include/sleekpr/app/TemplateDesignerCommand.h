#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"
#include "sleekpr/core/templates/TableElement.h"

#include <QList>
#include <QString>

namespace sleekpr::core {
struct TemplateDocument;
}

namespace sleekpr::app {

struct TemplateDesignerCommandResult
{
    QString targetId;
    bool changed = false;
    QString errorMessage;
};

class ElementPropertiesCommand final
{
public:
    explicit ElementPropertiesCommand(DesignerElementPropertyModel model);

    TemplateDesignerCommandResult apply(sleekpr::core::TemplateElement& element) const;

private:
    DesignerElementPropertyModel m_model;
};

class TablePropertiesCommand final
{
public:
    explicit TablePropertiesCommand(DesignerTablePropertyModel model);

    TemplateDesignerCommandResult apply(sleekpr::core::TemplateDocument& document, const QString& tableId) const;

    static QString formatColumns(const QList<sleekpr::core::TableColumn>& columns);
    static QList<sleekpr::core::TableColumn> parseColumns(const QString& text);

private:
    DesignerTablePropertyModel m_model;
};

} // 命名空间 sleekpr::app
