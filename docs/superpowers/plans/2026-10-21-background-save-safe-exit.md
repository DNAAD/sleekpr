# 后台保存队列与统一退出保护实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将设计器和设置窗口的 `settings.json` 保存移出 GUI 线程，并让正常退出在草稿、冲突和在途写入处理完毕后执行。

**Architecture:** 用纯数据保存基线记录用户观察与自身提交，用专用单线程执行器和应用持有的服务调度存储任务。窗口只提交冻结快照并消费结构化状态，退出协调器在事件循环仍运行时收集草稿、关闭新操作准入并等待任务收束。

**Tech Stack:** C++20、现有 Qt Core/Widgets/Network/Test、QThreadPool、QLockFile、QSaveFile、CMake/CTest；不引入第三方运行时。

**Spec:** [已确认设计规格](../specs/2026-10-21-background-save-safe-exit-design.md)。先读规格和 [AGENTS.md](../../../AGENTS.md)，本计划不能替代其约束。

## Global Constraints

- 首期只迁移设计器和设置窗口的 `settings.json` 保存；不迁移模板库、纸张规格和字段预设，不保存工作台业务输入。
- 关闭普通窗口仍然隐藏到托盘，不能把窗口关闭等同于退出程序。
- 旧保存成功不会覆盖新输入、撤销结果、非法 JSON 原文或更新的“应用但不保存”状态。
- 保存失败、冲突或非法草稿默认阻止真实退出，由用户明确选择处理方式。
- 同文件串行写入；每会话至多一个最新待处理累计意图；已开始的写入不强制取消。
- 只有 `Saved/Unchanged` 可确认成功；同目标外部修改仍需冲突，未知扩展、字面路径和可选值语义不得丢失。
- “应用但不保存”不产生持久化请求；旧保存完成不得回退更新的运行时应用。
- 退出等待期继续允许 `GET /health`，其他新请求使用现有错误包络返回 `503 Service Unavailable` 和 `CLIENT_EXITING`。
- 不迁移真实打印驱动线程，不承诺取消系统打印队列；测试仅用临时配置和模拟打印，不关闭用户客户端。
- 所有关键代码注释必须中文；源码 UTF-8；不使用 CodeRabbit，不恢复复杂表格，不改变原生 Qt 绘制。
- 不自动提交、推送、打包或修改正式发布目录；用户另行授权前只保留可审阅工作区变更。

## Review Focus

1. 已开始保存 A 时撤销回最初内容，即使相对旧基线暂时无差异，也必须最终写回撤销结果，不能提前丢掉待处理目标。任务 1、2、3 覆盖。
2. 自身提交只确认自己的路径；顺带读到的外部字段、未知扩展和另一个会话不能被当成用户已观察的新基线。任务 1、2、4 覆盖。
3. 重新读取失败前，正在执行的保存可能已经成功；失败不能退回更旧的成功基线，也不能复活已取消的旧请求。任务 2 覆盖。
4. 表单保存 A 后只编辑或应用 B，旧完成不能保存 B 或回退运行时 B；显示的可选上限兜底值仍不代表修改。任务 4 覆盖。
5. 退出等待后返回编辑、重复退出、放弃后又取消，以及已接受的定时打印提交，不得造成延迟退出、错误启用控件或提前销毁引擎。任务 6、7 覆盖。

## 文件与职责

下表路径均相对仓库根目录；新类只承担列出的职责，不借本次改动重组全部 UI 目标。

| 路径 | 职责 |
| --- | --- |
| `include/sleekpr/core/settings/SettingsSaveBaseline.h`、`src/core/settings/SettingsSaveBaseline.cpp` | 保存期望值与自身成功路径确认 |
| `src/core/settings/SettingsJsonPath.h` | 抽取既有结构化 JSON 路径读写供两处复用，不拆分字面键 |
| `include/sleekpr/infrastructure/settings/SettingsSaveSession.h`、`src/infrastructure/settings/SettingsSaveSession.cpp` | 只在存储线程使用的仓库会话、冲突暂停及控制操作 |
| `include/sleekpr/infrastructure/settings/SettingsPersistenceExecutor.h`、`src/infrastructure/settings/SettingsPersistenceExecutor.cpp` | 专用单工作线程和 GUI 回调投递 |
| `include/sleekpr/app/SettingsPersistenceService.h`、`src/app/SettingsPersistenceService.cpp` | 应用持有的有界队列、会话寿命与状态通知 |
| `include/sleekpr/app/SettingsPersistenceState.h` | 票据、会话视图与操作结果的应用侧值类型 |
| `src/app/TemplateDesignerPersistence.cpp`、`src/app/SettingsWindowPersistence.cpp` | 两个窗口各自的保存适配，不实现磁盘合并 |
| `include/sleekpr/app/ApplicationExitCoordinator.h`、`src/app/ApplicationExitCoordinator.cpp` | 退出代次、参与者收集、等待与取消 |
| `include/sleekpr/app/ApplicationExitParticipant.h` | 窗口与协调器共用的退出草稿状态及语义回调契约 |
| `include/sleekpr/app/ApplicationExitDialog.h`、`src/app/ApplicationExitDialog.cpp` | 原生非阻塞退出提示，不执行存储 |
| `tests/support/SettingsPersistenceTestSupport.h` | 临时配置、原子写入故障注入和可控写入屏障 |
| `tests/tst_settings_save_baseline.cpp`、`tests/tst_settings_save_session.cpp`、`tests/tst_settings_persistence.cpp`、`tests/tst_application_exit.cpp` | 领域、存储、队列和退出回归 |

