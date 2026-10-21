# 项目 Harness

这里只维护标签编辑、字段绑定、设置保存、异步预览及原生客户端交付中已暴露的知识与流程，不保存任务流水账。

## 按需入口

| 当前任务 | 应读内容 |
| --- | --- |
| 判断字段名称、作用域或兼容行为 | [标签绑定知识](wiki/label-binding.md) |
| 修改保存、处理冲突或恢复设置备份 | [设置保存与恢复](wiki/settings-persistence.md) |
| 排查模板选择、异步回填或模板来源冲突 | [两类模板身份](wiki/template-identity.md)、[异步预览与草稿规则](rules/async-preview-and-drafts.md) |
| 排查旧版本启动、增量构建或程序占用 | [Windows 构建知识](wiki/windows-build.md) |
| 修改属性面板、数据绑定或自动应用 | [属性编辑规则](rules/qt-property-editing.md)、[绑定修改技能](skills/qt-binding-change/SKILL.md) |
| 制作、验收或回滚 Windows Release 绿色包 | [原生 Release 交付技能](skills/native-release-delivery/SKILL.md) |
| 验证改动、准备交付或提交 | [验证技能](skills/verification/SKILL.md) |

## 维护边界

- [AGENTS.md](../AGENTS.md) 是项目强制约束入口；这里不复制其中的分层、中文注释及原生 Qt 要求。
- 用户操作细节继续维护在 [标签编辑文档](../docs/label-editor-interactions.md) 和 [打印工作台文档](../docs/print-workbench.md)，知识页只解释容易误判的机制和源码入口。
- 实现变化时同步修正对应知识或流程；命令以实际构建环境和注册测试为准，不把某次测试数量写成永久门槛。
- Harness 文件只提供上下文，不替代用户对提交、推送、进程关闭和真实设备操作的授权。
