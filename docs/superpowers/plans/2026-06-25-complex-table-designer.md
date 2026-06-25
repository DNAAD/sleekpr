# Complex Table Designer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a right-side Qt native column editor for complex table elements in the template designer.

**Architecture:** The feature stays in the existing app-layer presenter/command pattern. `TableColumnEditorPanel` owns only Qt controls and semantic UI signals; `TemplateInspectorPanel` composes it into the table tab; `TemplateDesignerPresenter` and `TemplateDesignerCommand` convert and apply structured column models to existing `core::TableColumn` data. Rendering, preview, printing, HTTP routes, and template JSON continue using the existing `TableElement` and Qt native rendering chain.

**Tech Stack:** C++20, Qt 6 Widgets, Qt Test, CMake/Ninja, existing `sleekpr_core`, `sleekpr_app_tests`.

---

## Preflight

The previous interaction-latency fix is committed as `1f0eb18 优化模板设计器交互响应`. Start from a clean worktree before editing.

Run:

```powershell
git status --short
```

Expected: no output.

## File Structure

- Create `include/sleekpr/app/TableColumnTextCodec.h`
  - Small app-layer utility for formatting/parsing the legacy advanced column text.
- Create `src/app/TableColumnTextCodec.cpp`
  - Moves existing `TablePropertiesCommand::formatColumns/parseColumns` behavior out of command so both Command and Inspector can use it.
- Create `include/sleekpr/app/TableColumnEditorPanel.h`
  - Qt widget that edits a list of `DesignerTableColumnModel` rows and emits semantic signals.
- Create `src/app/TableColumnEditorPanel.cpp`
  - Builds the `QTableWidget`, toolbar buttons, combo boxes, and row synchronization.
- Modify `include/sleekpr/app/TemplateDesignerPropertyModel.h`
  - Add `DesignerTableColumnModel` and `QList<DesignerTableColumnModel> columns`.
- Modify `include/sleekpr/app/TemplateDesignerPresenter.h`
  - Add conversion methods between `core::TableColumn` and `DesignerTableColumnModel`.
- Modify `src/app/TemplateDesignerPresenter.cpp`
  - Populate structured column models on table selection.
- Modify `include/sleekpr/app/TemplateDesignerCommand.h`
  - Replace command-local column formatter/parser with codec usage.
- Modify `src/app/TemplateDesignerCommand.cpp`
  - Apply structured columns from `DesignerTablePropertyModel`.
- Modify `include/sleekpr/app/TemplateInspectorPanel.h`
  - Own `TableColumnEditorPanel` and expose it for tests.
- Modify `src/app/TemplateInspectorPanel.cpp`
  - Add the column editor to the table tab, keep advanced text synchronized, emit `tablePropertiesEdited`.
- Modify `CMakeLists.txt`
  - Add new app source/header files to `sleekpr`.
- Modify `tests/CMakeLists.txt`
  - Add new app source/header files to `sleekpr_app_tests`.
- Modify `tests/tst_app.cpp`
  - Add tests for codec, presenter conversion, editor signals, inspector integration, and designer auto-apply.

## Task 1: Structured Column Model and Text Codec

**Files:**
- Create: `include/sleekpr/app/TableColumnTextCodec.h`
- Create: `src/app/TableColumnTextCodec.cpp`
- Modify: `include/sleekpr/app/TemplateDesignerPropertyModel.h`
- Modify: `include/sleekpr/app/TemplateDesignerCommand.h`
- Modify: `src/app/TemplateDesignerCommand.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_app.cpp`

- [x] **Step 1: Write failing codec and model tests**

Add test declarations in `tests/tst_app.cpp`:

```cpp
void tableColumnTextCodecFormatsAndParsesLegacyColumns();
void tableDesignerCommandUsesStructuredColumnsWhenPresent();
```

Add test bodies:

