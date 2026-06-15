# Template Designer Phase 3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Qt-native complete template designer with layers, import/export, version snapshots, device profiles, and printer DPI calibration while preserving existing phase two settings compatibility.

**Architecture:** Add a core template-document model and renderer, then connect it to a new `TemplateDesignerWindow`. Existing `SettingsWindow` remains a lightweight settings surface and opens the full designer as a separate window. Preview and print paths resolve a device profile by printer name and apply profile DPI, scale, and offset without moving UI or device logic into label business planning.

**Tech Stack:** C++20, Qt 6 Widgets/Core/PrintSupport/Test, existing CMake/Ninja project, Qt Test.

---

## File Structure

- Create `include/sleekpr/core/templates/TemplateDocument.h`: document, layer, version, and device profile structs.
- Create `include/sleekpr/core/templates/TemplateDocumentJson.h` and `src/core/templates/TemplateDocumentJson.cpp`: JSON import/export helpers.
- Create `include/sleekpr/core/templates/TemplateDocumentEditModel.h` and `src/core/templates/TemplateDocumentEditModel.cpp`: layer/element/version mutation logic.
- Create `include/sleekpr/core/templates/TemplateDocumentFactory.h` and `src/core/templates/TemplateDocumentFactory.cpp`: bootstrap a full editable template document from the existing native drawing plan.
- Create `include/sleekpr/core/templates/DeviceProfileResolver.h` and `src/core/templates/DeviceProfileResolver.cpp`: printer profile lookup and default profile fallback.
- Create `include/sleekpr/core/templates/TemplateDocumentRenderer.h` and `src/core/templates/TemplateDocumentRenderer.cpp`: converts visible template layers into `NativeDrawCommand`.
- Modify `include/sleekpr/core/settings/PrintClientSettings.h`: add `templateDocuments`.
- Modify `src/core/settings/PrintClientSettingsJson.cpp`: serialize and deserialize `templateDocuments`.
- Modify `include/sleekpr/infrastructure/preview/LabelPreviewService.h` and `src/infrastructure/preview/LabelPreviewService.cpp`: render previews from template documents and profiles.
- Modify `include/sleekpr/infrastructure/printing/QtLabelPrintEngine.h` and `src/infrastructure/printing/QtLabelPrintEngine.cpp`: accept render DPI and scale/offset data through drawing plan or render options.
- Modify `src/http/LocalHttpRouter.cpp`: resolve profile for `/print/tag`.
- Create `include/sleekpr/app/TemplateDesignerWindow.h` and `src/app/TemplateDesignerWindow.cpp`: Qt-native full template designer.
- Modify `include/sleekpr/app/SettingsWindow.h` and `src/app/SettingsWindow.cpp`: add a button that opens the designer.
- Modify `src/app/main.cpp`: own and show the designer window from the settings window callback.
- Modify `CMakeLists.txt` and `tests/CMakeLists.txt`: add new sources to app, core, and app tests.
- Modify `tests/tst_core.cpp`, `tests/tst_app.cpp`, and `tests/tst_http.cpp`: add coverage for model, JSON, renderer, UI entry points, and print profile integration.

## Commands

Use this full verification command after each task that touches production code:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

If `sleekpr.exe` is running and link fails with `LNK1168`, stop only that process:

```powershell
Get-Process sleekpr -ErrorAction SilentlyContinue | Stop-Process -Force
```

---

### Task 1: Template Document Model and JSON

**Files:**
- Create: `include/sleekpr/core/templates/TemplateDocument.h`
- Create: `include/sleekpr/core/templates/TemplateDocumentJson.h`
- Create: `src/core/templates/TemplateDocumentJson.cpp`
- Modify: `include/sleekpr/core/settings/PrintClientSettings.h`
- Modify: `src/core/settings/PrintClientSettingsJson.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write the failing JSON round-trip test**

Add includes to `tests/tst_core.cpp`:

```cpp
#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"

#include <QJsonArray>
#include <QJsonObject>
```

Add test slots:

```cpp
void templateDocumentJsonPersistsLayersVersionsAndProfiles();
void templateDocumentJsonRejectsInvalidImportDocument();
void settingsStorePersistsTemplateDocuments();
```

Add test body:

```cpp
void CoreTests::templateDocumentJsonPersistsLayersVersionsAndProfiles()
{
    TemplateElement fixedText;
    fixedText.id = "element-title";
    fixedText.type = TemplateElementType::FixedText;
    fixedText.text = "TITLE";
    fixedText.x = 3.0;
    fixedText.y = 4.0;
    fixedText.width = 20.0;
    fixedText.height = 3.0;
    fixedText.rotationDegrees = 90.0;
    fixedText.maxLines = 2;
    fixedText.ellipsis = true;

    TemplateLayer layer;
    layer.id = "layer-main";
    layer.name = "Main";
    layer.visible = true;
    layer.locked = false;
    layer.elements = {fixedText};

    TemplateVersion version;
    version.id = "version-1";
    version.name = "Initial";
    version.createdAt = "2026-06-15T10:00:00+08:00";
    version.note = "baseline";
    version.layersSnapshot = {layer};

    DeviceProfile profile;
    profile.id = "profile-default";
    profile.printerName = "";
    profile.dpi = 300.0;
    profile.scaleX = 1.01;
    profile.scaleY = 0.99;
    profile.offsetX = 0.2;
    profile.offsetY = -0.1;

    TemplateDocument expected;
    expected.id = "template-default";
    expected.name = "Default";
    expected.templateKey = "default";
    expected.schemaVersion = 1;
    expected.activeVersionId = "version-1";
    expected.layers = {layer};
    expected.versions = {version};
    expected.deviceProfiles = {profile};

    const auto json = TemplateDocumentJson::toJson(expected);
    const auto actual = TemplateDocumentJson::fromJson(json);

    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.layers.size(), 1);
    QCOMPARE(actual.layers.first().elements.first().id, QString("element-title"));
    QCOMPARE(actual.layers.first().elements.first().rotationDegrees, 90.0);
    QCOMPARE(actual.layers.first().elements.first().maxLines, 2);
    QCOMPARE(actual.layers.first().elements.first().ellipsis, true);
    QCOMPARE(actual.versions.size(), 1);
    QCOMPARE(actual.versions.first().layersSnapshot.first().id, QString("layer-main"));
    QCOMPARE(actual.deviceProfiles.size(), 1);
    QCOMPARE(actual.deviceProfiles.first().scaleX, 1.01);
}
```

Add import validation test:

```cpp
void CoreTests::templateDocumentJsonRejectsInvalidImportDocument()
{
    QJsonObject duplicateLayer;
    duplicateLayer["schemaVersion"] = 1;
    duplicateLayer["id"] = "template-invalid";
    duplicateLayer["templateKey"] = "default";

    QJsonArray layers;
    layers.append(QJsonObject{{"id", "same"}, {"name", "A"}});
    layers.append(QJsonObject{{"id", "same"}, {"name", "B"}});
    duplicateLayer["layers"] = layers;

    QString errorMessage;
    QVERIFY(!TemplateDocumentJson::validateForImport(duplicateLayer, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("图层")));
}
```

Add settings round-trip test:

```cpp
void CoreTests::settingsStorePersistsTemplateDocuments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TemplateElement element;
    element.id = "title";
    element.type = TemplateElementType::FixedText;
    element.text = "TITLE";

    TemplateLayer layer;
    layer.id = "layer-main";
    layer.elements = {element};

    TemplateDocument document;
    document.id = "template-default";
    document.templateKey = "default";
    document.layers = {layer};

    PrintClientSettings expected;
    expected.templateDocuments["default"] = document;

    FileSettingsStore store(dir.filePath("settings.json"));
    QVERIFY(store.save(expected));

    const auto actual = store.load();
    QVERIFY(actual.templateDocuments.contains("default"));
    QCOMPARE(actual.templateDocuments["default"].layers.first().elements.first().id, QString("title"));
}
```

- [ ] **Step 2: Run test and verify it fails because types do not exist**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build'
```

