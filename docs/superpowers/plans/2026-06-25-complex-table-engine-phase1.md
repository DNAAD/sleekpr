# Complex Table Engine Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the core complex-table model, JSON compatibility, layout result model, and native rendering foundation before adding advanced designer UI.

**Architecture:** Phase 1 stays inside `core/templates` plus focused core tests. Existing `TableElement` remains the persisted top-level table object; new row bands, cell templates, styles, merge regions, and pagination policy are optional fields with defaults derived from old `columns` templates.

**Tech Stack:** C++20, Qt Core JSON APIs, existing `NativeDrawCommand`, existing CMake/Ninja/MSVC test targets.

---

## Scope

Phase 1 implements:

- Extended table data structures in `include/sleekpr/core/templates/TableElement.h`.
- JSON round-trip for new optional fields in `TableElementJson`.
- Legacy fallback that derives header/detail rows from old `columns`.
- `TableElementLayout` core helper that outputs normalized pages and cell rectangles.
- Renderer support for `wrapText`, merged cell rectangles, summary/footer rows, and explicit overflow errors.

Phase 1 does not implement:

- Right-side complex table UI.
- Canvas column-width dragging.
- HTTP 422 routing changes.
- Actual Qt font metric measurement for precise text height. Phase 1 uses deterministic estimate logic in core; Qt measurement can replace the estimator later without changing the layout API.

## File Structure

- Modify: `include/sleekpr/core/templates/TableElement.h`
  - Owns serializable table model structs and enums.
- Modify: `include/sleekpr/core/native/NativeDrawCommand.h`
  - Adds optional cell fill/text color fields used by native renderers later.
- Modify: `include/sleekpr/core/templates/TableElementJson.h`
  - Keeps the public JSON API unchanged.
- Modify: `src/core/templates/TableElementJson.cpp`
  - Serializes/deserializes/validates new optional fields.
- Create: `include/sleekpr/core/templates/TableElementLayout.h`
  - Public layout result structs and `TableElementLayout` API.
- Create: `src/core/templates/TableElementLayout.cpp`
  - Core-only table layout, pagination, merge validation, and text-height estimate logic.
- Modify: `include/sleekpr/core/templates/TableElementRenderer.h`
  - Keeps old renderer API and may expose layout result through implementation only.
- Modify: `src/core/templates/TableElementRenderer.cpp`
  - Uses `TableElementLayout` to create `NativeDrawCommand` objects.
- Modify: `CMakeLists.txt`
  - Adds `TableElementLayout.cpp/.h` to main target.
- Modify: `tests/CMakeLists.txt`
  - Adds `TableElementLayout.cpp/.h` to core test target if tests compile sources directly.
- Modify: `tests/tst_core.cpp`
  - Adds all Phase 1 regression tests.

---

## Task 1: Extended Table Model and JSON Round Trip

**Files:**
- Modify: `include/sleekpr/core/templates/TableElement.h`
- Modify: `src/core/templates/TableElementJson.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write the failing JSON round-trip test**

Add declarations near the existing table tests in `CoreTests`:

```cpp
void tableElementJsonPersistsComplexTableModel();
void tableElementJsonKeepsLegacyColumnsCompatible();
```

Add test bodies after `tableElementJsonPersistsColumnsAndLayout()`:

```cpp
void CoreTests::tableElementJsonPersistsComplexTableModel()
{
    TableElement table;
    table.id = QStringLiteral("complex-table");
    table.layerId = QStringLiteral("main");
    table.displayName = QString::fromUtf8("复杂表格");
    table.dataPath = QStringLiteral("items");

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 24.0;
    nameColumn.wrapText = true;
    nameColumn.minWidthMm = 12.0;
    nameColumn.defaultCellStyleId = QStringLiteral("body");
    table.columns.append(nameColumn);

    TableColumn amountColumn;
    amountColumn.id = QStringLiteral("amount");
    amountColumn.title = QString::fromUtf8("金额");
    amountColumn.fieldKey = QStringLiteral("amount");
    amountColumn.alignment = TableCellAlignment::Right;
    table.columns.append(amountColumn);

    TableCellStyle bodyStyle;
    bodyStyle.id = QStringLiteral("body");
    bodyStyle.fontSizePt = 7.5;
    bodyStyle.wrapText = true;
    bodyStyle.paddingLeftMm = 0.8;
    bodyStyle.paddingRightMm = 0.8;
    bodyStyle.backgroundColor = QStringLiteral("#FFFFFF");
    bodyStyle.textColor = QStringLiteral("#111111");
    table.cellStyles.append(bodyStyle);

    TableRowBand headerBand;
    headerBand.id = QStringLiteral("header");
    headerBand.kind = TableRowBandKind::Header;
    headerBand.heightMode = TableRowHeightMode::Fixed;
    headerBand.heightMm = 5.0;
    headerBand.repeatOnPage = true;
    table.rowBands.append(headerBand);

    TableRowBand detailBand;
    detailBand.id = QStringLiteral("detail");
    detailBand.kind = TableRowBandKind::Detail;
    detailBand.heightMode = TableRowHeightMode::Auto;
    detailBand.minHeightMm = 4.0;
    table.rowBands.append(detailBand);

    TableCellTemplate detailName;
    detailName.id = QStringLiteral("detail-name");
    detailName.rowBandId = QStringLiteral("detail");
    detailName.columnId = QStringLiteral("name");
    detailName.textTemplate = QStringLiteral("${productName}\n${spec}");
    detailName.styleId = QStringLiteral("body");
    detailName.overflowPolicy = TableCellOverflowPolicy::Wrap;
    detailName.maxLines = 3;
    table.cellTemplates.append(detailName);

    TableMergeRegion merge;
    merge.id = QStringLiteral("header-title");
    merge.rowBandId = QStringLiteral("header");
    merge.startRowOffset = 0;
    merge.startColumnId = QStringLiteral("name");
    merge.rowSpan = 1;
    merge.colSpan = 2;
    table.mergeRegions.append(merge);

    table.pagination.repeatHeaderOnPage = true;
    table.pagination.maxPages = 5;
    table.pagination.overflowPolicy = TableTableOverflowPolicy::Error;

    const auto json = TableElementJson::toJson(table);

    QVERIFY(json.contains(QStringLiteral("rowBands")));
    QVERIFY(json.contains(QStringLiteral("cellTemplates")));
    QVERIFY(json.contains(QStringLiteral("cellStyles")));
    QVERIFY(json.contains(QStringLiteral("mergeRegions")));
    QVERIFY(json.contains(QStringLiteral("pagination")));

    QString errorMessage;
    QVERIFY2(TableElementJson::validate(json, QStringLiteral("main"), &errorMessage), qPrintable(errorMessage));

    const auto actual = TableElementJson::fromJson(json, QStringLiteral("main"));
    QCOMPARE(actual.columns.first().wrapText, true);
    QCOMPARE(actual.columns.first().minWidthMm, 12.0);
    QCOMPARE(actual.columns.first().defaultCellStyleId, QStringLiteral("body"));
    QCOMPARE(actual.cellStyles.size(), 1);
    QCOMPARE(actual.cellStyles.first().textColor, QStringLiteral("#111111"));
    QCOMPARE(actual.rowBands.size(), 2);
    QCOMPARE(actual.rowBands[1].heightMode, TableRowHeightMode::Auto);
    QCOMPARE(actual.cellTemplates.size(), 1);
    QCOMPARE(actual.cellTemplates.first().overflowPolicy, TableCellOverflowPolicy::Wrap);
    QCOMPARE(actual.mergeRegions.size(), 1);
    QCOMPARE(actual.pagination.maxPages, 5);
}

