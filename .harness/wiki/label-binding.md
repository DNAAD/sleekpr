# 标签绑定知识

## 数据键与显示名称

- `key` 是接口、JSON、预设和正式打印使用的数据标识。`displayName` 是给操作员看的说明，不自动成为标签正文，也不用于查找数据。
- 名称优先级为 `TemplateDocument.fieldDisplayNames` 非空覆盖、显式 `fieldSchema.displayName`、内置中文名称、原 key。显式名称即使等于 key 也不能被内置名称覆盖。
- 覆盖名称按模板内的 key 保存，同一模板相同 key 共用名称；不是每个元素独立命名，也不是全局字典。清空覆盖后恢复默认，不创建字段声明或修改必填、类型、默认值、样例数据。

## 不同元素的引用

| 元素 | 实际取值位置 | 编辑时的边界 |
| --- | --- | --- |
| 绑定字段 | `fieldKey` | 只修改当前元素的键 |
| 固定文本 | `text` 内的 `${key}` | 相同引用列一次，普通文字不能被替换 |
| 二维码 | `payload` 内的 `${key}`；空内容可回退 `fieldKey` | 旧单字段绑定不能因改名或坐标编辑被悄悄转换 |
| 数组网格 | `dataPath` 与 `arrayGridCellTemplate` | 数据源属于顶层输入，单元格字段属于行内上下文，不能提升为顶层必填字段 |
| 纯固定内容、矩形 | 不引用接口数据 | 不显示无意义的绑定配置 |

数组单元格模板为空时按现有渲染规则使用 `${value}`。改变 key 只改当前元素内的精确引用，不迁移其他元素、样例或预设；改变显示名称不改变打印数据。

## 源码定位

- [TemplateFieldNames](../../src/app/TemplateFieldNames.cpp)：名称优先级与共享中文兜底。
- [TemplateElementBindings](../../src/app/TemplateElementBindings.cpp)：引用发现、作用域和一次扫描的精确改名。
- [TemplateDesignerPresenter](../../src/app/TemplateDesignerPresenter.cpp) → [属性命令](../../src/app/TemplateDesignerCommand.cpp)：模型生成与元素、名称的原子写回。
- [ElementBindingEditor](../../src/app/ElementBindingEditor.cpp)、[TemplateInspectorPanel](../../src/app/TemplateInspectorPanel.cpp)：输入状态和语义信号，不直接负责打印。
- [TemplateInputSchema](../../src/core/templates/TemplateInputSchema.cpp)：输入发现、类型及完整性检查；不使用样例替代正式输入。
- [TemplatePrintService](../../src/infrastructure/printing/TemplatePrintService.cpp)：工作台与 HTTP 共用的模板规划。工作台的额外完整性检查不能擅自改变 HTTP 默认契约。

## 提示与输出

校验详情集中在打印前检查面板，底部状态栏只保留保存和提交等操作结果。用户文案使用显示名称，定位保留结构化 key、元素标识及 JSON 行列；不能解析或替换错误字符串来推断定位。

正式预览与打印仍使用 Qt 原生计划。字段名刷新不得丢弃非法草稿，纯名称变化不得触发内容变化确认或改变绘制命令。操作说明见 [标签编辑](../../docs/label-editor-interactions.md) 和 [打印工作台](../../docs/print-workbench.md)。