`CMakeLists.txt` 与 `tests/CMakeLists.txt` 随所属任务登记源码、含 `Q_OBJECT` 的头文件和测试。最终移除旧 `SettingsEditSession` 的 GUI 同步生产入口，其原有测试语义迁入新套件，不保留两套并行的保存状态机。

## 执行与验证约定

在获准执行后，先核对分支、已有变更和 `build`/`build-release` 缓存。本计划编写时，两份规格草稿尚未提交；`2026-10-19-document-flow-designer-design.md` 属于另一任务，不修改、暂存或实现。

每个任务遵循 RED -> GREEN：先写测试并运行，确认失败来自缺失行为而非缺 DLL、错误工具链或旧构建，再实现，最后运行关联回归。新类尚不存在时，首次编译失败可作为缺失入口证据；入口建立后仍须验证行为断言。

下面所有命令在仓库根目录、同一 MSVC x64 开发者会话中运行，Qt `bin` 已加入 `PATH`。普通 PowerShell 使用启动脚本发现环境，并在同一次 `cmd /c` 中调用 `VsDevCmd.bat` 和构建；不能假定子进程环境回传。每个原生命令后检查 `$LASTEXITCODE`，RED 的预期失败单独记录，不能覆盖 GREEN 失败。

新增测试目标均调用现有 `configure_qt_test_runtime()`。Qt GUI 测试串行，使用 `QTRY_*` 或屏障，不以固定睡眠判断完成；解引用票据、结果或信号参数之前先断言其存在。屏障测试的解锁清理必须早于服务/执行器析构，即使断言提前返回也不能挂死。

---

### 任务 1：可因果确认的保存基线与存储会话

**Files:** 创建文件表中的 `SettingsSaveBaseline`、`SettingsJsonPath.h`、`SettingsSaveSession` 及两个对应测试；修改 `src/core/settings/SettingsChangeSet.cpp`、根与测试 CMake。

**Interfaces:**
- `core::SettingsSaveBaseline(const SettingsReadResult& observed)`；`SettingsChangeSet changesFor(const PrintClientSettings&, SettingsEditScope) const`；`void acknowledge(const SettingsChangeSet&, const SettingsWriteResult&)`；`const PrintClientSettings& expectedSettings() const`；`void reset(const SettingsReadResult&)`。
- `infrastructure::SettingsStorageOperation { Open, Save, Reload, Restore }`；`SettingsStorageOutcome` 包含 `operation`、`observed`、`expectedSettings`、可选 `writeResult`、`baselineReset`。
- `infrastructure::SettingsSaveSession(QString path, SettingsRepository::AtomicWriter writer = {})`；`SettingsStorageOutcome open()`、`saveEdited(const PrintClientSettings&, SettingsEditScope)`、`reload()`、`restoreBackup(const QByteArray& expectedDigest)`。构造不读盘，所有方法只由存储任务调用。
- 会话保留 `Conflict` 直到显式成功重读/恢复；其他写入失败由调度层暂停后允许显式重试。失败读盘不替换已确认基线，`observed` 独立保留最近实际观察用于错误和恢复摘要。

- [x] **Step 1：添加失败测试。** `acknowledgesOnlySubmittedPaths` 先以 `printer=old`、HTTP 上限 16KB 建基线，成功提交 printer=new 的结果中混入外部 24KB；随后本地改为 32KB，断言该变化仍期望 16KB，并与外部 24KB 冲突。`ownWriteThenRevertUsesAcknowledgedValue` 断言成功 A 后提交原值，期望值是 A 而不是旧值。断言使用真实路径和值：