void CoreTests::tableElementJsonKeepsLegacyColumnsCompatible()
{
    TableElement table;
    table.id = QStringLiteral("legacy-table");
    table.layerId = QStringLiteral("main");
    table.dataPath = QStringLiteral("items");

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    table.columns.append(column);

    const auto json = TableElementJson::toJson(table);
    QVERIFY(json.contains(QStringLiteral("columns")));
    QVERIFY(!json.contains(QStringLiteral("rowBands")));
    QVERIFY(!json.contains(QStringLiteral("cellTemplates")));

    const auto actual = TableElementJson::fromJson(json, QStringLiteral("main"));
    QCOMPARE(actual.columns.size(), 1);
    QVERIFY(actual.rowBands.isEmpty());
    QVERIFY(actual.cellTemplates.isEmpty());
}
```

- [ ] **Step 2: Run RED**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_core_tests.exe tableElementJsonPersistsComplexTableModel tableElementJsonKeepsLegacyColumnsCompatible'
```

Expected: build fails because `TableCellStyle`, `TableRowBand`, `TableCellTemplate`, `TableMergeRegion`, `TablePaginationPolicy`, and related enums do not exist.

- [ ] **Step 3: Extend `TableElement.h`**

Add enums before `struct TableColumn`:

```cpp
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
```

Extend `TableColumn`:

```cpp
bool wrapText = false;
double minWidthMm = 1.0;
QString defaultCellStyleId;
```

Add structs after `TableColumn`:

```cpp
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
```

Extend `TableElement`:

```cpp
QList<TableCellStyle> cellStyles;
QList<TableRowBand> rowBands;
QList<TableCellTemplate> cellTemplates;
QList<TableMergeRegion> mergeRegions;
TablePaginationPolicy pagination;
```

All new comments must be Chinese.

- [ ] **Step 4: Implement JSON helpers**

In `TableElementJson.cpp`, add string conversion helpers for:

```cpp
TableVerticalAlignment
TableRowBandKind
TableRowHeightMode
TableRowPrintScope
TableCellOverflowPolicy
TableTableOverflowPolicy
```

Add `toJson/fromJson` helpers:

```cpp
QJsonObject cellStyleToJson(const TableCellStyle& style);
TableCellStyle cellStyleFromJson(const QJsonObject& json);
QJsonObject rowBandToJson(const TableRowBand& rowBand);
TableRowBand rowBandFromJson(const QJsonObject& json);
QJsonObject cellTemplateToJson(const TableCellTemplate& cell);
TableCellTemplate cellTemplateFromJson(const QJsonObject& json);
QJsonObject mergeRegionToJson(const TableMergeRegion& merge);
TableMergeRegion mergeRegionFromJson(const QJsonObject& json);
QJsonObject paginationToJson(const TablePaginationPolicy& pagination);
TablePaginationPolicy paginationFromJson(const QJsonObject& json, bool legacyRepeatHeaderOnPage);
```

In `columnToJson`, write the new fields only when meaningful:

```cpp
if (column.wrapText) {
    json["wrapText"] = true;
}
if (column.minWidthMm != 1.0) {
    json["minWidthMm"] = column.minWidthMm;
}
if (!column.defaultCellStyleId.trimmed().isEmpty()) {
    json["defaultCellStyleId"] = column.defaultCellStyleId;
}
```

In `columnFromJson`, read:

```cpp
column.wrapText = json["wrapText"].toBool(column.wrapText);
column.minWidthMm = json["minWidthMm"].toDouble(column.minWidthMm);
column.defaultCellStyleId = json["defaultCellStyleId"].toString();
```

In `TableElementJson::toJson`, write optional arrays only when non-empty:

