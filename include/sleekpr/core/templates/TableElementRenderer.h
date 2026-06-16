#pragma once

#include "sleekpr/core/native/NativeDrawCommand.h"
#include "sleekpr/core/templates/TableElement.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

struct TableRenderResult
{
    QList<NativeDrawCommand> commands;
    QString errorMessage;

    bool success() const;
};

class TableElementRenderer
{
public:
    static TableRenderResult renderSinglePage(const TableElement& table,
                                              const TemplateRenderContext& context,
                                              double offsetX = 0.0,
                                              double offsetY = 0.0);
};

} // 命名空间 sleekpr::core