```cpp
QCOMPARE(change.path, QStringList({QStringLiteral("localHttpLimits"), QStringLiteral("maxHeaderBytes")}));
QCOMPARE(change.expected.toInt(), 16 * 1024);
QCOMPARE(change.value.toInt(), 32 * 1024);
QVERIFY(!changes.merge(external24Kb).conflicts.isEmpty());
```

同时迁入旧会话七类回归：成功才推进、无关字段保留、冲突暂停、损坏配置、显式恢复、扩展随稳定身份、仅未知扩展变化冲突。补 `failedReloadKeepsPriorAcknowledgement` 和字面槽位 `a.b/c`、删除槽位、幂等、可选缺省值测试。

- [x] **Step 2：运行 RED。** `cmake --build build --parallel 4 --target sleekpr_settings_save_baseline_tests sleekpr_settings_save_session_tests`，随后分别运行两个同名 EXE 的 `-v1 -o build/<目标名>-red.log,txt`；记录缺失入口或具体错误断言。
- [x] **Step 3：实现基线投影。** 从既有 change-set 文件抽取 JSON 路径读写，`acknowledge()` 仅在 `result.ok()` 时复制 `applied.changes` 指定路径的实际成功值及原始扩展，绝不复制整份结果快照。重新生成对应 typed 期望值仍使用现有 JSON 规范化规则；`changesFor()` 始终对投影基线生成差异并附原始期望值。存储会话复用仓库，不复制锁、备份或原子提交。
- [x] **Step 4：运行 GREEN。** 运行 `ctest --test-dir build --output-on-failure --parallel 1 -R "sleekpr_settings_(save_baseline|save_session|change_set|repository|edit_session)_tests"`，期望零失败；此时旧 GUI 会话尚未删除，只作迁移对照。
- [x] **Step 5：检查任务差异与记录。** 保存 RED/GREEN 证据、检查 UTF-8 和 `git diff --check`；不提交。交付可独立测试的期望值协调和仓库会话。

### 任务 2：专用执行器与共享有界保存服务

**Files:** 创建文件表中的执行器、服务、应用状态头、测试支持头和 `tests/tst_settings_persistence.cpp`；修改根与测试 CMake，新增目标 `sleekpr_settings_persistence_tests`。

**Interfaces:**
- `SettingsPersistenceExecutor::submit(std::function<SettingsStorageOutcome()> work, std::function<void(SettingsStorageOutcome)> guiCompletion)`；`void shutdown()`，仅停止接收并收束已执行工作，不负责会话待处理队列。
- 应用类型：`using SettingsSessionId = quint64`；`SettingsSaveTicket { sessionId, epoch, revision }` 三字段均为 `quint64`，有效标识从 1 开始；`SettingsSavePhase { Loading, Saved, Queued, Saving, Failed, Conflict }`。
- `SettingsPersistenceOutcome` 含 `infrastructure::SettingsStorageOutcome storage` 和 `std::optional<SettingsSaveTicket> ticket`，非保存控制操作没有保存票据。`SettingsSessionView` 包含 `QString title`、`quint64 epoch/acceptedRevision/savedRevision/outcomeSerial`、`bool running/pending/detached`、`SettingsSavePhase phase`、`std::optional<SettingsPersistenceOutcome> lastOutcome`。状态排队通知不伪造新完成结果。
- `SettingsPersistenceService` 定义 `using AtomicWriter = infrastructure::SettingsRepository::AtomicWriter`；构造 `SettingsPersistenceService(QString path, AtomicWriter writer = {}, QObject* parent = nullptr)`，内部独占执行器；GUI 方法：`std::optional<SettingsSessionId> openSession(QString title)`、`std::optional<SettingsSaveTicket> requestSave(SettingsSessionId, PrintClientSettings, SettingsEditScope)`、`bool retry(SettingsSessionId)`、`bool requestReload(SettingsSessionId)`、`bool requestRestore(SettingsSessionId, QByteArray expectedDigest)`、`void detachSession(SettingsSessionId)`、`SettingsSessionView view(SettingsSessionId) const`、`QList<SettingsSessionId> sessions() const`。
- 收束方法：`void setAcceptingSaves(bool)`、`void setPendingSuspended(bool)`、`void discardPending(SettingsSessionId)`、`bool hasRunningWork() const`、`bool hasControlWork() const`、`bool isIdle() const`、`bool shutdownIfIdle()`；信号 `sessionChanged(SettingsSessionId)`、`activityChanged()`。拒绝新保存时返回空票据，不报告已接受。