```cpp
if (!table.cellStyles.isEmpty()) {
    json["cellStyles"] = cellStylesToJson(table.cellStyles);
}
if (!table.rowBands.isEmpty()) {
    json["rowBands"] = rowBandsToJson(table.rowBands);
}
if (!table.cellTemplates.isEmpty()) {
    json["cellTemplates"] = cellTemplatesToJson(table.cellTemplates);
}
if (!table.mergeRegions.isEmpty()) {
    json["mergeRegions"] = mergeRegionsToJson(table.mergeRegions);
}
if (hasNonDefaultPagination(table.pagination, table.repeatHeaderOnPage)) {
    json["pagination"] = paginationToJson(table.pagination);
}
```

In `fromJson`, read optional fields and keep legacy fallback empty when absent:

```cpp
table.cellStyles = cellStylesFromJson(json["cellStyles"].toArray());
table.rowBands = rowBandsFromJson(json["rowBands"].toArray());
table.cellTemplates = cellTemplatesFromJson(json["cellTemplates"].toArray());
table.mergeRegions = mergeRegionsFromJson(json["mergeRegions"].toArray());
table.pagination = paginationFromJson(json["pagination"].toObject(), table.repeatHeaderOnPage);
```

- [ ] **Step 5: Add validation for complex fields**

Extend `validate()`:

```cpp
QSet<QString> styleIds;
for (const auto& value : json["cellStyles"].toArray()) {
    if (!value.isObject()) {
        setError(errorMessage, QString::fromUtf8("表格样式必须是对象：%1").arg(tableId));
        return false;
    }
    const auto style = value.toObject();
    const auto styleId = style["id"].toString();
    if (isBlankOrPadded(styleId) || styleIds.contains(styleId)) {
        setError(errorMessage, QString::fromUtf8("表格样式 id 为空或重复：%1").arg(styleId));
        return false;
    }
    styleIds.insert(styleId);
}

QSet<QString> rowBandIds;
for (const auto& value : json["rowBands"].toArray()) {
    if (!value.isObject()) {
        setError(errorMessage, QString::fromUtf8("表格行带必须是对象：%1").arg(tableId));
        return false;
    }
    const auto rowBand = value.toObject();
    const auto rowBandId = rowBand["id"].toString();
    if (isBlankOrPadded(rowBandId) || rowBandIds.contains(rowBandId)) {
        setError(errorMessage, QString::fromUtf8("表格行带 id 为空或重复：%1").arg(rowBandId));
        return false;
    }
    if (rowBand["heightMm"].toDouble(5.0) <= 0.0 || rowBand["minHeightMm"].toDouble(4.0) <= 0.0) {
        setError(errorMessage, QString::fromUtf8("表格行带高度必须大于 0：%1").arg(rowBandId));
        return false;
    }
    rowBandIds.insert(rowBandId);
}

for (const auto& value : json["cellTemplates"].toArray()) {
    if (!value.isObject()) {
        setError(errorMessage, QString::fromUtf8("表格单元格模板必须是对象：%1").arg(tableId));
        return false;
    }
    const auto cell = value.toObject();
    const auto rowBandId = cell["rowBandId"].toString();
    const auto columnId = cell["columnId"].toString();
    const auto styleId = cell["styleId"].toString();
    if (!rowBandIds.isEmpty() && !rowBandIds.contains(rowBandId)) {
        setError(errorMessage, QString::fromUtf8("单元格模板引用了不存在的行带：%1").arg(rowBandId));
        return false;
    }
    if (!columnIds.contains(columnId)) {
        setError(errorMessage, QString::fromUtf8("单元格模板引用了不存在的列：%1").arg(columnId));
        return false;
    }
    if (!styleId.isEmpty() && !styleIds.contains(styleId)) {
        setError(errorMessage, QString::fromUtf8("单元格模板引用了不存在的样式：%1").arg(styleId));
        return false;
    }
    if (cell["colSpan"].toInt(1) <= 0 || cell["rowSpan"].toInt(1) <= 0 || cell["maxLines"].toInt(1) <= 0) {
        setError(errorMessage, QString::fromUtf8("单元格模板跨度和最大行数必须大于 0：%1").arg(cell["id"].toString()));
        return false;
    }
}

for (const auto& value : json["mergeRegions"].toArray()) {
    if (!value.isObject()) {
        setError(errorMessage, QString::fromUtf8("表格合并区域必须是对象：%1").arg(tableId));
        return false;
    }
    const auto merge = value.toObject();
    const auto rowBandId = merge["rowBandId"].toString();
    const auto startColumnId = merge["startColumnId"].toString();
    if (!rowBandIds.isEmpty() && !rowBandIds.contains(rowBandId)) {
        setError(errorMessage, QString::fromUtf8("合并区域引用了不存在的行带：%1").arg(rowBandId));
        return false;
    }
    if (!columnIds.contains(startColumnId)) {
        setError(errorMessage, QString::fromUtf8("合并区域引用了不存在的列：%1").arg(startColumnId));
        return false;
    }
    if (merge["rowSpan"].toInt(1) <= 0 || merge["colSpan"].toInt(1) <= 0) {
        setError(errorMessage, QString::fromUtf8("合并区域跨度必须大于 0：%1").arg(merge["id"].toString()));
        return false;
    }
}
```

Keep old templates valid when arrays are absent.

- [ ] **Step 6: Run GREEN**

Run the same command from Step 2.

Expected: both new tests pass, existing core tests still compile.

- [ ] **Step 7: Commit**

```powershell
git add include/sleekpr/core/templates/TableElement.h src/core/templates/TableElementJson.cpp tests/tst_core.cpp
git commit -m "扩展复杂表格核心模型"
```

---

## Task 2: Core Table Layout Result and Legacy Normalization