Expected: compile fails on missing `TemplateDocument.h` or missing `TemplateDocumentJson`.

- [ ] **Step 3: Add the core model**

Create `include/sleekpr/core/templates/TemplateDocument.h`:

```cpp
#pragma once

#include "sleekpr/core/settings/TemplateElement.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

struct TemplateLayer
{
    // 图层稳定标识，版本快照和元素归属都依赖它。
    QString id;
    QString name;
    bool visible = true;
    bool locked = false;
    QList<TemplateElement> elements;
};

struct TemplateVersion
{
    // 版本只保存版式快照，不保存设备 profile，避免恢复版式时覆盖设备校准。
    QString id;
    QString name;
    QString createdAt;
    QString note;
    QList<TemplateLayer> layersSnapshot;
};

struct DeviceProfile
{
    // printerName 为空表示默认 profile。
    QString id;
    QString printerName;
    double dpi = 300.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double offsetX = 0.0;
    double offsetY = 0.0;
    QString notes;
};

struct TemplateDocument
{
    // schemaVersion 用于后续导入时拒绝未知结构。
    int schemaVersion = 1;
    QString id;
    QString name;
    QString templateKey = QStringLiteral("default");
    QString activeVersionId;
    QList<TemplateLayer> layers;
    QList<TemplateVersion> versions;
    QList<DeviceProfile> deviceProfiles;
};

} // namespace sleekpr::core
```

- [ ] **Step 4: Extend `TemplateElement`**

Modify `include/sleekpr/core/settings/TemplateElement.h` by adding fields:

```cpp
    // 所属图层；为空时表示来自旧版 templateElements 列表。
    QString layerId;

    // 图层内排序值，数值越小越先绘制。
    int zIndex = 0;

    // 元素级显示和锁定状态，图层锁定时优先生效。
    bool visible = true;
    bool locked = false;

    // 文本旋转角度，现有竖排编号依赖该字段保持版式。
    double rotationDegrees = 0.0;

    // 文本最大行数，0 表示不限制。
    int maxLines = 0;

    // 文本超出区域时是否使用省略号。
    bool ellipsis = false;
```

- [ ] **Step 5: Add JSON helper declarations**

Create `include/sleekpr/core/templates/TemplateDocumentJson.h`:

```cpp
#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QJsonObject>
#include <QString>

namespace sleekpr::core {

class TemplateDocumentJson
{
public:
    // 模板 JSON 是导入导出和 settings.json 的共同格式，集中维护可减少字段漂移。
    static QJsonObject toJson(const TemplateDocument& document);
    static TemplateDocument fromJson(const QJsonObject& json);
    static bool validateForImport(const QJsonObject& json, QString* errorMessage);
};

} // namespace sleekpr::core
```

- [ ] **Step 6: Implement JSON helper**

Create `src/core/templates/TemplateDocumentJson.cpp`. Include functions for element, layer, version, and profile mapping. Use the same type strings as phase two: `fixedText`, `boundField`, `qrCode`, `rectangle`. Element JSON must persist `layerId`, `zIndex`, `visible`, `locked`, `rotationDegrees`, `maxLines`, and `ellipsis`. Clamp unsafe profile values:

```cpp
namespace {
double positiveOrDefault(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}
}
```

When reading:

```cpp
profile.dpi = positiveOrDefault(json["dpi"].toDouble(300.0), 300.0);
profile.scaleX = positiveOrDefault(json["scaleX"].toDouble(1.0), 1.0);
profile.scaleY = positiveOrDefault(json["scaleY"].toDouble(1.0), 1.0);
```

`validateForImport` must reject:

- missing `schemaVersion` or `schemaVersion > 1`;
- duplicate layer ids;
- duplicate element ids across all layers;
- unknown element type strings.

Return `false` and fill a Chinese `errorMessage` when validation fails. Keep `fromJson` tolerant for `settings.json` backward compatibility; the import path calls `validateForImport` before `fromJson`.

- [ ] **Step 7: Add `templateDocuments` to settings**

