# Template Designer Editor Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将模板设计器从普通三栏控件堆叠升级为专业编辑器布局。

**Architecture:** 只调整 `src/app/TemplateDesignerWindow.cpp` 的 Qt Widgets 外壳，不改变模板文档、预览渲染、真实打印和 HTTP 接口。使用 `QSplitter` 承载可调三栏，左侧用资源 Tab 管理图层和模板库，中间保持 Qt 原生画布预览，右侧用检查器 Tab 分组元素、表格、数组网格和设备校准属性。

**Tech Stack:** C++20、Qt 6 Widgets、Qt Test、CMake。

---

### Task 1: 专业编辑器布局测试

**Files:**
- Modify: `tests/tst_app.cpp`

- [ ] **Step 1: 写失败测试**

新增测试 `templateDesignerWindowUsesProfessionalEditorLayout()`，断言：
```cpp
auto* splitter = window.findChild<QSplitter*>(QStringLiteral("designerMainSplitter"));
auto* resourceTabs = window.findChild<QTabWidget*>(QStringLiteral("designerResourceTabs"));
auto* inspectorTabs = window.findChild<QTabWidget*>(QStringLiteral("designerInspectorTabs"));
QVERIFY(splitter != nullptr);
QVERIFY(resourceTabs != nullptr);
QVERIFY(inspectorTabs != nullptr);
QCOMPARE(resourceTabs->tabText(0), QString::fromUtf8("图层"));
QCOMPARE(resourceTabs->tabText(1), QString::fromUtf8("模板库"));
QVERIFY(inspectorTabs->tabText(0).contains(QString::fromUtf8("元素")));
QVERIFY(inspectorTabs->tabText(1).contains(QString::fromUtf8("表格")));
QVERIFY(inspectorTabs->tabText(2).contains(QString::fromUtf8("数组")));
QVERIFY(inspectorTabs->tabText(3).contains(QString::fromUtf8("设备")));
```

- [ ] **Step 2: 运行测试确认失败**

Run:
```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_app_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_app_tests --output-on-failure'
```

Expected: FAIL，因为 `designerMainSplitter`、资源 Tab、检查器 Tab 尚未存在。

### Task 2: 实现编辑器骨架

**Files:**
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Modify: `src/app/AppTheme.cpp`

- [ ] **Step 1: 引入 Qt 控件**

在 `TemplateDesignerWindow.cpp` 增加：
```cpp
#include <QSplitter>
#include <QTabWidget>
```

- [ ] **Step 2: 左侧资源 Tab**

把 `m_layerList` 和图层操作放入 `designerLayerTab`，把模板库搜索、列表和模板库操作放入 `designerLibraryTab`，再添加到 `designerResourceTabs`。

- [ ] **Step 3: 中间画布保持不变**

保留 `designerTopToolbar`、`designerPreviewLabel`、模拟数据和 `designerStatusLabel`，只把中间区放进 `designerMainSplitter` 的第二栏。

- [ ] **Step 4: 右侧检查器 Tab**

把元素列表、添加/删除/锁定/隐藏/排序和基础元素属性放入 `元素` Tab；表格属性放入 `表格` Tab；数组网格属性放入 `数组网格` Tab；设备 profile 与校准放入 `设备校准` Tab。

- [ ] **Step 5: 更新 QSS**

为 `QSplitter::handle`、`QTabWidget::pane`、`QTabBar::tab`、`designerResourceTabs`、`designerInspectorTabs` 增加样式。

### Task 3: 验证

**Files:**
- Verify only.

- [ ] **Step 1: 全量构建和测试**

Run:
```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected: 3/3 tests passed。

- [ ] **Step 2: 补丁格式检查**

Run:
```powershell
git diff --check
```

Expected: exit 0；CRLF 提示可接受。
