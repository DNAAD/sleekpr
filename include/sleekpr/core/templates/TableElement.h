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

enum class TableColumnWidthMode
{
    Fixed,
    Flex,
};

enum class TableVerticalAlignment
{
    Top,
    Middle,
    Bottom,
};

enum class TableRowBandKind
{
    Header,
    Detail,
    Summary,
    Subtotal,
    GroupHeader,
    GroupFooter,
    Footer,
};

enum class TableRowHeightMode
{
    Fixed,
    Auto,
};

enum class TableRowPrintScope
{
    FirstPage,
    EveryPage,
    LastPage,
};

enum class TableCellOverflowPolicy
{
    Clip,
    Ellipsis,
    Wrap,
    Shrink,
};

enum class TableTableOverflowPolicy
{
    Error,
    Clip,
    Continue,
};

struct TableColumn
{
    // 列 id 是表格内部稳定标识，后续列宽调整和单元格命令 key 都依赖它。
    QString id;
    QString title;
    QString fieldKey;
    double widthMm = 20.0;
    // fixed 使用 widthMm；flex 按 flexWeight 瓜分表格剩余宽度，兼容旧模板默认固定列。
    TableColumnWidthMode widthMode = TableColumnWidthMode::Fixed;
    double flexWeight = 1.0;
    TableCellAlignment alignment = TableCellAlignment::Left;
    double fontSizePt = 8.0;
    bool bold = false;
    bool ellipsis = false;
    // 换行和最小列宽给复杂表格布局使用；旧模板不写这些字段时继续沿用旧渲染语义。
    bool wrapText = false;
    double minWidthMm = 1.0;
    QString defaultCellStyleId;
};

struct TableCellStyle
{
    QString id;
    double fontSizePt = 8.0;
    bool bold = false;
    TableCellAlignment alignment = TableCellAlignment::Left;
    TableVerticalAlignment verticalAlignment = TableVerticalAlignment::Top;
    double paddingLeftMm = 0.6;
    double paddingTopMm = 0.4;
    double paddingRightMm = 0.6;
    double paddingBottomMm = 0.4;
    bool drawBorder = true;
    double borderWidthMm = 0.1;
    QString backgroundColor;
    QString textColor;
    bool wrapText = false;
    bool ellipsis = false;
};

struct TableRowBand
{
    QString id;
    TableRowBandKind kind = TableRowBandKind::Detail;
    QString title;
    QString dataPath;
    TableRowHeightMode heightMode = TableRowHeightMode::Fixed;
    double heightMm = 5.0;
    double minHeightMm = 4.0;
    bool repeatOnPage = false;
    TableRowPrintScope printOn = TableRowPrintScope::EveryPage;
};

struct TableCellTemplate
{
    QString id;
    QString rowBandId;
    QString columnId;
    QString textTemplate;
    QString fieldKey;
    QString styleId;
    TableCellOverflowPolicy overflowPolicy = TableCellOverflowPolicy::Clip;
    int maxLines = 1;
    int colSpan = 1;
    int rowSpan = 1;
    bool visible = true;
};

struct TableMergeRegion
{
    QString id;
    QString rowBandId;
    int startRowOffset = 0;
    QString startColumnId;
    int rowSpan = 1;
    int colSpan = 1;
};

struct TablePaginationPolicy
{
    bool repeatHeaderOnPage = true;
    bool keepGroupTogether = false;
    bool allowRowSplit = false;
    int maxPages = 100;
    TableTableOverflowPolicy overflowPolicy = TableTableOverflowPolicy::Error;
    int orphanDetailRows = 1;
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
    // 复杂表格模型字段全部可选；为空时从旧 columns 模型推导表头和明细行。
    QList<TableCellStyle> cellStyles;
    QList<TableRowBand> rowBands;
    QList<TableCellTemplate> cellTemplates;
    QList<TableMergeRegion> mergeRegions;
    TablePaginationPolicy pagination;
};

} // 命名空间 sleekpr::core