Modify `include/sleekpr/core/settings/PrintClientSettings.h`:

```cpp
#include "sleekpr/core/templates/TemplateDocument.h"
```

Add field:

```cpp
    // 完整模板设计器文档：第一层是模板名，例如 default 或 silver。
    QHash<QString, TemplateDocument> templateDocuments;
```

Modify `src/core/settings/PrintClientSettingsJson.cpp`:

```cpp
#include "sleekpr/core/templates/TemplateDocumentJson.h"
```

Serialize under root key `templateDocuments` and deserialize missing key as empty.

- [ ] **Step 8: Add sources to CMake**

Modify `CMakeLists.txt` under `add_library(sleekpr_core ...)`:

```cmake
    src/core/templates/TemplateDocumentJson.cpp
```

- [ ] **Step 9: Run full verification**

Run the full verification command. Expected: all existing tests plus the new core JSON tests pass.

- [ ] **Step 10: Commit**

```powershell
git add include/sleekpr/core/templates/TemplateDocument.h include/sleekpr/core/templates/TemplateDocumentJson.h src/core/templates/TemplateDocumentJson.cpp include/sleekpr/core/settings/TemplateElement.h include/sleekpr/core/settings/PrintClientSettings.h src/core/settings/PrintClientSettingsJson.cpp CMakeLists.txt tests/tst_core.cpp
git commit -m "feat: add template document serialization"
```

---

### Task 2: Template Edit Model for Layers and Versions

**Files:**
- Create: `include/sleekpr/core/templates/TemplateDocumentEditModel.h`
- Create: `src/core/templates/TemplateDocumentEditModel.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing edit model tests**

Add include:

```cpp
#include "sleekpr/core/templates/TemplateDocumentEditModel.h"
```

Add slots:

```cpp
void templateDocumentEditModelManagesLayers();
void templateDocumentEditModelReordersElements();
void templateDocumentEditModelRestoresVersionsWithoutProfiles();
void templateDocumentEditModelRejectsLockedLayerElementMove();
```

Add test for layers:

```cpp
void CoreTests::templateDocumentEditModelManagesLayers()
{
    TemplateDocument document;
    TemplateDocumentEditModel::addLayer(document, "base", "Base");
    TemplateDocumentEditModel::addLayer(document, "foreground", "Foreground");

    QCOMPARE(document.layers.size(), 2);
    QCOMPARE(document.layers[0].id, QString("base"));

    QVERIFY(TemplateDocumentEditModel::moveLayerDown(document, "base"));
    QCOMPARE(document.layers[1].id, QString("base"));

    QVERIFY(TemplateDocumentEditModel::setLayerVisible(document, "base", false));
    QCOMPARE(document.layers[1].visible, false);
}
```

Add element reorder test:

```cpp
void CoreTests::templateDocumentEditModelReordersElements()
{
    TemplateLayer layer;
    layer.id = "layer-main";

    TemplateElement first;
    first.id = "first";
    first.zIndex = 0;
    TemplateElement second;
    second.id = "second";
    second.zIndex = 1;
    layer.elements = {first, second};

    TemplateDocument document;
    document.layers = {layer};

    QVERIFY(TemplateDocumentEditModel::moveElementUp(document, "second"));
    QCOMPARE(document.layers.first().elements[0].id, QString("second"));
    QCOMPARE(document.layers.first().elements[0].zIndex, 0);
    QCOMPARE(document.layers.first().elements[1].zIndex, 1);
}
```

Add version restore test:

```cpp
void CoreTests::templateDocumentEditModelRestoresVersionsWithoutProfiles()
{
    TemplateDocument document;
    TemplateLayer layer;
    layer.id = "layer-old";
    document.layers = {layer};

    DeviceProfile profile;
    profile.id = "profile-printer";
    profile.printerName = "Printer A";
    profile.offsetX = 1.2;
    document.deviceProfiles = {profile};

    TemplateLayer snapshotLayer;
    snapshotLayer.id = "layer-snapshot";
    TemplateVersion version;
    version.id = "version-1";
    version.layersSnapshot = {snapshotLayer};
    document.versions = {version};

    QVERIFY(TemplateDocumentEditModel::restoreVersion(document, "version-1"));
    QCOMPARE(document.layers.first().id, QString("layer-snapshot"));
    QCOMPARE(document.deviceProfiles.first().printerName, QString("Printer A"));
}
```

Add locked layer move test:

```cpp
void CoreTests::templateDocumentEditModelRejectsLockedLayerElementMove()
{
    TemplateElement element;
    element.id = "locked-element";
    element.x = 1.0;
    element.y = 2.0;

    TemplateLayer layer;
    layer.id = "locked-layer";
    layer.locked = true;
    layer.elements = {element};

    TemplateDocument document;
    document.layers = {layer};

    QVERIFY(!TemplateDocumentEditModel::moveElement(document, "locked-element", 8.0, 9.0));
    QCOMPARE(document.layers.first().elements.first().x, 1.0);
    QCOMPARE(document.layers.first().elements.first().y, 2.0);
}
```

- [ ] **Step 2: Run test and verify it fails because edit model is missing**

Run build. Expected: compile fails on missing `TemplateDocumentEditModel.h`.

- [ ] **Step 3: Add edit model header**

Create `include/sleekpr/core/templates/TemplateDocumentEditModel.h`:

```cpp
#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QString>