**Files:**
- Create: `include/sleekpr/core/templates/TableElementLayout.h`
- Create: `src/core/templates/TableElementLayout.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing layout normalization test**

Add declarations:

```cpp
void tableElementLayoutDerivesLegacyHeaderAndDetailCells();
void tableElementLayoutRejectsInvalidMergeRegion();
```

Add tests:

```cpp
void CoreTests::tableElementLayoutDerivesLegacyHeaderAndDetailCells()
{
    TableElement table;
    table.id = QStringLiteral("legacy-table");
    table.dataPath = QStringLiteral("items");
    table.width = 50.0;
    table.height = 20.0;
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 25.0;
    table.columns.append(nameColumn);

    TableColumn amountColumn;
    amountColumn.id = QStringLiteral("amount");
    amountColumn.title = QString::fromUtf8("金额");
    amountColumn.fieldKey = QStringLiteral("amount");
    amountColumn.widthMm = 25.0;
    table.columns.append(amountColumn);

    TemplateRenderContext context;
    context.values = QJsonObject{
        {QStringLiteral("items"), QJsonArray{
            QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("戒指")}, {QStringLiteral("amount"), 1280}},
        }},
    };

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));
    QCOMPARE(result.pages.size(), 1);
    QVERIFY(result.pages.first().cells.size() >= 4);
    QCOMPARE(result.pages.first().cells[0].text, QString::fromUtf8("品名"));
    QCOMPARE(result.pages.first().cells[2].text, QString::fromUtf8("戒指"));
    QCOMPARE(result.pages.first().cells[2].rect.x(), 0.0);
    QCOMPARE(result.pages.first().cells[3].rect.x(), 25.0);
}

void CoreTests::tableElementLayoutRejectsInvalidMergeRegion()
{
    TableElement table;
    table.id = QStringLiteral("merge-table");
    table.dataPath = QStringLiteral("items");

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    table.columns.append(column);

    TableMergeRegion merge;
    merge.id = QStringLiteral("bad-merge");
    merge.rowBandId = QStringLiteral("detail");
    merge.startColumnId = QStringLiteral("name");
    merge.colSpan = 2;
    table.mergeRegions.append(merge);

    TemplateRenderContext context;
    context.values = QJsonObject{{QStringLiteral("items"), QJsonArray{QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("戒指")}}}}};

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(result.errorMessage.contains(QString::fromUtf8("合并")));
}
```

- [ ] **Step 2: Run RED**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_core_tests.exe tableElementLayoutDerivesLegacyHeaderAndDetailCells tableElementLayoutRejectsInvalidMergeRegion'
```

Expected: build fails because `TableElementLayout` does not exist.

- [ ] **Step 3: Add layout header**

Create `include/sleekpr/core/templates/TableElementLayout.h`:

```cpp
#pragma once

#include "sleekpr/core/templates/TableElement.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QList>
#include <QRectF>
#include <QString>

namespace sleekpr::core {

struct TableLayoutCell
{
    QString tableId;
    QString rowBandId;
    QString columnId;
    QString cellId;
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

struct TableLayoutResult
{
    QList<TableLayoutPage> pages;
    QString errorMessage;

    bool success() const;
};

class TableElementLayout
{
public:
    static TableLayoutResult layout(const TableElement& table, const TemplateRenderContext& context);
};

} // namespace sleekpr::core
```

- [ ] **Step 4: Add minimal layout implementation**

Create `src/core/templates/TableElementLayout.cpp`:

```cpp
#include "sleekpr/core/templates/TableElementLayout.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace sleekpr::core {
namespace {

QString valueToText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

QJsonValue valueAtPath(const QJsonObject& object, const QString& path)
{
    const auto normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return QJsonValue();
    }
    if (object.contains(normalizedPath)) {
        return object.value(normalizedPath);
    }

    QJsonValue current = object;
    for (const auto& part : normalizedPath.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        if (!current.isObject()) {
            return QJsonValue();
        }
        const auto currentObject = current.toObject();
        if (!currentObject.contains(part)) {
            return QJsonValue();
        }
        current = currentObject.value(part);
    }
    return current;
}

QList<double> resolvedColumnWidths(const TableElement& table)
{
    QList<double> widths;
    widths.reserve(table.columns.size());

    double fixedTotal = 0.0;
    double flexWeightTotal = 0.0;
    for (const auto& column : table.columns) {
        if (column.widthMode == TableColumnWidthMode::Flex) {
            flexWeightTotal += column.flexWeight > 0.0 ? column.flexWeight : 1.0;
        } else {
            fixedTotal += std::max(column.minWidthMm, column.widthMm);
        }
    }

    const auto remainingWidth = std::max(0.0, table.width - fixedTotal);
    for (const auto& column : table.columns) {
        if (column.widthMode == TableColumnWidthMode::Flex && flexWeightTotal > 0.0) {
            const auto weight = column.flexWeight > 0.0 ? column.flexWeight : 1.0;
            widths.append(std::max(column.minWidthMm, remainingWidth * weight / flexWeightTotal));
        } else {
            widths.append(std::max(column.minWidthMm, column.widthMm));
        }
    }
    return widths;
}

bool mergeRegionFits(const TableElement& table, const TableMergeRegion& merge)
{
    auto columnIndex = -1;
    for (int index = 0; index < table.columns.size(); ++index) {
        if (table.columns[index].id == merge.startColumnId) {
            columnIndex = index;
            break;
        }
    }
    return columnIndex >= 0 && merge.colSpan > 0 && columnIndex + merge.colSpan <= table.columns.size();
}

TableCellStyle styleForColumn(const TableColumn& column)
{
    TableCellStyle style;
    style.id = column.defaultCellStyleId;
    style.fontSizePt = column.fontSizePt;
    style.bold = column.bold;
    style.alignment = column.alignment;
    style.wrapText = column.wrapText;
    style.ellipsis = column.ellipsis;
    return style;
}

} // namespace

bool TableLayoutResult::success() const
{
    return errorMessage.isEmpty();
}

TableLayoutResult TableElementLayout::layout(const TableElement& table, const TemplateRenderContext& context)
{
    TableLayoutResult result;
    if (table.columns.isEmpty()) {
        result.errorMessage = QString::fromUtf8("表格至少需要一列");
        return result;
    }
    for (const auto& merge : table.mergeRegions) {
        if (!mergeRegionFits(table, merge)) {
            result.errorMessage = QString::fromUtf8("表格 %1 的合并区域 %2 越界").arg(table.id, merge.id);
            return result;
        }
    }

    const auto rowsValue = valueAtPath(context.values, table.dataPath);
    if (!rowsValue.isArray()) {
        result.errorMessage = QString::fromUtf8("表格数据 %1 必须是数组").arg(table.dataPath);
        return result;
    }

    TableLayoutPage page;
    page.pageNumber = 1;
    const auto columnWidths = resolvedColumnWidths(table);

    double x = 0.0;
    for (int columnIndex = 0; columnIndex < table.columns.size(); ++columnIndex) {
        const auto& column = table.columns[columnIndex];
        TableLayoutCell cell;
        cell.tableId = table.id;
        cell.rowBandId = QStringLiteral("header");
        cell.columnId = column.id;
        cell.cellId = QStringLiteral("%1.header.%2").arg(table.id, column.id);
        cell.rect = QRectF(x, 0.0, columnWidths[columnIndex], table.headerRowHeightMm);
        cell.text = column.title;
        cell.style = styleForColumn(column);
        cell.overflowPolicy = column.wrapText ? TableCellOverflowPolicy::Wrap : TableCellOverflowPolicy::Clip;
        page.cells.append(cell);
        x += columnWidths[columnIndex];
    }

    const auto rows = rowsValue.toArray();
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        if (!rows[rowIndex].isObject()) {
            result.errorMessage = QString::fromUtf8("表格数据 %1 的第 %2 行必须是对象").arg(table.dataPath).arg(rowIndex + 1);
            return result;
        }
        const auto row = rows[rowIndex].toObject();
        x = 0.0;
        for (int columnIndex = 0; columnIndex < table.columns.size(); ++columnIndex) {
            const auto& column = table.columns[columnIndex];
            TableLayoutCell cell;
            cell.tableId = table.id;
            cell.rowBandId = QStringLiteral("detail");
            cell.columnId = column.id;
            cell.cellId = QStringLiteral("%1.row%2.%3").arg(table.id).arg(rowIndex).arg(column.id);
            cell.sourceRowIndex = rowIndex;
            cell.rect = QRectF(x,
                table.headerRowHeightMm + rowIndex * table.detailRowHeightMm,
                columnWidths[columnIndex],
                table.detailRowHeightMm);
            cell.text = valueToText(valueAtPath(row, column.fieldKey));
            cell.style = styleForColumn(column);
            cell.overflowPolicy = column.wrapText ? TableCellOverflowPolicy::Wrap : TableCellOverflowPolicy::Clip;
            page.cells.append(cell);
            x += columnWidths[columnIndex];
        }
    }
    page.rowCount = rows.size();
    result.pages.append(page);
    return result;
}

} // namespace sleekpr::core
```

- [ ] **Step 5: Add files to CMake**

Add to the main source list in `CMakeLists.txt`:

```cmake
src/core/templates/TableElementLayout.cpp
include/sleekpr/core/templates/TableElementLayout.h
```

Add the same files to `tests/CMakeLists.txt` if that test target lists core sources explicitly.

- [ ] **Step 6: Run GREEN**

Run the same command from Step 2.

Expected: both layout tests pass.

- [ ] **Step 7: Commit**

```powershell
git add include/sleekpr/core/templates/TableElementLayout.h src/core/templates/TableElementLayout.cpp CMakeLists.txt tests/CMakeLists.txt tests/tst_core.cpp
git commit -m "新增复杂表格布局基础"
```

---

## Task 3: Auto Row Height and Wrap Layout

**Files:**
- Modify: `src/core/templates/TableElementLayout.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing auto-height test**

Add declaration:

```cpp
void tableElementLayoutCalculatesAutoRowHeightForWrappedCells();
```

Add test:

```cpp
void CoreTests::tableElementLayoutCalculatesAutoRowHeightForWrappedCells()
{
    TableElement table;
    table.id = QStringLiteral("auto-height");
    table.dataPath = QStringLiteral("items");
    table.width = 20.0;
    table.height = 40.0;
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 4.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 20.0;
    column.wrapText = true;
    table.columns.append(column);

    TableRowBand detailBand;
    detailBand.id = QStringLiteral("detail");
    detailBand.kind = TableRowBandKind::Detail;
    detailBand.heightMode = TableRowHeightMode::Auto;
    detailBand.minHeightMm = 4.0;
    table.rowBands.append(detailBand);

    TemplateRenderContext context;
    context.values = QJsonObject{{QStringLiteral("items"),
        QJsonArray{QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("很长很长的商品名称需要自动换行显示")}}}}};

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));

    const auto detailCell = std::find_if(result.pages.first().cells.cbegin(), result.pages.first().cells.cend(), [](const TableLayoutCell& cell) {
        return cell.rowBandId == QStringLiteral("detail");
    });
    QVERIFY(detailCell != result.pages.first().cells.cend());
    QVERIFY(detailCell->rect.height() > table.detailRowHeightMm);
    QCOMPARE(detailCell->overflowPolicy, TableCellOverflowPolicy::Wrap);
}
```

- [ ] **Step 2: Run RED**

Run targeted test. Expected: test fails because row height stays fixed.

- [ ] **Step 3: Implement deterministic text height estimate**

In `TableElementLayout.cpp`, add:

```cpp
double estimatedLineHeightMm(double fontSizePt)
{
    return std::max(1.0, fontSizePt * 0.352778 * 1.25);
}