```cpp
void AppTests::tableColumnTextCodecFormatsAndParsesLegacyColumns()
{
    using sleekpr::app::TableColumnTextCodec;
    using sleekpr::core::TableCellAlignment;
    using sleekpr::core::TableColumn;
    using sleekpr::core::TableColumnWidthMode;

    TableColumn fixed;
    fixed.id = QStringLiteral("name");
    fixed.title = QString::fromUtf8("品名");
    fixed.fieldKey = QStringLiteral("productName");
    fixed.widthMm = 26.0;
    fixed.widthMode = TableColumnWidthMode::Fixed;
    fixed.alignment = TableCellAlignment::Left;

    TableColumn flex;
    flex.id = QStringLiteral("weight");
    flex.title = QString::fromUtf8("重量");
    flex.fieldKey = QStringLiteral("weight");
    flex.widthMm = 18.0;

    const auto text = TableColumnTextCodec::format({fixed, flex});
    QCOMPARE(text, QString::fromUtf8("品名=productName:26.00,重量=weight:18.00"));

    QString errorMessage;
    const auto parsed = TableColumnTextCodec::parse(text, &errorMessage);
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(parsed.size(), 2);
    QCOMPARE(parsed[0].id, QStringLiteral("productName"));
    QCOMPARE(parsed[0].title, QString::fromUtf8("品名"));
    QCOMPARE(parsed[0].fieldKey, QStringLiteral("productName"));
    QCOMPARE(parsed[0].widthMm, 26.0);
}

void AppTests::tableDesignerCommandUsesStructuredColumnsWhenPresent()
{
    using sleekpr::app::DesignerTableColumnModel;
    using sleekpr::app::DesignerTablePropertyModel;
    using sleekpr::app::TablePropertiesCommand;
    using sleekpr::core::TableCellAlignment;
    using sleekpr::core::TableColumnWidthMode;
    using sleekpr::core::TableElement;
    using sleekpr::core::TemplateDocument;
    using sleekpr::core::TemplateLayer;

    TableElement table;
    table.id = QStringLiteral("table-1");
    table.layerId = QStringLiteral("main");
    table.displayName = QString::fromUtf8("明细");
    table.dataPath = QStringLiteral("items");
    table.columns = sleekpr::app::TableColumnTextCodec::parse(QString::fromUtf8("旧列=oldField:20.00"));

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.tables.append(table);

    TemplateDocument document;
    document.layers.append(layer);

    DesignerTableColumnModel column;
    column.columnId = QStringLiteral("structured-name");
    column.title = QString::fromUtf8("新列");
    column.fieldKey = QStringLiteral("newField");
    column.widthMode = TableColumnWidthMode::Flex;
    column.widthMm = 22.0;
    column.flexWeight = 2.0;
    column.alignment = TableCellAlignment::Right;
    column.fontSizePt = 7.0;
    column.bold = true;
    column.ellipsis = true;

    DesignerTablePropertyModel model;
    model.tableId = table.id;
    model.visible = true;
    model.canEdit = true;
    model.displayName = table.displayName;
    model.dataPath = table.dataPath;
    model.width = table.width;
    model.height = table.height;
    model.headerRowHeightMm = table.headerRowHeightMm;
    model.detailRowHeightMm = table.detailRowHeightMm;
    model.repeatHeaderOnPage = table.repeatHeaderOnPage;
    model.drawBorders = table.drawBorders;
    model.columnsText = QString::fromUtf8("旧列=oldField:20.00");
    model.columns = {column};

    const auto result = TablePropertiesCommand(model).apply(document, table.id);
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));
    QVERIFY(result.changed);
    const auto actual = document.layers.first().tables.first().columns.first();
    QCOMPARE(actual.id, QStringLiteral("structured-name"));
    QCOMPARE(actual.fieldKey, QStringLiteral("newField"));
    QCOMPARE(actual.widthMode, TableColumnWidthMode::Flex);
    QCOMPARE(actual.alignment, TableCellAlignment::Right);
    QVERIFY(actual.bold);
    QVERIFY(actual.ellipsis);
}
```

- [x] **Step 2: Run tests to verify RED**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_app_tests.exe tableColumnTextCodecFormatsAndParsesLegacyColumns tableDesignerCommandUsesStructuredColumnsWhenPresent'
```

Expected: build fails because `TableColumnTextCodec` and `DesignerTableColumnModel` do not exist.

- [x] **Step 3: Add `DesignerTableColumnModel`**

Modify `include/sleekpr/app/TemplateDesignerPropertyModel.h`:

```cpp
#include "sleekpr/core/templates/TableElement.h"

#include <QList>
```

Add before `DesignerTablePropertyModel`:

```cpp
struct DesignerTableColumnModel
{
    QString columnId;
    QString title;
    QString fieldKey;
    sleekpr::core::TableColumnWidthMode widthMode = sleekpr::core::TableColumnWidthMode::Fixed;
    double widthMm = 20.0;
    double flexWeight = 1.0;
    sleekpr::core::TableCellAlignment alignment = sleekpr::core::TableCellAlignment::Left;
    double fontSizePt = 8.0;
    bool bold = false;
    bool ellipsis = false;
};
```

Add to `DesignerTablePropertyModel`:

```cpp
QList<DesignerTableColumnModel> columns;
```

- [x] **Step 4: Add codec files**

Create `include/sleekpr/app/TableColumnTextCodec.h`:

```cpp
#pragma once

#include "sleekpr/core/templates/TableElement.h"

#include <QList>
#include <QString>

