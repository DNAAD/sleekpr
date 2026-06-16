# 通用模板打印接口实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 `POST /print/template`，让 HTTP 调用方可以按完整模板文档、字段值和本机打印机配置生成多页打印任务。

**Architecture:** 路由层负责解析请求、选择模板和打印机；核心层继续负责字段值合并、模板渲染和多页绘制计划。打印设备层只接收 `NativePrintDrawingPlan`，不理解业务字段。

**Tech Stack:** C++20、Qt 6、QtTest、现有 `LocalHttpRouter`、`TemplateRenderContextBuilder`、`TemplateDocumentRenderer`。

---

### Task 1: `/print/template` 最小闭环

**Files:**
- Modify: `tests/tst_http.cpp`
- Modify: `src/core/templates/TemplateDocumentRenderer.cpp`
- Modify: `src/http/LocalHttpRouter.cpp`

- [ ] **Step 1: Write the failing test**

在 `tests/tst_http.cpp` 增加测试：配置一个完整 `TemplateDocument`，其中包含绑定字段 `customerName`；调用 `POST /print/template`，请求体传入 `templateKey`、`values`、`executePrint=true`；断言打印引擎收到多页入口，第一条命令文本为请求字段值。

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_http_tests && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_http_tests --output-on-failure'
```

Expected: FAIL because `/print/template` still returns `NOT_FOUND` or the print engine is not called.

- [ ] **Step 3: Implement minimal route**

在 `TemplateDocumentRenderer.cpp` 中：
- 完整模板文档的普通元素渲染要优先从 `TemplateRenderContext.values` 读取绑定字段和动态二维码。
- 当上下文没有对应字段时，再回退旧 `LabelRenderPlan` 字段，保持 `/print/tag` 兼容。

在 `LocalHttpRouter.cpp` 中：
- 引入 `TemplateRenderContextBuilder`。
- 增加请求 JSON helper，解析 `templateKey/templateId`、`values`、`executePrint`、`requestId`。
- 从 `settings.templateDocuments` 选择模板。
- 用 `TemplateRenderContextBuilder::build(document.fieldSchema, {}, values)` 生成上下文。
- 必填字段缺失时返回 `400 BAD_REQUEST`。
- 调用 `TemplateDocumentRenderer().renderPrint(...)`。
- 打印成功后返回现有 `printResultJson` 包络。

- [ ] **Step 4: Run targeted test**

Run the same `sleekpr_http_tests` command. Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add docs/superpowers/plans/2026-06-16-print-template-endpoint.md tests/tst_http.cpp src/core/templates/TemplateDocumentRenderer.cpp src/http/LocalHttpRouter.cpp
git commit -m "feat: add template print endpoint"
```

### Task 2: Validation and final verification

**Files:**
- Modify: `tests/tst_http.cpp`
- Modify: `src/http/LocalHttpRouter.cpp`

- [ ] **Step 1: Add missing required field test**

在 `tests/tst_http.cpp` 增加测试：模板字段 `customerName.required=true`，请求不传该值，断言接口返回 400，错误 message 包含字段中文名。

- [ ] **Step 2: Run red**

Run targeted HTTP test command. Expected: FAIL until route validates missing required fields.

- [ ] **Step 3: Implement validation response**

在 route 中检查 `context.hasMissingRequiredFields()`，返回中文错误：`缺少必填字段：字段名`。

- [ ] **Step 4: Run full verification**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

Expected: 3/3 tests passed.

- [ ] **Step 5: Run deploy target**

Run:

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target deploy_sleekpr'
```

Expected: exit code 0. Existing Qt translation/OpenSSL warnings are non-blocking.
