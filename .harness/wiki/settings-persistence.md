# 设置保存与恢复

## 保存入口与冲突粒度

- 桌面窗口通过 `SettingsEditSession` 或设计器保存回调提交 `SettingsChangeSet`，不是把旧窗口持有的整份设置写回。默认打印机、Origin、偏移与 HTTP 上限按各自变更合并，模板文档按槽位检查冲突。
- 比较已知字段前用同一 JSON 序列化规则规范化缺省值；JSON 对象字段重排不等于业务修改。输出从最新原始 JSON 起步，只更新已知语义变化，不用类型模型序列化整槽来覆盖未知扩展。
- `withSourceBaseline(sourceJson)` 保留原始比较基线；它使同一模板槽位内仅修改未知扩展的外部写入也能触发冲突。只调用 typed `between()` 不足以提供这项保护。数组对象重排时，扩展需跟随稳定 `id` 或 `key`，不能移到另一元素。
- 冲突时整次变更不落盘，成功基线不推进，窗口草稿与撤销历史保留；该编辑会话暂停重复提交，显式成功重读后才能继续。保存失败不发布“已保存”观察通知。
- 设置表单显示的兜底值不自动算用户修改；未编辑的可选 HTTP 上限不能被写成显式默认值并覆盖外部更新。

`POST /settings` 仍是显式整体替换，不强制新增 revision。`FileSettingsStore` 保留旧门面语义，内部委托同一仓库；不要把其 `save()` 当成桌面差量保存。整体替换不承诺保留请求模型之外的未知字段。

## 仓库与失败状态

`SettingsRepository` 的写入在同一 `<settingsPath>.lock` 下完成读取、比较、备份和 `QSaveFile` 原子提交。`tryLock(0)` 忙时返回，不循环阻塞 GUI。它协调遵守该锁的入口，不是对任意外部编辑器的无条件并发保证。

读取状态为 `Missing/Ready/InvalidJson/IoError`。损坏或不可读文件阻止自动差量保存，不能静默用默认值重建。兼容门面 `load()` 的默认值不代表读取成功；界面判断应使用结构化读取快照。

写入只有 `Saved/Unchanged` 算成功。`Conflict/Busy/ReadFailed/BackupFailed/WriteFailed` 均需保留草稿并呈现失败，不能推进成功基线。备份或主文件提交失败时，主文件原文必须保持不变。

## 备份与恢复

| 文件 | 语义 |
| --- | --- |
| `.bak` | 普通保存前的最近有效主文件，也是显式恢复来源 |
| `.corrupt.bak` | 显式替换或恢复损坏文件前保留的坏文件原文，不覆盖有效 `.bak` |
| `.restore.bak` | 从有效主文件主动恢复时保留的当前有效内容，避免轮换正在读取的 `.bak` |

`restoreBackup(expectedCurrentDigest)` 在锁内校验当前原文字节摘要，再读取有效 `.bak`。摘要必须来自本次观察/失败快照，而不是盲用上次保存成功的摘要。外部文件再变化时返回冲突，不能恢复到未经重新确认的文件上。

恢复失败必须保留主文件和来源 `.bak`，使相同来源仍可重试；缺少有效备份时停止。当前 UI 对损坏或缺失文件提供恢复，仓库 API 也支持有效文件主动回退。恢复不是自动启动动作，也不是模板版本历史或 Release 二进制回滚。

摘要保护的是显式恢复确认；普通差量保存不能一律要求整文件摘要相同，否则会误拒绝无关字段的并发修改。

## 草稿边界与源码

非法模拟 JSON 原文尚未进入有效模型，但仍是未保存草稿；其他属性保存成功不能清除它。同一模板全量刷新需保留原文，关闭需确认，再打开复用草稿窗口。显式重读经确认后可放弃草稿。当前只保证本次运行内保留，不承诺跨进程恢复非法原文；导出有效模型也不代表导出了非法编辑器文本。

- [SettingsChangeSet](../../src/core/settings/SettingsChangeSet.cpp)：规范化比较、原始扩展保护和合并。
- [SettingsRepository](../../src/infrastructure/settings/SettingsRepository.cpp)：锁、原子提交、备份与恢复。
- [SettingsEditSession](../../src/app/SettingsEditSession.cpp)：成功基线与冲突暂停。
- [设计器持久化](../../src/app/TemplateDesignerPersistence.cpp)、[设置窗口](../../src/app/SettingsWindow.cpp)：保存反馈与显式重读。
- [存储测试](../../tests/tst_settings_repository.cpp)、[会话测试](../../tests/tst_settings_edit_session.cpp)、[设计器测试](../../tests/tst_label_designer.cpp)：失败与草稿回归。
