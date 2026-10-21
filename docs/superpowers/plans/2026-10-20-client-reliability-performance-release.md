# 客户端可靠性、预览性能与正式包实施计划

> 面向执行代理：必须使用 `superpowers:executing-plans` 在当前对话逐项实施，或在用户选择后使用 `superpowers:subagent-driven-development`。执行方式待用户选择，以下使用 `- [ ]` 跟踪；本计划不授予提交、推送、强杀进程或真实打印权限。

**Goal:** 按保存冲突保护、隐藏窗口及 UI 线程减负、预览资源预算、版本识别与正式绿色包的顺序交付可回归的改进。

**Architecture:** 核心层定义差量合并、预算及状态数据；基础设施层统一持久化、后台 Qt 图像工作和构建诊断；应用层保留草稿、调度最新预览并展示结构化结果。保留旧存储门面与 HTTP 契约，不重写整个设计器。

**Tech Stack:** C++20、现有 Qt 6.8.3 Core/Gui/Widgets/Network/PrintSupport/Test、MSVC 2022、CMake/Ninja、PowerShell、现有 Node 测试；不增加 Qt 以外运行时依赖。

**Spec:** [已确认设计规格](../specs/2026-10-20-client-reliability-performance-release-design.md)。执行者必须同时阅读规格、`AGENTS.md` 和 `.harness/skills/verification/SKILL.md`。

## 全局约束

- 固定顺序为四个阶段；每阶段形成可用且独立验证的交付，阶段门通过前不开始下一阶段。
- 保持正常标签毫米坐标、打印 DPI、字段优先级及 Qt 原生输出，不恢复复杂表格入口或实施动态单据规格。
- 旧 `POST /settings` 保持显式整体替换，不新增必填 revision；客户端差量保存才提供本轮乐观冲突保护。
- 真实打印驱动线程、任务队列和任务历史不在本轮范围，不跨线程调用现有借用的 `LabelPrintEngine`。
- 新预览默认单边上限为 16,384 像素、单页上限为 64,000,000 像素，不静默降低打印或正式预览 DPI。
- 先写测试，再观察目标断言失败，最后实现。缺失头文件或链接失败只能说明测试尚未就绪，不能当成业务失败回归；最终不留接口桩。
- 注释中文、源码 UTF-8、禁止 CodeRabbit。GUI 测试串行，仅临时配置和模拟打印。
- 保留现有未提交的 `AGENTS.md`、`.harness/` 与动态单据规格，不自动暂存或改写它们。阶段完成不等于允许提交；如另获授权，使用中文备注。

## 评审重点

1. JSON 重排、缺省值和未知字段不应导致无关数据丢失或伪冲突；由任务 1 的规范化与未知字段测试约束。
2. 备份、锁和恢复过程失败时原文件不能变化；恢复前外部再次修改也不能被覆盖，由任务 2 约束。
3. 慢预览完成、窗口关闭和应用退出交错时，不得发布旧图或访问已销毁成员，由任务 4、5 约束。
4. 页数从 9 变 10、JSON 转义及编码失败不能绕过资源限制或返回伪成功，由任务 6、7 约束。
5. 无 Git 元数据、源码未提交、发布目录为重解析点或验证被跳过时，不得伪报已验收正式版本，由任务 8、9 约束。

## 文件与验证约定

下文路径均相对仓库根目录。新增组件以对应同名头文件和实现文件成对创建；不把新编排继续塞进 `TemplateDesignerWindow.cpp`。新测试目标统一在 `tests/CMakeLists.txt` 注册，并调用现有 `configure_qt_test_runtime`。

构建命令在已初始化的 MSVC x64 环境运行；单独执行 Qt 测试前配置当前 Qt `bin`。每个原生命令立刻检查退出码。