namespace sleekpr::app {

class TableColumnTextCodec final
{
public:
    static QString format(const QList<sleekpr::core::TableColumn>& columns);
    static QList<sleekpr::core::TableColumn> parse(const QString& text, QString* errorMessage = nullptr);
};

} // namespace sleekpr::app
```

Create `src/app/TableColumnTextCodec.cpp` by moving the existing format/parse behavior from `TablePropertiesCommand`:

```cpp
#include "sleekpr/app/TableColumnTextCodec.h"

#include <QStringList>

namespace sleekpr::app {

QString TableColumnTextCodec::format(const QList<sleekpr::core::TableColumn>& columns)
{
    QStringList parts;
    for (const auto& column : columns) {
        parts.append(QStringLiteral("%1=%2:%3").arg(
            column.title,
            column.fieldKey,
            QString::number(column.widthMm, 'f', 2)));
    }
    return parts.join(QStringLiteral(","));
}

QList<sleekpr::core::TableColumn> TableColumnTextCodec::parse(const QString& text, QString* errorMessage)
{
    QList<sleekpr::core::TableColumn> columns;
    const auto parts = text.split(QStringLiteral(","), Qt::SkipEmptyParts);
    for (const auto& part : parts) {
        const auto trimmed = part.trimmed();
        const auto equalsIndex = trimmed.indexOf(QStringLiteral("="));
        const auto widthIndex = trimmed.lastIndexOf(QStringLiteral(":"));
        if (equalsIndex <= 0 || widthIndex <= equalsIndex + 1 || widthIndex >= trimmed.size() - 1) {
            if (errorMessage != nullptr) {
                *errorMessage = QString::fromUtf8("列配置格式应为：标题=字段:宽度");
            }
            return {};
        }

        bool widthOk = false;
        const auto width = trimmed.mid(widthIndex + 1).trimmed().toDouble(&widthOk);
        const auto title = trimmed.left(equalsIndex).trimmed();
        const auto fieldKey = trimmed.mid(equalsIndex + 1, widthIndex - equalsIndex - 1).trimmed();
        if (!widthOk || width <= 0.0 || title.isEmpty() || fieldKey.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QString::fromUtf8("列标题、字段和宽度必须有效");
            }
            return {};
        }

        sleekpr::core::TableColumn column;
        column.id = fieldKey;
        column.title = title;
        column.fieldKey = fieldKey;
        column.widthMm = width;
        columns.append(column);
    }
    return columns;
}

} // namespace sleekpr::app
```

- [x] **Step 5: Wire codec into command**

Modify `TemplateDesignerCommand.cpp`:

```cpp
#include "sleekpr/app/TableColumnTextCodec.h"
```

Replace `TablePropertiesCommand::formatColumns` body with:

```cpp
return TableColumnTextCodec::format(columns);
```

Replace `TablePropertiesCommand::parseColumns` body with:

```cpp
QString errorMessage;
auto columns = TableColumnTextCodec::parse(text, &errorMessage);
Q_UNUSED(errorMessage);
return columns;
```

Add helper in anonymous namespace:

```cpp
sleekpr::core::TableColumn columnFromModel(const DesignerTableColumnModel& model)
{
    sleekpr::core::TableColumn column;
    column.id = model.columnId.trimmed().isEmpty() ? model.fieldKey.trimmed() : model.columnId.trimmed();
    column.title = model.title;
    column.fieldKey = model.fieldKey.trimmed();
    column.widthMode = model.widthMode;
    column.widthMm = std::max(0.1, model.widthMm);
    column.flexWeight = std::max(0.1, model.flexWeight);
    column.alignment = model.alignment;
    column.fontSizePt = std::max(1.0, model.fontSizePt);
    column.bold = model.bold;
    column.ellipsis = model.ellipsis;
    return column;
}
```

In `TablePropertiesCommand::apply`, replace parsed-column assignment with:

```cpp
QList<sleekpr::core::TableColumn> nextColumns;
if (!m_model.columns.isEmpty()) {
    for (const auto& columnModel : m_model.columns) {
        nextColumns.append(columnFromModel(columnModel));
    }
} else {
    nextColumns = parseColumns(m_model.columnsText);
}
if (nextColumns.isEmpty()) {
    result.errorMessage = QString::fromUtf8("表格列配置无效");
    return result;
}
updated.columns = nextColumns;
```

- [x] **Step 6: Add files to CMake**

Modify `CMakeLists.txt` `add_executable(sleekpr ...)`:

```cmake
src/app/TableColumnTextCodec.cpp
include/sleekpr/app/TableColumnTextCodec.h
```

Modify `tests/CMakeLists.txt` `add_executable(sleekpr_app_tests ...)`:

```cmake
../src/app/TableColumnTextCodec.cpp
../include/sleekpr/app/TableColumnTextCodec.h
```

- [x] **Step 7: Run GREEN**

Run the same targeted command from Step 2.

Expected: both tests pass.

- [x] **Step 8: Commit**

```powershell
git add include/sleekpr/app/TemplateDesignerPropertyModel.h include/sleekpr/app/TableColumnTextCodec.h src/app/TableColumnTextCodec.cpp include/sleekpr/app/TemplateDesignerCommand.h src/app/TemplateDesignerCommand.cpp CMakeLists.txt tests/CMakeLists.txt tests/tst_app.cpp
git commit -m "引入表格列结构化设计模型"
```

## Task 2: Presenter Column Conversion

**Files:**
- Modify: `include/sleekpr/app/TemplateDesignerPresenter.h`
- Modify: `src/app/TemplateDesignerPresenter.cpp`
- Test: `tests/tst_app.cpp`

- [x] **Step 1: Write failing presenter test**

Add declaration:

```cpp
void templateDesignerPresenterMapsTableColumns();
```

Add body:

```cpp
void AppTests::templateDesignerPresenterMapsTableColumns()
{
    using sleekpr::app::TemplateDesignerPresenter;
    using sleekpr::core::TableCellAlignment;
    using sleekpr::core::TableColumn;
    using sleekpr::core::TableColumnWidthMode;
    using sleekpr::core::TableElement;

    TableColumn column;
    column.id = QStringLiteral("price");
    column.title = QString::fromUtf8("单价");
    column.fieldKey = QStringLiteral("price");
    column.widthMode = TableColumnWidthMode::Flex;
    column.widthMm = 18.0;
    column.flexWeight = 3.0;
    column.alignment = TableCellAlignment::Right;
    column.fontSizePt = 7.0;
    column.bold = true;
    column.ellipsis = true;

    TableElement table;
    table.id = QStringLiteral("items");
    table.columns.append(column);

    TemplateDesignerPresenter presenter;
    const auto model = presenter.tablePropertyModel(table, true);
    QCOMPARE(model.columns.size(), 1);
    QCOMPARE(model.columns.first().columnId, QStringLiteral("price"));
    QCOMPARE(model.columns.first().fieldKey, QStringLiteral("price"));
    QCOMPARE(model.columns.first().widthMode, TableColumnWidthMode::Flex);
    QCOMPARE(model.columns.first().alignment, TableCellAlignment::Right);
    QVERIFY(model.columns.first().bold);
    QVERIFY(model.columns.first().ellipsis);

    const auto roundTrip = presenter.tableColumnsFromModels(model.columns);
    QCOMPARE(roundTrip.size(), 1);
    QCOMPARE(roundTrip.first().id, QStringLiteral("price"));
    QCOMPARE(roundTrip.first().flexWeight, 3.0);
}
```

- [x] **Step 2: Run RED**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_app_tests.exe templateDesignerPresenterMapsTableColumns'
```

