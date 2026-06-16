# 模板设计器接入模板库 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 Qt 模板设计器可以把当前模板保存到独立模板库，并从模板库加载模板，打通“设计、保存、复用、打印”的本地闭环。

**Architecture:** `TemplateDesignerWindow` 继续负责 Qt 原生编辑交互，新增可选模板库目录参数；模板文件读写仍交给 core 层 `TemplateLibraryStore`。主程序从 `settings.json` 同级推导 `templates` 目录传给设计器，保持 HTTP 模板库和 Qt 设计器使用同一套落盘位置。

**Tech Stack:** C++17、Qt Widgets、Qt Test、`TemplateLibraryStore`、`TemplateDocumentJson`。

---

### Task 1: App 测试定义模板库按钮与保存加载行为

**Files:**
- Modify: `D:\print-project\sleekpr\tests\tst_app.cpp`
- Test: `D:\print-project\sleekpr\tests\tst_app.cpp`

- [ ] **Step 1: Write the failing test**

在 `AppTests` 私有槽里新增：

```cpp
void templateDesignerWindowSavesAndLoadsTemplateLibraryDocument();
```

新增测试函数：

```cpp
void AppTests::templateDesignerWindowSavesAndLoadsTemplateLibraryDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    TemplateDesignerWindow source(PrintClientSettings{}, nullptr, templateDirectoryPath);

    auto* addLayerButton = source.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* saveLibraryButton = source.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(saveLibraryButton != nullptr);

    addLayerButton->click();
    saveLibraryButton->click();

    sleekpr::core::TemplateLibraryStore store(templateDirectoryPath);
    const auto savedTemplate = store.loadTemplate(QStringLiteral("default"));
    QVERIFY(savedTemplate.has_value());
    QCOMPARE(savedTemplate->layers.size(), 2);

    PrintClientSettings importedSettings;
    TemplateDesignerWindow target(PrintClientSettings{}, [&importedSettings](const PrintClientSettings& nextSettings) {
        importedSettings = nextSettings;
    }, templateDirectoryPath);

    auto* loadLibraryButton = target.findChild<QPushButton*>(QStringLiteral("loadTemplateFromLibraryButton"));
    auto* targetLayerList = target.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(loadLibraryButton != nullptr);
    QVERIFY(targetLayerList != nullptr);

    loadLibraryButton->click();

    QCOMPARE(targetLayerList->count(), 2);
    QCOMPARE(importedSettings.templateDocuments.value("default").layers.size(), 2);
}
```

- [ ] **Step 2: Run app targeted test to verify it fails**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_app_tests --output-on-failure'
```

Expected: 编译失败或测试失败，原因是构造函数尚未支持模板库目录，按钮对象名尚不存在。

### Task 2: TemplateDesignerWindow 接入 TemplateLibraryStore

**Files:**
- Modify: `D:\print-project\sleekpr\include\sleekpr\app\TemplateDesignerWindow.h`
- Modify: `D:\print-project\sleekpr\src\app\TemplateDesignerWindow.cpp`

- [ ] **Step 1: Extend constructor and private methods**

在头文件中把构造函数改成：

```cpp
explicit TemplateDesignerWindow(
    sleekpr::core::PrintClientSettings settings,
    SettingsChangedCallback onSettingsChanged,
    QString templateLibraryDirectoryPath = QString(),
    QWidget* parent = nullptr);
```

增加私有方法和成员：

```cpp
void saveCurrentTemplateToLibrary();
void loadCurrentTemplateFromLibrary();
QString m_templateLibraryDirectoryPath;
```

- [ ] **Step 2: Wire buttons in UI**

在文档按钮区域增加两个按钮：

```cpp
auto* saveToLibraryButton = createButton(QString::fromUtf8("保存到模板库"), QStringLiteral("saveTemplateToLibraryButton"), layerPanel);
auto* loadFromLibraryButton = createButton(QString::fromUtf8("从模板库加载"), QStringLiteral("loadTemplateFromLibraryButton"), layerPanel);
```

并连接：

```cpp
connect(saveToLibraryButton, &QPushButton::clicked, this, [this] { saveCurrentTemplateToLibrary(); });
connect(loadFromLibraryButton, &QPushButton::clicked, this, [this] { loadCurrentTemplateFromLibrary(); });
```

- [ ] **Step 3: Implement save/load**

保存时调用 `TemplateLibraryStore(m_templateLibraryDirectoryPath).saveTemplate(document, &errorMessage)`；加载时调用 `loadTemplate(m_templateKey, &errorMessage)` 并复用 `applyImportedTemplateDocument`。模板库路径为空时在状态栏提示“未配置模板库目录”。

- [ ] **Step 4: Run app targeted test to verify it passes**

Run the same `sleekpr_app_tests` command from Task 1.

Expected: `sleekpr_app_tests` 通过。

### Task 3: 主程序传入模板库目录

**Files:**
- Modify: `D:\print-project\sleekpr\src\app\main.cpp`

- [ ] **Step 1: Add helper**

新增：

```cpp
QString templateLibraryDirectoryPath(const QString& settingsFilePath)
{
    return QFileInfo(settingsFilePath).absoluteDir().filePath(QStringLiteral("templates"));
}
```

- [ ] **Step 2: Pass path to designer**

构造 `TemplateDesignerWindow` 时传入：

```cpp
templateLibraryDirectoryPath(settingsFilePath)
```

- [ ] **Step 3: Run full verification**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected: `3/3 tests passed`。

### Task 4: 部署验证与提交

**Files:**
- Modify: `D:\print-project\sleekpr\docs\superpowers\plans\2026-06-16-template-designer-library.md`

- [ ] **Step 1: Run deploy target**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target deploy_sleekpr'
```

Expected: deploy target exit code 0；Qt translations 和 `qopensslbackendd.dll` 警告仍可作为既有非阻断警告记录。

- [ ] **Step 2: Commit only this batch**

Run:

```powershell
git add include/sleekpr/app/TemplateDesignerWindow.h src/app/TemplateDesignerWindow.cpp src/app/main.cpp tests/tst_app.cpp docs/superpowers/plans/2026-06-16-template-designer-library.md
git commit -m "feat: connect designer to template library"
```

Expected: 提交只包含本批文件，不包含 `README.md` 和 `docs/superpowers/plans/2026-06-15-settings-window.md`。

## 自审

- 覆盖保存到模板库、从模板库加载、主程序传入真实目录三个需求。
- 不做模板库列表 UI、不做多模板选择，避免本批范围过大。
- 继续保留 settings 内嵌模板兼容路径，加载后仍通过 `notifySettingsChanged` 同步旧配置。
