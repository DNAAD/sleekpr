#pragma once

#include "sleekpr/core/templates/TableElement.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QList>
#include <QRectF>
#include <QString>

namespace sleekpr::core {

struct TableLayoutCell
{
    QString elementKey;
    QString rowBandId;
    QString columnId;
    int sourceRowIndex = -1;
    QRectF rect;
    QString text;
    TableCellStyle style;
    TableCellOverflowPolicy overflowPolicy = TableCellOverflowPolicy::Clip;
    int maxLines = 1;
    bool coveredByMerge = false;
};

struct TableLayoutPage
{
    int pageNumber = 1;
    int firstRowIndex = 0;
    int rowCount = 0;
    QList<TableLayoutCell> cells;
};

enum class TableLayoutDiagnosticCode
{
    OrphanRowsMoved,
    GroupMovedToNextPage,
    MergeRegionCrossesPage,
};

struct TableLayoutDiagnostic
{
    TableLayoutDiagnosticCode code = TableLayoutDiagnosticCode::OrphanRowsMoved;
    QString message;
    int pageNumber = 1;
    int sourceRowIndex = -1;
    QString mergeId;
    QString rowBandId;
};

struct TableLayoutResult
{
    QList<TableLayoutPage> pages;
    // 诊断不一定代表渲染失败；设计器用它展示分页调整、跨页风险等提示。
    QList<TableLayoutDiagnostic> diagnostics;
    QString errorMessage;

    bool success() const;
};

class TableElementLayout
{
public:
    // 布局层只输出毫米坐标和语义单元格，不直接生成绘图命令，方便后续复用到预览、打印和设计器命中。
    static TableLayoutResult layout(const TableElement& table, const TemplateRenderContext& context);
};

} // namespace sleekpr::core
