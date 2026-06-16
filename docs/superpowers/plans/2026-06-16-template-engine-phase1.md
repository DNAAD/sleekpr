# Generic Print Template Engine Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first generic template-engine foundation: reusable paper specs, template document paper metadata, and a file-backed template library.

**Architecture:** Keep the new foundation in `core/templates` so preview, HTTP, and future UI can all reuse it. Persist structured data through focused JSON serializers and QSaveFile-backed stores. Do not change the existing `/print/tag` behavior in this phase.

**Tech Stack:** C++20, Qt Core JSON/file APIs, Qt Test, existing CMake targets.

---

## File Structure

- `include/sleekpr/core/templates/PaperSpec.h`: paper size, orientation, kind, and optional multi-label grid model.
- `include/sleekpr/core/templates/PaperSpecJson.h`: paper spec JSON serializer API.
- `src/core/templates/PaperSpecJson.cpp`: paper spec JSON implementation and validation.
- `include/sleekpr/core/templates/TemplateLibraryStore.h`: file-backed template library API.
- `src/core/templates/TemplateLibraryStore.cpp`: QSaveFile-backed template save/load/list/delete.
- `include/sleekpr/core/templates/TemplateDocument.h`: add `category` and `paperSpecId`.
- `src/core/templates/TemplateDocumentJson.cpp`: persist and import-validate new template metadata.
- `tests/tst_core.cpp`: TDD coverage for all new core behavior.
- `CMakeLists.txt`: add new core implementation files.

---

### Task 1: PaperSpec Model And JSON

**Files:**
- Create: `include/sleekpr/core/templates/PaperSpec.h`
- Create: `include/sleekpr/core/templates/PaperSpecJson.h`
- Create: `src/core/templates/PaperSpecJson.cpp`
- Modify: `CMakeLists.txt`
- Modify/Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write the failing test**

Add slots in `tests/tst_core.cpp`:

```cpp
void paperSpecJsonPersistsLabelAndSheetGrid();
void paperSpecJsonRejectsInvalidDimensions();
```

Add tests:

```cpp
void CoreTests::paperSpecJsonPersistsLabelAndSheetGrid()
{
    PaperSpec spec;
    spec.id = QStringLiteral("a4-2x5");
    spec.name = QString::fromUtf8("A4 双列标签");
    spec.kind = PaperSpecKind::Sheet;
    spec.orientation = PaperOrientation::Portrait;
    spec.widthMm = 210.0;
    spec.heightMm = 297.0;
    spec.marginLeftMm = 5.0;
    spec.marginTopMm = 6.0;
    spec.marginRightMm = 5.0;
    spec.marginBottomMm = 6.0;
    spec.defaultDpi = 300.0;
    spec.labelGrid.enabled = true;
    spec.labelGrid.rows = 5;
    spec.labelGrid.columns = 2;
    spec.labelGrid.horizontalGapMm = 3.0;
    spec.labelGrid.verticalGapMm = 4.0;

    const auto json = PaperSpecJson::toJson(spec);
    QString errorMessage;
    QVERIFY(PaperSpecJson::validate(json, &errorMessage));
    const auto actual = PaperSpecJson::fromJson(json);

    QCOMPARE(actual.id, QString("a4-2x5"));
    QCOMPARE(actual.kind, PaperSpecKind::Sheet);
    QCOMPARE(actual.orientation, PaperOrientation::Portrait);
    QCOMPARE(actual.widthMm, 210.0);
    QCOMPARE(actual.heightMm, 297.0);
    QVERIFY(actual.labelGrid.enabled);
    QCOMPARE(actual.labelGrid.rows, 5);
    QCOMPARE(actual.labelGrid.columns, 2);
    QCOMPARE(actual.labelGrid.horizontalGapMm, 3.0);
    QCOMPARE(actual.labelGrid.verticalGapMm, 4.0);
}

void CoreTests::paperSpecJsonRejectsInvalidDimensions()
{
    QJsonObject json;
    json["schemaVersion"] = 1;
    json["id"] = QStringLiteral("bad-paper");
    json["name"] = QString::fromUtf8("错误纸张");
    json["kind"] = QStringLiteral("label");
    json["orientation"] = QStringLiteral("portrait");
    json["widthMm"] = 0.0;
    json["heightMm"] = 30.0;

    QString errorMessage;
    QVERIFY(!PaperSpecJson::validate(json, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("宽高")));
}
```

