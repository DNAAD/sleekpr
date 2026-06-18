# 设置窗口第一阶段 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Qt 原生客户端中新增可保存、可预览的打印设置窗口。

**Architecture:** 把可测试的设置编辑逻辑放入 core 层，Qt 窗口只负责控件和交互。设置窗口读取 `FileSettingsStore`，编辑 `PrintClientSettings` 的内存副本，保存后刷新主预览窗口。

**Tech Stack:** C++20, Qt 6 Widgets, Qt PrintSupport, CMake, Qt Test.

---

## 执行说明

当前迁移代码大量处于未跟踪状态，因此本计划在现有 `D:\print-project\sleekpr` 工作区执行，不新建 git worktree。所有新增注释必须使用中文，关键逻辑必须带中文注释。

## 文件结构

- Create: `include/sleekpr/core/settings/TemplateElementCatalog.h`
- Create: `src/core/settings/TemplateElementCatalog.cpp`
- Create: `include/sleekpr/core/settings/SettingsEditModel.h`
- Create: `src/core/settings/SettingsEditModel.cpp`
- Create: `include/sleekpr/app/SettingsWindow.h`
- Create: `src/app/SettingsWindow.cpp`
- Modify: `include/sleekpr/app/PreviewWindow.h`
- Modify: `src/app/PreviewWindow.cpp`
- Modify: `src/app/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/tst_core.cpp`

## Task 1: 元素清单与设置编辑模型

**Files:**
- Create: `include/sleekpr/core/settings/TemplateElementCatalog.h`
- Create: `src/core/settings/TemplateElementCatalog.cpp`
- Create: `include/sleekpr/core/settings/SettingsEditModel.h`
- Create: `src/core/settings/SettingsEditModel.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/tst_core.cpp`

- [ ] **Step 1: 写失败测试**

在 `tests/tst_core.cpp` 增加测试槽：

```cpp
void templateElementCatalogReturnsEditableDefaults();
void settingsEditModelResetsElementOverridesOnly();
```

测试内容：

```cpp
void CoreTests::templateElementCatalogReturnsEditableDefaults()
{
    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan);
    const auto elements = TemplateElementCatalog::fromDrawingPlan(drawingPlan);

    QVERIFY(elements.size() >= 10);
    QCOMPARE(elements.first().key, QString("identifier"));
    QCOMPARE(elements.first().displayName, QString::fromUtf8("编号"));
    const auto qualityMark = TemplateElementCatalog::find(elements, "qualityMark");
    QVERIFY(qualityMark.has_value());
    QCOMPARE(qualityMark->displayName, QString::fromUtf8("合格证"));
    QCOMPARE(qualityMark->defaultValue.x.value(), drawingPlan.commands[1].x);
}
```

```cpp
void CoreTests::settingsEditModelResetsElementOverridesOnly()
{
    PrintClientSettings settings;
    settings.defaultPrinter = "Printer A";
    settings.labelOffset = LabelOffset{1.0, 2.0};
    settings.templateOverrides["default"]["productName"] = TemplateElementOverride{10.0, 1.0, 5.0, true};
    settings.templateOverrides["default"]["qrCode"] = TemplateElementOverride{2.0, 1.0, std::nullopt, std::nullopt};

    SettingsEditModel::resetElement(settings, "default", "productName");
    QVERIFY(!settings.templateOverrides["default"].contains("productName"));
    QVERIFY(settings.templateOverrides["default"].contains("qrCode"));

    SettingsEditModel::resetAllElements(settings, "default");
    QCOMPARE(settings.defaultPrinter, QString("Printer A"));
    QCOMPARE(settings.labelOffset.x, 1.0);
    QVERIFY(settings.templateOverrides["default"].isEmpty());
}
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected: 编译失败，提示 `TemplateElementCatalog` 或 `SettingsEditModel` 未声明。

- [ ] **Step 3: 实现最小 core 逻辑**

实现元素描述结构、从绘制命令提取默认值、按 key 查找元素、重置当前元素、重置全部元素。

- [ ] **Step 4: 运行测试确认通过**

Run 同 Step 2。Expected: 测试通过。

## Task 2: 预览窗口刷新能力

**Files:**
- Modify: `include/sleekpr/app/PreviewWindow.h`
- Modify: `src/app/PreviewWindow.cpp`

- [ ] **Step 1: 写失败测试或编译用例**

本任务是 Qt 窗口 API，先在后续 `SettingsWindow` 编译中调用 `PreviewWindow::refreshFromSettings()` 形成编译约束。

- [ ] **Step 2: 实现刷新方法**

给 `PreviewWindow` 增加：

```cpp
void refreshFromSettings(const PrintClientSettings& settings);
```

方法根据传入设置重新生成绘制计划并刷新图片。

- [ ] **Step 3: 保持启动逻辑读取本地设置**

构造函数仍从 `FileSettingsStore` 读取设置并绘制首次预览。

## Task 3: Qt 设置窗口

**Files:**
- Create: `include/sleekpr/app/SettingsWindow.h`
- Create: `src/app/SettingsWindow.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 创建窗口类**

`SettingsWindow` 继承 `QWidget`，构造参数包含 `FileSettingsStore&` 和 `std::function<void(const PrintClientSettings&)>`。

- [ ] **Step 2: 搭建三栏布局**

左侧：打印机下拉、刷新打印机按钮、X/Y 偏移。中间：预览图片。右侧：元素列表、X/Y/字号/粗细。

- [ ] **Step 3: 实现交互**

实现加载设置、选择元素、收集表单、应用但不保存、保存、重置当前元素、重置全部元素、刷新预览。

- [ ] **Step 4: 编译验证**

Run Task 1 的完整构建测试命令。Expected: 编译通过。

## Task 4: 托盘入口与最终验证

**Files:**
- Modify: `src/app/main.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 托盘菜单新增“设置”**

在托盘菜单中创建 `设置` action，点击后显示 `SettingsWindow`。

- [ ] **Step 2: 设置应用后刷新主预览**

`SettingsWindow` 保存或应用后调用 `PreviewWindow::refreshFromSettings(settings)`。

- [ ] **Step 3: 构建和测试**

Run Task 1 的完整构建测试命令。Expected: build 和 tests 通过。

- [ ] **Step 4: 启动人工验证**

Run:

```powershell
Start-Process -FilePath 'D:\print-project\sleekpr\build\sleekpr.exe' -WorkingDirectory 'D:\print-project\sleekpr\build'
```

Expected: 能看到预览窗口，托盘菜单能打开设置窗口。
