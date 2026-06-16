#pragma once

#include "sleekpr/core/native/NativeDrawCommand.h"
#include "sleekpr/core/templates/TableElement.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

struct TableRenderPage
{
    // pageNumber 使用从 1 开始的页码，便于打印日志、预览分页和用户提示直接使用。
    int pageNumber = 1;
    int firstRowIndex = 0;
    int rowCount = 0;
    QList<NativeDrawCommand> commands;
};

struct TableRenderResult
{
    QList<NativeDrawCommand> commands;
    QList<TableRenderPage> pages;
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
    static TableRenderResult renderPages(const TableElement& table,
                                         const TemplateRenderContext& context,
                                         double offsetX = 0.0,
                                         double offsetY = 0.0);
};

} // 命名空间 sleekpr::core
