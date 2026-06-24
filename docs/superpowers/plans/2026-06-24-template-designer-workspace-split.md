# 模板设计器画布工作区拆分 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `TemplateDesignerWindow` 中间画布工作区 UI 抽成独立面板，继续降低窗口类体积和维护风险。

**Architecture:** 新增 `TemplateWorkspacePanel` 负责顶部工具栏、纸张规格选择、Qt 原生预览画布、模拟数据编辑器和状态栏控件创建。`TemplateDesignerWindow` 保留预览渲染、按钮连接、样本数据解析和模板状态写回，确保正式预览和打印仍走 Qt 原生链路。

**Tech Stack:** C++20、Qt 6 Widgets、CMake、CTest。

---

### Task 1: 新增画布工作区面板

**Files:**
- Create: `include/sleekpr/app/TemplateWorkspacePanel.h`
- Create: `src/app/TemplateWorkspacePanel.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 定义工作区访问器**

新增 `TemplateWorkspacePanel`，暴露保存、预打印、导入、导出、撤销、重做、缩放、标尺精度、辅助线、纸张规格、纸张管理、字段预设、预览标签、模拟数据编辑器、模拟数据错误标签和状态标签访问器。

- [ ] **Step 2: 迁移 UI 构建**

将 `TemplateDesignerWindow::buildUi()` 中间工作区 UI 创建迁移到 `TemplateWorkspacePanel` 构造函数，保持原有 objectName、按钮文字、默认缩放和模拟数据初始值不变。

- [ ] **Step 3: 接入构建系统**

将新 `.cpp` 加入主程序和 app 测试目标。

### Task 2: 窗口层接入工作区面板

**Files:**
- Modify: `src/app/TemplateDesignerWindow.cpp`

- [ ] **Step 1: 替换内联工作区 UI**

`TemplateDesignerWindow::buildUi()` 使用 `TemplateWorkspacePanel` 创建中间区域，并从面板访问器取得原有控件指针。

- [ ] **Step 2: 保留行为连接**

保存、导入、导出、预打印、纸张规格、缩放、辅助线、模拟数据编辑、画布拖拽和键盘微调连接继续由窗口层维护。

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
