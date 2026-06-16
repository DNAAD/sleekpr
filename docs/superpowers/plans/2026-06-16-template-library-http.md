# 模板库 HTTP 接口第一批实施计划

## 目标

把模板从仅保存在 settings 内的兼容结构，推进到独立模板库文件，并通过本地 HTTP 接口提供最小可用的增删查改能力。`/print/template` 需要优先兼容 settings 内模板，同时能读取模板库里的模板。

## 范围

1. 增加 `GET /templates`，返回模板库摘要列表。
2. 增加 `GET /templates/{id}`，返回单个模板完整 JSON。
3. 增加 `POST /templates`，保存完整模板 JSON。
4. 增加 `PUT /templates/{id}`，按路径 id 更新模板，并校验正文 id 一致。
5. 增加 `DELETE /templates/{id}`，删除指定模板。
6. 升级 `POST /print/template`，找不到 settings 内模板时继续查找模板库。

## 设计约束

- 模板文件由 `TemplateLibraryStore` 负责读写，HTTP 层只做协议解析和响应封装。
- 模板目录放在 `settings.json` 同级的 `templates` 目录，方便随客户端配置一起备份和迁移。
- 保存前统一走 `TemplateDocumentJson::validateForImport`，避免坏模板写入模板库。
- 保持现有 settings 内模板兼容逻辑，避免破坏旧配置。
- 注释和计划文档保持中文。

## 验证

1. 新增 HTTP 单元测试覆盖模板保存、读取、列表、删除。
2. 新增 HTTP 单元测试覆盖 `/print/template` 使用模板库模板打印。
3. 运行 HTTP 定向测试。
4. 运行全量构建和测试。
5. 运行部署目标，确认打包链路仍可用。
