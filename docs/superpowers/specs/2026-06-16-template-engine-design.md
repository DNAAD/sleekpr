# 通用打印模板引擎设计

## 背景

sleekpr 当前已经具备 Qt 原生标签预览、模板设计器、图层、模板导入导出、版本管理和设备 profile。现有模型主要面向 80mm x 30mm 标签，模板数据保存在 `PrintClientSettings::templateDocuments` 中。

后续需求会覆盖不同尺寸标签和复杂表格数据打印，因此需要把现有标签模板能力升级为通用打印模板引擎。目标是同时支持小标签、吊牌、热敏单据、A4 单据和带动态明细行的表格模板，并且允许保存模板和字段值方案供后续复用。

## 目标

1. 支持任意纸张规格，包括标签宽高、A4、热敏纸、横向和纵向。
2. 支持标签类自由布局模板，保留现有文本、绑定字段、二维码和矩形能力。
3. 支持表格类模板，包含动态行、表头、表尾、分页、重复表头和合计字段。
4. 支持字段定义和字段值方案，字段值可以自定义、保存、复用，并能被本次打印请求覆盖。
5. 保留旧接口和旧配置兼容，避免当前标签打印流程一次性重写。
6. 保持分层清晰，核心模板渲染不依赖 Qt 窗口，不依赖具体打印设备。

## 非目标

1. 第一版不引入数据库，优先使用文件型模板库。
2. 第一版不实现可视化表格公式编辑器，只提供明确的数据模型和基础 UI。
3. 第一版不实现云同步、多用户权限和审计日志。
4. 第一版不移除旧 `templateElements` 和 `/print/tag`，只做兼容迁移。

## 推荐方案

采用文件型模板库方案。`settings.json` 只保存本机设置和默认选择，模板、纸张规格、字段定义、字段值方案拆成独立文件。

推荐目录结构：

```text
AppData/zytxt/sleekpr/
  settings.json
  paper-specs.json
  field-presets.json
  templates/
    gold-label-80x30.sleekpr-template.json
    silver-label-80x30.sleekpr-template.json
    sale-order-a4.sleekpr-template.json
```

这个方案便于导入导出、人工排查、备份和后续迁移到 SQLite。模板文件仍使用 `QSaveFile` 原子保存。

## 核心模型

### PaperSpec

`PaperSpec` 表示纸张规格，不属于某次打印数据。

字段建议：

```text
id
name
kind: label | roll | sheet
widthMm
heightMm
orientation
marginLeftMm
marginTopMm
marginRightMm
marginBottomMm
defaultDpi
labelGrid
```

`labelGrid` 用于 A4 多标签纸或一页多枚标签，包含行数、列数、横向间距、纵向间距。普通单张标签可以不启用。

### TemplateDocument

`TemplateDocument` 表示模板文档，继续承载图层、元素、版本和设备 profile，但需要新增 `paperSpecId` 和模板分类。

字段建议：

```text
schemaVersion
id
name
category: label | document | table
paperSpecId
activeVersionId
layers
versions
deviceProfiles
fieldSchema
```

模板只保存版式和字段定义，不保存某次业务字段值。

### TemplateElement

继续保留现有元素类型，并扩展为：

```text
fixedText
boundField
qrCode
barcode
rectangle
line
image
table
```

普通元素继续使用毫米坐标。表格元素作为一级元素存在，不能用多个文本框模拟。

### TableElement

`TableElement` 表示复杂表格区域。

字段建议：

```text
id
x
y
width
height
dataPath
headerRows
detailRow
summaryRows
footerRows
columns
repeatHeaderOnPage
rowHeightPolicy
pageBreakPolicy
```

`dataPath` 例如 `items`，表示明细数组来源。列定义包含字段 key、列宽、对齐、字体、边框、换行、省略号和格式化规则。

表格渲染必须负责：

1. 根据可用高度拆分明细行。
2. 跨页时重复表头。
3. 渲染页内小计和全文合计。
4. 处理单元格换行、省略和最小行高。
5. 保证分页后每页输出都是确定的绘制命令。

### FieldSchema

`FieldSchema` 描述模板需要哪些字段。

字段建议：

```text
key
displayName
type: text | number | money | weight | date | datetime | boolean | list | object
required
defaultValue
sampleValue
format
source: builtIn | custom
```

内置字段来自现有 `LabelRenderPlan` 和业务打印请求，自定义字段由用户在模板或字段管理窗口中创建。

### FieldPreset

`FieldPreset` 表示可复用字段值方案。

字段建议：

```text
id
name
templateId
values
updatedAt
```

典型用途包括门店地址、执行标准、固定备注、二维码前缀、打印员名称、表格底部声明。

### TemplateRenderContext

渲染入口统一接收 `TemplateRenderContext`。

字段值优先级：