- [x] **Step 1：添加队列和线程 RED。** `retainsOnePendingIntentPerSession` 用主文件写入屏障阻塞 A，连续请求 100 次，再请求另一个会话，断言主文件执行峰值为 1、每会话待处理数不超过 1、另一个会话不饿死、最终目标为第 100 次。`guiHeartbeatWhileWriteBlocked` 在写入屏障未释放时用 GUI 零延迟定时器增加计数，验证输入事件仍可派发，且仓库工作线程不同于 GUI、通知回到 GUI。
- [x] **Step 2：添加因果、错误和生命周期 RED 并运行。** 用真实临时仓库测试 `revertWhileFirstWriteRunningIsNotDropped`、`sessionsConflictOnlyOnSameTarget`、`unknownExtensionConflictSurvivesOwnAcknowledgement`、`reloadBarrierKeepsCompletedSaveOnReadFailure`、`restoreUsesLatestObservedDigest`、`detachedObserverDoesNotCancelAcceptedSave`、`reentrantNotificationDoesNotLoseNewSubmission`。注入锁忙/读错/备份错/主文件提交错，断言暂停且没有无限重试或假成功。执行目标及 EXE：`cmake --build build --parallel 4 --target sleekpr_settings_persistence_tests`、`.\build\sleekpr_settings_persistence_tests.exe -v1 -o build/settings-persistence-red.log,txt`。
- [x] **Step 3：实现队列与控制屏障。** 全局只向专用池提交一个操作，公平轮转就绪会话；每个后台会话对象只在该工作线程访问，GUI 不计算模板差量。未开始目标由同会话最新完整目标替换，即使相对旧基线看似无变化也不得提前删除。不为每个被合并票据保留无限结果表；窗口请求上下文也最多跟随进行中和最新待处理目标。重读/恢复先取消请求会话的未开始保存、等在途操作结束，控制成功才更新代次；失败保留此前自身成功确认，不重放旧目标。
- [x] **Step 4：实现完成通知和安全收束并运行 GREEN。** 先更新服务状态再发信号，发信号后重新检查可能被重入改变的状态；每个已执行任务都结算，窗口只可忽略界面更新，服务不能丢掉写入结果。独立线程异常转结构化失败，不能造成队列永远 running。`detachSession` 保留已接受意图及失败；清理无工作且无阻断的脱离会话。`shutdownIfIdle()` 非空闲返回 false，不在日常 GUI 回调阻塞等待。运行 `ctest --test-dir build --output-on-failure --parallel 1 -R "sleekpr_settings_(save_baseline|save_session|persistence|repository)_tests"`。
- [x] **Step 5：检查取消与资源边界。** `setPendingSuspended(true)` 只保留并暂停尚未开始的普通保存，控制操作仍需串行结算；`hasControlWork()` 报告已接受的打开/重读/恢复是否尚未结束，`isIdle()` 不忽略暂停的保存。只有最终放弃确认才 `discardPending()`，不影响在途写入。补 `suspendedSavesDoNotHideControlWork`，恢复后原保存仍能运行。测试服务销毁前释放屏障并安全完成；记录队列上界、RED/GREEN 和差异，不提交。

### 任务 3：设计器异步保存与可观察状态

**Files:** 修改 `include/sleekpr/app/TemplateDesignerWindow.h`、`src/app/TemplateDesignerPersistence.cpp`、`src/app/TemplateDesignerWindow.cpp`、`src/app/main.cpp`、`tests/tst_label_designer.cpp`、根与测试 CMake。复用 `SettingsPersistenceNotice` 的错误面板，不让“面板是否可见”承担脏状态判断。

**Interfaces:** 设计器新增 `void attachPersistence(SettingsPersistenceService&, SettingsSessionId)`、`void flushPersistenceEdits()`、`bool hasInvalidPersistenceDraft() const`、`void setExitPaused(bool)`；保留 `bool hasUnsavedPersistenceChanges() const`，实现改为模型、原文、计时器及服务状态判断。删除同步 save/reload/restore 回调契约；纯模型测试可不附加持久化服务，生产必须附加。

