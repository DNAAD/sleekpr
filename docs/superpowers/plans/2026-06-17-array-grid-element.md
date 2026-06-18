# Array Grid Element Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在模板设计器中新增“数组网格”元素，用固定行列数渲染接口或模拟数据中的数组。

**Architecture:** 数组网格作为 `TemplateElementType::ArrayGrid` 存在，复用普通元素的几何、字体、加粗、旋转等属性，并新增 `dataPath`、行数、列数、单元格模板、边框开关。渲染仍由 core 层 `TemplateDocumentRenderer` 根据 `TemplateRenderContext` 输出原生绘制命令，Qt 预览和真实打印共享结果。

**Tech Stack:** C++、Qt Widgets、Qt JSON、Qt Test、现有模板文档和绘制计划模型。

---

### Task 1: Core Rendering And JSON Tests

**Files:**
- Modify: `tests/tst_core.cpp`
- Modify: `include/sleekpr/core/settings/TemplateElement.h`
- Modify: `src/core/settings/PrintClientSettingsJson.cpp`
- Modify: `src/core/templates/TemplateDocumentRenderer.cpp`

- [ ] **Step 1: Write failing tests**

Add tests that create an `ArrayGrid` element with `dataPath=header_items`, `arrayGridRows=2`, `arrayGridColumns=3`, `arrayGridCellTemplate=${text}:${value}` and assert 6 cell borders plus 4 text commands are rendered from a 4-item array. Add JSON round-trip assertions for the new properties.

- [ ] **Step 2: Verify red**

Run `.\rebuild.cmd -NoDeploy`; expected compile failure because `ArrayGrid` fields and enum value do not exist.

- [ ] **Step 3: Implement minimal core support**

Add enum/property fields, persist them in JSON, and render row-major cells. Missing or invalid data renders only empty cells, not a crash.

### Task 2: Designer UI Tests And Implementation

**Files:**
- Modify: `tests/tst_app.cpp`
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`

- [ ] **Step 1: Write failing UI test**

Add a test that finds `designerAddArrayGridButton`, clicks it, edits `arrayGridDataPathEdit`, `arrayGridRowsSpin`, `arrayGridColumnsSpin`, `arrayGridCellTemplateEdit`, and verifies settings contain an `ArrayGrid` element.

- [ ] **Step 2: Verify red**

Run `.\rebuild.cmd -NoDeploy`; expected failure because the designer controls do not exist.

- [ ] **Step 3: Implement UI**

Add the button, default array grid element, property editor controls, refresh and apply logic.

### Task 3: Final Verification

**Files:**
- All modified files

- [ ] **Step 1: Run full verification**

Run:

```powershell
.\rebuild.cmd -NoDeploy
```

Then:

```powershell
ctest --test-dir build --output-on-failure
```

Expected: all 3 test targets pass.
