# 通用打印模板引擎阶段四 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立表格分页基础能力，让复杂明细表格可以被拆成多页确定绘制命令，并保留现有单页标签打印兼容入口。

**Architecture:** 阶段四第一批只进入 `core/native` 和 `core/templates`，新增通用多页绘制计划 `NativePrintDrawingPlan`，不修改真实打印设备适配层。`TableElementRenderer` 负责把一个表格拆成多页；`TemplateDocumentRenderer` 新增多页渲染入口，旧 `render()` 继续返回第一页，避免影响现有 `/print/tag` 流程。

**Tech Stack:** C++20、Qt Core、Qt Test、现有 CMake/Ninja 构建。

---

## 文件结构

- `include/sleekpr/core/native/NativePrintDrawingPlan.h`：多页打印绘制计划模型，提供从单页计划转换和取第一页兼容计划的能力。
- `include/sleekpr/core/templates/TableElementRenderer.h`：新增表格分页结果和 `renderPages()` 接口。
- `src/core/templates/TableElementRenderer.cpp`：实现固定行高分页、重复表头、过短页面错误。
- `include/sleekpr/core/templates/TemplateDocumentRenderer.h`：新增 `renderPrint()` 多页模板渲染入口。
- `src/core/templates/TemplateDocumentRenderer.cpp`：把普通元素放到第一页，把表格分页命令合并到多页计划。
- `tests/tst_core.cpp`：覆盖多页计划、表格分页、模板文档分页渲染。

---

### 任务 1：新增多页绘制计划模型

**Files:**
- Create: `include/sleekpr/core/native/NativePrintDrawingPlan.h`
- Test: `tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

在 `tests/tst_core.cpp` 增加 include：

```cpp
#include "sleekpr/core/native/NativePrintDrawingPlan.h"
```

增加槽函数：

```cpp
void nativePrintDrawingPlanWrapsSinglePagePlan();
```

新增测试：

```cpp
void CoreTests::nativePrintDrawingPlanWrapsSinglePagePlan()
{
    NativeLabelDrawingPlan labelPlan;
    labelPlan.paperSize = LabelPaperSize::create80x30();
    labelPlan.renderDpi = 203.0;
    labelPlan.commands.append(NativeLabelDrawingPlanner::text(1.0, 2.0, 10.0, 3.0, QStringLiteral("A"), 4.0, false, QStringLiteral("title")));

    const auto printPlan = NativePrintDrawingPlan::fromSinglePage(labelPlan);
    QCOMPARE(printPlan.pages.size(), 1);
    QCOMPARE(printPlan.pages.first().pageNumber, 1);
    QCOMPARE(printPlan.pages.first().commands.first().elementKey, QString("title"));

    const auto firstPage = printPlan.firstPageAsLabelPlan();
    QCOMPARE(firstPage.renderDpi, 203.0);
    QCOMPARE(firstPage.commands.size(), 1);
    QCOMPARE(firstPage.commands.first().text, QString("A"));
}
```

- [ ] **步骤 2：运行红灯测试**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests'
```

Expected: 编译失败，因为 `NativePrintDrawingPlan.h` 尚不存在。

- [ ] **步骤 3：实现模型**

`NativePrintDrawingPlan.h` 定义：

```cpp
struct NativePrintPage
{
    // pageNumber 使用从 1 开始的页码，便于后续日志和用户提示直接展示。
    int pageNumber = 1;
    QList<NativeDrawCommand> commands;
};

struct NativePrintDrawingPlan
{
    // 多页计划沿用单页计划的纸张和 DPI 语义，页面内容通过 pages 拆分。
    LabelPaperSize paperSize = LabelPaperSize::create80x30();
    double renderDpi = 300.0;
    QList<NativePrintPage> pages;

    static NativePrintDrawingPlan fromSinglePage(const NativeLabelDrawingPlan& plan);
    NativeLabelDrawingPlan firstPageAsLabelPlan() const;
};
```

- [ ] **步骤 4：运行绿灯测试并提交**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure'
git add include/sleekpr/core/native/NativePrintDrawingPlan.h tests/tst_core.cpp
git commit -m "feat: add native print drawing plan"
```

---

### 任务 2：表格分页渲染器

**Files:**
- Modify: `include/sleekpr/core/templates/TableElementRenderer.h`
- Modify: `src/core/templates/TableElementRenderer.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

增加槽函数：

```cpp
void tableElementRendererSplitsRowsAcrossPagesAndRepeatsHeader();
void tableElementRendererRejectsPageTooShortForOneRow();
```

分页测试：

```cpp
void CoreTests::tableElementRendererSplitsRowsAcrossPagesAndRepeatsHeader()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.x = 5.0;
    table.y = 8.0;
    table.width = 45.0;
    table.height = 15.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;
    table.columns.append(nameColumn);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品1")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品2")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品3")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品4")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品5")}},
    };

    const auto result = TableElementRenderer::renderPages(table, context);
    QVERIFY(result.success());
    QCOMPARE(result.pages.size(), 3);
    QCOMPARE(result.pages[0].firstRowIndex, 0);
    QCOMPARE(result.pages[1].firstRowIndex, 2);
    QCOMPARE(result.pages[2].firstRowIndex, 4);
    QCOMPARE(result.pages[1].commands[1].text, QString::fromUtf8("品名"));
    QCOMPARE(result.pages[1].commands[3].text, QString::fromUtf8("产品3"));
    QCOMPARE(result.pages[2].commands[3].text, QString::fromUtf8("产品5"));
}
```