```text
本次打印请求字段 > 已选字段值方案 > 模板字段默认值 > 空值
```

复杂表格数据也放在上下文中，例如：

```json
{
  "customerName": "张三",
  "items": [
    { "productName": "足金戒指", "weight": "3.21g", "price": "1280.00" }
  ]
}
```

## 渲染流程

通用渲染流程：

```text
TemplateDocument
+ PaperSpec
+ TemplateRenderContext
+ DeviceProfile
=> PrintRenderDocument
=> NativeLabelDrawingPlan 或后续更通用的 NativePrintDrawingPlan
=> Qt 预览 / Qt 打印
```

第一阶段可以继续输出现有 `NativeLabelDrawingPlan`，但需要命名上预留升级空间。等表格分页进入后，应新增 `NativePrintDrawingPlan`，支持多页绘制命令。

## HTTP 接口

保留现有接口：

```text
GET /settings
POST /settings
GET /printers
POST /print/tag
```

新增接口建议：

```text
GET /templates
GET /templates/{id}
POST /templates
PUT /templates/{id}
DELETE /templates/{id}
GET /paper-specs
PUT /paper-specs/{id}
GET /field-presets
POST /field-presets
PUT /field-presets/{id}
POST /print/template
```

`/print/tag` 内部映射到默认标签模板，确保旧调用方不需要立刻调整。

## Qt 界面

建议拆成三个窗口或三个页签：

1. 模板库：模板列表、新建、复制、删除、导入、导出。
2. 纸张规格：标签尺寸、A4、热敏纸、一页多标签规格管理。
3. 模板设计器：自由元素、表格元素、字段面板、版本和设备 profile。

字段值方案可以放在模板设计器右侧，也可以作为独立“字段方案”页签。第一版建议放在模板库详情区域，避免设计器继续膨胀。

## 存储和兼容

1. 旧 `settings.json` 中的 `templateDocuments` 继续读取。
2. 启动时发现旧模板，可以迁移或复制到 `templates/` 目录。
3. 旧 `templateElements` 继续作为兼容输入，打开设计器时仍可迁移到完整 `TemplateDocument`。
4. 新模板文件需要包含 `schemaVersion`。导入未知版本时拒绝并给出中文错误。
5. 保存使用临时文件和原子替换，避免模板文件半写入。

## 错误处理

1. 缺少纸张规格时，阻止打印并提示选择纸张规格。
2. 缺少必填字段时，阻止打印并列出字段中文名。
3. 表格数据不是数组时，阻止打印并说明 `dataPath`。
4. 表格分页无法容纳单行时，阻止打印并提示调整行高、字体或纸张。
5. 二维码或条码内容超过当前编码能力时，阻止打印并提示内容长度。
6. 设备 profile 不匹配打印机时，使用默认 profile，并在日志中记录。

## 测试策略

核心测试：

1. `PaperSpecJson` 能保存和读取不同尺寸、方向和多标签网格。
2. 模板库能新增、覆盖、删除、导入、导出模板。
3. 字段值优先级符合规则。
4. 旧 `settings.json` 能迁移到模板库，不丢失旧标签元素。
5. 表格元素能把明细数组渲染成确定的绘制命令。
6. 表格分页能重复表头并保留合计。
7. `/print/tag` 兼容旧流程。
8. `/print/template` 能用模板、字段方案和本次字段值生成打印任务。

界面测试：

1. 模板库窗口能列出模板并打开设计器。
2. 纸张规格窗口能保存不同尺寸。
3. 模板设计器能新增表格元素。
4. 字段方案保存后重新打开仍可使用。

## 实施分期

### 第一阶段：纸张规格和模板库

新增 `PaperSpec`、`TemplateRepository`、模板文件保存目录和模板列表 UI。现有 80mm x 30mm 标签迁移为默认 `PaperSpec`。

### 第二阶段：字段定义和字段值方案

新增 `FieldSchema`、`FieldPreset`、字段值合并器和字段管理 UI。预览和打印使用统一 `TemplateRenderContext`。

### 第三阶段：表格元素基础能力

新增 `TableElement`、列定义、动态行绑定和单页表格渲染。先支持固定行高和基础边框。

### 第四阶段：表格分页和复杂规则

支持自动分页、重复表头、页内小计、总计、单元格换行和溢出策略。

### 第五阶段：通用打印接口和打包完善

新增 `/print/template`，完成模板库导入导出、设备 profile 和不同打印机 DPI 校准闭环。

## 设计决策

1. 第一版使用文件型模板库，不引入数据库。
2. 表格是一级元素，不用文本框模拟。
3. 模板保存版式，字段方案保存可复用值，本次请求保存临时覆盖值。
4. 旧接口继续保留，新能力通过新接口和新模板库逐步接入。
5. core 只负责模板模型和渲染计划，Qt 窗口和真实打印设备留在 app 和 infrastructure 层。
