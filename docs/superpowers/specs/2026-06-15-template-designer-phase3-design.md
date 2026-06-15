# 阶段三完整模板设计器设计

## 目标

在 `sleekpr` 原生 Qt 客户端中新增一个完整模板设计器，用于管理本地标签模板、图层、导入导出、版本快照和设备 profile。设计器必须继续服务长期方向：原生桌面客户端、托盘程序、后续扩展设备能力。正式预览仍走 Qt 原生窗口，不恢复浏览器预览。

## 范围

本阶段包含：

- 新增独立 `TemplateDesignerWindow`，不继续把复杂设计器功能塞进 `SettingsWindow`。
- 新增模板文档模型 `TemplateDocument`，承载模板元信息、图层、元素、版本和设备 profile。
- 支持图层列表：显示/隐藏、锁定/解锁、上移/下移、选择当前图层。
- 支持元素在图层内排序，绘制顺序由图层顺序和元素顺序共同决定。
- 支持模板导入导出，文件格式为本地 JSON：`.sleekpr-template.json`。
- 支持版本管理：保存当前版本快照、查看版本列表、恢复到指定版本。
- 支持设备 profile：按打印机名称保存 DPI、缩放和偏移校准参数。
- 预览和打印链路根据当前打印机选择应用对应设备 profile。
- 保留现有 `templateOverrides` 和 `templateElements` 兼容读取能力。

本阶段不包含：

- 云端模板库、登录、多人协作。
- 自动探测每台打印机真实物理 DPI。
- 图像元素、条码、线段、多边形等额外元素类型。
- 复杂排版引擎、表达式语言或脚本系统。
- 恢复浏览器预览页面作为正式预览方式。

## 架构

阶段三新增一条独立模板设计器链路：

1. `SettingsWindow` 保留现有打印机、偏移、简单元素编辑能力，并新增“打开模板设计器”入口。
2. `TemplateDesignerWindow` 负责完整模板编辑体验。
3. `TemplateDocument` 作为 core 层模板文档，不依赖 Qt UI。
4. `TemplateDocumentJson` 负责导入导出和 settings.json 中的集中序列化。
5. `TemplateDocumentRenderer` 或现有 `NativeLabelDrawingPlanner` 的新入口负责把模板文档转成 `NativeDrawCommand`。
6. `DeviceProfileResolver` 负责从当前打印机名匹配 profile，并把 DPI/缩放/偏移传给预览和打印链路。

`core` 只处理模板文档、绘制计划和设备 profile 数据；`app` 只处理窗口交互；`infrastructure/printing` 继续承接真实打印设备差异。

## 数据模型

### `TemplateDocument`

字段：

- `id`：模板唯一标识。
- `name`：模板名称。
- `templateKey`：当前业务模板键，例如 `default`、`silver`。
- `schemaVersion`：模板文档结构版本，初始为 `1`。
- `activeVersionId`：当前激活版本。
- `layers`：图层列表。
- `versions`：版本快照列表。
- `deviceProfiles`：设备 profile 列表。

### `TemplateLayer`

字段：

- `id`：图层唯一标识。
- `name`：图层名称。
- `visible`：是否参与预览和打印。
- `locked`：锁定后不能移动、删除或编辑元素。
- `elements`：图层内元素列表。

### `TemplateElement`

沿用阶段二的元素模型，并新增：

- `layerId`：所属图层。
- `zIndex`：图层内排序值。
- `visible`：元素级显示状态。
- `locked`：元素级锁定状态。

元素类型继续限定为：

- `fixedText`
- `boundField`
- `qrCode`
- `rectangle`

### `TemplateVersion`

字段：

- `id`：版本唯一标识。
- `name`：版本名称。
- `createdAt`：保存时间。
- `layersSnapshot`：图层和元素快照。
- `note`：版本说明。

版本快照只保存模板内容，不复制设备 profile，避免一个版式版本意外覆盖设备校准。

### `DeviceProfile`

字段：

- `id`：profile 唯一标识。
- `printerName`：匹配的打印机名称，空字符串表示默认 profile。
- `dpi`：渲染 DPI。
- `scaleX`：横向缩放校准。
- `scaleY`：纵向缩放校准。
- `offsetX`：设备级横向偏移，单位毫米。
- `offsetY`：设备级纵向偏移，单位毫米。
- `notes`：备注。

最终坐标处理顺序：

1. 模板元素毫米坐标。
2. 全局 `labelOffset`。
3. 当前设备 profile 的 `offsetX/offsetY`。
4. 渲染时应用 `scaleX/scaleY` 和 `dpi`。

## 文件格式

