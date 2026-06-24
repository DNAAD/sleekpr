# 模板设计器图层面板拆分 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `TemplateDesignerWindow` 左侧图层资源区 UI 抽成独立面板，让窗口类继续瘦身。

**Architecture:** 新增 `TemplateLayerPanel` 负责左侧整体容器、模板库面板、图层列表、图层操作按钮和版本按钮的控件创建。`TemplateDesignerWindow` 继续负责图层业务操作、模板库保存/加载、选择刷新和设置通知，确保模板渲染、预览、打印链路不变。

**Tech Stack:** C++20、Qt 6 Widgets、CMake、CTest。

---

### Task 1: 新增图层面板类

**Files:**
- Create: `include/sleekpr/app/TemplateLayerPanel.h`
- Create: `src/app/TemplateLayerPanel.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 定义访问器**

新增 `TemplateLayerPanel`，暴露模板库控件、图层列表、图层按钮、版本按钮访问器。

- [ ] **Step 2: 迁移 UI 构建**

将 `TemplateDesignerWindow::buildUi()` 中左侧 `designerLayerPanel`、`designerLeftResourceSplitter`、`designerLayerSection` 相关 UI 创建迁移到 `TemplateLayerPanel`，保持原有 objectName 和按钮文字不变。

- [ ] **Step 3: 接入构建系统**

将新 `.cpp` 加入主程序和 app 测试目标。

### Task 2: 窗口层接入图层面板

**Files:**
- Modify: `src/app/TemplateDesignerWindow.cpp`

- [ ] **Step 1: 替换内联左侧 UI**

`TemplateDesignerWindow::buildUi()` 使用 `TemplateLayerPanel` 创建左侧区域，并从面板访问器取得原有控件指针。

- [ ] **Step 2: 保留行为连接**

图层增删锁隐、上下移动、版本保存恢复、模板库搜索/加载/新建/删除/复制/重命名连接继续由窗口层维护。

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
