#pragma once

#include <QList>
#include <QString>

namespace sleekpr::core {

enum class TableCellAlignment
{
    Left,
    Center,
    Right,
};

struct TableColumn
{
    // 列 id 是表格内部稳定标识，后续列宽调整和单元格命令 key 都依赖它。
    QString id;
    QString title;
    QString fieldKey;
    double widthMm = 20.0;
    TableCellAlignment alignment = TableCellAlignment::Left;
    double fontSizePt = 8.0;
    bool bold = false;
    bool ellipsis = false;
};

struct TableElement
{
    // 表格作为一级模板元素保存，不能拆成多个普通文本框模拟。
    QString id;
    QString layerId;
    int zIndex = 0;
    bool visible = true;
    bool locked = false;
    QString displayName;
    double x = 0.0;
    double y = 0.0;
    double width = 80.0;
    double height = 30.0;

    // dataPath 指向 TemplateRenderContext 中的明细数组，例如 items。
    QString dataPath;
    double headerRowHeightMm = 6.0;
    double detailRowHeightMm = 5.0;
    bool drawBorders = true;
    bool repeatHeaderOnPage = true;
    QList<TableColumn> columns;
};

} // 命名空间 sleekpr::core
