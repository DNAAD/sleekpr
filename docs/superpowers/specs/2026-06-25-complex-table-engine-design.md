# 复杂表格引擎设计规格

## 目标

在现有 `TableElement`、右侧列配置表格和 Qt 原生渲染链路基础上，升级出可长期演进的复杂表格能力。目标覆盖画布拖拽列宽、自动行高、自动换行、多行单元格、汇总行、小计行、分组行、页脚行、合并单元格、单元格级样式覆盖，以及更完整的分页、重复表头和溢出处理。

正式预览、HTTP 预览和真实打印必须继续共用 Qt 原生渲染链路。Web 端只接收并显示 PNG，不参与表格排版。

## 范围

本设计包含：

- core 层复杂表格数据模型扩展。
- core 层表格布局和分页结果模型。
- infrastructure 层沿用 `NativeDrawCommand` 到 Qt 预览/打印的链路。
- app 层右侧属性面板扩展，包括行、单元格、合并区域和样式配置。
- app 层画布列边界命中和拖拽调宽。
- JSON 持久化兼容旧模板。
- HTTP 预览和打印错误传播，不静默吞掉表格布局错误。

本设计不包含：

- 浏览器端表格排版。
- 云端协同编辑。
- Excel 文件导入导出。
- 公式引擎和跨表引用。
- 数据库迁移。

## 分层原则

core 层只描述业务模型、布局规则和绘制命令，不依赖 Qt Widgets。

infrastructure 层负责把绘制命令交给 Qt 预览和 Qt 打印，不理解设计器控件。

http 层只负责本地接口、请求限制、错误码和预览图片响应，不包含表格排版逻辑。

app 层负责设计器 UI、画布命中、属性面板、命令和 Presenter，不直接实现打印级排版算法。

## 推荐实施阶段

### 第一阶段：核心模型和布局引擎

扩展 `TableElement`，新增行、单元格、样式、合并区域和溢出策略模型。新增独立布局器，把表格数据和模板转换成分页后的单元格矩形和文本绘制命令。

第一阶段只要求通过 core 测试，不强依赖复杂 UI。

### 第二阶段：Qt 原生渲染能力

让 `TableElementRenderer` 支持自动换行、多行文本、自动行高、合并单元格、单元格样式和分页错误传播。`TemplateDocumentRenderer::tryRenderPrint()` 必须返回具体错误，预览和打印不能静默丢表格。

### 第三阶段：设计器右侧复杂表格面板

在现有表格属性页中新增 Tab 或折叠区：

- 列配置
- 行配置
- 单元格样式
- 合并单元格
- 分页与溢出

面板只发语义信号，窗口层通过 Presenter/Command 修改模型。

### 第四阶段：画布交互

画布命中表格列边界时进入列宽拖拽模式。拖拽只调整当前表格的列宽，不移动整个表格。命中普通区域仍保持当前整体拖拽行为。

### 第五阶段：产品化校验和 HTTP 错误

完善校验提示、预览错误 UI、HTTP 413/422 响应和分页上限联动，确保复杂模板不会拖垮本地客户端。

## 核心模型

### TableColumn

保留现有字段，并新增列级默认样式和文本策略：

```text
id
title
fieldKey
widthMode: fixed | flex
widthMm
flexWeight
alignment
fontSizePt
bold
ellipsis
wrapText
minWidthMm
defaultCellStyleId
```

旧模板没有 `wrapText`、`minWidthMm` 和 `defaultCellStyleId` 时，读取时使用默认值，保持旧行为。

### TableRowBand

新增表格行带模型，表示不同语义的行：

```text
id
kind: header | detail | summary | subtotal | groupHeader | groupFooter | footer
title
dataPath
heightMode: fixed | auto
heightMm
minHeightMm
repeatOnPage
printOn: firstPage | everyPage | lastPage
cells
```

`detail` 行从 `TableElement.dataPath` 读取数组。`summary`、`subtotal` 和 `footer` 行可以绑定普通字段，也可以使用内置聚合字段。

### TableCellTemplate

新增单元格模板模型：

```text
id
rowBandId
columnId
textTemplate
fieldKey
styleId
overflowPolicy: clip | ellipsis | wrap | shrink
maxLines
colSpan
rowSpan
visible
```

如果 `textTemplate` 非空，优先按 `${field}` 占位符渲染。否则使用 `fieldKey`。旧列模型没有单元格模板时，由渲染器按列定义自动生成表头和明细单元格，兼容现有模板。

### TableCellStyle

新增单元格样式模型：

```text
id
fontSizePt
bold
alignment
verticalAlignment: top | middle | bottom
paddingLeftMm
paddingTopMm
paddingRightMm
paddingBottomMm
drawBorder
borderWidthMm
backgroundColor
textColor
wrapText
ellipsis
```

颜色使用 `#RRGGBB` 字符串保存。Qt 原生渲染时使用 `QColor` 解析，解析失败回退默认颜色。

### TableMergeRegion

新增合并单元格区域：

```text
id
rowBandId
startRowOffset
startColumnId
rowSpan
colSpan
```

合并区域只允许在同一个行带内生效。跨页合并不支持；如果合并区域跨分页边界，布局器返回错误，提示调整行高、纸张或分页策略。