Expected: build fails because `tableColumnsFromModels` does not exist.

- [x] **Step 3: Add presenter APIs**

In `TemplateDesignerPresenter.h`:

```cpp
    DesignerTableColumnModel tableColumnModel(const sleekpr::core::TableColumn& column) const;
    sleekpr::core::TableColumn tableColumnFromModel(const DesignerTableColumnModel& model) const;
    QList<sleekpr::core::TableColumn> tableColumnsFromModels(const QList<DesignerTableColumnModel>& models) const;
```

Include `QList` and `TableElement.h` if needed.

- [x] **Step 4: Implement conversion**

In `TemplateDesignerPresenter.cpp`:

```cpp
DesignerTableColumnModel TemplateDesignerPresenter::tableColumnModel(const sleekpr::core::TableColumn& column) const
{
    DesignerTableColumnModel model;
    model.columnId = column.id;
    model.title = column.title;
    model.fieldKey = column.fieldKey;
    model.widthMode = column.widthMode;
    model.widthMm = column.widthMm;
    model.flexWeight = column.flexWeight;
    model.alignment = column.alignment;
    model.fontSizePt = column.fontSizePt;
    model.bold = column.bold;
    model.ellipsis = column.ellipsis;
    return model;
}

sleekpr::core::TableColumn TemplateDesignerPresenter::tableColumnFromModel(const DesignerTableColumnModel& model) const
{
    sleekpr::core::TableColumn column;
    column.id = model.columnId.trimmed().isEmpty() ? model.fieldKey.trimmed() : model.columnId.trimmed();
    column.title = model.title;
    column.fieldKey = model.fieldKey.trimmed();
    column.widthMode = model.widthMode;
    column.widthMm = std::max(0.1, model.widthMm);
    column.flexWeight = std::max(0.1, model.flexWeight);
    column.alignment = model.alignment;
    column.fontSizePt = std::max(1.0, model.fontSizePt);
    column.bold = model.bold;
    column.ellipsis = model.ellipsis;
    return column;
}

QList<sleekpr::core::TableColumn> TemplateDesignerPresenter::tableColumnsFromModels(const QList<DesignerTableColumnModel>& models) const
{
    QList<sleekpr::core::TableColumn> columns;
    columns.reserve(models.size());
    for (const auto& model : models) {
        columns.append(tableColumnFromModel(model));
    }
    return columns;
}
```