namespace sleekpr::core {

class TemplateDocumentEditModel
{
public:
    static bool addLayer(TemplateDocument& document, const QString& id, const QString& name);
    static bool deleteLayer(TemplateDocument& document, const QString& layerId);
    static bool moveLayerUp(TemplateDocument& document, const QString& layerId);
    static bool moveLayerDown(TemplateDocument& document, const QString& layerId);
    static bool setLayerVisible(TemplateDocument& document, const QString& layerId, bool visible);
    static bool setLayerLocked(TemplateDocument& document, const QString& layerId, bool locked);
    static bool addElement(TemplateDocument& document, const QString& layerId, const TemplateElement& element);
    static bool deleteElement(TemplateDocument& document, const QString& elementId);
    static bool moveElement(TemplateDocument& document, const QString& elementId, double x, double y);
    static bool moveElementUp(TemplateDocument& document, const QString& elementId);
    static bool moveElementDown(TemplateDocument& document, const QString& elementId);
    static bool setElementVisible(TemplateDocument& document, const QString& elementId, bool visible);
    static bool setElementLocked(TemplateDocument& document, const QString& elementId, bool locked);
    static bool saveVersion(TemplateDocument& document, const QString& versionId, const QString& name, const QString& createdAt, const QString& note);
    static bool restoreVersion(TemplateDocument& document, const QString& versionId);
};

} // namespace sleekpr::core
```

- [ ] **Step 4: Implement edit model**

Create `src/core/templates/TemplateDocumentEditModel.cpp`. Key rules:

- `addLayer` rejects duplicate ids.
- `deleteLayer` rejects locked layers.
- `deleteElement`, `moveElement`, `moveElementUp`, and `moveElementDown` reject locked elements and elements inside locked layers.
- `moveElementUp` and `moveElementDown` swap list order and then normalize `zIndex` to `0..n-1`.
- `restoreVersion` only replaces `document.layers` and `activeVersionId`.

Use a small helper:

```cpp
namespace {
int layerIndex(const TemplateDocument& document, const QString& layerId)
{
    for (int index = 0; index < document.layers.size(); ++index) {
        if (document.layers[index].id == layerId) {
            return index;
        }
    }
    return -1;
}
}
```

- [ ] **Step 5: Add source to CMake**

Add to `sleekpr_core`:

```cmake
    src/core/templates/TemplateDocumentEditModel.cpp
```

- [ ] **Step 6: Run full verification**

Expected: all tests pass.

- [ ] **Step 7: Commit**

```powershell
git add include/sleekpr/core/templates/TemplateDocumentEditModel.h src/core/templates/TemplateDocumentEditModel.cpp CMakeLists.txt tests/tst_core.cpp
git commit -m "feat: add template document edit model"
```

---

### Task 3: Document Bootstrap, Renderer, and Device Profile Resolver

**Files:**
- Create: `include/sleekpr/core/templates/TemplateDocumentFactory.h`
- Create: `src/core/templates/TemplateDocumentFactory.cpp`
- Create: `include/sleekpr/core/templates/DeviceProfileResolver.h`
- Create: `src/core/templates/DeviceProfileResolver.cpp`
- Create: `include/sleekpr/core/templates/TemplateDocumentRenderer.h`
- Create: `src/core/templates/TemplateDocumentRenderer.cpp`
- Modify: `src/core/native/NativeLabelDrawingPlanner.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/tst_core.cpp`

- [ ] **Step 1: Write failing renderer and profile tests**

Add includes:

```cpp
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/TemplateDocumentFactory.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"

#include <algorithm>
```

Add slots:

```cpp
void deviceProfileResolverUsesPrinterSpecificProfile();
void templateDocumentFactoryBootstrapsExistingDrawingPlan();
void templateDocumentRendererSkipsHiddenLayersAndSortsElements();
```

Add profile test:

```cpp
void CoreTests::deviceProfileResolverUsesPrinterSpecificProfile()
{
    DeviceProfile defaultProfile;
    defaultProfile.id = "default";
    defaultProfile.dpi = 300.0;

    DeviceProfile printerProfile;
    printerProfile.id = "printer-a";
    printerProfile.printerName = "Printer A";
    printerProfile.dpi = 203.0;
    printerProfile.scaleX = 1.02;

    const QList<DeviceProfile> profiles{defaultProfile, printerProfile};

    const auto matched = DeviceProfileResolver::resolve(profiles, "Printer A");
    QCOMPARE(matched.id, QString("printer-a"));
    QCOMPARE(matched.dpi, 203.0);

    const auto fallback = DeviceProfileResolver::resolve(profiles, "Printer B");
    QCOMPARE(fallback.id, QString("default"));
}
```

Add bootstrap test:

```cpp
void CoreTests::templateDocumentFactoryBootstrapsExistingDrawingPlan()
{
    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto baseDrawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan);

    const auto document = TemplateDocumentFactory::fromDrawingPlan(
        QStringLiteral("default"),
        QString::fromUtf8("默认标签"),
        baseDrawingPlan,
        QList<TemplateElement>{});

    QCOMPARE(document.templateKey, QString("default"));
    QCOMPARE(document.layers.size(), 1);
    QVERIFY(document.layers.first().elements.size() >= baseDrawingPlan.commands.size());
    QCOMPARE(document.layers.first().elements.first().zIndex, 0);

    const auto verticalIt = std::find_if(
        document.layers.first().elements.cbegin(),
        document.layers.first().elements.cend(),
        [](const TemplateElement& element) { return element.id == QString("verticalIdentifier"); });
    QVERIFY(verticalIt != document.layers.first().elements.cend());
    QCOMPARE(verticalIt->rotationDegrees, 90.0);
}
```

Add renderer test:

```cpp
void CoreTests::templateDocumentRendererSkipsHiddenLayersAndSortsElements()
{
    TemplateElement back;
    back.id = "back";
    back.type = TemplateElementType::Rectangle;
    back.zIndex = 2;
    back.width = 8.0;
    back.height = 3.0;

    TemplateElement front;
    front.id = "front";
    front.type = TemplateElementType::FixedText;
    front.text = "FRONT";
    front.zIndex = 1;
    front.width = 10.0;
    front.height = 2.0;

    TemplateLayer visibleLayer;
    visibleLayer.id = "visible";
    visibleLayer.visible = true;
    visibleLayer.elements = {back, front};

    TemplateLayer hiddenLayer;
    hiddenLayer.id = "hidden";
    hiddenLayer.visible = false;
    hiddenLayer.elements = {TemplateElement{.id = "hidden-text"}};

    TemplateDocument document;
    document.layers = {hiddenLayer, visibleLayer};

    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto drawingPlan = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{});

    QCOMPARE(drawingPlan.commands.size(), 2);
    QCOMPARE(drawingPlan.commands[0].elementKey, QString("front"));
    QCOMPARE(drawingPlan.commands[1].elementKey, QString("back"));
}
```

- [ ] **Step 2: Run tests and verify missing renderer/profile classes**

Run build. Expected: compile fails on missing headers.

- [ ] **Step 3: Add document factory**

Create `include/sleekpr/core/templates/TemplateDocumentFactory.h`:

```cpp
#pragma once