### TablePaginationPolicy

新增分页策略：

```text
repeatHeaderOnPage
keepGroupTogether
allowRowSplit
maxPages
overflowPolicy: error | clip | continue
orphanDetailRows
```

默认策略为 `repeatHeaderOnPage=true`、`allowRowSplit=false`、`overflowPolicy=error`，确保打印结果可预测。

## 布局和渲染规则

表格布局器输入：

```text
TableElement
TemplateRenderContext
availablePageRect
```

输出：

```text
TableLayoutResult
  pages
  cells
  commands
  errorMessage
```

布局顺序：

1. 解析列宽，固定列优先，弹性列瓜分剩余宽度。
2. 生成行带：表头、分组、明细、汇总、页脚。
3. 计算每个单元格文本。
4. 根据换行、最大行数、字体和 padding 计算单元格期望高度。
5. 固定行高使用 `heightMm`，自动行高取同一行所有单元格期望高度的最大值。
6. 应用合并单元格，隐藏被合并覆盖的单元格。
7. 按分页策略拆页。
8. 输出 `NativeDrawCommand`。

自动换行和多行单元格必须由 Qt 原生文本测量或等价的核心测量适配完成，不能让浏览器决定换行。

## 溢出处理

`clip`：按单元格矩形裁切，不报错。

`ellipsis`：单行或多行末尾显示省略号。

`wrap`：自动换行，如果高度不足则按分页策略处理。

`shrink`：在单元格最小字号范围内缩小字体；仍放不下时按 `overflowPolicy` 返回错误或裁切。

正式打印默认不允许静默溢出。若 `overflowPolicy=error`，渲染器必须返回具体表格、行、列和页码。

## 画布列宽拖拽

新增表格命中结果：

```text
TableHitResult
  tableId
  kind: body | columnBoundary
  columnId
  nextColumnId
  positionMm
```

命中列边界时：

1. 鼠标样式变为水平调整。
2. 按下后进入列宽拖拽模式。
3. 拖拽过程中只更新当前列宽，预览走已有合并刷新队列。
4. 松开后写入撤销历史。

固定列拖拽直接修改 `widthMm`。弹性列拖拽先转换为固定列，避免拖拽结果随表格总宽变化而不可预测。

## 设计器属性面板

右侧表格属性页继续保留基础属性和列配置表格。新增：

- 行配置表：行类型、标题、高度模式、固定高度、重复页、显示页。
- 单元格配置表：行带、列、文本模板、字段、样式、溢出策略、最大行数。
- 合并区域表：行带、起始列、跨列、跨行。
- 样式库表：字号、加粗、对齐、padding、边框、颜色。
- 分页设置：重复表头、组不拆分、允许行拆分、最大页数、溢出策略。

属性面板不直接读写 `TemplateDocument`。它只产出设计器属性模型，由 `DesignerPresenter` 和 `Command` 应用。

## JSON 兼容

旧模板只有 `columns` 时继续可读可写。新增字段放在 `TableElement` 下：

```json
{
  "columns": [],
  "rowBands": [],
  "cellTemplates": [],
  "cellStyles": [],
  "mergeRegions": [],
  "pagination": {}
}
```

读取旧模板时：

- 自动生成一个 header 行带。
- 自动生成一个 detail 行带。
- 表头单元格来自 `TableColumn.title`。
- 明细单元格来自 `TableColumn.fieldKey`。
- 旧 `repeatHeaderOnPage` 映射到 `pagination.repeatHeaderOnPage`。

保存时继续保留 `columns` 字段，方便旧调试工具查看。新字段只有在用户启用复杂表格能力后写入。

## 校验和错误提示

保存模板前校验：

- 表格至少一列。
- 列 id 不重复。
- 行带 id 不重复。
- 单元格引用的行带和列必须存在。
- 合并区域不能越界，不能重叠。
- 合并区域不能跨分页边界。
- 自动行高不能超过页面可用高度。
- 最大页数不能超过本地设置中的预览页数限制。

错误应在设计器状态栏和表格属性页内同时显示。HTTP 预览和打印返回结构化错误，状态码优先使用 422；请求或响应体超限仍使用 413。

## 测试策略

core 测试：

- JSON 兼容旧 `columns`。
- 单元格样式 round-trip。
- 合并单元格布局。
- 自动换行和自动行高。
- 分页重复表头。
- 页脚和汇总行。
- 溢出错误传播。

app 测试：

- 属性面板展示行、单元格、合并和样式配置。
- 画布命中列边界。
- 拖拽列宽后预览变化并写入模板。
- 自动应用不重建图层列表。

http 测试：

- `/preview/template` 返回复杂表格预览图片。
- 布局错误返回 422。
- 预览页数和响应体大小仍受本地上限控制。

## 验收标准

- 用户可以在设计器里配置复杂表格，而不需要手写 JSON。
- 自动换行、多行单元格和自动行高在预览和真实打印中一致。
- 汇总、小计、分组和页脚行可保存、预览和打印。
- 合并单元格可保存、预览和打印，非法合并有明确错误。
- 画布列宽拖拽能稳定修改列宽，并支持撤销。
- Web 预览仍只显示 Qt 生成的图片。
- 旧模板继续可加载、预览和打印。
