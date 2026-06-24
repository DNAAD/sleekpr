# TemplateWorkspaceToolbar 拆分计划

## 目标

继续降低 `TemplateWorkspacePanel` 的 UI 构建职责，把顶部工具栏独立为可组合控件，保留既有对象名、按钮语义和外部访问方式。

## 步骤

1. 新增 `TemplateWorkspaceToolbar`，集中创建保存、预打印、导入、导出、撤销、重做、缩放、标尺精度、辅助线和纸张规格控件。
2. `TemplateWorkspacePanel` 改为组合工具栏，原有 getter 继续透出同一批控件，降低调用方改动范围。
3. 更新主程序和测试工程构建清单。
4. 运行 app 测试、全量 CTest、JS 测试和 diff 检查。