int estimatedLineCount(const QString& text, double widthMm, const TableCellStyle& style, int maxLines)
{
    const auto contentWidth = std::max(1.0, widthMm - style.paddingLeftMm - style.paddingRightMm);
    const auto averageCharWidth = std::max(0.6, style.fontSizePt * 0.352778 * 0.55);
    const auto charsPerLine = std::max(1, static_cast<int>(std::floor(contentWidth / averageCharWidth)));
    auto explicitLines = text.split(QLatin1Char('\n')).size();
    auto estimated = 0;
    for (const auto& line : text.split(QLatin1Char('\n'))) {
        estimated += std::max(1, static_cast<int>(std::ceil(static_cast<double>(line.size()) / charsPerLine)));
    }
    estimated = std::max(estimated, explicitLines);
    if (maxLines > 0) {
        estimated = std::min(estimated, maxLines);
    }
    return estimated;
}

double estimatedCellHeightMm(const QString& text, double widthMm, const TableCellStyle& style, int maxLines)
{
    return style.paddingTopMm
        + style.paddingBottomMm
        + estimatedLineCount(text, widthMm, style, maxLines) * estimatedLineHeightMm(style.fontSizePt);
}
```

When a `detail` row band uses `heightMode == TableRowHeightMode::Auto`, calculate all detail cells first, find the maximum estimated height, and assign that height to every cell in the same row.

- [ ] **Step 4: Run GREEN**

Run the targeted test.

Expected: test passes.

- [ ] **Step 5: Commit**

```powershell
git add src/core/templates/TableElementLayout.cpp tests/tst_core.cpp
git commit -m "支持复杂表格自动行高"
```

---

## Task 4: Merge Regions and Cell Templates

**Files:**
- Modify: `src/core/templates/TableElementLayout.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing merge/template test**

Add declaration:

```cpp
void tableElementLayoutAppliesCellTemplatesAndMergeRegions();
```

Add test:

```cpp
void CoreTests::tableElementLayoutAppliesCellTemplatesAndMergeRegions()
{
    TableElement table;
    table.id = QStringLiteral("merge-layout");
    table.dataPath = QStringLiteral("items");
    table.width = 60.0;
    table.height = 30.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 30.0;
    table.columns.append(nameColumn);

    TableColumn specColumn;
    specColumn.id = QStringLiteral("spec");
    specColumn.title = QString::fromUtf8("规格");
    specColumn.fieldKey = QStringLiteral("spec");
    specColumn.widthMm = 30.0;
    table.columns.append(specColumn);

    TableCellTemplate mergedHeader;
    mergedHeader.id = QStringLiteral("merged-header");
    mergedHeader.rowBandId = QStringLiteral("header");
    mergedHeader.columnId = QStringLiteral("name");
    mergedHeader.textTemplate = QString::fromUtf8("商品信息");
    mergedHeader.colSpan = 2;
    table.cellTemplates.append(mergedHeader);

    TableMergeRegion merge;
    merge.id = QStringLiteral("header-merge");
    merge.rowBandId = QStringLiteral("header");
    merge.startColumnId = QStringLiteral("name");
    merge.colSpan = 2;
    table.mergeRegions.append(merge);

    TemplateRenderContext context;
    context.values = QJsonObject{{QStringLiteral("items"),
        QJsonArray{QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("戒指")}, {QStringLiteral("spec"), QStringLiteral("16#")}}}}};

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));

    const auto headerCell = std::find_if(result.pages.first().cells.cbegin(), result.pages.first().cells.cend(), [](const TableLayoutCell& cell) {
        return cell.rowBandId == QStringLiteral("header") && !cell.coveredByMerge;
    });
    QVERIFY(headerCell != result.pages.first().cells.cend());
    QCOMPARE(headerCell->text, QString::fromUtf8("商品信息"));
    QCOMPARE(headerCell->rect.width(), 60.0);
}
```

- [ ] **Step 2: Run RED**

Run targeted test. Expected: test fails because merge region width is not applied and template text is ignored.

- [ ] **Step 3: Implement cell-template lookup**

Add helper:

```cpp
const TableCellTemplate* findCellTemplate(const TableElement& table, const QString& rowBandId, const QString& columnId)
```

When building a header/detail cell:

- If a matching cell template exists and `visible == false`, skip the cell.
- If `textTemplate` is not empty, interpolate `${field}` using the current row object or context root.
- Else if `fieldKey` is not empty, use `fieldKey`.
- Else fall back to column title or column field key.

- [ ] **Step 4: Implement merge width and covered cells**

For a merge starting at `startColumnId`:

- Expand the start cell width by the sum of spanned columns.
- Mark subsequent same-row cells as `coveredByMerge=true`.
- Do not emit covered cells in renderer later.

Keep Phase 1 rowSpan limited to `1`. If `rowSpan > 1`, return:

```cpp
QString::fromUtf8("表格 %1 的合并区域 %2 暂不支持跨行合并").arg(table.id, merge.id)
```

- [ ] **Step 5: Run GREEN**

Run targeted test.

Expected: test passes.

- [ ] **Step 6: Commit**

```powershell
git add src/core/templates/TableElementLayout.cpp tests/tst_core.cpp
git commit -m "支持复杂表格单元格模板和合并"
```

---

## Task 5: Summary and Footer Bands