#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/templates/TemplateDocument.h"

namespace sleekpr::core {

class TemplateDocumentFactory
{
public:
    static TemplateDocument fromDrawingPlan(
        const QString& templateKey,
        const QString& name,
        const NativeLabelDrawingPlan& drawingPlan,
        const QList<TemplateElement>& customElements);
};

} // namespace sleekpr::core
```

Create `src/core/templates/TemplateDocumentFactory.cpp`. It builds one editable layer named `基础图层`, maps every existing `NativeDrawCommand` to a `TemplateElement`, then appends existing phase two `customElements` with larger `zIndex` values:

- `NativeDrawCommandType::Text` maps to `TemplateElementType::FixedText`;
- `NativeDrawCommandType::QrCode` maps to `TemplateElementType::QrCode`;
- `NativeDrawCommandType::Rectangle` maps to `TemplateElementType::Rectangle`;
- text commands with these element keys map to `TemplateElementType::BoundField`:
  `identifier -> identifierText`, `verticalIdentifier -> identifierText`, `productName -> productName`, `standardText -> standardText`, `addressText -> addressText`, `salesCode -> salesCode`, `priceText -> priceText`, `additionalPrice -> additionalPriceText`, `footerText -> footerText`;
- the `qrCode` command uses `TemplateElementType::QrCode` with `fieldKey = "qrPayload"` and empty `payload`, so QR content remains dynamic;
- use `command.elementKey` as element id when present, otherwise use `element-<index>`;
- copy `x/y/width/height/text/fontSizePt/bold/rotationDegrees/maxLines/ellipsis`;
- set `schemaVersion = 1`, `id = "template-" + templateKey`, and `templateKey`.

- [ ] **Step 4: Add profile resolver**

Create `include/sleekpr/core/templates/DeviceProfileResolver.h`:

```cpp
#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

namespace sleekpr::core {

class DeviceProfileResolver
{
public:
    // 优先匹配打印机名，找不到时回退 printerName 为空的默认 profile。
    static DeviceProfile resolve(const QList<DeviceProfile>& profiles, const QString& printerName);
};

} // namespace sleekpr::core
```

Create `src/core/templates/DeviceProfileResolver.cpp` with case-insensitive printer matching and a default `DeviceProfile{}` fallback.

- [ ] **Step 5: Add document renderer**

Create `include/sleekpr/core/templates/TemplateDocumentRenderer.h`:

```cpp
#pragma once

#include "sleekpr/core/labels/LabelRenderPlan.h"
#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/settings/LabelOffset.h"
#include "sleekpr/core/templates/TemplateDocument.h"

namespace sleekpr::core {

class TemplateDocumentRenderer
{
public:
    NativeLabelDrawingPlan render(
        const TemplateDocument& document,
        const LabelRenderPlan& labelPlan,
        const LabelOffset& labelOffset,
        const DeviceProfile& profile) const;
};

} // namespace sleekpr::core
```

Implementation rule:

- Flatten visible layers in list order.
- Skip hidden elements.
- Sort each layer by `zIndex`.
- Reuse `NativeLabelDrawingPlanner::appendTemplateElements(...)`.
- Update `appendTemplateElements` text command creation to pass `element.rotationDegrees`, `element.maxLines`, and `element.ellipsis`.
- Set `drawingPlan.paperSize = labelPlan.paperSize`.
- Apply combined offset `{labelOffset.x + profile.offsetX, labelOffset.y + profile.offsetY}`.
- Apply `scaleX/scaleY` to command position and size after generating commands.

- [ ] **Step 6: Add sources to CMake**

Add:

```cmake
    src/core/templates/DeviceProfileResolver.cpp
    src/core/templates/TemplateDocumentFactory.cpp
    src/core/templates/TemplateDocumentRenderer.cpp
```

- [ ] **Step 7: Run full verification**

Expected: all tests pass.

- [ ] **Step 8: Commit**

```powershell
git add include/sleekpr/core/templates/TemplateDocumentFactory.h src/core/templates/TemplateDocumentFactory.cpp include/sleekpr/core/templates/DeviceProfileResolver.h src/core/templates/DeviceProfileResolver.cpp include/sleekpr/core/templates/TemplateDocumentRenderer.h src/core/templates/TemplateDocumentRenderer.cpp src/core/native/NativeLabelDrawingPlanner.cpp CMakeLists.txt tests/tst_core.cpp
git commit -m "feat: render template documents with device profiles"
```

---

### Task 4: Preview and Print Integration

**Files:**
- Modify: `include/sleekpr/infrastructure/preview/LabelPreviewService.h`
- Modify: `src/infrastructure/preview/LabelPreviewService.cpp`
- Modify: `include/sleekpr/core/native/NativeLabelDrawingPlan.h`
- Modify: `src/core/templates/TemplateDocumentRenderer.cpp`
- Modify: `include/sleekpr/infrastructure/printing/QtLabelPrintEngine.h`
- Modify: `src/infrastructure/printing/QtLabelPrintEngine.cpp`
- Modify: `src/http/LocalHttpRouter.cpp`
- Test: `tests/tst_core.cpp`
- Test: `tests/tst_http.cpp`

- [ ] **Step 1: Write failing preview test**

Add core test:

```cpp
void CoreTests::labelPreviewServiceUsesTemplateDocumentDeviceProfile()
{
    TemplateElement element;
    element.id = "custom-profile-text";
    element.type = TemplateElementType::FixedText;
    element.text = "PROFILE";
    element.width = 20.0;
    element.height = 3.0;

    TemplateLayer layer;
    layer.id = "layer-main";
    layer.elements = {element};

    DeviceProfile profile;
    profile.id = "profile-default";
    profile.dpi = 203.0;

    PrintClientSettings settings;
    TemplateDocument document;
    document.id = "template-default";
    document.templateKey = "default";
    document.layers = {layer};
    document.deviceProfiles = {profile};
    settings.templateDocuments["default"] = document;

    const auto image = sleekpr::infrastructure::LabelPreviewService().renderDemoPreview(settings);
    QVERIFY(!image.isNull());
    QVERIFY(image.width() < 945);
}
```

- [ ] **Step 2: Write failing HTTP profile test**

In `tests/tst_http.cpp`, configure settings with `templateDocuments["default"].deviceProfiles` for selected printer and assert the fake print engine receives a drawing plan whose command coordinates include `profile.offsetX/offsetY`.

Use an existing fake print engine pattern in the file. If none exists, add one:

```cpp
class CapturingPrintEngine final : public sleekpr::infrastructure::LabelPrintEngine
{
public:
    bool print(const sleekpr::core::NativeLabelDrawingPlan& plan, const QString& printerName) override
    {
        lastPlan = plan;
        lastPrinterName = printerName;
        return true;
    }