- [x] **Step 1：添加窗口 RED。** `designerOldSaveDoesNotReplaceNewDraft` 在 A 写入途中编辑 B，断言属性框和文档保持 B，历史只按编辑增长；`designerUndoWhileSavingPersistsOriginal` 检查撤销回原值后磁盘最终正确；`designerHideKeepsQueuedSession` 检查隐藏不等盘、重开使用原会话；`designerInvalidJsonSurvivesOtherSaveCompletion` 保留非法原文和行列错误。用可控屏障和 `QTRY_*`，不保留旧同步写入次数即时断言。
- [x] **Step 2：运行 RED。** `cmake --build build --parallel 4 --target sleekpr_label_app_tests`、`.\build\sleekpr_label_app_tests.exe designerOldSaveDoesNotReplaceNewDraft -v1 -o build/designer-save-red.log,txt`；同时迁移原有冲突、失败、重读和恢复测试为真实异步服务回归。
- [x] **Step 3：接入服务。** `notifySettingsChanged()` 的历史记录与持久化提交分开；提交冻结后的有效模型、`TemplatesOnly` 和票据，完成只更新保存确认与范围通知，不用整份结果替换 `m_settings`。新增紧凑 `persistenceStateLabel`，展示待保存/正在保存/已保存，错误仍使用原 notice；读取初始化期间不允许工厂默认模型触发自动保存。
- [x] **Step 4：接入异步加载和隐藏，并运行 GREEN。** `main.cpp` 在选定实际或临时路径后、窗口前创建共享服务；设计器用新会话异步打开，Loading 时暂缓有效编辑，读取完成后只初始化一次。Ready/Missing 建立有效基线；InvalidJson/IoError 显示错误和兜底文档，不无限停在 Loading，也不允许自动覆盖损坏或不可读配置。隐藏期间继续排队保存；失败/冲突/非法原文沿用确认。无草稿且无任务才可重建，尚在加载也复用。运行完整 `sleekpr_label_app_tests` 及任务 2 套件，等待当前正式预览，检查焦点和中文状态不重叠。
- [x] **Step 5：检查刷新开销。** 只发布成功提交通知；历史、属性和预览不从保存完成再次全量刷新。通过现有性能诊断确认隐藏设置页没有因设计器每次成功写盘而枚举设备；记录结果，不提交。

### 任务 4：设置窗口显式保存与运行时应用隔离

**Files:** 创建 `src/app/SettingsWindowPersistence.cpp`；修改 `include/sleekpr/app/SettingsWindow.h`、`src/app/SettingsWindow.cpp`、`src/app/SettingsWindowRefresh.cpp`、`src/app/SettingsWindowPreview.cpp`、`src/app/main.cpp`、`tests/tst_app.cpp`、`tests/tst_label_designer.cpp`、根与测试 CMake；移除旧 `include/sleekpr/app/SettingsEditSession.h`、`src/app/SettingsEditSession.cpp` 和已迁完的旧测试/目标。

**Interfaces:** 两个 `SettingsWindow` 构造重载在 `settingsPath` 后增加 `SettingsPersistenceService&` 参数，保留 callback、可选模板目录和 parent 顺序；内部创建自身会话。提供 `bool hasUnsavedPersistenceChanges() const`、`void flushPersistenceEdits()`、`void saveForExit()`、`void setExitPaused(bool)`。原 `reloadFromDisk()` 变为异步请求入口，不再表示立即完成。

- [x] **Step 1：添加授权与表单 RED。** `settingsLateSaveDoesNotRollbackNewRuntimeApply`：阻塞保存 A，再点击应用 B，放行 A 后断言磁盘为 A、应用 callback 最后为 B。`settingsEditAfterSaveIsNotImplicitlyPersisted`：只编辑 B 不点击保存，B 仍在表单且磁盘仍 A。`settingsRevertDuringSaveIsNotTreatedAsUntouched`：A 写入中输入最初值后再保存，最终磁盘必须回到最初值。

```cpp
QCOMPARE(repository.read().settings.localHttpLimits.maxHeaderBytes.value(), qsizetype(16 * 1024));
QCOMPARE(appliedSettings.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024));
QCOMPARE(headerSpin->value(), 24);
QVERIFY(window.hasUnsavedPersistenceChanges());
```

该断言场景的保存 A 为 16KB、后应用 B 为 24KB，使用测试已注入的模拟运行时 callback，不触碰真实服务配置。

