# 通用打印模板引擎阶段二实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 建立字段定义、字段值方案和字段值合并规则，让标签、单据和后续复杂表格模板都能使用统一字段上下文。

**架构：** 阶段二第一批只进入 `core/templates`，不修改现有 `/print/tag` 兼容流程。模板保存字段定义，字段值方案保存可复用默认值，本次打印请求值通过合并器覆盖方案值和字段默认值。

**技术栈：** C++20、Qt Core JSON、Qt Test、现有 CMake 目标。

---

## 文件结构

- `include/sleekpr/core/templates/FieldSchema.h`：字段类型、字段来源和字段定义模型。
- `include/sleekpr/core/templates/FieldSchemaJson.h`：字段定义 JSON 序列化和校验接口。
- `src/core/templates/FieldSchemaJson.cpp`：字段定义 JSON 实现。
- `include/sleekpr/core/templates/FieldPreset.h`：可复用字段值方案模型。
- `include/sleekpr/core/templates/FieldPresetJson.h`：字段值方案 JSON 序列化和校验接口。
- `src/core/templates/FieldPresetJson.cpp`：字段值方案 JSON 实现。
- `include/sleekpr/core/templates/TemplateRenderContext.h`：渲染字段上下文模型。
- `include/sleekpr/core/templates/TemplateRenderContextBuilder.h`：字段值优先级合并器接口。
- `src/core/templates/TemplateRenderContextBuilder.cpp`：字段值合并实现。
- `include/sleekpr/core/templates/TemplateDocument.h`：新增 `fieldSchema` 字段。
- `src/core/templates/TemplateDocumentJson.cpp`：保存、读取和导入校验 `fieldSchema`。
- `tests/tst_core.cpp`：TDD 覆盖字段模型、方案模型和合并优先级。
- `CMakeLists.txt`：加入新增 core 源文件。

---

### 任务 1：字段定义模型和 JSON

**文件：**
- 创建：`include/sleekpr/core/templates/FieldSchema.h`
- 创建：`include/sleekpr/core/templates/FieldSchemaJson.h`
- 创建：`src/core/templates/FieldSchemaJson.cpp`
- 修改：`CMakeLists.txt`
- 测试：`tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

在 `tests/tst_core.cpp` 增加槽函数：

```cpp
void fieldSchemaJsonPersistsCustomFieldDefinitions();
void fieldSchemaJsonRejectsInvalidFieldDefinitions();
```

测试字段 key、中文名、类型、必填、默认值、示例值、格式和来源能完整往返；无 key 或未知类型应被拒绝并返回中文错误。

- [ ] **步骤 2：运行红灯测试**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build --target sleekpr_core_tests'
```

预期：编译失败，因为 `FieldSchema` 和 `FieldSchemaJson` 尚不存在。

- [ ] **步骤 3：实现模型**

`FieldSchema.h` 定义：

```cpp
enum class FieldValueType { Text, Number, Money, Weight, Date, DateTime, Boolean, List, Object };
enum class FieldSource { BuiltIn, Custom };

struct FieldDefinition {
    QString key;
    QString displayName;
    FieldValueType type = FieldValueType::Text;
    bool required = false;
    QString defaultValue;
    QString sampleValue;
    QString format;
    FieldSource source = FieldSource::Custom;
};
```

- [ ] **步骤 4：实现 JSON**

`FieldSchemaJson` 提供 `toJson()`、`fromJson()`、`validate()`，字段数组名称使用 `fields`。校验必须拒绝空 key、重复 key、未知 type、未知 source。

- [ ] **步骤 5：加入 CMake 并跑绿灯**

运行与步骤 2 相同的 build 命令，并追加：

```powershell
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build -R sleekpr_core_tests --output-on-failure
```

- [ ] **步骤 6：提交**

```powershell
git add CMakeLists.txt include/sleekpr/core/templates/FieldSchema.h include/sleekpr/core/templates/FieldSchemaJson.h src/core/templates/FieldSchemaJson.cpp tests/tst_core.cpp
git commit -m "feat: add template field schema json"
```

---

### 任务 2：模板文档保存字段定义

