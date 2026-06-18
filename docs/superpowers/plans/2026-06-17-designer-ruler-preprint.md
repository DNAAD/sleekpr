# Designer Ruler And Preprint Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在模板设计器中显示毫米标尺和可选刻度精度，并提供“预打印当前模板”入口，复用 Qt 原生打印链路。

**Architecture:** 标尺属于 `app` 层的设计器预览控件能力，只负责画布辅助线，不改变模板文档。预打印从设计器当前模板、当前纸张规格、模拟数据和设备 profile 生成 `NativePrintDrawingPlan`，再通过可注入回调调用默认 Qt 打印实现，测试中使用回调拦截计划避免真实打印。

**Tech Stack:** C++20, Qt Widgets, Qt PrintSupport, CTest/QtTest.

---

### Task 1: 设计器标尺

**Files:**
- Modify: `include/sleekpr/app/TemplatePreviewLabel.h`
- Modify: `src/app/TemplatePreviewLabel.cpp`
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Test: `tests/tst_app.cpp`

- [ ] **Step 1: Write the failing test**

Add a QtTest case that constructs `TemplateDesignerWindow`, finds `designerRulerPrecisionCombo`, asserts the default precision is `1.0`, changes it to `0.5`, and checks `designerPreviewLabel->rulerPrecisionMm()` changed to `0.5`.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target sleekpr_app_tests && build\sleekpr_app_tests.exe templateDesignerWindowConfiguresRulerPrecision`

Expected: FAIL because the combo and preview label ruler APIs do not exist yet.

- [ ] **Step 3: Implement minimal ruler UI**

Add ruler state and `paintEvent` to `TemplatePreviewLabel`. Add a precision combo to `TemplateDesignerWindow`, connect it to `setRulerPrecisionMm`, and update paper size during preview refresh.

- [ ] **Step 4: Run test to verify it passes**

Run the same app test and expect PASS.

### Task 2: 预打印当前模板

**Files:**
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`
- Test: `tests/tst_app.cpp`

- [ ] **Step 1: Write the failing test**

Add a QtTest case that injects a preprint callback, clicks `prePrintCurrentTemplateButton`, and asserts the callback receives one `NativePrintDrawingPlan` with the current paper size and sample-data-rendered commands.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target sleekpr_app_tests && build\sleekpr_app_tests.exe templateDesignerWindowPrePrintsCurrentTemplate`

Expected: FAIL because the button and callback do not exist yet.

- [ ] **Step 3: Implement preprint callback and default Qt printing**

Add a `PrePrintCallback` to the designer constructor. If absent, call `QtLabelPrintEngine`. Build the plan with `TemplateDocumentRenderer::renderPrint` and the current sample data, validate parse errors, and set a Chinese status message.

- [ ] **Step 4: Run test to verify it passes**

Run the app test and expect PASS.

### Task 3: Full verification

**Files:**
- No new files.

- [ ] **Step 1: Run full tests**

Run: `ctest --test-dir build --output-on-failure`

Expected: 3/3 tests passed.

- [ ] **Step 2: Run rebuild script**

Run: `.\rebuild.cmd -NoDeploy`

Expected: exit code 0.