**Files:**
- Modify: `src/core/templates/TableElementLayout.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing summary/footer test**

Add declaration:

```cpp
void tableElementLayoutRendersSummaryAndFooterBands();
```

Add test:

```cpp
void CoreTests::tableElementLayoutRendersSummaryAndFooterBands()
{
    TableElement table;
    table.id = QStringLiteral("summary-table");
    table.dataPath = QStringLiteral("items");
    table.width = 40.0;
    table.height = 40.0;

    TableColumn labelColumn;
    labelColumn.id = QStringLiteral("label");
    labelColumn.title = QString::fromUtf8("项目");
    labelColumn.fieldKey = QStringLiteral("name");
    labelColumn.widthMm = 20.0;
    table.columns.append(labelColumn);

    TableColumn amountColumn;
    amountColumn.id = QStringLiteral("amount");
    amountColumn.title = QString::fromUtf8("金额");
    amountColumn.fieldKey = QStringLiteral("amount");
    amountColumn.widthMm = 20.0;
    table.columns.append(amountColumn);

    TableRowBand summaryBand;
    summaryBand.id = QStringLiteral("summary");
    summaryBand.kind = TableRowBandKind::Summary;
    summaryBand.heightMm = 5.0;
    table.rowBands.append(summaryBand);

    TableCellTemplate summaryLabel;
    summaryLabel.id = QStringLiteral("summary-label");
    summaryLabel.rowBandId = QStringLiteral("summary");
    summaryLabel.columnId = QStringLiteral("label");
    summaryLabel.textTemplate = QString::fromUtf8("合计");
    table.cellTemplates.append(summaryLabel);

    TableCellTemplate summaryAmount;
    summaryAmount.id = QStringLiteral("summary-amount");
    summaryAmount.rowBandId = QStringLiteral("summary");
    summaryAmount.columnId = QStringLiteral("amount");
    summaryAmount.textTemplate = QStringLiteral("${totalAmount}");
    table.cellTemplates.append(summaryAmount);

    TemplateRenderContext context;
    context.values = QJsonObject{
        {QStringLiteral("totalAmount"), QStringLiteral("1880.00")},
        {QStringLiteral("items"), QJsonArray{QJsonObject{{QStringLiteral("name"), QString::fromUtf8("戒指")}, {QStringLiteral("amount"), 1880}}}},
    };

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));

    const auto summaryCell = std::find_if(result.pages.first().cells.cbegin(), result.pages.first().cells.cend(), [](const TableLayoutCell& cell) {
        return cell.rowBandId == QStringLiteral("summary") && cell.columnId == QStringLiteral("amount");
    });
    QVERIFY(summaryCell != result.pages.first().cells.cend());
    QCOMPARE(summaryCell->text, QStringLiteral("1880.00"));
    QVERIFY(summaryCell->rect.y() > table.headerRowHeightMm);
}
```

- [ ] **Step 2: Run RED**

Run targeted test. Expected: summary band is ignored.

- [ ] **Step 3: Implement static row bands**

In layout, after detail rows, append row bands whose kind is:

```cpp
Summary
Subtotal
GroupFooter
Footer
```

For Phase 1:

- Render them once after all detail rows.
- Use context root values for `${field}` interpolation.
- Use `heightMm` for fixed height.
- Use `minHeightMm` plus auto estimate for auto height.

- [ ] **Step 4: Run GREEN**

Run targeted test. Expected: summary/footer cells are included.

- [ ] **Step 5: Commit**

```powershell
git add src/core/templates/TableElementLayout.cpp tests/tst_core.cpp
git commit -m "支持复杂表格汇总和页脚行"
```

---

## Task 6: Renderer Uses Layout Result

**Files:**
- Modify: `src/core/templates/TableElementRenderer.cpp`
- Modify: `include/sleekpr/core/native/NativeDrawCommand.h`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing renderer test**

Add declaration:

```cpp
void tableElementRendererUsesComplexLayoutCells();
```

Add test:

```cpp
void CoreTests::tableElementRendererUsesComplexLayoutCells()
{
    TableElement table;
    table.id = QStringLiteral("render-complex");
    table.dataPath = QStringLiteral("items");
    table.width = 40.0;
    table.height = 30.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 40.0;
    column.wrapText = true;
    table.columns.append(column);

    TableCellStyle style;
    style.id = QStringLiteral("highlight");
    style.fontSizePt = 9.0;
    style.bold = true;
    style.wrapText = true;
    style.backgroundColor = QStringLiteral("#FFFFCC");
    style.textColor = QStringLiteral("#222222");
    table.cellStyles.append(style);

    TableCellTemplate detail;
    detail.id = QStringLiteral("detail-name");
    detail.rowBandId = QStringLiteral("detail");
    detail.columnId = QStringLiteral("name");
    detail.textTemplate = QStringLiteral("${productName}\n${spec}");
    detail.styleId = QStringLiteral("highlight");
    detail.overflowPolicy = TableCellOverflowPolicy::Wrap;
    detail.maxLines = 2;
    table.cellTemplates.append(detail);

    TemplateRenderContext context;
    context.values = QJsonObject{{QStringLiteral("items"),
        QJsonArray{QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("戒指")}, {QStringLiteral("spec"), QStringLiteral("16#")}}}}};

    const auto result = TableElementRenderer::renderSinglePage(table, context);
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));

    auto textCommand = std::find_if(result.commands.cbegin(), result.commands.cend(), [](const NativeDrawCommand& command) {
        return command.elementKey == QStringLiteral("render-complex.row0.name");
    });
    QVERIFY(textCommand != result.commands.cend());
    QCOMPARE(textCommand->text, QString::fromUtf8("戒指\n16#"));
    QVERIFY(textCommand->wrapText);
    QCOMPARE(textCommand->maxLines, 2);
    QCOMPARE(textCommand->fontSizePt, 9.0);
    QVERIFY(textCommand->bold);
    QCOMPARE(textCommand->backgroundColor, QStringLiteral("#FFFFCC"));
    QCOMPARE(textCommand->textColor, QStringLiteral("#222222"));
}
```

- [ ] **Step 2: Run RED**

Run targeted test. Expected: fails because renderer ignores `TableElementLayout`, cell style colors, and cell template.

- [ ] **Step 3: Extend `NativeDrawCommand`**

Add fields:

```cpp
// 单元格背景色是给 Qt 原生预览和打印后端使用的可选样式提示，空值表示透明。
QString backgroundColor;

