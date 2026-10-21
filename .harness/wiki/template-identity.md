# 两类模板身份

## 选择身份与响应标识

| 类型 | 来源与用途 | 不能用于 |
| --- | --- | --- |
| 列表选择键 `TemplatePrintChoice.key` | `@settings:` 加编码后的设置槽位，或 `@library:` 加编码后的库文档 ID；用于精确选择模板来源 | 剥掉来源后用名称或裸 ID 猜测模板 |
| 快照/响应 `templateKey` | 服务按 `TemplateDocument.templateKey` 生成，缺省时按请求标识或 `default` 回退；用于兼容的输出语义和同类快照比较 | 直接与带来源的列表选择键比较 |

例如内嵌槽位为 `default`，文档 `templateKey` 也是 `default`，列表选择键会是 `@settings:default`。两者不相等是正常现象，不能据此丢弃正常预览或禁用打印。同名的设置模板与库模板也不能仅靠响应 `templateKey` 区分。

`TemplateDocument.id` 是文档标识，不自动等于设置槽位、列表键或响应标识。这里的模板身份也不同于字段绑定的 `key/displayName`，见 [标签绑定知识](label-binding.md)。

## 工作台与异步结果

- 列表键应原样交给 `findTemplate/resolveSnapshot`，沿用服务对来源和编码的解析；不能从展示名称反推身份，或自行字符串替换掉来源前缀。
- 当前选择、活动选择与请求代号共同界定预览新鲜度。切换或清空目标时需失效队列，并清理待处理/已准备快照，不能让旧任务回填。
- 发布时检查仍存在同一个有效选择；打印前再比较当前来源、数据和设备输入与冻结快照，变化后重新预览并再次确认。
- 不把 `m_pendingSnapshot->templateKey == m_activeTemplateKey` 当保护条件；前者是响应标识，后者是来源选择键。队列代号已经负责任务的新鲜度；比较快照时应比较同类身份和相关内容。

生命周期和草稿约束见 [异步预览与草稿规则](../rules/async-preview-and-drafts.md)。

## 源码入口

- [TemplatePrintService](../../src/infrastructure/printing/TemplatePrintService.cpp)：`templates()`、`responseTemplateKey()`、`findTemplate()`、`resolveSnapshot()`。
- [工作台选择与打印](../../src/app/PrintWorkbenchWindow.cpp)：选择键、清空目标和提交前快照检查。
- [工作台后台预览](../../src/app/PrintWorkbenchPreview.cpp)：提交与结果发布。
- [工作台回归](../../tests/tst_print_workbench.cpp)：正向打印、来源变化、搜索无匹配与在途任务失效。
