# 设置窗口第一阶段设计

## 目标

在 `sleekpr` 原生 Qt 客户端中新增一个设置窗口，用来迁移当前最关键的打印设置能力：选择默认打印机、调整标签整体偏移、调整标签元素的位置/字体大小/字体粗细，并且能保存、重置和刷新预览。第一阶段只做本地客户端内的设置能力，不恢复浏览器预览页。

## 范围

本阶段包含：

- 托盘菜单新增“设置”入口。
- 设置窗口使用 Qt 原生控件实现。
- 打印机列表来自 `QPrinterInfo::availablePrinters()`。
- 默认打印机保存到现有 `PrintClientSettings.defaultPrinter`。
- 整体偏移保存到现有 `PrintClientSettings.labelOffset`。
- 元素级设置保存到现有 `PrintClientSettings.templateOverrides["default"]`。
- 支持调整元素 `x`、`y`、`fontSizePt`、`bold`。
- 支持保存、应用但不保存、刷新预览、重置当前元素、重置全部元素。
- 保存或应用后刷新 Qt 预览窗口。

本阶段不包含：

- 浏览器设置页。
- 测试打印、坐标轴打印、真实打印链路改造。
- 多模板管理界面。
- 设备能力配置。

## 界面布局

设置窗口采用三栏布局，适合长期维护和后续扩展：

- 左侧：打印机和整体偏移设置。
- 中间：标签预览区域，显示当前设置应用后的预览图。
- 右侧：标签元素列表和当前元素编辑表单。

窗口底部放置操作按钮：

- `保存`：写入本地设置文件，并刷新预览窗口。
- `应用但不保存`：只更新本次设置窗口内的预览和主预览窗口，不落盘。
- `刷新预览`：重新根据当前表单值绘制预览。
- `重置当前元素`：删除当前元素在 `templateOverrides["default"]` 中的覆盖值。
- `重置全部元素`：清空 `templateOverrides["default"]`，但不重置打印机和整体偏移。
- `关闭`：关闭设置窗口。

## 主要组件

### `SettingsWindow`

负责 Qt 窗口和控件编排，放在 `src/app` 与 `include/sleekpr/app` 下。窗口不使用复杂自定义信号，优先通过 lambda 连接 Qt 控件事件，避免为简单窗口引入额外 moc 复杂度。

### `TemplateElementCatalog`

负责提供可编辑元素清单和默认值来源，放在 `core` 或 `infrastructure/preview` 附近。它根据当前 `NativeLabelDrawingPlanner` 生成的默认绘制命令，整理出可编辑元素：

- 编号：`identifier`
- 合格证：`qualityMark`
- 二维码：`qrCode`
- 二维码说明：`qrNote`
- 商品名称：`productName`
- 执行标准：`standardText`
- 地址：`addressText`
- 销售码：`salesCode`
- 成品重标题：`finishedWeightLabel`
- 成品重数值：`finishedWeightValue`
- 总件重标题：`roughWeightLabel`
- 总件重数值：`roughWeightValue`
- 附加金额：`additionalPrice`
- 明细行：`partRow`
- 底部备注：`footerText`
- 竖排编号：`verticalIdentifier`

### `FileSettingsStore`

继续作为设置读写入口。当前已有 `load()` 和 `save()`，本阶段复用它，不改变 JSON 结构。

### `PreviewWindow`

继续作为主预览窗口。设置窗口保存或应用设置后调用刷新逻辑，让用户能立即看到变化。

## 数据流

打开设置窗口时：

1. 从 `FileSettingsStore` 读取本地设置。
2. 枚举系统打印机。
3. 通过 `TemplateElementCatalog` 生成元素清单。
4. 将设置值填充到左侧和右侧控件。
5. 使用当前设置渲染中间预览。

编辑元素时：

1. 用户在右侧选择元素。
2. 表单显示当前覆盖值；如果没有覆盖值，则显示默认绘制命令的值。
3. 用户调整 `x`、`y`、`fontSizePt`、`bold`。
4. 点击“刷新预览”或“应用但不保存”后，把表单值写入内存中的 settings 副本并重绘。

保存时：

1. 从控件收集打印机、整体偏移、元素覆盖值。
2. 调用 `FileSettingsStore::save()`。
3. 刷新主预览窗口。

## 错误处理

- 没有可用打印机时，打印机下拉框保留“使用系统默认打印机”选项，保存为空字符串。
- 设置文件读取失败时，沿用现有默认设置，并允许用户重新保存。
- 预览渲染失败时，设置窗口显示简短错误文本，不阻塞用户调整设置。
- 保存失败时，用 `QMessageBox` 提示用户失败原因。

## 测试策略

第一阶段优先测试非 UI 逻辑：

- 元素清单能输出稳定的 key 和中文显示名称。
- 元素清单能从默认绘制计划中读取默认坐标和字号。
- 重置当前元素只删除当前 key。
- 重置全部元素只清空 `templateOverrides["default"]`，不影响打印机和整体偏移。
- 设置保存后仍保持现有 JSON 兼容格式。

Qt 窗口交互本阶段用人工验证：

- 托盘菜单能打开设置窗口。
- 打印机下拉框能看到本机打印机。
- 修改偏移或元素值后，预览窗口能刷新。
- 保存后重启程序仍能读取设置。

## 验收标准

- 程序启动后托盘菜单有“设置”入口。
- 点击“设置”能看到三栏设置窗口。
- 能选择默认打印机并保存。
- 能调整整体 X/Y 偏移并保存。
- 能选择标签元素，并调整位置、字号、粗细。
- 保存或应用后，Qt 预览窗口立即更新。
- 代码注释全部使用中文，关键逻辑有中文注释。
- `cmake --build build` 和 `ctest --test-dir build --output-on-failure` 通过。