- [x] **Step 2：补冲突和可选值 RED 并运行。** `settingsUntouchedFallbackDoesNotBecomeExplicit` 检查打开时未配置的可选上限仍为未配置；外部改该上限后只改打印机不覆盖它。保存成功不得重新载入新草稿；隐藏不丢表单；显式重读/恢复失败保留草稿。运行 `cmake --build build --parallel 4 --target sleekpr_app_tests sleekpr_label_app_tests` 和对应新增单项 `-v1 -o` 日志，观察失败。
- [x] **Step 3：迁移窗口保存编排。** 将保存、加载、重试、恢复和状态消费移入新文件；所有设置读写经共享服务。每次显式保存捕获目标模型、所显示表单快照、编辑修订和运行时应用序号，之后编辑不改该目标。采集未改可选值时使用“用户观察 + 自身确认”期望基线，不使用结果中顺带观察的外部值。自身完成推进被请求的显示基线而不改控件，从而正确识别输入回原值。
- [x] **Step 4：隔离回调并运行 GREEN。** “应用”只增加运行时应用序号并调用原 callback，不入保存队列。旧保存成功只有没有更新运行时应用时才应用相应已请求内容；保存通知与运行时 callback 分开。外部刷新在隐藏、有草稿、Loading、在途或排队时延后，避免其异步重读丢掉意图；草稿判断复用编辑状态，不在保存完成的 GUI 回调里反复序列化整份配置。更新所有构造调用和 GUI 用例为状态等待，保留一般保存后的原有生效行为。完整运行 app、label-app 和保存套件。
- [x] **Step 5：移除同步生产路径。** 用 `rg -n "SettingsEditSession|setSettingsSaveHandler|setSettingsReloadHandler|setSettingsRestoreHandler" src include tests CMakeLists.txt` 核对没有残余编译调用；历史设计文档允许保留原名。确认七类旧会话测试都已迁移，再移除旧文件和目标，不仅删除测试来获得通过。记录验证，不提交。

### 任务 5：可撤销的 HTTP 退出准入门禁

**Files:** 修改 `include/sleekpr/http/LocalHttpServer.h`、`include/sleekpr/http/LocalHttpRouter.h`、`src/http/LocalHttpServer.cpp`、`src/http/LocalHttpRouter.cpp`、`tests/tst_http.cpp`。

**Interfaces:** `LocalHttpServer::setExiting(bool)` 委托 `LocalHttpRouter::setExiting(bool)`；router 在 GUI 线程维护门禁，不跨线程、不关闭重绑 socket。

- [x] **Step 1：添加路由 RED。** `exitingAllowsOnlyHealth` 检查健康成功，GET settings/printers、POST settings/preview/print/template 管理以及 OPTIONS 均不进入业务；合法请求得到 503、`success=false`、`code=CLIENT_EXITING`、原错误包络和合法 CORS 头；拒绝 Origin 仍为 403，超限仍为 413。
- [x] **Step 2：添加 socket RED 并运行。** `exitGateRejectsBodyCompletedAfterExitStarted` 在随机本地测试端口发送半个 POST，开启门禁后送完，断言文件摘要未变、假打印引擎调用为 0；`cancelExitReopensAdmissionWithoutRebind` 检查监听端口不变、正常接口恢复。执行 `cmake --build build --parallel 4 --target sleekpr_http_tests` 和 `.\build\sleekpr_http_tests.exe -v1 -o build/http-exit-red.log,txt`。
- [x] **Step 3：实现门禁位置。** 保留请求大小检查、Origin 校验及 CORS；在 OPTIONS 快速返回和业务模块分发之前判断退出，只放行精确 `GET /health`。503 使用已有 `failEnvelope/jsonResponse`，`statusReason()` 增加 `503 -> Service Unavailable`，不复制响应序列化。
- [x] **Step 4：运行 GREEN。** 执行 `ctest --test-dir build --output-on-failure --parallel 1 -R "^sleekpr_http_tests$"`，既有接口、413、半包和新门禁回归都通过；不启动生产端口 37122，不发送真实打印。
- [x] **Step 5：检查契约。** `POST /settings` 仍显式整体替换，无新必填字段；门禁关闭后所有正常路由不受影响。记录状态码、文件摘要和假引擎调用断言，不提交。

### 任务 6：退出协调器、原生提示与生命周期接入

**Files:** 创建退出参与者契约、退出协调器、退出对话框及 `tests/tst_application_exit.cpp`；修改两个窗口 header/持久化文件、`PrintWorkbenchWindow.h/.cpp`、`PrintWorkbenchPreview.cpp`、`src/app/main.cpp`、`tests/tst_print_workbench.cpp`、根与测试 CMake，新增 `sleekpr_application_exit_tests`。

