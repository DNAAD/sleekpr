# UI Workbench Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将当前分散、平铺的 Qt Widgets 界面整理成更清晰的桌面工作台第一阶段。

**Architecture:** 只修改 `app` 层的窗口外壳、样式和控件层级，不改变 `core`、`infrastructure`、`http` 的业务、预览、打印和接口行为。样式集中到独立的 app 主题模块，设置窗口提供工作台导航和状态概览，模板设计器补齐顶部工具栏与三栏区域命名，便于后续继续拆分。

**Tech Stack:** C++20、Qt 6 Widgets、Qt Test、CMake。

---

### Task 1: 锁定工作台 UI 行为

**Files:**
- Modify: `tests/tst_app.cpp`

- [ ] **Step 1: 添加失败测试**

新增测试槽位：
```cpp
void settingsWindowShowsWorkbenchNavigationAndOverview();
void templateDesignerWindowUsesWorkbenchShell();
void appThemeDefinesWorkbenchStyle();
```

测试内容：
```cpp
SettingsWindow settingsWindow(settingsPath, nullptr);
QVERIFY(settingsWindow.findChild<QWidget*>(QStringLiteral("settingsWorkbenchShell")) != nullptr);
QVERIFY(settingsWindow.findChild<QWidget*>(QStringLiteral("settingsNavigationRail")) != nullptr);
QVERIFY(settingsWindow.findChild<QLabel*>(QStringLiteral("settingsServiceStatusLabel")) != nullptr);
QVERIFY(settingsWindow.findChild<QPushButton*>(QStringLiteral("openTemplateDesignerButton")) != nullptr);

TemplateDesignerWindow designerWindow(PrintClientSettings{}, nullptr);
QVERIFY(designerWindow.findChild<QWidget*>(QStringLiteral("designerWorkbenchShell")) != nullptr);
QVERIFY(designerWindow.findChild<QWidget*>(QStringLiteral("designerTopToolbar")) != nullptr);
QVERIFY(designerWindow.findChild<QWidget*>(QStringLiteral("designerLayerPanel")) != nullptr);
QVERIFY(designerWindow.findChild<QWidget*>(QStringLiteral("designerWorkspacePanel")) != nullptr);
QVERIFY(designerWindow.findChild<QWidget*>(QStringLiteral("designerInspectorPanel")) != nullptr);

const auto styleSheet = sleekpr::app::sleekprAppStyleSheet();
QVERIFY(styleSheet.contains(QStringLiteral("settingsNavigationRail")));
QVERIFY(styleSheet.contains(QStringLiteral("designerTopToolbar")));
```

- [ ] **Step 2: 运行 app 测试确认失败**

Run:
```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\tests\sleekpr_app_tests.exe settingsWindowShowsWorkbenchNavigationAndOverview templateDesignerWindowUsesWorkbenchShell appThemeDefinesWorkbenchStyle'
```

Expected: FAIL，因为新对象名和主题模块尚未实现。

### Task 2: 增加集中主题模块

**Files:**
- Create: `include/sleekpr/app/AppTheme.h`
- Create: `src/app/AppTheme.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/app/main.cpp`

- [ ] **Step 1: 实现 `sleekprAppStyleSheet()`**

新增只属于 `app` 层的主题模块，返回 Qt QSS 字符串。样式覆盖窗口背景、导航栏、工具栏、主要按钮、危险按钮、输入框、列表、状态标签和预览画布。

- [ ] **Step 2: 在主程序应用全局样式**

`main.cpp` 包含 `sleekpr/app/AppTheme.h`，在 `QApplication` 初始化后调用：
```cpp
app.setStyleSheet(sleekpr::app::sleekprAppStyleSheet());
```

- [ ] **Step 3: 重新运行主题测试**

Run:
```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\tests\sleekpr_app_tests.exe appThemeDefinesWorkbenchStyle'
```

Expected: PASS。

### Task 3: 设置窗口工作台化

**Files:**
- Modify: `include/sleekpr/app/SettingsWindow.h`
- Modify: `src/app/SettingsWindow.cpp`

- [ ] **Step 1: 拆出工作台导航和概览面板**

新增私有函数：
```cpp
QWidget* createWorkbenchNavigation();
QWidget* createOverviewPanel();
void refreshOverview();
```

设置窗口根布局改成左侧导航栏 + 右侧内容区。右侧顶部显示服务状态、默认打印机、当前模板，下面保留原打印机设置和 Qt 原生预览。

- [ ] **Step 2: 保持既有入口对象名**

继续保留：
```cpp
openTemplateDesignerButton
openPaperSpecManagerButton
openFieldPresetManagerButton
saveSettingsButton
```

这样现有测试和用户操作路径不被破坏。

- [ ] **Step 3: 运行设置窗口相关测试**

Run:
```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\tests\sleekpr_app_tests.exe settingsWindowShowsWorkbenchNavigationAndOverview settingsWindowHasTemplateDesignerEntry settingsWindowHasPaperSpecManagerEntry settingsWindowHasFieldPresetManagerEntry settingsWindowShowsPreviewOnlyWithoutElementEditor'
```

Expected: PASS。

### Task 4: 模板设计器工作区化

**Files:**
- Modify: `src/app/TemplateDesignerWindow.cpp`

- [ ] **Step 1: 给三大区域命名并加入顶部工具栏**

根对象名为 `designerWorkbenchShell`。左侧区域命名为 `designerLayerPanel`，中间区域命名为 `designerWorkspacePanel`，右侧属性区域命名为 `designerInspectorPanel`。中间区域上方新增 `designerTopToolbar`，承载纸张规格、标尺精度、辅助线和预打印入口。

- [ ] **Step 2: 降低按钮噪音**

保持原对象名和信号连接，给主操作设置动态属性：
```cpp
button->setProperty("buttonRole", "primary");
button->setProperty("buttonRole", "danger");
```

- [ ] **Step 3: 运行设计器相关测试**

Run:
```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && build\tests\sleekpr_app_tests.exe templateDesignerWindowUsesWorkbenchShell templateDesignerWindowAddsLayer templateDesignerWindowPrePrintsCurrentTemplate templateDesignerWindowSelectsPaperSpecFromStore'
```

Expected: PASS。

### Task 5: 全量验证

**Files:**
- Verify only.

- [ ] **Step 1: 全量构建和测试**

Run:
```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected: 所有 CTest 通过。

- [ ] **Step 2: 检查补丁格式**

Run:
```powershell
git diff --check
```

Expected: 无输出，退出码 0。
