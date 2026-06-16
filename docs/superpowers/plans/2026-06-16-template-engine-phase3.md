# 通用打印模板引擎阶段三 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增表格元素基础能力，让模板可以保存一级表格元素，并把 `TemplateRenderContext` 中的明细数组渲染成单页、固定行高、带基础边框的绘制命令。

**Architecture:** 阶段三先限定在 `core/templates`，表格模型独立于旧 `TemplateElement`，并通过 `TemplateLayer::tables` 挂到模板文档。表格渲染器只输出现有 `NativeDrawCommand` 的 `Rectangle` 和 `Text` 命令，后续分页阶段再升级为多页计划。

**Tech Stack:** C++20、Qt Core JSON、Qt Test、现有 CMake/Ninja 构建。

---

## 文件结构

- `include/sleekpr/core/templates/TableElement.h`：表格元素、列定义和对齐枚举。
- `include/sleekpr/core/templates/TableElementJson.h`：表格元素 JSON 序列化和导入校验接口。
- `src/core/templates/TableElementJson.cpp`：表格元素 JSON 实现。
- `include/sleekpr/core/templates/TableElementRenderer.h`：单页表格渲染结果和渲染器接口。
- `src/core/templates/TableElementRenderer.cpp`：把 `TemplateRenderContext` 中的数组数据转成绘制命令。
- `include/sleekpr/core/templates/TemplateDocument.h`：`TemplateLayer` 新增 `QList<TableElement> tables`。
- `src/core/templates/TemplateDocumentJson.cpp`：保存、读取、校验图层表格。
- `src/core/templates/TemplateDocumentRenderer.cpp`：新增带 `TemplateRenderContext` 的渲染重载，并追加表格命令。
- `tests/tst_core.cpp`：TDD 覆盖模型 JSON、导入校验、单页渲染和文档渲染接入。
- `CMakeLists.txt`：加入新增 core 源文件。

---

### 任务 1：表格元素模型和 JSON

**Files:**
- Create: `include/sleekpr/core/templates/TableElement.h`
- Create: `include/sleekpr/core/templates/TableElementJson.h`
- Create: `src/core/templates/TableElementJson.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

在 `tests/tst_core.cpp` 增加 include：

```cpp
#include "sleekpr/core/templates/TableElementJson.h"
```

增加槽函数：

```cpp
void tableElementJsonPersistsColumnsAndLayout();
void tableElementJsonRejectsInvalidTable();
```

新增测试：

```cpp
void CoreTests::tableElementJsonPersistsColumnsAndLayout()
{
    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("main");
    table.displayName = QString::fromUtf8("销售明细");
    table.x = 5.0;
    table.y = 10.0;
    table.width = 90.0;
    table.height = 40.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 6.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 50.0;
    table.columns.append(nameColumn);

    TableColumn weightColumn;
    weightColumn.id = QStringLiteral("weight");
    weightColumn.title = QString::fromUtf8("重量");
    weightColumn.fieldKey = QStringLiteral("weight");
    weightColumn.widthMm = 20.0;
    weightColumn.alignment = TableCellAlignment::Right;
    table.columns.append(weightColumn);

    const auto json = TableElementJson::toJson(table);
    QString errorMessage;
    QVERIFY(TableElementJson::validate(json, &errorMessage));

    const auto actual = TableElementJson::fromJson(json, QStringLiteral("main"));
    QCOMPARE(actual.id, QString("saleItems"));
    QCOMPARE(actual.dataPath, QString("items"));
    QCOMPARE(actual.columns.size(), 2);
    QCOMPARE(actual.columns[1].alignment, TableCellAlignment::Right);
}

