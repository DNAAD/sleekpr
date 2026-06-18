# Template Designer Sample Data Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在模板设计器中新增模拟数据 JSON 输入区，让 `${field}` 占位符实时用模拟数据渲染预览。

**Architecture:** 模拟数据只属于设计器 UI 状态，不写入模板文件。`TemplateDesignerWindow` 负责解析 JSON 并构造 `TemplateRenderContext`，继续复用 `TemplateDocumentRenderer` 的占位符渲染逻辑，避免重复业务规则。

**Tech Stack:** C++、Qt Widgets、Qt Test、现有 `TemplateRenderContext` 和 `TemplateDocumentRenderer`。

---

### Task 1: 设计器测试

**Files:**
- Modify: `tests/tst_app.cpp`

- [ ] **Step 1: Write the failing test**

新增测试 `templateDesignerWindowRendersPlaceholdersWithSampleData`：

```cpp
void templateDesignerWindowRendersPlaceholdersWithSampleData();
```

测试创建包含 `${product_name}` 固定文本的模板，找到 `templateSampleDataEdit`，输入不同 JSON 后比较预览图片发生变化，并验证非法 JSON 会在状态栏提示。

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
.\rebuild.cmd -NoDeploy
```

Expected: FAIL because `templateSampleDataEdit` does not exist.

### Task 2: UI 和实时刷新

**Files:**
- Modify: `include/sleekpr/app/TemplateDesignerWindow.h`
- Modify: `src/app/TemplateDesignerWindow.cpp`

- [ ] **Step 1: Add state and helper declarations**

在 `TemplateDesignerWindow` 增加 `QPlainTextEdit* m_sampleDataEdit`，以及解析模拟数据的私有方法。

- [ ] **Step 2: Add the sample data editor**

在中间预览区域状态栏上方增加“模拟数据”输入区，默认填入销售示例 JSON，编辑变化时调用 `refreshPreview()`。

- [ ] **Step 3: Feed context into renderer**

`refreshPreview()` 解析 JSON 成 `TemplateRenderContext`，传入 `TemplateDocumentRenderer().render(...)`。解析失败时状态栏显示错误，并使用空上下文继续渲染。

- [ ] **Step 4: Run tests**

Run:

```powershell
.\rebuild.cmd -NoDeploy
```

Expected: app/core/http tests pass.