    sleekpr::core::NativeLabelDrawingPlan lastPlan;
    QString lastPrinterName;
};
```

- [ ] **Step 3: Run tests and verify failure**

Expected: compile or assertion failure because preview and HTTP do not use `templateDocuments`.

- [ ] **Step 4: Add preview service document path**

In `LabelPreviewService`, add:

```cpp
QImage renderPreview(
    const sleekpr::core::LabelItem& item,
    const sleekpr::core::PrintClientSettings& settings,
    const QString& printerName) const;
```

Implementation:

- Build `LabelRenderPlan`.
- Determine `templateKey`.
- If `settings.templateDocuments` contains that key, resolve profile and render via `TemplateDocumentRenderer`.
- Render image using `LabelPreviewImageRenderer().renderImage(drawingPlan, drawingPlan.renderDpi)`.
- If no document, use existing phase two path.

- [ ] **Step 5: Add print integration**

In `/print/tag`, before print:

- Resolve label plan.
- If matching template document exists, use `TemplateDocumentRenderer`.
- Otherwise use existing `NativeLabelDrawingPlanner`.
- Pass selected printer name unchanged to `m_printEngine->print`.

- [ ] **Step 6: Carry render DPI through the drawing plan**

Keep `QtLabelPrintEngine::print(plan, printerName)` signature and add one device-neutral field to `NativeLabelDrawingPlan`:

```cpp
    // 渲染 DPI 来自设备 profile，用于把同一毫米坐标计划映射到不同打印机分辨率。
    double renderDpi = 300.0;
```

`TemplateDocumentRenderer` writes `profile.dpi` into `drawingPlan.renderDpi`. `LabelPreviewService` calls `LabelPreviewImageRenderer().renderImage(drawingPlan, drawingPlan.renderDpi)`. `QtLabelPrintEngine` uses `plan.renderDpi > 0.0 ? plan.renderDpi : printer.resolution()` when converting millimeters to device pixels.

- [ ] **Step 7: Run full verification**

Expected: all tests pass.

- [ ] **Step 8: Commit**

```powershell
git add include/sleekpr/infrastructure/preview/LabelPreviewService.h src/infrastructure/preview/LabelPreviewService.cpp include/sleekpr/core/native/NativeLabelDrawingPlan.h src/core/templates/TemplateDocumentRenderer.cpp include/sleekpr/infrastructure/printing/QtLabelPrintEngine.h src/infrastructure/printing/QtLabelPrintEngine.cpp src/http/LocalHttpRouter.cpp tests/tst_core.cpp tests/tst_http.cpp
git commit -m "feat: apply template documents in preview and print"
```

---

### Task 5: Template Designer Window Skeleton and Layer UI

**Files:**
- Create: `include/sleekpr/app/TemplateDesignerWindow.h`
- Create: `src/app/TemplateDesignerWindow.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_app.cpp`

- [ ] **Step 1: Write failing app test for layer creation**

Add include:

```cpp
#include "sleekpr/app/TemplateDesignerWindow.h"
```

Add slot:

```cpp
void templateDesignerWindowAddsLayer();
```

Test:

```cpp
void AppTests::templateDesignerWindowAddsLayer()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(layerList != nullptr);

    const auto initialCount = layerList->count();
    QVERIFY(initialCount >= 1);
    addLayerButton->click();

    QCOMPARE(layerList->count(), initialCount + 1);
}
```

- [ ] **Step 2: Run test and verify missing window**

Expected: compile fails on `TemplateDesignerWindow.h`.

- [ ] **Step 3: Add window header**

Create `include/sleekpr/app/TemplateDesignerWindow.h`:

```cpp
#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QWidget>

#include <functional>

class QLabel;
class QListWidget;
class QPushButton;

namespace sleekpr::app {

class TemplateDesignerWindow final : public QWidget
{
public:
    using SettingsChangedCallback = std::function<void(const sleekpr::core::PrintClientSettings&)>;

    explicit TemplateDesignerWindow(
        sleekpr::core::PrintClientSettings settings,
        SettingsChangedCallback onSettingsChanged,
        QWidget* parent = nullptr);

private:
    void buildUi();
    void ensureCurrentTemplateDocument();
    void addLayer();
    void refreshLayerList();

