# 复杂表格设计器设计规格

## 目标

在模板设计器右侧属性面板中提供专业的复杂表格列编辑器，让用户可以用可视化表格配置列标题、字段绑定、列宽策略、对齐、字号、加粗和省略号，而不是依赖手写列配置文本。第一阶段只增强设计器编辑体验，复用现有 `TableElement`、分页、重复表头、固定/弹性列宽和 Qt 原生渲染链路。

## 范围

本阶段包含：

- 在右侧“表格”属性页中新增列配置表格。
- 支持新增列、删除列、复制列、上移、下移、恢复默认列。
- 支持编辑列标题、字段 key、宽度模式、固定宽度、弹性权重、对齐、字号、加粗、省略号。
- 修改后自动应用，并继续走轻量刷新队列，避免属性编辑卡顿。
- 保留现有列配置文本作为高级文本配置入口，确保旧模板和已有测试不被破坏。
- 预览和打印仍使用现有 Qt 原生表格渲染链路，浏览器只展示图片，不参与排版。

本阶段不包含：

- 画布上直接拖拽列宽。
- 自动行高、自动换行、多行单元格。
- 汇总行、小计行、分组行、页脚行。
- Excel 式合并单元格。
- 表格内部单元格级样式覆盖。

## 用户体验

用户选中表格元素后，右侧属性面板自动切到“表格”页。页面上方显示表格基础属性，包括名称、`dataPath`、位置、尺寸、表头行高、明细行高、重复表头和边框。页面下方显示“列配置”区域。

列配置区域使用 Qt 原生表格控件展示列定义，每一行代表一个 `TableColumn`。用户可以直接编辑单元格，也可以通过按钮新增、删除、复制和排序。选中一行后，删除、复制、上移和下移按钮作用于当前列。恢复默认列会生成与现有默认表格一致的列集合，并弹出确认提示，避免误覆盖。

高级文本配置保留在折叠区域中，用于兼容已有的 `标题=字段:宽度` 简写能力。用户直接修改高级文本配置时，系统继续使用现有解析逻辑写回列集合；用户通过列配置表格编辑时，系统同步更新高级文本配置文本。

## 架构

### app 层

新增 `TableColumnEditorPanel`，只负责列配置 UI、控件状态和语义信号，不直接修改 `TemplateDocument`。它接收设计器侧列模型并渲染到 Qt 表格控件，对外发出以下语义信号：

- `columnEdited(int row, DesignerTableColumnModel column)`
- `columnAddRequested()`
- `columnDeleteRequested(int row)`
- `columnDuplicateRequested(int row)`
- `columnMoveRequested(int fromRow, int toRow)`
- `columnsResetRequested()`

新增 `DesignerTableColumnModel`，放在设计器属性模型中，作为 `TableColumnEditorPanel` 与 Presenter 之间的数据结构。它包含列 id、标题、字段 key、宽度模式、固定宽度、弹性权重、对齐、字号、加粗和省略号。

`TemplateInspectorPanel` 负责组合表格基础属性和 `TableColumnEditorPanel`，并继续只发语义信号。窗口层不直接读写列编辑器内部控件，只读取 `DesignerTablePropertyModel` 或响应语义信号。

### Presenter 和 Command

`TemplateDesignerPresenter` 负责：

- `TableColumn` 到 `DesignerTableColumnModel` 的转换。
- `DesignerTableColumnModel` 到 `TableColumn` 的转换。
- 表格属性模型中列集合和高级文本配置的同步。

`TemplateDesignerCommand` 负责：

- 应用整张表格属性。
- 应用单列属性。
- 新增、删除、复制、排序、恢复默认列。
- 保证列 id 稳定，新增和复制列时生成新的列 id。

窗口层只协调当前选中表格、调用 Presenter/Command、触发轻量刷新、合并设置变更和撤销历史。

### core 和 infrastructure 层

第一阶段不修改 `TableElement` 的持久化结构，不修改 `TableElementRenderer` 的分页和绘制语义。列配置表格最终仍写回现有 `QList<TableColumn>`。

预览和打印链路保持：

`TemplateDocument` -> `TemplateDocumentRenderer` -> `NativeDrawCommand` -> `LabelPreviewImageRenderer` / `QtLabelPrintEngine`

Web 预览和打印接口不新增排版逻辑。

## 数据流

1. 用户选中表格元素。
2. `TemplateDesignerWindow` 获取当前 `TableElement`。
3. `TemplateDesignerPresenter` 生成 `DesignerTablePropertyModel`，其中包含列模型列表和高级文本配置。
4. `TemplateInspectorPanel` 将基础属性渲染到控件，将列模型传给 `TableColumnEditorPanel`。
5. 用户编辑列配置。
6. `TableColumnEditorPanel` 发出语义信号。
7. `TemplateDesignerWindow` 调用 `TemplateDesignerCommand` 修改当前 `TableElement.columns`。
8. 窗口触发轻量属性刷新、预览刷新和设置变更合并。
9. 预览和打印继续走 Qt 原生渲染链路。

## 校验与错误处理

- 表格至少保留一列；删除最后一列时拒绝，并在状态栏提示。
- 列标题为空时允许保存，但字段 key 为空时预览显示空文本，不中断渲染。
- 固定宽度小于等于 0 时自动修正到最小宽度。
- 弹性权重小于等于 0 时自动修正为 1。
- 对齐、宽度模式使用下拉选项，避免非法值。
- 高级文本配置解析失败时，不覆盖当前列集合，并在表格页内显示错误提示。

## 兼容性

现有模板中的 `columns` JSON 结构保持不变。现有高级文本配置语法继续可用。旧模板加载后，列配置表格按现有 `TableColumn` 数据展示；用户保存后仍写回同一结构。

已有 `/preview/template`、`/print/template`、模板库导入导出、版本保存恢复不需要新增 schema。只要模板文档含有合法 `TableColumn` 列集合，所有入口都能复用同一渲染能力。

## 测试策略

核心测试：

- `TableColumnEditorPanel` 能展示列集合并发出新增、删除、复制、移动和编辑信号。
- `TemplateDesignerPresenter` 能在 `TableColumn` 与 `DesignerTableColumnModel` 之间双向转换。
- `TemplateDesignerCommand` 能新增、删除、复制、排序和应用列属性，并保持列 id 稳定。
- `TemplateDesignerWindow` 在列配置编辑后自动应用，并保持元素列表不重建。
- 复杂列配置保存到模板 JSON 后可重新加载。
- 修改列宽、字段和对齐后，预览命令发生对应变化。

回归测试：

- 原有列配置文本仍可解析并写回。
- 表格分页、重复表头和固定/弹性列宽渲染测试继续通过。
- Web 预览和打印不出现浏览器排版路径。

## 交付顺序

1. 新增设计器侧列模型和 Presenter 转换。
2. 新增 `TableColumnEditorPanel` 及单元测试。
3. 接入 `TemplateInspectorPanel`，保留高级文本配置。
4. 增加 `TemplateDesignerCommand` 的列操作能力。
5. 接入 `TemplateDesignerWindow` 自动应用和轻量刷新。
6. 补充模板 JSON、预览命令和 UI 回归测试。

## 验收标准

- 用户无需手写列配置文本即可完成常用复杂表格列设计。
- 新增、删除、复制、排序和编辑列后，画布预览能及时更新。
- 旧模板和旧列配置文本不丢失、不被错误重写。
- 预览、打印、模板库、版本恢复仍走现有 Qt 原生链路。
- 相关 core、app、http 测试全部通过。
