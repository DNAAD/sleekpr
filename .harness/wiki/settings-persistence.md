# 设置保存与恢复

## 保存入口与冲突粒度

- 桌面窗口通过应用持有的 `SettingsPersistenceService` 提交冻结目标；专用单工作线程中的 `SettingsSaveSession` 生成 `SettingsChangeSet`，不是把旧窗口持有的整份设置写回。默认打印机、Origin、偏移与 HTTP 上限按各自变更合并，模板文档按槽位检查冲突。
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

### 后台队列与自身确认

- 全局只有一个进行中的存储操作，各会话最多保留一个最新累计保存目标，按会话公平轮转；不能把设计器高频输入逐次堆进线程池，也不共用预览工作池。
- `SettingsSaveBaseline` 只确认本会话实际提交的路径。结果顺带读到的外部字段不进入其他路径的编辑期望，未知扩展仍受原始 JSON 保护。
- A 执行中编辑 B，A 的结果只结算 A，不替换当前文档、表单、焦点或撤销历史。撤销回最初值也须保留目标，不能因相对旧基线无差异提前丢弃。
- 保存失败保留最新已接受目标并暂停自动提交；重试只处理允许重试的失败。Conflict 和 ReadFailed 不能通过反复点重试绕过。
- 窗口保存上下文按服务实际 `runningTicket` 与最新目标保留，累计成功后移除更早的失败票据；不能用最小修订号猜测在途请求，否则连续失败/重试可能把显示值错误视为未修改。
- 重读/恢复受理即进入 Loading 并拒绝新编辑；在途写入先结算。只有控制操作成功才推进代次并替换草稿，失败保留此前确认的自身写入与草稿。
- 设置窗口只在显式保存时提交目标；之后仅编辑或“应用但不保存”的 B 不混入 A。运行时应用有独立序号，旧保存不回退新的运行状态。窗口保存上下文也保持有界。

### 正常退出与取消

- 普通关闭依旧隐藏到托盘，合法排队写入可继续；失败、冲突和非法原文保留确认及本次运行内重开保护。
- 应用自有退出入口通过 `ApplicationExitCoordinator`，先冻结新操作、提交有效待应用编辑，再处理手动表单选择，在 GUI 事件循环仍运行时等待存储与已接受打印提交。
- “不保存并退出”只跳过尚未请求保存的手动表单，不取消此前接受的保存。放弃全部未保存内容需要二次确认，仍等待正在执行的写入，不回滚已经完成的保存。
- 放弃等待期间只是暂停未开始意图；直到最终授权点才清除。返回编辑会恢复准入、待处理意图及控件原有状态；退出代次阻止旧回调在取消后终止程序。
- 工作台暂停会记录待准备/正在规划与仅份数检查；返回后分别恢复，纯份数检查不重建基础图片。不能只停止防抖计时器而丢掉重启标记，导致一直无法打印。
- 等待期间 HTTP 保持监听，精确 `GET /health` 可用，其余合法新请求返回现有错误包络的 `503/CLIENT_EXITING`；Origin 和请求大小校验仍优先。取消后不重新绑定端口。
- 此阶段只保护设计器和设置窗口的 settings.json，不代表独立模板库/纸张库/预设库草稿已迁移，也不承诺强制关机、崩溃或断电时的跨进程恢复。

非法模拟 JSON 原文尚未进入有效模型，但仍是未保存草稿；其他属性保存成功不能清除它。同一模板全量刷新需保留原文，关闭需确认，再打开复用草稿窗口。显式重读经确认后可放弃草稿。当前只保证本次运行内保留，不承诺跨进程恢复非法原文；导出有效模型也不代表导出了非法编辑器文本。

- [SettingsChangeSet](../../src/core/settings/SettingsChangeSet.cpp)：规范化比较、原始扩展保护和合并。
- [SettingsRepository](../../src/infrastructure/settings/SettingsRepository.cpp)：锁、原子提交、备份与恢复。
- [SettingsSaveBaseline](../../src/core/settings/SettingsSaveBaseline.cpp)：按自身成功路径推进期望。
- [SettingsSaveSession](../../src/infrastructure/settings/SettingsSaveSession.cpp)：存储线程的仓库会话与冲突暂停。
- [共享保存服务](../../src/app/SettingsPersistenceService.cpp)：有界队列、会话寿命与控制屏障。
- [退出协调器](../../src/app/ApplicationExitCoordinator.cpp)：收集、等待、取消与最终退出授权。
- [设计器持久化](../../src/app/TemplateDesignerPersistence.cpp)、[设置窗口](../../src/app/SettingsWindow.cpp)：保存反馈与显式重读。
- [存储测试](../../tests/tst_settings_repository.cpp)、[会话测试](../../tests/tst_settings_save_session.cpp)、[后台队列测试](../../tests/tst_settings_persistence.cpp)、[退出测试](../../tests/tst_application_exit.cpp)、[设计器测试](../../tests/tst_label_designer.cpp)：失败、竞态与草稿回归。
