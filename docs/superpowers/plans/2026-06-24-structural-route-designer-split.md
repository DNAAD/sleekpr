# 结构性拆分 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将过大的模板设计器和本地 HTTP 路由拆成更清晰的模块，降低后续维护风险，同时保持现有接口、UI objectName 和渲染链路不变。

**Architecture:** 先拆 HTTP，保留 `LocalHttpRouter` 作为总入口，把 settings/template/preview/print 分到独立路由模块。再拆设计器，保留 `TemplateDesignerWindow` 作为协调层，逐步抽出状态模型、模板库面板、属性面板和画布控制器。

**Tech Stack:** C++20、Qt 6 Widgets、Qt Network、CTest、QtTest。

---

### Task 1: HTTP 路由公共响应与上下文

**Files:**
- Create: `include/sleekpr/http/LocalHttpRouteSupport.h`
- Create: `src/http/LocalHttpRouteSupport.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/http/LocalHttpRouter.cpp`

- [ ] **Step 1: 新增公共响应工具**

创建 `LocalHttpRouteSupport`，提供 `okEnvelope`、`failEnvelope`、`jsonResponse`、`emptyResponse`、CORS header 和路径解析工具。

- [ ] **Step 2: 编译并运行 HTTP 测试**

Run: C++ 构建和 `ctest -R http`。
Expected: HTTP 测试全部通过。

### Task 2: HTTP settings/template/preview/print 模块

**Files:**
- Create: `include/sleekpr/http/SettingsRoutes.h`
- Create: `src/http/SettingsRoutes.cpp`
- Create: `include/sleekpr/http/TemplateRoutes.h`
- Create: `src/http/TemplateRoutes.cpp`
- Create: `include/sleekpr/http/PreviewRoutes.h`
- Create: `src/http/PreviewRoutes.cpp`
- Create: `include/sleekpr/http/PrintRoutes.h`
- Create: `src/http/PrintRoutes.cpp`
- Create: `include/sleekpr/http/TemplateRequestPlanner.h`
- Create: `src/http/TemplateRequestPlanner.cpp`
- Modify: `src/http/LocalHttpRouter.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 拆 settings 路由**

迁移 `/health`、`/settings`、`/printers` 到 `SettingsRoutes`。

- [ ] **Step 2: 拆模板资源路由**

迁移 `/templates`、`/paper-specs`、`/field-presets` 到 `TemplateRoutes`。

- [ ] **Step 3: 拆预览和打印路由**

迁移 `/preview/template` 到 `PreviewRoutes`，迁移 `/print/tag` 和 `/print/template` 到 `PrintRoutes`。

- [ ] **Step 4: 抽模板打印计划构建**

迁移模板查找、字段预设合并、纸张规格、设备 profile、渲染错误传播到 `TemplateRequestPlanner`。

- [ ] **Step 5: 验证**

Run: C++ 构建、完整 CTest、JS 客户端测试。
Expected: 所有测试通过。

### Task 3: 设计器状态模型第一阶段

**Files:**
- Create: `include/sleekpr/app/TemplateDesignerStateModel.h`
- Create: `src/app/TemplateDesignerStateModel.cpp`
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 抽纯状态操作**

将当前模板保证、当前纸张规格、导入模板应用、历史记录管理移入 `TemplateDesignerStateModel`。

- [ ] **Step 2: 窗口调用模型**

`TemplateDesignerWindow` 保留 UI 控件和信号，文档状态读写通过模型完成。

- [ ] **Step 3: 验证**

Run: `ctest -R app`。
Expected: 设计器现有测试全部通过。

### Task 4: 设计器模板库面板第一阶段

**Files:**
- Create: `include/sleekpr/app/TemplateLibraryPanel.h`
- Create: `src/app/TemplateLibraryPanel.cpp`
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 抽模板库 UI 构建**

将模板库搜索、名称输入、列表和按钮布局移入 `TemplateLibraryPanel`，保持原 objectName。

- [ ] **Step 2: 抽模板库刷新和事件**

将搜索、保存、加载、删除、复制、重命名的 UI 驱动逻辑迁移到面板信号，实际文档变更仍由窗口协调。

- [ ] **Step 3: 验证**

Run: `ctest -R app`。
Expected: 模板库相关测试全部通过。

### Task 5: 收尾验证

**Files:**
- Modify only as required by previous tasks.

- [ ] **Step 1: 完整构建和测试**

Run: 常用 VS + CMake + CTest 命令。
Expected: `3/3 tests passed`。

- [ ] **Step 2: JS 客户端测试**

Run: `node --test tests/js/sleekpr-print-client.test.mjs`。
Expected: `5/5 pass`。

- [ ] **Step 3: 空白检查**

Run: `git diff --check`。
Expected: 无错误，仅允许既有 CRLF 提示。