In `tablePropertyModel`, append:

```cpp
for (const auto& column : table.columns) {
    model.columns.append(tableColumnModel(column));
}
```

- [x] **Step 5: Run GREEN**

Run the command from Step 2.

Expected: test passes.

- [x] **Step 6: Commit**

```powershell
git add include/sleekpr/app/TemplateDesignerPresenter.h src/app/TemplateDesignerPresenter.cpp tests/tst_app.cpp
git commit -m "完善表格列属性转换"
```

## Task 3: Standalone Table Column Editor Panel

**Files:**
- Create: `include/sleekpr/app/TableColumnEditorPanel.h`
- Create: `src/app/TableColumnEditorPanel.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_app.cpp`

- [x] **Step 1: Write failing panel test**

Add declaration:

```cpp
void tableColumnEditorPanelEditsAndReordersColumns();
```

Add body:

```cpp
void AppTests::tableColumnEditorPanelEditsAndReordersColumns()
{
    using sleekpr::app::DesignerTableColumnModel;
    using sleekpr::app::TableColumnEditorPanel;
    using sleekpr::core::TableCellAlignment;
    using sleekpr::core::TableColumnWidthMode;

    TableColumnEditorPanel panel;
    DesignerTableColumnModel first;
    first.columnId = QStringLiteral("name");
    first.title = QString::fromUtf8("品名");
    first.fieldKey = QStringLiteral("productName");
    first.widthMode = TableColumnWidthMode::Fixed;
    first.widthMm = 30.0;

    DesignerTableColumnModel second;
    second.columnId = QStringLiteral("weight");
    second.title = QString::fromUtf8("重量");
    second.fieldKey = QStringLiteral("weight");
    second.alignment = TableCellAlignment::Right;
    panel.setColumns({first, second});

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("tableColumnEditorGrid"));
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 2);

    QSignalSpy editedSpy(&panel, &TableColumnEditorPanel::columnsEdited);
    table->item(0, 1)->setText(QStringLiteral("sku"));
    QTRY_COMPARE(editedSpy.count(), 1);
    QCOMPARE(panel.columns().first().fieldKey, QStringLiteral("sku"));

    panel.selectColumn(1);
    QSignalSpy movedSpy(&panel, &TableColumnEditorPanel::columnsEdited);
    auto* moveUp = panel.findChild<QPushButton*>(QStringLiteral("tableColumnMoveUpButton"));
    QVERIFY(moveUp != nullptr);
    moveUp->click();
    QTRY_COMPARE(movedSpy.count(), 1);
    QCOMPARE(panel.columns().first().columnId, QStringLiteral("weight"));
}
```

- [x] **Step 2: Run RED**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_app_tests.exe tableColumnEditorPanelEditsAndReordersColumns'
```

Expected: build fails because `TableColumnEditorPanel` does not exist.

- [x] **Step 3: Add panel header**

Create `include/sleekpr/app/TableColumnEditorPanel.h`:

```cpp
#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"

#include <QWidget>

class QPushButton;
class QTableWidget;

namespace sleekpr::app {

class TableColumnEditorPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TableColumnEditorPanel(QWidget* parent = nullptr);

    void setColumns(const QList<DesignerTableColumnModel>& columns);
    QList<DesignerTableColumnModel> columns() const;
    void setEditable(bool editable);
    void selectColumn(int row);

signals:
    void columnsEdited();
    void columnAddRequested();
    void columnDeleteRequested(int row);
    void columnDuplicateRequested(int row);
    void columnMoveRequested(int fromRow, int toRow);
    void columnsResetRequested();

private:
    void rebuildTable();
    void updateButtonState();
    void emitEdited();
    int currentRow() const;
    DesignerTableColumnModel columnFromRow(int row) const;
    void writeColumnToRow(int row, const DesignerTableColumnModel& column);
    DesignerTableColumnModel createDefaultColumn() const;

