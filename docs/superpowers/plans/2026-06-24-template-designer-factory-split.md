# TemplateDesignerFactory 拆分计划

## 目标

引入工厂模式，移除 `TemplateDesignerWindow` 中默认模板、默认元素、默认表格和默认 id 的构造细节，让窗口类只负责调度 UI 行为。

## 设计

`TemplateDesignerFactory` 位于 `app` 层，因为这些默认值服务于模板设计器体验，而不是模板渲染核心规则。工厂负责创建空白模板、默认模拟数据、默认图层名称、默认元素、默认表格和设计器 id。

## 步骤

1. 新增 `TemplateDesignerFactory`，集中封装默认对象创建。
2. `TemplateDesignerWindow` 改为调用工厂，不再直接拼装默认模板元素。
3. 增加 app 测试，锁定默认元素、默认表格和空白模板的关键字段。
4. 更新主程序与 app 测试 CMake 清单。