void CoreTests::tableElementJsonRejectsInvalidTable()
{
    QJsonObject invalidTable;
    invalidTable["id"] = QStringLiteral("saleItems");
    invalidTable["dataPath"] = QStringLiteral("items");
    invalidTable["width"] = 80.0;
    invalidTable["height"] = 30.0;
    invalidTable["columns"] = QJsonArray{};

    QString errorMessage;
    QVERIFY(!TableElementJson::validate(invalidTable, QStringLiteral("main"), &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("列")));

    QJsonObject invalidColumn;
    invalidColumn["id"] = QStringLiteral("name");
    invalidColumn["title"] = QString::fromUtf8("品名");
    invalidColumn["fieldKey"] = QStringLiteral("productName");
    invalidColumn["widthMm"] = 0.0;
    invalidTable["columns"] = QJsonArray{invalidColumn};

    QVERIFY(!TableElementJson::validate(invalidTable, QStringLiteral("main"), &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("列宽")));
}
```

- [ ] **步骤 2：运行红灯测试**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests'
```

Expected: 编译失败，因为 `TableElementJson.h` 尚不存在。

- [ ] **步骤 3：实现模型**

`TableElement.h` 定义：

```cpp
enum class TableCellAlignment { Left, Center, Right };

struct TableColumn
{
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
    QString dataPath;
    double headerRowHeightMm = 6.0;
    double detailRowHeightMm = 5.0;
    bool drawBorders = true;
    bool repeatHeaderOnPage = true;
    QList<TableColumn> columns;
};
```

- [ ] **步骤 4：实现 JSON**

`TableElementJson` 提供：

```cpp
static QJsonObject toJson(const TableElement& table);
static TableElement fromJson(const QJsonObject& json, const QString& parentLayerId = QString());
static bool validate(const QJsonObject& json, const QString& parentLayerId, QString* errorMessage = nullptr);
```

校验必须拒绝：空 `id`、首尾空白 `id`、与父图层不一致的 `layerId`、非正数 `width/height/headerRowHeightMm/detailRowHeightMm`、空 `dataPath`、空 `columns`、空列 `id`、重复列 `id`、空 `fieldKey`、非正数 `widthMm`、未知 `alignment`。

- [ ] **步骤 5：跑绿灯并提交**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure'
git add CMakeLists.txt include/sleekpr/core/templates/TableElement.h include/sleekpr/core/templates/TableElementJson.h src/core/templates/TableElementJson.cpp tests/tst_core.cpp
git commit -m "feat: add table element json"
```

---

### 任务 2：单页表格渲染器

**Files:**
- Create: `include/sleekpr/core/templates/TableElementRenderer.h`
- Create: `src/core/templates/TableElementRenderer.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

增加 include：

```cpp
#include "sleekpr/core/templates/TableElementRenderer.h"
```

增加槽函数：

```cpp
void tableElementRendererBuildsSinglePageCommands();
void tableElementRendererRejectsNonArrayData();
```

新增测试：

```cpp
void CoreTests::tableElementRendererBuildsSinglePageCommands()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.x = 5.0;
    table.y = 10.0;
    table.width = 70.0;
    table.height = 25.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;
    table.columns.append(nameColumn);

    TableColumn weightColumn;
    weightColumn.id = QStringLiteral("weight");
    weightColumn.title = QString::fromUtf8("重量");
    weightColumn.fieldKey = QStringLiteral("weight");
    weightColumn.widthMm = 25.0;
    table.columns.append(weightColumn);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("足金戒指")}, {QStringLiteral("weight"), QStringLiteral("3.21g")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("足金项链")}, {QStringLiteral("weight"), QStringLiteral("5.88g")}},
    };

    const auto result = TableElementRenderer::renderSinglePage(table, context, 1.0, 2.0);
    QVERIFY(result.success());
    QCOMPARE(result.commands.size(), 12);
    QCOMPARE(result.commands[0].type, NativeDrawCommandType::Rectangle);
    QCOMPARE(result.commands[1].text, QString::fromUtf8("品名"));
    QCOMPARE(result.commands[5].text, QString::fromUtf8("足金戒指"));
    QCOMPARE(result.commands[9].text, QString::fromUtf8("足金项链"));
    QCOMPARE(result.commands[0].x, 6.0);
    QCOMPARE(result.commands[0].y, 12.0);
}

void CoreTests::tableElementRendererRejectsNonArrayData()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.dataPath = QStringLiteral("items");
    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    table.columns.append(column);

    TemplateRenderContext context;
    context.values["items"] = QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("足金戒指")}};

    const auto result = TableElementRenderer::renderSinglePage(table, context);
    QVERIFY(!result.success());
    QVERIFY(result.errorMessage.contains(QStringLiteral("items")));
    QVERIFY(result.errorMessage.contains(QString::fromUtf8("数组")));
}
```