- [ ] **Step 2: Run the red test**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests'
```

Expected: compile failure because `PaperSpec`, `PaperSpecJson`, `PaperSpecKind`, and `PaperOrientation` do not exist.

- [ ] **Step 3: Add PaperSpec model**

Create `include/sleekpr/core/templates/PaperSpec.h`:

```cpp
#pragma once

#include <QString>

namespace sleekpr::core {

enum class PaperSpecKind
{
    Label,
    Roll,
    Sheet,
};

enum class PaperOrientation
{
    Portrait,
    Landscape,
};

struct LabelGridSpec
{
    // 多标签纸启用后，同一页会按行列切分成多个标签单元。
    bool enabled = false;
    int rows = 1;
    int columns = 1;
    double horizontalGapMm = 0.0;
    double verticalGapMm = 0.0;
};

struct PaperSpec
{
    // schemaVersion 用于后续拒绝未知纸张结构。
    int schemaVersion = 1;
    QString id;
    QString name;
    PaperSpecKind kind = PaperSpecKind::Label;
    PaperOrientation orientation = PaperOrientation::Portrait;
    double widthMm = 80.0;
    double heightMm = 30.0;
    double marginLeftMm = 0.0;
    double marginTopMm = 0.0;
    double marginRightMm = 0.0;
    double marginBottomMm = 0.0;
    double defaultDpi = 300.0;
    LabelGridSpec labelGrid;
};

} // 命名空间 sleekpr::core
```

- [ ] **Step 4: Add PaperSpecJson API**

Create `include/sleekpr/core/templates/PaperSpecJson.h`:

```cpp
#pragma once

#include "sleekpr/core/templates/PaperSpec.h"

#include <QJsonObject>
#include <QString>

namespace sleekpr::core {

class PaperSpecJson
{
public:
    static QJsonObject toJson(const PaperSpec& spec);
    static PaperSpec fromJson(const QJsonObject& json);
    static bool validate(const QJsonObject& json, QString* errorMessage = nullptr);
};

} // 命名空间 sleekpr::core
```

- [ ] **Step 5: Add PaperSpecJson implementation**

Create `src/core/templates/PaperSpecJson.cpp` with string conversions for `label`, `roll`, `sheet`, `portrait`, and `landscape`. `validate()` must reject unsupported schema versions, blank ids, non-positive width/height, non-positive DPI, and enabled grids with rows or columns below 1.

- [ ] **Step 6: Add CMake source**

Add `src/core/templates/PaperSpecJson.cpp` to `sleekpr_core`.

- [ ] **Step 7: Run green test**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure'
```

Expected: `sleekpr_core_tests` passes.

- [ ] **Step 8: Commit**

```powershell
git add CMakeLists.txt include/sleekpr/core/templates/PaperSpec.h include/sleekpr/core/templates/PaperSpecJson.h src/core/templates/PaperSpecJson.cpp tests/tst_core.cpp
git commit -m "feat: add paper spec json model"
```

---

### Task 2: TemplateDocument Paper Metadata

**Files:**
- Modify: `include/sleekpr/core/templates/TemplateDocument.h`
- Modify: `src/core/templates/TemplateDocumentJson.cpp`
- Modify/Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write the failing test**

Add slots:

```cpp
void templateDocumentJsonPersistsPaperMetadata();
void templateDocumentImportRequiresPaperSpecIdForGenericTemplates();
```

Add tests:

```cpp
void CoreTests::templateDocumentJsonPersistsPaperMetadata()
{
    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.name = QString::fromUtf8("主图层");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    element.text = QString::fromUtf8("销售单");
    layer.elements.append(element);
    document.layers.append(layer);

    const auto json = TemplateDocumentJson::toJson(document);
    QCOMPARE(json["category"].toString(), QString("document"));
    QCOMPARE(json["paperSpecId"].toString(), QString("a4"));

    const auto actual = TemplateDocumentJson::fromJson(json);
    QCOMPARE(actual.category, QString("document"));
    QCOMPARE(actual.paperSpecId, QString("a4"));
}

void CoreTests::templateDocumentImportRequiresPaperSpecIdForGenericTemplates()
{
    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    layer.elements.append(element);
    document.layers.append(layer);

    QString errorMessage;
    QVERIFY(!TemplateDocumentJson::validateForImport(TemplateDocumentJson::toJson(document), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("paperSpecId")));
}
```

- [ ] **Step 2: Run the red test**

Expected: compile failure because `TemplateDocument::category` and `paperSpecId` do not exist.

- [ ] **Step 3: Add metadata fields**

Add to `TemplateDocument`:

```cpp
// 模板分类决定设计器默认展示的工具集合，例如标签、单据或表格模板。
QString category = QStringLiteral("label");

// 纸张规格独立保存，模板通过 paperSpecId 引用它。
QString paperSpecId = QStringLiteral("label-80x30");
```

- [ ] **Step 4: Persist metadata**

In `TemplateDocumentJson::toJson()`, write `category` and `paperSpecId`. In `fromJson()`, read them with defaults `label` and `label-80x30`.

- [ ] **Step 5: Validate metadata**

In `validateForImport()`, reject blank `paperSpecId` when `category` is not `label`. For old label templates, blank `paperSpecId` remains import-compatible and defaults during `fromJson()`.

- [ ] **Step 6: Run green test**

Run the same `sleekpr_core_tests` command as Task 1.

- [ ] **Step 7: Commit**

```powershell
git add include/sleekpr/core/templates/TemplateDocument.h src/core/templates/TemplateDocumentJson.cpp tests/tst_core.cpp
git commit -m "feat: add template paper metadata"
```

---

### Task 3: File-Backed Template Library Store

**Files:**
- Create: `include/sleekpr/core/templates/TemplateLibraryStore.h`
- Create: `src/core/templates/TemplateLibraryStore.cpp`
- Modify: `CMakeLists.txt`
- Modify/Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write the failing test**

Add slots:

```cpp
void templateLibraryStoreSavesListsAndLoadsTemplates();
void templateLibraryStoreRejectsInvalidTemplateImport();
```

Add tests:

```cpp
void CoreTests::templateLibraryStoreSavesListsAndLoadsTemplates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    layer.elements.append(element);
    document.layers.append(layer);

    TemplateLibraryStore store(dir.path());
    QVERIFY(store.saveTemplate(document));

    const auto ids = store.templateIds();
    QCOMPARE(ids, QStringList{QStringLiteral("sale-order")});

    const auto loaded = store.loadTemplate(QStringLiteral("sale-order"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->id, QString("sale-order"));
    QCOMPARE(loaded->paperSpecId, QString("a4"));
}

void CoreTests::templateLibraryStoreRejectsInvalidTemplateImport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(QDir(dir.path()).filePath(QStringLiteral("broken.sleekpr-template.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({"schemaVersion":1,"id":"broken","templateKey":"broken","layers":[]})json");
    file.close();

    QString errorMessage;
    TemplateLibraryStore store(dir.path());
    QVERIFY(!store.loadTemplate(QStringLiteral("broken"), &errorMessage).has_value());
    QVERIFY(errorMessage.contains(QString::fromUtf8("图层")));
}
```

- [ ] **Step 2: Run the red test**

Expected: compile failure because `TemplateLibraryStore` does not exist.

- [ ] **Step 3: Add store API**

Create `include/sleekpr/core/templates/TemplateLibraryStore.h`:

```cpp
#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace sleekpr::core {

class TemplateLibraryStore
{
public:
    explicit TemplateLibraryStore(QString templateDirectoryPath);

    QStringList templateIds() const;
    std::optional<TemplateDocument> loadTemplate(const QString& templateId, QString* errorMessage = nullptr) const;
    bool saveTemplate(const TemplateDocument& document, QString* errorMessage = nullptr) const;
    bool removeTemplate(const QString& templateId) const;

private:
    QString templatePath(const QString& templateId) const;

    QString m_templateDirectoryPath;
};

} // 命名空间 sleekpr::core
```

- [ ] **Step 4: Add store implementation**

Create `src/core/templates/TemplateLibraryStore.cpp`. Save files as `<templateId>.sleekpr-template.json` using `QSaveFile`. `templateIds()` lists that suffix, sorts ids, and strips suffix. `loadTemplate()` validates JSON through `TemplateDocumentJson::validateForImport()` before returning the document.

- [ ] **Step 5: Add CMake source**

Add `src/core/templates/TemplateLibraryStore.cpp` to `sleekpr_core`.

- [ ] **Step 6: Run green test**

Run the same `sleekpr_core_tests` command as Task 1.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt include/sleekpr/core/templates/TemplateLibraryStore.h src/core/templates/TemplateLibraryStore.cpp tests/tst_core.cpp
git commit -m "feat: add file template library store"
```

---

### Task 4: Full Verification

**Files:**
- Test only.

- [ ] **Step 1: Run full verification**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected:

```text
100% tests passed, 0 tests failed out of 3
```

- [ ] **Step 2: Check final diff**

```powershell
git status --short
git log --oneline -5
```

Expected: only pre-existing unrelated `README.md` and `docs/superpowers/plans/2026-06-15-settings-window.md` remain dirty.