    sleekpr::core::PrintClientSettings m_settings;
    SettingsChangedCallback m_onSettingsChanged;
    QString m_templateKey = QStringLiteral("default");
    QListWidget* m_layerList = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // namespace sleekpr::app
```

- [ ] **Step 4: Add minimal window implementation**

Create `src/app/TemplateDesignerWindow.cpp`:

- Use a horizontal layout.
- Left panel contains `QListWidget` named `templateLayerList`.
- Button `addLayerButton` calls `addLayer()`.
- Constructor calls `ensureCurrentTemplateDocument()` before `refreshLayerList()`.
- `ensureCurrentTemplateDocument()` uses `TemplateDocumentFactory::fromDrawingPlan(...)` when `m_settings.templateDocuments` does not contain `m_templateKey`; it uses the existing phase two planner, overrides, and template elements to bootstrap a full editable base layer, but does not save back to `settings.json` until the user changes the document.
- `addLayer()` creates `TemplateLayer` with generated id and name.
- `addLayer()` selects the newly created layer after refreshing the list.
- Store it in `m_settings.templateDocuments[m_templateKey].layers`.

All comments in this file must be Chinese.

- [ ] **Step 5: Add sources to CMake**

Add to executable `sleekpr`:

```cmake
    src/app/TemplateDesignerWindow.cpp
```

Add to `sleekpr_app_tests`:

```cmake
    ../src/app/TemplateDesignerWindow.cpp
```

- [ ] **Step 6: Run full verification**

Expected: all tests pass.

- [ ] **Step 7: Commit**

```powershell
git add include/sleekpr/app/TemplateDesignerWindow.h src/app/TemplateDesignerWindow.cpp CMakeLists.txt tests/CMakeLists.txt tests/tst_app.cpp
git commit -m "feat: add template designer layer window"
```

---

### Task 6: Element Editing, Locking, and Canvas Preview in Designer

**Files:**
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Test: `tests/tst_app.cpp`

- [ ] **Step 1: Write failing test for adding element to current layer**

Add slot:

```cpp
void templateDesignerWindowAddsFixedTextToCurrentLayer();
void templateDesignerWindowDeletesSelectedEmptyLayer();
void templateDesignerWindowDoesNotEditLockedLayer();
```

Test:

```cpp
void AppTests::templateDesignerWindowAddsFixedTextToCurrentLayer()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(elementList != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();

    QCOMPARE(elementList->count(), 1);
    QCOMPARE(elementList->item(0)->data(Qt::UserRole + 1).toString(), QString("fixedText"));
}
```

- [ ] **Step 2: Write failing test for deleting selected empty layer**

Test:

```cpp
void AppTests::templateDesignerWindowDeletesSelectedEmptyLayer()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* deleteLayerButton = window.findChild<QPushButton*>(QStringLiteral("deleteLayerButton"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(deleteLayerButton != nullptr);
    QVERIFY(layerList != nullptr);

    const auto initialCount = layerList->count();
    addLayerButton->click();
    deleteLayerButton->click();

    QCOMPARE(layerList->count(), initialCount);
}
```

- [ ] **Step 3: Write failing test for locked layer**

Test:

```cpp
void AppTests::templateDesignerWindowDoesNotEditLockedLayer()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* lockLayerButton = window.findChild<QPushButton*>(QStringLiteral("lockLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));

    addLayerButton->click();
    lockLayerButton->click();
    addFixedTextButton->click();

    QCOMPARE(elementList->count(), 0);
}
```

- [ ] **Step 4: Run tests and verify failure**

Expected: missing controls or count mismatch.

- [ ] **Step 5: Add element panel and buttons**

In `TemplateDesignerWindow` add:

- `QListWidget* m_elementList`
- buttons named `designerAddFixedTextButton`, `designerAddBoundFieldButton`, `designerAddQrCodeButton`, `designerAddRectangleButton`
- `deleteLayerButton`, `lockLayerButton`, `hideLayerButton`, `moveLayerUpButton`, `moveLayerDownButton`
- `deleteElementButton`, `lockElementButton`, `hideElementButton`, `moveElementUpButton`, `moveElementDownButton`

Use `TemplateDocumentEditModel::addElement`.
Use `TemplateDocumentEditModel::deleteLayer`, `deleteElement`, `moveElementUp`, and `moveElementDown` for the matching controls. Empty layer deletion can happen directly; deleting a layer that contains elements must ask for confirmation in production UI.

- [ ] **Step 6: Add canvas preview**

Add a `TemplatePreviewLabel` named `designerPreviewLabel`. Reuse:

- `TemplateElementHitTester`
- `TemplateDragCoordinateMapper`
- `TemplateDocumentRenderer`

Rules:

- Dragging hidden or locked layer elements returns false from drag start callback.
- Keyboard nudge uses the same 0.1mm and Shift+1.0mm behavior as settings window.
- Grid snap can reuse `snapToGridCheck` if the designer has its own checkbox.

- [ ] **Step 7: Run full verification**

Expected: all tests pass.

- [ ] **Step 8: Commit**

```powershell
git add include/sleekpr/app/TemplateDesignerWindow.h src/app/TemplateDesignerWindow.cpp tests/tst_app.cpp
git commit -m "feat: edit template designer elements"
```

---

### Task 7: Import, Export, Versions, and Device Profile UI

**Files:**
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Test: `tests/tst_app.cpp`

- [ ] **Step 1: Write failing app test for version restore**

Add slot:

```cpp
void templateDesignerWindowSavesAndRestoresVersion();
```

Test through buttons:

```cpp
void AppTests::templateDesignerWindowSavesAndRestoresVersion()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* saveVersionButton = window.findChild<QPushButton*>(QStringLiteral("saveTemplateVersionButton"));
    auto* restoreVersionButton = window.findChild<QPushButton*>(QStringLiteral("restoreTemplateVersionButton"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));

    const auto initialCount = layerList->count();
    QVERIFY(initialCount >= 1);
    addLayerButton->click();
    saveVersionButton->click();
    addLayerButton->click();
    restoreVersionButton->click();

    QCOMPARE(layerList->count(), initialCount + 1);
}
```

- [ ] **Step 2: Write failing core-style import/export app test**

Use a temporary file and direct window helper if exposed. If UI file dialogs block tests, keep import/export file operations in a non-UI helper method:

```cpp
QVERIFY(window.exportTemplateToFile(path));
QVERIFY(window.importTemplateFromFile(path));
```

These helpers can be public only if they are production features, not test-only hooks.

- [ ] **Step 3: Run tests and verify failure**

Expected: missing controls or helper methods.

- [ ] **Step 4: Add version controls**

Add buttons:

- `saveTemplateVersionButton`
- `restoreTemplateVersionButton`

Use `TemplateDocumentEditModel::saveVersion` and `restoreVersion`. Version id can be `version-<currentMSecsSinceEpoch>` for user-created snapshots.

- [ ] **Step 5: Add import/export controls**

Add buttons:

- `importTemplateButton`
- `exportTemplateButton`

Production click handlers use `QFileDialog`. The non-dialog helpers:

```cpp
bool exportTemplateToFile(const QString& path) const;
bool importTemplateFromFile(const QString& path);
```

Implementation uses `QSaveFile` for export and `QFile` for import. Export must write `QJsonDocument(TemplateDocumentJson::toJson(document)).toJson(QJsonDocument::Indented)`. The export dialog default suffix is `.sleekpr-template.json`.

Import must parse the file as `QJsonDocument`, call `TemplateDocumentJson::validateForImport(...)`, and only replace the current document after validation succeeds. On validation failure, return `false` from `importTemplateFromFile` and let the click handler show the Chinese error message. The production `importTemplateButton` click handler must show a Chinese summary containing template name, layer count, and version count, then replace the current template only after the user confirms; the non-dialog helper may apply directly for tests.

- [ ] **Step 6: Add device profile panel**

Add controls:

- `deviceProfilePrinterEdit`
- `deviceProfileDpiSpin`
- `deviceProfileScaleXSpin`
- `deviceProfileScaleYSpin`
- `deviceProfileOffsetXSpin`
- `deviceProfileOffsetYSpin`
- `saveDeviceProfileButton`

Saving writes to `document.deviceProfiles`, replacing an existing profile with the same `printerName`.

- [ ] **Step 7: Run full verification**

Expected: all tests pass.

- [ ] **Step 8: Commit**

```powershell
git add include/sleekpr/app/TemplateDesignerWindow.h src/app/TemplateDesignerWindow.cpp tests/tst_app.cpp
git commit -m "feat: manage template import export versions and profiles"
```

---

### Task 8: Settings Entry Point, Backward Compatibility, and Final Verification

**Files:**
- Modify: `include/sleekpr/app/SettingsWindow.h`
- Modify: `src/app/SettingsWindow.cpp`
- Modify: `src/app/main.cpp`
- Modify: `tests/tst_app.cpp`
- Test: `tests/tst_core.cpp`
- Test: `tests/tst_http.cpp`

- [ ] **Step 1: Write failing test for settings entry point**

Add slot:

```cpp
void settingsWindowHasTemplateDesignerEntry();
```

Test:

```cpp
void AppTests::settingsWindowHasTemplateDesignerEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    FileSettingsStore store(settingsPath);
    QVERIFY(store.save(PrintClientSettings{}));

    SettingsWindow window(settingsPath, nullptr);
    auto* openDesignerButton = window.findChild<QPushButton*>(QStringLiteral("openTemplateDesignerButton"));

    QVERIFY(openDesignerButton != nullptr);
}
```

- [ ] **Step 2: Run test and verify failure**

Expected: button not found.

- [ ] **Step 3: Add settings window callback**

In `SettingsWindow.h`, add:

```cpp
using OpenTemplateDesignerCallback = std::function<void()>;
void setOpenTemplateDesignerCallback(OpenTemplateDesignerCallback callback);
OpenTemplateDesignerCallback m_openTemplateDesigner;
```

In `SettingsWindow.cpp`, add button `openTemplateDesignerButton` and connect:

```cpp
connect(openDesignerButton, &QPushButton::clicked, this, [this] {
    if (m_openTemplateDesigner) {
        m_openTemplateDesigner();
    }
});
```

- [ ] **Step 4: Connect from main**

In `src/app/main.cpp`, keep a `std::unique_ptr<TemplateDesignerWindow>` or stack-owned window next to settings window. On open:

- Load latest settings through `FileSettingsStore`.
- Construct or reload designer with current settings.
- Show and activate the designer.

- [ ] **Step 5: Add backward compatibility tests**

In `tests/tst_core.cpp`, add:

```cpp
void settingsJsonKeepsOldTemplateElementsWhenTemplateDocumentsMissing();
```

Test loads JSON with only `templateElements` and verifies `templateDocuments.isEmpty()` and existing phase two elements remain.

- [ ] **Step 6: Add final HTTP compatibility test**

In `tests/tst_http.cpp`, assert `/settings` returns both:

- `templateOverrides`
- `templateElements`
- `templateDocuments`

For old settings, `templateDocuments` should be an object and can be empty.

- [ ] **Step 7: Run full verification**

Run the full verification command. Expected:

```text
100% tests passed, 0 tests failed out of 3
```

- [ ] **Step 8: Restart app and smoke test health**

```powershell
Get-Process sleekpr -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Process -FilePath 'D:\print-project\sleekpr\build\sleekpr.exe' -WorkingDirectory 'D:\print-project\sleekpr\build'
try { (Invoke-WebRequest -UseBasicParsing -Uri 'http://127.0.0.1:37122/health' -TimeoutSec 5).Content } catch { $_.Exception.Message }
```

Expected JSON contains `"success":true`.

- [ ] **Step 9: Commit**

```powershell
git add include/sleekpr/app/SettingsWindow.h src/app/SettingsWindow.cpp src/app/main.cpp tests/tst_app.cpp tests/tst_core.cpp tests/tst_http.cpp
git commit -m "feat: connect template designer to settings"
```

---

## Self-Review Checklist

- Every spec requirement maps to a task:
  - Layers: Tasks 2, 5, 6.
  - Import/export: Task 7.
  - Version management: Tasks 2, 7.
  - Device profile and DPI calibration: Tasks 3, 4, 7.
  - Preview and print integration: Task 4.
  - Settings entry point and compatibility: Task 8.
- No browser preview is introduced.
- Device logic stays out of label business planning; profile resolving is isolated in `core/templates` and print execution stays in `infrastructure/printing`.
- New comments in planned code are Chinese.
- CodeRabbit is not used.