```powershell
cmake --build build --parallel 4
if ($LASTEXITCODE -ne 0) { throw '构建失败' }
ctest --test-dir build --output-on-failure --parallel 1
if ($LASTEXITCODE -ne 0) { throw 'CTest 失败' }
node --test tests/js/sleekpr-print-client.test.mjs
if ($LASTEXITCODE -ne 0) { throw '客户端脚本测试失败' }
git diff --check
if ($LASTEXITCODE -ne 0) { throw '差异检查失败' }
```

单项目标先 `cmake --build build --target 目标名 --parallel 4`，检查退出码后运行 `build/目标名.exe -o build/目标名.log,txt`，保存退出码、读取 UTF-8 日志，再据退出码判断。每个新测试先单项 RED，再单项 GREEN；阶段门使用上述完整验证，而不是仅编译测试程序。

## 第一阶段：保存冲突保护

### 任务 1：纯设置变更模型

**文件：** 新增 `include/sleekpr/core/settings/SettingsChangeSet.h`、`src/core/settings/SettingsChangeSet.cpp`、`tests/tst_settings_change_set.cpp`；修改 `CMakeLists.txt`、`tests/CMakeLists.txt`。

**接口：** `SettingsEditScope { TemplatesOnly, AllChangedFields }`；`SettingsFieldChange { QStringList path; QJsonValue expected; QJsonValue value; }`；`SettingsChangeSet` 保存有序变更列表；`SettingsMergeResult { bool changed; QJsonObject document; QStringList conflicts; }`。

```cpp
SettingsChangeSet SettingsChangeSet::between(const PrintClientSettings& base, const PrintClientSettings& edited, SettingsEditScope scope);
SettingsMergeResult SettingsChangeSet::merge(const QJsonObject& latest) const;
```

- [x] 写失败测试 `mergesTemplateWithoutRestoringOldPrinter`：base 打印机为 `old`，edited 只改 default 模板，latest 打印机为 `new`；断言合并后打印机仍为 `new` 且模板更新。
- [x] 加入 `rejectsOverlappingTemplateAtomically`、`acceptsIdempotentTarget`、`keepsUnknownFieldsAndNormalizesDefaults`、`distinguishesMissingEntryFromNull`。冲突时 `document` 保持 latest 完整不变，`changed` 为 false。
- [x] 运行 `sleekpr_settings_change_set_tests`，确认业务断言失败，再实现。复用既有设置及模板 JSON 规范化；比较时用规范化值，输出从原 latest 对象起步，保留未改的未知字段。
- [x] 变更粒度固定为默认打印机、Origin 列表、偏移 x/y、五个 HTTP 上限各自的值；模板文档按槽位、旧元素列表按模板键、旧元素覆盖按模板键及元素键处理。路径用 `QStringList`，不以点号拆用户键；Undefined 表示缺失，不与 Null 混同。
- [x] 验证 TemplatesOnly 不产生任何其他字段变更；完整模板作为原子值比较，不按 mtime 或显示名称判断。单项 GREEN 后评审此任务。

### 任务 2：统一持久化、锁和备份

**文件：** 新增 `include/sleekpr/core/settings/SettingsPersistenceResult.h`、`include/sleekpr/infrastructure/settings/SettingsRepository.h`、`src/infrastructure/settings/SettingsRepository.cpp`、`tests/tst_settings_repository.cpp`；将旧 `src/core/settings/FileSettingsStore.cpp` 移为 `src/infrastructure/settings/FileSettingsStore.cpp` 兼容门面；更新 CMake 源文件列表。

**接口：** `SettingsReadStatus { Missing, Ready, InvalidJson, IoError }`；`SettingsReadResult { status, settings, sourceJson, sourceDigest, message }`。`SettingsWriteStatus { Saved, Unchanged, Conflict, Busy, ReadFailed, BackupFailed, WriteFailed }`；`SettingsWriteResult { status, snapshot, conflicts, message }`，`ok()` 仅对 Saved/Unchanged 为 true。

```cpp
SettingsReadResult SettingsRepository::read() const;
SettingsWriteResult SettingsRepository::apply(const core::SettingsChangeSet& changes) const;
SettingsWriteResult SettingsRepository::replace(const core::PrintClientSettings& settings) const;
SettingsWriteResult SettingsRepository::restoreBackup(const QByteArray& expectedCurrentDigest) const;
```