// 单元格文字色是给 Qt 原生预览和打印后端使用的可选样式提示，空值表示使用默认画笔。
QString textColor;

// 单元格边框宽度使用毫米单位，0 表示沿用当前后端默认线宽。
double borderWidthMm = 0.0;
```

- [ ] **Step 4: Update renderer to use layout**

In `TableElementRenderer.cpp`:

- Include `TableElementLayout.h`.
- Replace duplicated layout loops with:

```cpp
const auto layout = TableElementLayout::layout(table, context);
if (!layout.success()) {
    result.errorMessage = layout.errorMessage;
    return result;
}
```

- For each visible layout cell:
  - Emit rectangle if `cell.style.drawBorder`.
  - Emit text command unless text is empty.
  - Use `cell.rect` plus `table.x + offsetX` and `table.y + offsetY`.
  - Set `wrapText`, `maxLines`, `ellipsis`, `backgroundColor`, `textColor`, `borderWidthMm`.

- [ ] **Step 5: Preserve existing pagination tests**

`renderPages()` must map `TableLayoutPage` to `TableRenderPage`:

```cpp
page.pageNumber = layoutPage.pageNumber;
page.firstRowIndex = layoutPage.firstRowIndex;
page.rowCount = layoutPage.rowCount;
page.commands = commandsForLayoutPage(table, layoutPage, offsetX, offsetY);
```

If full pagination is not yet in `TableElementLayout`, keep current fixed-height pagination behavior for legacy tables. Only route through layout when complex arrays are present:

```cpp
const auto usesComplexModel = !table.rowBands.isEmpty()
    || !table.cellTemplates.isEmpty()
    || !table.cellStyles.isEmpty()
    || !table.mergeRegions.isEmpty();
```

- [ ] **Step 6: Run GREEN**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_core_tests.exe tableElementRendererUsesComplexLayoutCells tableElementRendererSplitsRowsAcrossPagesAndRepeatsHeader tableElementRendererBuildsSinglePageCommands'
```

Expected: all targeted tests pass.

- [ ] **Step 7: Commit**

```powershell
git add include/sleekpr/core/native/NativeDrawCommand.h src/core/templates/TableElementRenderer.cpp tests/tst_core.cpp
git commit -m "让表格渲染器使用复杂布局结果"
```

---

## Task 7: Overflow and Page Limit Errors

**Files:**
- Modify: `src/core/templates/TableElementLayout.cpp`
- Modify: `src/core/templates/TableElementRenderer.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing overflow test**

Add declaration:

```cpp
void tableElementLayoutReportsOverflowWhenRowCannotFit();
```

Add test:

```cpp
void CoreTests::tableElementLayoutReportsOverflowWhenRowCannotFit()
{
    TableElement table;
    table.id = QStringLiteral("overflow-table");
    table.dataPath = QStringLiteral("items");
    table.width = 20.0;
    table.height = 8.0;
    table.headerRowHeightMm = 5.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 20.0;
    column.wrapText = true;
    table.columns.append(column);

    TableRowBand detailBand;
    detailBand.id = QStringLiteral("detail");
    detailBand.kind = TableRowBandKind::Detail;
    detailBand.heightMode = TableRowHeightMode::Auto;
    detailBand.minHeightMm = 4.0;
    table.rowBands.append(detailBand);
    table.pagination.overflowPolicy = TableTableOverflowPolicy::Error;

    TemplateRenderContext context;
    context.values = QJsonObject{{QStringLiteral("items"),
        QJsonArray{QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("很长很长很长很长很长的商品名称")}}}}};

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(result.errorMessage.contains(QString::fromUtf8("overflow-table")));
    QVERIFY(result.errorMessage.contains(QString::fromUtf8("第 1 行")));
}
```

- [ ] **Step 2: Run RED**

Run targeted test. Expected: row is laid out beyond page height or no error is returned.

- [ ] **Step 3: Implement overflow checks**

When calculating a detail row:

- If row height exceeds available page height after header and `overflowPolicy == Error`, return:

```cpp
QString::fromUtf8("表格 %1 第 %2 行高度超过页面可用区域").arg(table.id).arg(rowIndex + 1)
```

- If generated page count exceeds `pagination.maxPages`, return:

```cpp
QString::fromUtf8("表格 %1 分页超过最大页数 %2").arg(table.id).arg(table.pagination.maxPages)
```

For Phase 1, `Clip` may keep current clipped layout and `Continue` may behave like normal pagination.

- [ ] **Step 4: Run GREEN**

Run targeted test and existing render page tests.

Expected: overflow test and existing pagination tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/core/templates/TableElementLayout.cpp src/core/templates/TableElementRenderer.cpp tests/tst_core.cpp
git commit -m "完善复杂表格溢出错误"
```

---

## Task 8: Full Verification

**Files:**
- No source edits expected.

- [ ] **Step 1: Run diff check**

Run:

```powershell
git diff --check
```

Expected: no whitespace errors. CRLF warnings are acceptable in this repository.

- [ ] **Step 2: Run full CTest**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests sleekpr_app_tests sleekpr_http_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected: `100% tests passed, 0 tests failed out of 3`.

- [ ] **Step 3: Inspect status**

Run:

```powershell
git status --short --branch
git log --oneline -8
```

Expected: clean worktree after final commit.

- [ ] **Step 4: Report remaining phases**

Report that Phase 1 is complete and list remaining work:

- Phase 2 app property panels.
- Phase 3 canvas column-boundary dragging.
- Phase 4 HTTP 422 productized layout errors.
- Phase 5 native renderer color/background styling if not already consumed by preview/print.