    QTableWidget* m_table = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_duplicateButton = nullptr;
    QPushButton* m_moveUpButton = nullptr;
    QPushButton* m_moveDownButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QList<DesignerTableColumnModel> m_columns;
    bool m_editable = true;
    bool m_updating = false;
};

} // namespace sleekpr::app
```

- [x] **Step 4: Implement minimal panel**

Create `src/app/TableColumnEditorPanel.cpp` with:

- A `QTableWidget` named `tableColumnEditorGrid`.
- Buttons named `tableColumnAddButton`, `tableColumnDeleteButton`, `tableColumnDuplicateButton`, `tableColumnMoveUpButton`, `tableColumnMoveDownButton`, `tableColumnResetButton`.
- Columns: 标题, 字段, 宽度模式, 宽度(mm), 弹性, 对齐, 字号, 加粗, 省略.
- `QComboBox` editors for width mode and alignment.
- `QCheckBox` editors for bold and ellipsis.
- `QSignalBlocker` or `m_updating` guard when rebuilding rows.
- All added comments in Chinese.

Use this row mutation rule:

```cpp
void TableColumnEditorPanel::emitEdited()
{
    if (m_updating) {
        return;
    }
    m_columns.clear();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        m_columns.append(columnFromRow(row));
    }
    emit columnsEdited();
}
```

- [x] **Step 5: Add files to CMake**

Modify app and app-test targets:

```cmake
src/app/TableColumnEditorPanel.cpp
include/sleekpr/app/TableColumnEditorPanel.h
```

For tests:

```cmake
../src/app/TableColumnEditorPanel.cpp
../include/sleekpr/app/TableColumnEditorPanel.h
```

- [x] **Step 6: Run GREEN**

Run the command from Step 2.

Expected: test passes.

- [x] **Step 7: Commit**

```powershell
git add include/sleekpr/app/TableColumnEditorPanel.h src/app/TableColumnEditorPanel.cpp CMakeLists.txt tests/CMakeLists.txt tests/tst_app.cpp
git commit -m "新增表格列编辑面板"
```

## Task 4: Integrate Column Editor into Inspector Panel

**Files:**
- Modify: `include/sleekpr/app/TemplateInspectorPanel.h`
- Modify: `src/app/TemplateInspectorPanel.cpp`
- Test: `tests/tst_app.cpp`

- [x] **Step 1: Write failing inspector test**

Add declaration:

```cpp
void templateInspectorPanelExposesTableColumnEditor();
```

Add body:

```cpp
void AppTests::templateInspectorPanelExposesTableColumnEditor()
{
    using sleekpr::app::DesignerTableColumnModel;
    using sleekpr::app::DesignerTablePropertyModel;
    using sleekpr::app::TableColumnEditorPanel;

    TemplateInspectorPanel panel;
    DesignerTableColumnModel column;
    column.columnId = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");

    DesignerTablePropertyModel model;
    model.tableId = QStringLiteral("table");
    model.visible = true;
    model.canEdit = true;
    model.columns = {column};
    model.columnsText = QString::fromUtf8("品名=productName:20.00");

    QSignalSpy editedSpy(&panel, &TemplateInspectorPanel::tablePropertiesEdited);
    panel.setTableProperties(model);

    auto* editor = panel.findChild<TableColumnEditorPanel*>(QStringLiteral("tableColumnEditorPanel"));
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->columns().size(), 1);

    auto* addButton = panel.findChild<QPushButton*>(QStringLiteral("tableColumnAddButton"));
    QVERIFY(addButton != nullptr);
    addButton->click();
    QTRY_VERIFY(editedSpy.count() > 0);
    QCOMPARE(panel.tableProperties().columns.size(), 2);
}
```

- [x] **Step 2: Run RED**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_app_tests.exe templateInspectorPanelExposesTableColumnEditor'
```

Expected: fails because inspector does not create the editor.

- [x] **Step 3: Add member and accessor**

In `TemplateInspectorPanel.h`:

```cpp
class TableColumnEditorPanel;
```

Add public accessor:

```cpp
TableColumnEditorPanel* tableColumnEditor() const;
```

Add member:

```cpp
TableColumnEditorPanel* m_tableColumnEditor = nullptr;
```

- [x] **Step 4: Create and place editor**

In `TemplateInspectorPanel.cpp` include:

```cpp
#include "sleekpr/app/TableColumnEditorPanel.h"
#include "sleekpr/app/TableColumnTextCodec.h"
```

Add this helper in the anonymous namespace of `TemplateInspectorPanel.cpp`:

```cpp
QList<sleekpr::core::TableColumn> tableColumnsFromDesignerModels(
    const QList<sleekpr::app::DesignerTableColumnModel>& models)
{
    QList<sleekpr::core::TableColumn> columns;
    columns.reserve(models.size());
    for (const auto& model : models) {
        sleekpr::core::TableColumn column;
        column.id = model.columnId.trimmed().isEmpty() ? model.fieldKey.trimmed() : model.columnId.trimmed();
        column.title = model.title;
        column.fieldKey = model.fieldKey.trimmed();
        column.widthMode = model.widthMode;
        column.widthMm = std::max(0.1, model.widthMm);
        column.flexWeight = std::max(0.1, model.flexWeight);
        column.alignment = model.alignment;
        column.fontSizePt = std::max(1.0, model.fontSizePt);
        column.bold = model.bold;
        column.ellipsis = model.ellipsis;
        columns.append(column);
    }
    return columns;
}
```

In constructor/table tab layout after table property grid:

```cpp
m_tableColumnEditor = new TableColumnEditorPanel(elementPanel);
m_tableColumnEditor->setObjectName(QStringLiteral("tableColumnEditorPanel"));
tableTabLayout->addWidget(m_tableColumnEditor);
```

Keep `m_tableColumnsEdit` visible below as advanced text config.

- [x] **Step 5: Synchronize set/get**

In `setTableProperties`:

```cpp
if (m_tableColumnEditor != nullptr) {
    m_tableColumnEditor->setEditable(model.canEdit);
    m_tableColumnEditor->setColumns(model.columns);
}
```

In `tableProperties()`:

```cpp
if (m_tableColumnEditor != nullptr) {
    model.columns = m_tableColumnEditor->columns();
}
```

- [x] **Step 6: Connect signals**

In `connectPropertySignals()`:

```cpp
connect(m_tableColumnEditor, &TableColumnEditorPanel::columnsEdited, this, [this] {
    if (!m_settingTableProperties) {
        if (m_tableColumnsEdit != nullptr && m_tableColumnEditor != nullptr) {
            const QSignalBlocker blocker(m_tableColumnsEdit);
            m_tableColumnsEdit->setText(TableColumnTextCodec::format(
                tableColumnsFromDesignerModels(m_tableColumnEditor->columns())));
        }
        emit tablePropertiesEdited(ControlAutoApplyDelayMs);
    }
});
```

- [x] **Step 7: Run GREEN**

Run the command from Step 2.

Expected: test passes.

- [x] **Step 8: Commit**

```powershell
git add include/sleekpr/app/TemplateInspectorPanel.h src/app/TemplateInspectorPanel.cpp tests/tst_app.cpp
git commit -m "接入表格列编辑面板"
```

## Task 5: Designer Window Auto-Apply and Preview Regression

**Files:**
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Modify: `tests/tst_app.cpp`

- [x] **Step 1: Write failing designer integration test**

Add declaration:

```cpp
void templateDesignerWindowAutoAppliesTableColumnEditorChanges();
```

Add body:

```cpp
void AppTests::templateDesignerWindowAutoAppliesTableColumnEditorChanges()
{
    using sleekpr::app::TableColumnEditorPanel;

    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addTableButton = window.findChild<QPushButton*>(QStringLiteral("designerAddTableButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* editor = window.findChild<TableColumnEditorPanel*>(QStringLiteral("tableColumnEditorPanel"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addTableButton != nullptr);
    QVERIFY(elementList != nullptr);
    QVERIFY(editor != nullptr);

    addLayerButton->click();
    addTableButton->click();
    QVERIFY(elementList->currentItem() != nullptr);

    QSignalSpy modelResetSpy(elementList->model(), &QAbstractItemModel::modelReset);
    auto columns = editor->columns();
    QVERIFY(!columns.isEmpty());
    columns.first().title = QString::fromUtf8("货号");
    columns.first().fieldKey = QStringLiteral("sku");
    editor->setColumns(columns);
    emit editor->columnsEdited();

    QTRY_COMPARE(
        changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().tables.first().columns.first().fieldKey,
        QStringLiteral("sku"));
    QCOMPARE(modelResetSpy.count(), 0);
}
```