- [x] 写 `staleTemplateIsRejectedAndFileUnchanged`、`independentChangesSurvive`、`replacementUsesSameLockAsPatch`；保持另一个 `QLockFile` 持锁时两种写入都返回 Busy。
- [x] 写 `corruptFileBlocksAutomaticPatch`、`explicitReplacementPreservesCorruptBytes`、`backupFailureDoesNotReplaceMainFile`、`restoreRejectsChangedDigest`、`missingFileInitializesWithoutError`。所有文件位于 `QTemporaryDir`；用临时同名目录制造备份失败，不修改用户 ACL。
- [x] 对主文件提交失败采用可注入 `AtomicWriter = std::function<bool(const QString&, const QByteArray&, QString*)>`，默认实现用 QSaveFile；断言失败时主文件原文和成功快照不变。测试运行目标 `sleekpr_settings_repository_tests`，观察 RED。
- [x] 实现仓库并使用 `<settingsPath>.lock`、`QLockFile::tryLock(0)`，Busy 立即返回，不循环等待。锁覆盖读取、合并、备份、替换全过程。最近有效原文保存至 `.bak`；显式替换损坏文件前保存 `.corrupt.bak`，不以坏内容覆盖有效备份。
- [x] 旧 `core::FileSettingsStore` 的头文件 API 不变，基础设施中的门面实现委托仓库：load 继续兼容默认值，save 调用 replace；不可保留旧的无锁写入实现。仓库内部不回调门面，避免递归锁。
- [x] GREEN 后运行旧 core、HTTP、工作台存储回归，确认 JSON 与整体替换契约不变，并评审此任务。

### 任务 3：保存结果接入编辑会话与窗口

**文件：** 新增 `include/sleekpr/app/SettingsEditSession.h`、`src/app/SettingsEditSession.cpp`；修改 `TemplateDesignerWindow.h/.cpp`、`SettingsWindow.h/.cpp`、`main.cpp`、`src/http/SettingsRoutes.cpp`；扩展 `tst_label_designer.cpp`、`tst_app.cpp`、`tst_http.cpp`、`tst_print_workbench.cpp` 及 CMake。

**接口：** `SettingsEditSession` 保存仓库和最后成功快照，提供 `saveEdited(const PrintClientSettings&, SettingsEditScope) -> SettingsWriteResult`、`reload() -> SettingsReadResult`、`baseline() const -> const SettingsReadResult&`。新增设计器 `setSettingsSaveHandler(std::function<SettingsWriteResult(const SettingsChangeSet&)>)`、`setSettingsReloadHandler(std::function<SettingsReadResult()>)` 和 `setPersistenceSnapshot(const SettingsReadResult&)`；最后一个入口只初始化保存基线与读取状态，不覆盖草稿。保留既有 void 变更观察回调，不再把它当保存成功确认。

- [x] 写 GUI 失败回归 `designerConflictKeepsDraftAndHistory`、`designerSaveFailureDoesNotAdvanceBaseline`、`settingsSavePreservesExternalTemplate`、`httpSettingsReplacementRemainsCompatible`。夹具先建立成功快照，再模拟外部修改，断言失败提示、草稿及文件内容。
- [x] 写 `corruptSettingsCannotBeAutoRecreatedByEditing` 与 `reloadConflictRequiresConfirmation`：拒绝重新读取时仍保留本地文档，确认后才加载外部版本；导出使用当前有效文档模型，不谎称包含尚未应用的非法控件原文。
- [x] 运行对应测试观察 RED，再接入 session、结构化保存状态与“重新读取／导出草稿”入口。冲突后暂停重复保存该冲突版本；只有成功才推进基线并发布提交后的快照。
- [x] main 的变更观察回调不再无条件 `FileSettingsStore.save(settings)`。设置窗口按实际变更保存，HTTP 保持整体 replace；读、锁、备份、提交失败分别映射现有失败包络，不新增必填字段。
- [x] 维护未应用/未保存/已保存状态，保存或关闭操作不得把失败描述为成功。所有自动应用、撤销、名称刷新旧回归保持 GREEN；完成第一阶段完整验证并评审后，才进入第二阶段。