**Interfaces:**
- `ExitState { Running, Collecting, WaitingForSaves, Blocked, Ready, Quitting }`；`ExitDraftState` 包含 `manualDraft`、`invalidDraft`、`activeSubmission` 和 `QStringList issues`。
- `ExitParticipant` 为值对象，含 `id/title` 及 GUI 回调 `flushEdits()`、`inspect() -> ExitDraftState`、`saveManualDraft()`、`setPaused(bool)`、`focus()`；窗口销毁后回调用 GUI 弱观察安全失效，服务会话失败仍可独立列出。
- `ExitDraftState` 与 `ExitParticipant` 在 `ApplicationExitParticipant.h` 中定义；两个编辑窗口各新增 `ExitDraftState exitDraftState() const`，供组装层读取语义状态而不是遍历其控件。
- `ApplicationExitCoordinator(SettingsPersistenceService&, std::function<void(bool)> setExitingAdmission, std::function<void()> quit, QObject* parent = nullptr)`；`void addParticipant(ExitParticipant)`、`requestExit()`、`cancelExit()`、`saveManualAndContinue()`、`continueWithoutSavingManual()`、`retryAndContinue()`、`discardAndContinue()`、`refresh()`；`ExitState state() const`；信号 `stateChanged()`。生产 quit 注入 `QApplication::quit`，测试注入计数 callback。
- `ApplicationExitDialog(ApplicationExitCoordinator&, QWidget* parent = nullptr)` 使用 `show/open` 而非 `exec()`；关闭对话框等同返回编辑。工作台新增 `bool hasActivePrintSubmission() const`、`void setExitPaused(bool)`、`void setSubmissionStateChangedCallback(std::function<void()>)`，无需为单个回调把整窗口变为信号中心。

- [x] **Step 1：添加状态机 RED。** 测试 `waitsForAcceptedSaveAndPendingTimers`、`invalidOrFailedDraftBlocksExit`、`manualDraftRequiresExplicitChoice`、`skipManualDraftStillFinishesPreviouslyAcceptedSaves`、`repeatedExitDoesNotFlushTwice`、`cancelExitInvalidatesOldCompletion`。有屏障的存储未结束时 `quitCalls == 0`，解除且无阻断才恰好为 1；Conflict/非法输入不允许“重试”绕过。运行 `cmake --build build --parallel 4 --target sleekpr_application_exit_tests` 和 `.\build\sleekpr_application_exit_tests.exe -v1 -o build/application-exit-red.log,txt`，保存 RED 日志。
- [x] **Step 2：添加取消和打印 RED。** `discardWaitsForRunningWrite`、`cancelAfterDiscardRestoresPendingIntent`、`resumePreservesExplicitDisabledChild`、`acceptedTimerPrintFinishesBeforeQuit`。模拟引擎只记录已接受提交，测试退出等待不会销毁它或再调用第二次。对待保存/失败/冲突/非法原文和未保存表单分别检查默认按钮为返回编辑，放弃有二次确认。
- [x] **Step 3：实现协调。** 每次退出生成代次，冻结用户修改、调用 flush，再收集手动保存选择；`manualDraft` 只表示当前未请求保存的表单，不重复询问同值在途目标。`continueWithoutSavingManual()` 仅放弃本次手动表单目标，仍等待此前已经接受的保存，不等于放弃全部排队保存。接受退出目标后禁止新 requestSave，重试已有目标仍可调用 retry。事件驱动重算状态，不阻塞 GUI、不递归 processEvents、不按超时强退。等待期启用任务 5 门禁。取消恢复接受请求及门禁，并使旧退出代次失效。
- [x] **Step 4：实现可取消的放弃与原生提示。** 选择放弃后先 `setPendingSuspended(true)`，保留未开始意图与界面草稿，等待正在执行操作和控制操作结束；期间返回编辑则解除暂停，原意图和原文仍在。确认没有运行任务或控制操作后，才清除待处理保存，再检查 `isIdle()` 才进入 Ready 并授权 quit；已完成写入不回滚。对话框显示来源、问题和队列状态，使用规格中的保存/不保存/返回/重试/放弃文案，默认不破坏数据；二次确认也使用非阻塞原生提示，缩窄窗口仍能读完整原因。
- [x] **Step 5：接入应用并运行 GREEN。** 托盘退出只请求协调器；正常启动、纸张/预设入口、预览和编辑新请求都受退出期门禁，已有同步打印按原方式结算。只暂停窗口根内容和语义入口，记录根原启用状态，不逐个强行启用子控件；返回后只恢复必要的预览计划。服务/执行器先创建、后于窗口销毁，真正 quit 前服务已空闲，最后清理不新发保存。运行退出、工作台、HTTP、app 和 label-app 套件，记录结果，不提交。

### 任务 7：真实客户端隔离冒烟、性能与 Harness 验收

**Files:** 修改 `src/app/main.cpp`，创建 `src/app/ApplicationExitSmokeTest.cpp`、`include/sleekpr/app/ApplicationExitSmokeTest.h`；修改 `tests/CMakeLists.txt`、`tests/tst_label_designer.cpp`、`include/sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h`、其实现及 `tests/tst_client_performance_metrics.cpp`。按实际落地更新 `.harness/wiki/settings-persistence.md`、`.harness/rules/async-preview-and-drafts.md`、`.harness/skills/verification/SKILL.md`，新增 `.harness/skills/verification/references/background-save-exit.md`。