- [x] **Step 2: Run RED**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_app_tests.exe templateDesignerWindowAutoAppliesTableColumnEditorChanges'
```

Expected: fails until inspector integration and command apply structured columns correctly.

- [x] **Step 3: Adjust lightweight table apply**

In `TemplateDesignerWindow::applyCurrentTableProperties(bool lightweightRefresh)`, ensure it reads `m_inspectorPanel->tableProperties()` after the column editor has updated its model. Keep existing behavior:

```cpp
if (lightweightRefresh) {
    schedulePreviewRefresh(kPreviewRefreshDelayMs);
} else {
    refreshAll();
    selectElement(tableId);
}
notifySettingsChanged();
```

No full `refreshAll()` should happen for column editor auto-apply.

- [x] **Step 4: Run GREEN**

Run command from Step 2.

Expected: test passes.

- [x] **Step 5: Commit**

```powershell
git add src/app/TemplateDesignerWindow.cpp tests/tst_app.cpp
git commit -m "联通表格列编辑自动应用"
```

## Task 6: Persistence and Rendering Regression

**Files:**
- Modify: `tests/tst_app.cpp`

- [x] **Step 1: Write persistence/preview regression test**

Add declaration:

```cpp
void templateDesignerWindowTableColumnEditorAffectsPreviewCommands();
```

Add body:

```cpp
void AppTests::templateDesignerWindowTableColumnEditorAffectsPreviewCommands()
{
    using sleekpr::app::TableColumnEditorPanel;

    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addTableButton = window.findChild<QPushButton*>(QStringLiteral("designerAddTableButton"));
    auto* editor = window.findChild<TableColumnEditorPanel*>(QStringLiteral("tableColumnEditorPanel"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }

    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addTableButton != nullptr);
    QVERIFY(editor != nullptr);
    QVERIFY(previewLabel != nullptr);

    addLayerButton->click();
    addTableButton->click();
    QVERIFY(!previewLabel->pixmap().isNull());
    const auto beforePreview = imageBytes(previewLabel->pixmap());

    auto columns = editor->columns();
    QVERIFY(!columns.isEmpty());
    columns.first().title = QString::fromUtf8("测试列");
    columns.first().fieldKey = QStringLiteral("productName");
    columns.first().widthMm = columns.first().widthMm + 8.0;
    editor->setColumns(columns);
    emit editor->columnsEdited();

    QTRY_VERIFY(imageBytes(previewLabel->pixmap()) != beforePreview);
    QTRY_COMPARE(
        changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().tables.first().columns.first().title,
        QString::fromUtf8("测试列"));
}
```

- [x] **Step 2: Run regression test**

Run targeted test. This is a regression test for the completed integration path; it is acceptable for the first run to pass if Task 5 already implemented the behavior.

- [x] **Step 3: Diagnose any regression failure**

When the test fails on persisted column data, inspect `TemplateInspectorPanel::tableProperties()` and verify it assigns `model.columns = m_tableColumnEditor->columns();`.

When the test fails on preview update, inspect `TemplateInspectorPanel::connectPropertySignals()` and verify `TableColumnEditorPanel::columnsEdited` emits `tablePropertiesEdited(ControlAutoApplyDelayMs)`.

When the test fails on advanced text synchronization, update `m_tableColumnsEdit` with `TableColumnTextCodec::format(tableColumnsFromDesignerModels(m_tableColumnEditor->columns()))` under `QSignalBlocker`.

- [x] **Step 4: Run GREEN**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\sleekpr_app_tests.exe templateDesignerWindowTableColumnEditorAffectsPreviewCommands'
```

Expected: test passes.

- [x] **Step 5: Commit**

```powershell
git add tests/tst_app.cpp src/app/TemplateInspectorPanel.cpp src/app/TemplateDesignerPresenter.cpp src/app/TemplateDesignerCommand.cpp
git commit -m "补充表格列编辑预览回归"
```

## Task 7: Full Verification

**Files:**
- No source edits expected.

- [x] **Step 1: Run diff check**

```powershell
git diff --check
```

Expected: no whitespace errors. CRLF warnings are acceptable in this repository.

- [x] **Step 2: Build test targets and run CTest**

If `build\sleekpr.exe` is running, do not run full `cmake --build build`; link will fail with `LNK1168`. Build test targets directly:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests sleekpr_app_tests sleekpr_http_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected: `100% tests passed, 0 tests failed out of 3`.

- [x] **Step 3: Inspect final status**

```powershell
git status --short
git log --oneline -5
```

Expected: clean worktree after the final commit, with feature commits above the spec commit.

- [x] **Step 4: Report outcome**

Summarize:

- New right-side table column editor.
- Structured column model and legacy text compatibility.
- Tests run and result.
- Any remaining known limitations: no canvas column drag, no auto row height, no summary rows.

## Completion Notes

- 已完成右侧表格列配置编辑器、结构化列模型、旧列文本兼容和预览回归验证。
- 合并前补强已完成：最后一列删除保护、重置确认弹窗、复制和新增列的唯一 `columnId` 生成。
- 验证命令：`git diff --check` 通过，仅有 CRLF 提示；`ctest --test-dir build --output-on-failure` 结果为 3/3 tests passed。
- 当前阶段仍不包含：画布拖拽列宽、自动行高、多行单元格、汇总/分组/页脚行、合并单元格和单元格级样式覆盖。