## 第二阶段：隐藏窗口及 UI 线程减负

### 任务 4：最新预览队列与生命周期

**文件：** 新增 `include/sleekpr/infrastructure/preview/PreviewExecutor.h`、`src/infrastructure/preview/PreviewExecutor.cpp`、`include/sleekpr/app/LatestPreviewQueue.h`、`src/app/LatestPreviewQueue.cpp`、`tests/tst_latest_preview_queue.cpp`，修改 CMake。

**接口：** `PreviewWorkResult` 保存可选原生计划、QImage、错误代码/文字与规划/栅格化耗时。`PreviewExecutor` 是 GUI 线程拥有的 QObject，使用现有 QtCore 的 QThreadPool/QRunnable，全局最大并发为 2；应用拥有执行器，其生命周期长于窗口。`PreviewExecutor::submit(std::function<PreviewWorkResult()> work, std::function<void(PreviewWorkResult)> guiCompletion)` 在工作线程计算，在 GUI 线程完成回调；队列回调只捕获其状态的弱所有权。

```cpp
quint64 LatestPreviewQueue::submit(std::function<PreviewWorkResult()> work);
void LatestPreviewQueue::invalidate();
void LatestPreviewQueue::resultReady(quint64 generation, const PreviewWorkResult& result);
```

- [x] 写 `publishesOnlyNewestResult`、`retainsOneRunningAndOnePending`、`invalidatedQueueDropsResults`、`destroyedOwnerIsNeverCalled`、`workRunsOffGuiThread`。用信号量控制 A 阻塞，依次提交 B/C，断言 B 未启动且只发布 C；不用真实睡眠猜顺序。
- [x] 写独立进程 `applicationExitDrainsPreviewWorkSafely`，确认关闭窗口不等待旧任务发布，退出时在 QApplication 销毁前收束执行器；运行 `sleekpr_preview_queue_tests` 观察 RED。
- [x] 实现单飞及最新待处理任务，递增代号判断结果。工作只捕获不可变数据，不捕获 QWidget、QPixmap 或窗口成员引用。
- [x] 执行器通过自身受控生命周期接收工作结果，再在 GUI 线程检查队列状态的弱所有权；窗口关闭后不调用其函数。不能从后台读取 GUI QPointer 状态或对可能已销毁的窗口直接投递。
- [x] GREEN 后评审所有权、应用退出顺序与缓存线程安全；不新增 QtConcurrent 等部署模块，不接触打印引擎线程。

### 任务 5：接入预览、按需刷新与份数元数据

**文件：** 新增 `include/sleekpr/infrastructure/preview/TemplatePreviewJob.h`、`src/infrastructure/preview/TemplatePreviewJob.cpp`；修改 `TemplateDesignerWindow.h/.cpp`、`SettingsWindow.h/.cpp`、`PrintWorkbenchWindow.h/.cpp`、`PrintPreviewPanel.h/.cpp`、`main.cpp`、`TemplatePrintService.h/.cpp`、`PrintCopyPlanner.h/.cpp` 及相关现有测试。

**接口：** `TemplatePreviewSnapshot` 冻结文档、设置、输入上下文、纸张、设备 profile、预览 DPI/缩放/DPR 与所选页；`TemplatePreviewJob::run(const TemplatePreviewSnapshot&) -> PreviewWorkResult`。共享打印服务增加 `resolveSnapshot(const PrintClientSettings&, const TemplatePrintInput&, QString* errorCode, QString* errorMessage) -> std::optional<TemplatePrintSnapshot>`，负责解析并冻结来源，不布局或栅格化；增加不重新读盘的 `prepareSnapshot(const TemplatePrintSnapshot&, PrintPreflightReport*)`，旧 prepare 委托这两个入口，避免复制规则。`TemplatePrintSnapshot` 包含已解析文档、真实上下文、纸张/偏移/profile、模板标识和完整性检查标记。