**文件：**
- 修改：`include/sleekpr/core/templates/TemplateDocument.h`
- 修改：`src/core/templates/TemplateDocumentJson.cpp`
- 测试：`tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

增加测试：

```cpp
void templateDocumentJsonPersistsFieldSchema();
void templateDocumentImportRejectsInvalidFieldSchema();
```

验证 `TemplateDocument::fieldSchema` 可往返，导入模板时字段定义不合法会拒绝。

- [ ] **步骤 2：运行红灯测试**

预期：编译失败，因为 `TemplateDocument::fieldSchema` 尚不存在。

- [ ] **步骤 3：添加字段**

在 `TemplateDocument` 增加：

```cpp
// 模板只保存字段定义，不保存某次业务打印值；字段值由方案和本次请求合并生成。
QList<FieldDefinition> fieldSchema;
```

- [ ] **步骤 4：接入 JSON**

`TemplateDocumentJson::toJson()` 写入 `fieldSchema`，`fromJson()` 读取，`validateForImport()` 在字段存在时调用 `FieldSchemaJson::validate()`。

- [ ] **步骤 5：跑绿灯并提交**

运行 `sleekpr_core_tests`，通过后提交：

```powershell
git add include/sleekpr/core/templates/TemplateDocument.h src/core/templates/TemplateDocumentJson.cpp tests/tst_core.cpp
git commit -m "feat: persist template field schema"
```

---

### 任务 3：字段值方案模型和 JSON

**文件：**
- 创建：`include/sleekpr/core/templates/FieldPreset.h`
- 创建：`include/sleekpr/core/templates/FieldPresetJson.h`
- 创建：`src/core/templates/FieldPresetJson.cpp`
- 修改：`CMakeLists.txt`
- 测试：`tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

增加测试：

```cpp
void fieldPresetJsonPersistsReusableValues();
void fieldPresetJsonRejectsInvalidPreset();
```

验证方案 id、名称、templateId、updatedAt 和 `values` 可往返；空 id、空 templateId 或非对象 values 应拒绝。

- [ ] **步骤 2：实现模型**

`FieldPreset.h` 定义：

```cpp
struct FieldPreset {
    int schemaVersion = 1;
    QString id;
    QString name;
    QString templateId;
    QJsonObject values;
    QString updatedAt;
};
```

- [ ] **步骤 3：实现 JSON 并跑绿灯**

`FieldPresetJson` 提供 `toJson()`、`fromJson()`、`validate()`，错误信息必须是中文。

- [ ] **步骤 4：提交**

```powershell
git add CMakeLists.txt include/sleekpr/core/templates/FieldPreset.h include/sleekpr/core/templates/FieldPresetJson.h src/core/templates/FieldPresetJson.cpp tests/tst_core.cpp
git commit -m "feat: add field preset json"
```

---

### 任务 4：字段值合并器

**文件：**
- 创建：`include/sleekpr/core/templates/TemplateRenderContext.h`
- 创建：`include/sleekpr/core/templates/TemplateRenderContextBuilder.h`
- 创建：`src/core/templates/TemplateRenderContextBuilder.cpp`
- 修改：`CMakeLists.txt`
- 测试：`tests/tst_core.cpp`

- [ ] **步骤 1：写失败测试**

增加测试：

```cpp
void templateRenderContextBuilderMergesValuesByPriority();
void templateRenderContextBuilderReportsMissingRequiredFields();
```

优先级必须是：本次请求字段值 > 字段值方案 > 字段默认值 > 空值。必填字段最终为空时，返回中文字段名。

- [ ] **步骤 2：实现上下文模型**

`TemplateRenderContext` 至少包含：

```cpp
QJsonObject values;
QStringList missingRequiredFieldNames;
bool hasMissingRequiredFields() const;
```

- [ ] **步骤 3：实现合并器**

`TemplateRenderContextBuilder::build(fieldSchema, presetValues, requestValues)` 按优先级生成 `values`，并记录缺失的必填字段中文名。

- [ ] **步骤 4：跑绿灯并提交**

```powershell
git add CMakeLists.txt include/sleekpr/core/templates/TemplateRenderContext.h include/sleekpr/core/templates/TemplateRenderContextBuilder.h src/core/templates/TemplateRenderContextBuilder.cpp tests/tst_core.cpp
git commit -m "feat: add template render context builder"
```

---

### 任务 5：完整验证

**文件：**
- 仅测试。

- [ ] **步骤 1：运行完整验证**

```powershell
& $env:ComSpec /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build && set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH% && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure'
```

预期：

```text
100% tests passed, 0 tests failed out of 3
```

- [ ] **步骤 2：检查工作区**

```powershell
git status --short
git log --oneline -6
```

预期：只剩既有无关 `README.md` 和 `docs/superpowers/plans/2026-06-15-settings-window.md`。
