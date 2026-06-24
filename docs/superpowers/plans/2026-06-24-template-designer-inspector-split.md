# 模板设计器检查器面板拆分 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `TemplateDesignerWindow` 右侧元素/表格/数组网格/设备校准检查器 UI 抽成独立面板，降低窗口类体积和维护风险。

**Architecture:** 保留 `TemplateDesignerWindow` 作为协调层，继续负责模板文档写回、选择、刷新和信号连接。新增 `TemplateInspectorPanel` 只负责创建右侧检查器控件、维护原有 objectName、暴露控件集合与按钮入口，不改变 Qt 原生预览/渲染链路。

**Tech Stack:** C++20、Qt 6 Widgets、CMake、CTest。

---

### Task 1: 新增检查器面板类

**Files:**
- Create: `include/sleekpr/app/TemplateInspectorPanel.h`
- Create: `src/app/TemplateInspectorPanel.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 定义控件集合接口**

新增 `TemplateInspectorPanel`，提供元素列表、属性编辑、表格编辑、数组网格编辑、设备校准控件和按钮/action 访问器。

- [ ] **Step 2: 迁移 UI 构建**

将 `TemplateDesignerWindow::buildUi()` 中右侧 `elementScrollArea` 内部控件创建迁移到 `TemplateInspectorPanel` 构造函数，保持原有 objectName 和按钮文字不变。

- [ ] **Step 3: 接入构建系统**

将新 `.cpp` 加入主程序和 app 测试目标。

### Task 2: 窗口层接入面板

**Files:**
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`

- [ ] **Step 1: 替换内联 UI 创建**

`TemplateDesignerWindow::buildUi()` 使用 `TemplateInspectorPanel` 创建右侧区域，并从面板访问器取得原有控件指针。

- [ ] **Step 2: 保留现有行为连接**

原有按钮、右键菜单、拖拽排序、自动应用、防抖和设备校准连接继续由窗口层维护，确保行为不变。

- [ ] **Step 3: 验证设计器测试**

Run: `ctest -R app --output-on-failure`。Expected: app 测试通过。

### Task 3: 收尾验证

**Files:**
- Modify only as required by previous tasks.

- [ ] **Step 1: 完整 C++ 构建和 CTest**

Run: 常用 VS + CMake + CTest 命令。Expected: `3/3 tests passed`。

- [ ] **Step 2: JS 客户端测试**

Run: `node --test tests/js/sleekpr-print-client.test.mjs`。Expected: `5/5 pass`。

- [ ] **Step 3: 空白检查**

Run: `git diff --check`。Expected: 无空白错误；仅允许已知 CRLF 提示。