`PrintCopyPlanner::validateCopies(const NativePrintDrawingPlan&, int, QString*) -> std::optional<int>` 返回总页数而不构造重复页面；repeat 复用该校验。窗口新增 `requestExternalRefresh(...)`，区分名称与内容/设备变化；隐藏时记录失效状态，显示后处理。

- [x] 写 `hiddenSettingsDoesNotRenderOnTemplateChange`、`hiddenWorkbenchRefreshesOnceOnShow`、`visibleSettingsDraftSurvivesExternalChange`、`stalePreviewNeverEnablesPrint`、`nameOnlyRefreshDoesNotReplan`，观察 RED。
- [x] 将 GUI 读取控件与构造快照留在主线程，规划、字号测量、QImage 放到任务 4 队列；模板/预设/纸张解析复用服务后冻结，工作阶段不再重新读取变化中的配置。QPixmap 和预览覆盖层仅在 GUI 发布。
- [x] 翻页、缩放和 DPR 更新也走受控栅格化；缓存键包含计划身份、页码、倍率、DPR。隐藏或切换目标立即失效旧任务，工作台输入一变同步废弃旧打印计划。调整现有显示前预览测试为等待可观察结果，不要求异步实现仍同步完成。
- [x] 写 `copyValidationDoesNotMaterializePages`，对同一计划比较元数据校验和 repeat 的总页数；打印按钮检查只调用 validateCopies，提交前 repeat 一次，现有完整页序及上限断言不变。
- [x] 新增不记录字段内容的耗时与计数采样：规划、栅格化、保存、结果发布、隐藏窗口刷新。以相同构建模式和固定输入记录优化前后 P50/P95，不用机器相关耗时作为不稳定单元测试断言。
- [x] 验证名称、光标、非法 JSON、先核对新预览再打印、缩放及关闭窗口回归；完成第二阶段完整验证、原生截图和评审后再进入第三阶段。

## 第三阶段：预览资源预算

### 任务 6：分配前预算及渲染失败传播

**文件：** 新增 `include/sleekpr/core/printing/PreviewResourceBudget.h`、`src/core/printing/PreviewResourceBudget.cpp`、`tests/tst_preview_resource_budget.cpp`；修改 `LabelPreviewImageRenderer.h/.cpp`、`TemplatePreviewJob.h/.cpp`、`TemplatePrintService.h/.cpp`、`TemplateDocumentRenderer.h/.cpp`、`TableElementLayout.h/.cpp`、`TableElementRenderer.h/.cpp`、`tst_print_preview.cpp`、`tst_http.cpp` 及 CMake。

**接口：** `PreviewResourceBudget` 默认 `maxSidePixels=16384`、`maxPixels=64000000`、`maxImageBytes=256000000`。`PreviewImageRequest` 包含纸张毫米、DPI、缩放、DPR；`PreviewImageCheck` 返回可用 QSize 或类型化 `InvalidInput/ResourceLimit`。检验使用溢出安全计算，ARGB32 每像素 4 字节。

增加 `tryRenderImage(plan, request, budget)`、`tryRenderPng(plan, request, budget, maxEncodedBytes)`，返回带 `None/InvalidInput/ResourceLimit/AllocationFailed/PaintFailed/EncodeFailed` 的结构化结果。request 保存基础 DPI、缩放和 DPR，预算与渲染共用一次计算出的实际像素尺寸，不能重复乘缩放。旧 renderImage/renderPng 包装失败时只返回空值，生产入口必须使用结构化方法。

`PreviewPlanningBudget` 的页数采用调用方传入的剩余页数；本轮预览任务初始上限为 200,000 个绘制命令和 200,000 个布局单元格，按整个任务累计，正式打印未启用这些新增预览限制时语义不变。