**Interfaces:** `int runApplicationExitSmokeTest(QApplication&)` 仅处理 `--smoke-test-safe-exit` 的隔离运行，使用真实保存服务、窗口和退出协调器，不复制其业务逻辑；返回进程退出码。性能阶段增加 `SaveEnqueue`，后台落盘继续使用 `Saving`，仍按原 512 样本环记录纯数值。

- [x] **Step 1：添加进程冒烟 RED。** CTest 新增 `sleekpr_safe_exit_launch_test`，执行真实 `sleekpr --smoke-test-safe-exit`，沿用当前测试运行库设置，TIMEOUT 20。该模式必须在生产设置目录、端口和托盘初始化前分流，内部使用临时配置、`WA_DontShowOnScreen` 与模拟打印；覆盖保存后退出、取消后可再编辑再退出、非法草稿阻断再返回。超时仅是测试失败，不是产品强制退出策略。
- [x] **Step 2：更新原真实启动回归。** 原 `--smoke-test-designer` 不再在固定 100ms 内假设异步加载完成；保留实际 exe 入口点击、无脏状态可重建及有任务复用对象的断言，改为按加载/保存状态推进。运行 RED 验证未支持新退出模式或旧同步假设的失败，不以普通启动真实客户端代替测试。
- [x] **Step 3：实现独立冒烟与可比较指标。** 冒烟逻辑单列文件，main 只分流和组装，`--build-info-json` 仍最先处理且不触碰配置。在 GUI 仅计入队阶段，读/差量/写盘计入后台 Saving；测试新阶段命名与环上界。Release 中同样本连续编辑/拖拽，分别报告入队和落盘 P50/P95；不把 Debug 旧样本与 Release 新样本比较，不声称真实驱动已不阻塞。
- [x] **Step 4：运行整体 GREEN。** 先完整构建 Debug/Release，再执行下列命令；同一模式失败先修复根因，不增加固定等待掩盖错误。对话框和两个保存状态的截图可由 Qt 测试在临时场景生成，实际查看紧凑窗口、150% 缩放、长错误文本、默认按钮和焦点；不启动用户实例或真实打印。

```powershell
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 1 --output-junit build/debug-ctest.xml
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure --parallel 1 --output-junit build-release/release-ctest.xml
node --test tests/js/sleekpr-print-client.test.mjs
git diff --check
```

每个命令执行后立即核对退出码；本段以已核对的单配置 Ninja 缓存为前提，多配置先补实际 `--config/-C`。记录实际总数、失败、跳过及逐用例日志，不固定要求历史 30 项。保留原预览慢工作池回归，证明保存专用执行器不被预览占用影响。

- [x] **Step 5：独立复核与最终说明。** 按规格逐项对照原文、服务队列、基线与运行时状态、取消退出和 HTTP 门禁；请独立审阅者检查整体差异，修复发现后重跑受影响测试及完整门槛。Harness 只记录本轮最终实现和可复现命令，不更新无关技能，不把强制终止写成受保护退出。报告代码更新、实际验证及正式包未重建的事实，不自行提交或推送。

## 规格覆盖与执行门槛

| 规格章节 | 对应任务 |
| --- | --- |
| 分层、复用仓库、观察基线与自身确认 | 1、2 |
| 单线程、有界公平队列、失败暂停与控制屏障 | 2 |
| 设计器历史、原文、隐藏与重开 | 3 |
| 手动保存、可选值、运行时应用隔离 | 4 |
| HTTP 正常兼容与退出准入恢复 | 5 |
| 收集、阻断、重试、放弃、取消与在途打印 | 6 |
| 应用寿命、实际 exe 冒烟、性能与 Harness | 7 |

计划自检要求：接口名称和类型在后续任务引用时保持一致；所有 Review Focus 已指向具名回归；范围外存储、打印线程、正式打包和无关草案均不纳入实施。

已按用户选择完成当前对话顺序实施及独立整体复核。复核的两项问题均已补真实 RED/GREEN 回归并修复；最终 Debug 34/34（224.03 秒）、Release 34/34（109.45 秒），两种原生隔离冒烟通过；另完成 Qt 倍率 1.0/1.5 截图检查、慢预览工作池及独立真实拖拽采样。未提交、未合并、未推送或重新打包，保留本计划的本地执行证据。