- [ ] **步骤 2：运行红灯测试**

Expected: 编译失败，因为 `TableElementRenderer.h` 尚不存在。

- [ ] **步骤 3：实现渲染器**

`TableElementRenderer.h` 定义：

```cpp
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
```

渲染规则：
- `context.values[dataPath]` 必须是数组。
- 最大明细行数为 `(height - headerRowHeightMm) / detailRowHeightMm`。
- 明细行超过容量时返回中文错误，不静默截断。
- 每个表头和明细单元格输出一个 `Rectangle` 边框命令和一个 `Text` 文本命令。
- 文本命令使用 0.6mm 内边距，`elementKey` 使用 `tableId.header.columnId` 和 `tableId.rowN.columnId`。

- [ ] **步骤 4：跑绿灯并提交**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure'
git add CMakeLists.txt include/sleekpr/core/templates/TableElementRenderer.h src/core/templates/TableElementRenderer.cpp tests/tst_core.cpp
git commit -m "feat: render table element commands"
```

---

### 任务 3：模板文档保存表格并接入渲染

**Files:**
- Modify: `include/sleekpr/core/templates/TemplateDocument.h`
- Modify: `include/sleekpr/core/templates/TemplateDocumentRenderer.h`
- Modify: `src/core/templates/TemplateDocumentJson.cpp`
- Modify: `src/core/templates/TemplateDocumentRenderer.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

增加槽函数：

```cpp
void templateDocumentJsonPersistsLayerTables();
void templateDocumentRendererAppendsTableCommands();
```

新增测试覆盖：
- `TemplateLayer::tables` 能随模板文档 JSON 往返。
- 非法表格能被 `TemplateDocumentJson::validateForImport()` 拒绝。
- `TemplateDocumentRenderer::render(..., context)` 能把表格命令追加到普通元素后。

- [ ] **步骤 2：运行红灯测试**

Expected: 编译失败，因为 `TemplateLayer::tables` 和新渲染重载尚不存在。

- [ ] **步骤 3：接入文档模型和 JSON**

在 `TemplateLayer` 新增：

```cpp
// 表格是一级模板元素，不能拆成多个普通文本框保存。
QList<TableElement> tables;
```

`TemplateDocumentJson` 的 layer JSON 新增 `tables` 数组；导入校验调用 `TableElementJson::validate()`，并在整个模板内检查普通元素 id 和表格 id 不重复。

- [ ] **步骤 4：接入模板文档渲染**

`TemplateDocumentRenderer` 新增重载：

```cpp
NativeLabelDrawingPlan render(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const LabelOffset& labelOffset,
    const DeviceProfile& profile,
    const TemplateRenderContext& context) const;
```

旧重载继续存在，并传入空 `TemplateRenderContext`，确保旧 `/print/tag` 流程不受影响。新重载先渲染普通元素，再按图层顺序和 `zIndex` 追加可见表格命令。

- [ ] **步骤 5：跑绿灯并提交**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure'
git add include/sleekpr/core/templates/TemplateDocument.h include/sleekpr/core/templates/TemplateDocumentRenderer.h src/core/templates/TemplateDocumentJson.cpp src/core/templates/TemplateDocumentRenderer.cpp tests/tst_core.cpp
git commit -m "feat: add table elements to template documents"
```

---

### 任务 4：完整验证

**Files:**
- Test only.

- [ ] **步骤 1：运行完整构建和测试**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected:

```text
100% tests passed, 0 tests failed out of 3
```

- [ ] **步骤 2：检查工作区**

```powershell
git status --short
git log --oneline -8
```

Expected: 只剩本次未触碰的 `README.md` 和 `docs/superpowers/plans/2026-06-15-settings-window.md`。