- [x] 写 `rejectsSideBeforeAllocation`、`rejectsPixelProductBeforeAllocation`、`rejectsNonFiniteGeometry`、`includesZoomAndDpr`、`acceptsExactBudgetBoundary`。边界测试只调用预算函数，不实际分配 64,000,000 像素图片。
- [x] 主断言示例为 `maxSidePixels == 16384`、`maxPixels == 64000000`；对允许单边但总像素超限的尺寸断言 ResourceLimit，对 NaN/Inf 断言 InvalidInput。运行 `sleekpr_preview_budget_tests` 观察 RED。
- [x] 在取整、转换 int、QImage 和编码缓冲分配前验证。使用实际尺寸与字节布局，并检查 image.isNull、QPainter 状态及 image.save 返回值；PNG 写入使用有界 QIODevice，超过 maxEncodedBytes 返回 ResourceLimit，不先分配完整超限缓冲。编码失败用可注入编码函数确定性测试，不依赖本机缺少 PNG 插件。
- [x] 将可选规划预算沿共享文档/表格规划入口传递，在数组展开、表格布局与命令追加前检查剩余额度，不能等全量布局列表生成才计数。页数取调用方剩余额度，200,000 命令及 200,000 单元格跨表、跨页累计；预算未开启的既有正式打印调用不悄悄降低 DPI。
- [x] 通过任务 4 结果类型把失败送回 GUI，超预算显示具体调整方向而非旧图。加入失败后旧图不可打印的断言，单项 GREEN 后评审。

### 任务 7：HTTP 预算与线性响应计量

**文件：** 新增 `include/sleekpr/http/PreviewResponseBudget.h`、`src/http/PreviewResponseBudget.cpp`；修改 `src/http/PreviewRoutes.cpp`、共享模板规划入口、`tests/tst_http.cpp`、`docs/preview-template-api.md` 及 CMake。

**接口：** `PreviewResponseBudget::appendPage(const QJsonObject&) -> bool` 累计单页紧凑 JSON 字节和逗号；`bodyBytes(const QString& requestId, int totalItems, int totalPages, const QJsonObject& paper) const -> qsizetype` 用实际包络大小计算总字节，最后完整序列化复核。字节累计有溢出检查。

- [x] 写 `previewRejectsPixelsBeforePng`、`previewResponseCountsEscapesAndPageDigits`、`previewBudgetFailureNeverReturnsEmptySuccess`；覆盖中文及引号 requestId、9/10 页边界、等于/大于限制各 1 字节，观察 RED。
- [x] 路由使用 tryRenderPng；ResourceLimit 映射 413/REQUEST_TOO_LARGE，其余失败沿用既有失败包络，不从文字推断错误种类。单条/批量均检查剩余页数和图片预算，不能只检查 body 大小。
- [x] 删除每页重新序列化全部既有 Base64 页面的计量；累计单页并最终精确复核。保留 pages、paper、requestId 等响应结构及旧正常请求的内容。
- [x] 运行 HTTP、预览、模板规划回归；更新 API 文档中的限制说明，不新增强制参数。完成第三阶段完整验证与评审。

## 第四阶段：版本识别与正式绿色包

### 任务 8：增量构建信息与隔离诊断

**文件：** 新增 `cmake/GenerateBuildInfo.cmake`、`packaging/BuildVersion.h.in`、`include/sleekpr/app/BuildInfo.h`、`src/app/BuildInfo.cpp`、`include/sleekpr/app/BuildInfoDialog.h`、`src/app/BuildInfoDialog.cpp`、`tests/check_build_info.cmake`；修改 CMake、`main.cpp`、工作台/设置版本入口及 `quick-start.ps1`。

**接口：** `BuildInfo::current()` 返回项目版本、完整 revision 或 unknown、configuration、sourceDirty、workspaceDirty、sourceDigest；无法判断的脏状态用可空值，不伪报 clean。`BuildInfo::toJson()` 输出相同字段；`--build-info-json` 在读取设置、启动 HTTP 或托盘前输出 JSON 并退出。