过短页面测试：

```cpp
void CoreTests::tableElementRendererRejectsPageTooShortForOneRow()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.height = 5.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    table.columns.append(column);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品1")}}};

    const auto result = TableElementRenderer::renderPages(table, context);
    QVERIFY(!result.success());
    QVERIFY(result.errorMessage.contains(QString::fromUtf8("单行")));
}
```

- [ ] **步骤 2：运行红灯测试**

Expected: 编译失败，因为 `renderPages()` 和 `pages` 字段尚不存在。

- [ ] **步骤 3：实现分页结果和渲染**

在头文件新增：

```cpp
struct TableRenderPage
{
    // firstRowIndex 使用原始明细数组下标，方便错误定位和后续页内小计。
    int pageNumber = 1;
    int firstRowIndex = 0;
    int rowCount = 0;
    QList<NativeDrawCommand> commands;
};
```

`TableRenderResult` 新增：

```cpp
QList<TableRenderPage> pages;
```

`TableElementRenderer` 新增：

```cpp
static TableRenderResult renderPages(const TableElement& table,
                                     const TemplateRenderContext& context,
                                     double offsetX = 0.0,
                                     double offsetY = 0.0);
```

分页规则：
- 第一页始终绘制表头。
- `repeatHeaderOnPage=true` 时，后续页继续绘制表头。
- 每页明细容量为可用高度除以 `detailRowHeightMm`。
- 单页无法容纳一行明细时返回中文错误，不静默截断。
- 明细行 `elementKey` 使用全局行号，重复表头使用 `pageN.header`，避免跨页 key 冲突。

- [ ] **步骤 4：运行绿灯测试并提交**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure'
git add include/sleekpr/core/templates/TableElementRenderer.h src/core/templates/TableElementRenderer.cpp tests/tst_core.cpp
git commit -m "feat: paginate table element rendering"
```

---

### 任务 3：模板文档多页渲染入口

**Files:**
- Modify: `include/sleekpr/core/templates/TemplateDocumentRenderer.h`
- Modify: `src/core/templates/TemplateDocumentRenderer.cpp`
- Test: `tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

增加槽函数：

```cpp
void templateDocumentRendererBuildsMultiPagePrintPlan();
```

新增测试：

```cpp
void CoreTests::templateDocumentRendererBuildsMultiPagePrintPlan()
{
    TemplateElement title;
    title.id = QStringLiteral("title");
    title.layerId = QStringLiteral("main");
    title.text = QString::fromUtf8("销售单");
    title.width = 20.0;
    title.height = 4.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;

    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("main");
    table.x = 5.0;
    table.y = 8.0;
    table.width = 45.0;
    table.height = 15.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;
    table.columns.append(nameColumn);

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.elements.append(title);
    layer.tables.append(table);

    TemplateDocument document;
    document.layers.append(layer);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品1")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品2")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品3")}},
    };

    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto printPlan = TemplateDocumentRenderer().renderPrint(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(printPlan.pages.size(), 2);
    QCOMPARE(printPlan.pages[0].commands.first().elementKey, QString("title"));
    QVERIFY(std::any_of(printPlan.pages[0].commands.cbegin(), printPlan.pages[0].commands.cend(), [](const NativeDrawCommand& command) {
        return command.text == QString::fromUtf8("产品2");
    }));
    QVERIFY(std::none_of(printPlan.pages[1].commands.cbegin(), printPlan.pages[1].commands.cend(), [](const NativeDrawCommand& command) {
        return command.elementKey == QStringLiteral("title");
    }));
    QVERIFY(std::any_of(printPlan.pages[1].commands.cbegin(), printPlan.pages[1].commands.cend(), [](const NativeDrawCommand& command) {
        return command.text == QString::fromUtf8("产品3");
    }));
}
```

- [ ] **步骤 2：运行红灯测试**

Expected: 编译失败，因为 `renderPrint()` 尚不存在。

- [ ] **步骤 3：实现多页入口**

`TemplateDocumentRenderer` 新增：

```cpp
NativePrintDrawingPlan renderPrint(
    const TemplateDocument& document,
    const LabelRenderPlan& labelPlan,
    const LabelOffset& labelOffset,
    const DeviceProfile& profile,
    const TemplateRenderContext& context) const;
```

实现规则：
- 普通模板元素只放在第一页。
- 每个表格调用 `TableElementRenderer::renderPages()`。
- 输出页数取所有表格页数的最大值，第一页合并普通元素和表格第一页。
- 旧 `render()` 继续返回 `renderPrint(...).firstPageAsLabelPlan()`。
- 设备 profile 缩放应用到所有页面命令。

- [ ] **步骤 4：运行绿灯测试并提交**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure'
git add include/sleekpr/core/templates/TemplateDocumentRenderer.h src/core/templates/TemplateDocumentRenderer.cpp tests/tst_core.cpp
git commit -m "feat: render template documents as print pages"
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

Expected: 只剩本轮未触碰的 `README.md` 和 `docs/superpowers/plans/2026-06-15-settings-window.md`。