导出文件使用 JSON，扩展名建议为 `.sleekpr-template.json`。

顶层结构：

```json
{
  "schemaVersion": 1,
  "id": "template-default",
  "name": "默认标签模板",
  "templateKey": "default",
  "activeVersionId": "version-current",
  "layers": [],
  "versions": [],
  "deviceProfiles": []
}
```

导入规则：

- `schemaVersion` 缺失或大于当前支持版本时拒绝导入。
- 图层 `id` 重复时拒绝导入。
- 元素 `id` 重复时拒绝导入。
- 坐标、尺寸、DPI、缩放值缺失时使用安全默认值。
- 不认识的元素类型不导入，并给出错误提示。

导出规则：

- 只导出当前模板文档，不导出本机 HTTP 白名单、默认打印机等客户端设置。
- 导出内容必须能再次导入并生成同等绘制计划。

## Qt 界面

`TemplateDesignerWindow` 使用四区布局：

- 顶部工具栏：导入、导出、保存版本、恢复版本、选择设备 profile。
- 左侧图层面板：图层列表、新增图层、删除图层、上移、下移、显示、锁定。
- 中间画布：Qt 原生标签预览，支持选择、拖拽、键盘微调、网格吸附。
- 右侧属性面板：当前元素属性、当前图层属性、当前设备 profile 属性。

交互规则：

- 锁定图层内的元素不可选中、不可拖拽、不可编辑。
- 隐藏图层不参与预览和打印。
- 当前图层决定新增元素落到哪里。
- 删除图层前如果图层有元素，使用确认对话框。
- 恢复版本会替换当前图层和元素，但保留设备 profile。
- 导入模板后不自动覆盖当前模板，先显示导入摘要，用户确认后替换。

## 预览和打印链路

预览：

- `LabelPreviewService` 新增使用 `TemplateDocument` 的入口。
- 未启用完整模板文档时，继续使用阶段二的 `templateOverrides + templateElements` 路径。
- 启用模板文档时，由文档生成 `NativeDrawCommand`。
- 预览 DPI 来自当前设备 profile，默认 300。

打印：

- `/print/tag` 根据当前打印机解析 `DeviceProfile`。
- `QtLabelPrintEngine` 接收已经应用 profile 的绘制计划或渲染参数。
- 不把 profile 逻辑写入 `core` 的业务标签规划器。

## 兼容策略

- 现有 `settings.json` 没有 `templateDocuments` 时，启动后自动生成一个默认模板文档视图，但不强制写回文件。
- 现有 `templateOverrides` 和 `templateElements` 继续可用。
- 新设计器保存时可以把当前默认模板迁移成 `templateDocuments["default"]`。
- HTTP `/settings` 保持返回旧字段，同时新增模板文档字段。
- 旧客户端忽略新字段时不影响基本打印。

## 错误处理

- 导入失败使用 `QMessageBox` 显示明确原因。
- 版本恢复失败不修改当前模板。
- profile 数值非法时回退到默认值，并在状态栏提示。
- 打印机名没有匹配 profile 时使用默认 profile。
- 导出失败提示文件路径和错误原因。

## 测试策略

core 测试：

- `TemplateDocumentJson` 能往返序列化图层、元素、版本、profile。
- 隐藏图层不会生成绘制命令。
- 锁定图层不会被编辑模型修改。
- 图层排序会改变绘制命令顺序。
- 版本恢复只恢复图层和元素，不覆盖 device profile。
- `DeviceProfileResolver` 能按打印机名匹配 profile，并回退默认 profile。

app 测试：

- 设计器窗口能新增图层。
- 设计器窗口能新增元素到当前图层。
- 锁定图层后拖拽不改变元素坐标。
- 导出按钮能生成模板 JSON。
- 导入按钮能加载模板 JSON。

集成测试：

- `LabelPreviewService` 使用模板文档生成预览图。
- `/print/tag` 走设备 profile 的 DPI 和校准参数。
- 完整 `cmake --build build` 和 `ctest --test-dir build --output-on-failure` 通过。

## 验收标准

- 设置窗口有“模板设计器”入口。
- 模板设计器能新增、删除、排序、隐藏、锁定图层。
- 模板设计器能在当前图层新增固定文本、绑定字段、二维码、矩形。
- 模板能导出为 `.sleekpr-template.json` 并重新导入。
- 能保存版本快照并恢复到历史版本。
- 能为不同打印机配置独立 DPI、缩放和偏移。
- Qt 预览和真实打印链路都能使用当前设备 profile。
- 现有阶段二配置继续兼容。
- 所有新增注释为中文，关键代码有中文注释。
- 不使用 CodeRabbit。