- [x] 写进程/构建测试 `reportsBuildModeAndRevision`、`updatesWithoutManualReconfigure`、`marksMissingGitUnknown`、`distinguishesDocsFromSourceDirty`，以及 `diagnosticDoesNotTouchUserSettings`，观察 RED。
- [x] 构建前目标探测 Git 与源码指纹，仅内容变化时更新 `build/generated/sleekpr/BuildVersion.h`，不因每次执行时钟变化强制重编译。源码集合包含 CMake、src/include/resources 和发布脚本；workspaceDirty 另包含非忽略的文档改动。
- [x] 在临时 Git 仓库测试 HEAD 变化、未跟踪源码、仅文档变化与无 Git 情况，不提交或移动真实工作区 HEAD。生成头文件不写机器绝对路径，sourceDigest 标识同一提交下不同未提交源码。
- [x] Qt 应用版本、启动日志和诊断窗口复用 BuildInfo；路径在运行时取得，复制诊断不包含设置正文。启动器显示目标模式、路径和同一 JSON 标识，不改变已有启动/重建开关语义。
- [x] 用 Debug/Release 构建分别断言 configuration；CLI 诊断不创建操作员配置或监听端口，单项 GREEN 后评审。

### 任务 9：正式包验收与安全发布

**文件：** 新增 `packaging/PortablePackage.psm1`、`tests/check_portable_package.ps1`；修改 `package-portable.ps1`、CMake 发布目标、`packaging/README-PORTABLE.txt` 及测试注册。

**接口：** 模块集中提供发布目标路径校验、构建信息读取、构建清单/文件摘要生成和阶段结果；`package-portable.ps1` 保留现有参数，默认不使用 StopRunning。版本和归档名读取统一项目版本，不保留脚本中的 0.1.0 硬编码。

- [x] 用临时目录及可注入命令调用器写 `rejectsEscapingOrReparseOutputPath`、`failedReleaseTestsDoNotPublish`、`manifestMatchesExecutable`、`noTestsIsMarkedUnverified`、`archiveContainsNoDebugArtifacts`。测试不调用真实编译器、不清理真实 dist，不依赖新增测试框架。
- [x] 观察 RED 后，在独立 build-release 构建并先运行 Release CTest。暂存部署目录、manifest 和 zip 均先验收再替换最终输出；取消不必要的无条件清空整个 build-release，清理前解析并验证仓库内预期路径，拒绝越界和重解析点。
- [x] 包内 manifest 记录 BuildInfo、文件相对路径与 SHA-256、测试/启动验收状态；不计算 manifest 自身摘要，避免循环依赖，归档摘要在包外验证记录中保存。枚举并排除 PDB、测试配置、日志与截图；NoTests 只能生成明确标记未验收的候选包，不能报告正式包验收完成。
- [x] 清除进程级开发 Qt 插件和 PATH 注入后，从暂存 Release 目录运行隔离 build-info 与 smoke-test-designer；参数验证失败或任一子进程非零立即停止。保留旧发布结果直至候选目录和归档均通过。
- [x] 运行真实 Release 打包流程，提供绿色目录、zip、manifest、构建标识和验证记录。未提交源码如实标记 dirty，不擅自提交；不声称已完成物理出纸或无 Qt 新机器验收。
- [x] 最后运行完整 Debug/Release 回归、JS 与差异检查，核对产物和当前工作区；只有此任务全部验收后，才报告四阶段完成。

## 评审与执行交接

计划自检关注规格覆盖、接口名称一致、五项评审重点对应测试及阶段门。任务 1–3、4–5、6–7、8–9 分别构成四阶段，不能跨阶段并行修改共享接口。

建议在当前对话由同一执行者依次完成，阶段内先保持接口一致，全部结束后增加一次独立整体复核。也可由用户选择逐任务实现与逐任务评审的子代理方式。用户审阅本计划并选择方式之前，不进入业务实现。
