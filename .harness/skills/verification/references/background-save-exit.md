# 后台保存与退出验证

先按 [验证技能](../SKILL.md) 准备同一 MSVC x64/Qt 环境，核对构建缓存。不要强杀用户客户端、发送真实打印或修改永久系统策略。

## 定向回归

以下命令适用于当前单配置 Debug 缓存。每条原生命令后立即检查退出码；Qt GUI 套件串行，记录 `-v1 -o` 逐用例日志，不用固定睡眠替代状态等待。

```powershell
cmake --build build --parallel 4
if ($LASTEXITCODE -ne 0) { throw '构建失败' }
ctest --test-dir build --output-on-failure --parallel 1 -R 'sleekpr_(settings_(save_baseline|save_session|persistence|repository|change_set)_tests|application_exit_tests|safe_exit_launch_test|designer_launch_test)$'
if ($LASTEXITCODE -ne 0) { throw '后台保存或隔离冒烟失败' }
.\build\sleekpr_label_app_tests.exe designerOldSaveDoesNotReplaceNewDraft designerUndoWhileSavingPersistsOriginal settingsLateSaveDoesNotRollbackNewRuntimeApply settingsRevertDuringSaveIsNotTreatedAsUntouched settingsFailedTicketCannotDisplaceLaterRunningContext exitPauseDuringInitialLoadResumesUsableWindows resumePreservesExplicitDisabledChild -v1 -o build/save-exit-regressions.log,txt
$result = $LASTEXITCODE
Get-Content -LiteralPath build/save-exit-regressions.log -Encoding UTF8
if ($result -ne 0) { throw '窗口保存与退出回归失败' }
.\build\sleekpr_workbench_tests.exe exitPauseKeepsAcceptedPrintSubmission exitPauseResumesPendingPreparation exitPauseResumesCopyCheckWithoutRerender -v1 -o build/accepted-print-exit.log,txt
$result = $LASTEXITCODE
Get-Content -LiteralPath build/accepted-print-exit.log -Encoding UTF8
if ($result -ne 0) { throw '已接受打印提交收束失败' }
```

写入屏障使用测试 AtomicWriter 与临时主文件；释放守卫早于服务析构。QTRY 的条件不能调用消耗型 `tryAcquire()`，应观察 `available()`，否则一次成功会被重复求值消耗，导致夹具假失败。

`--smoke-test-safe-exit` 使用临时配置和真实应用对象验证保存、取消、非法原文阻断及最终退出；没有 HTTP、托盘或真实引擎。`--smoke-test-designer` 仍检查实际 EXE 的工作台/设置入口，并等待异步加载、窗口待应用编辑及正式预览，不假设在固定 100ms 内完成。CTest 的两个真实 EXE 冒烟强制 Qt stderr/控制台日志，失败时检查阶段、存储空闲和未完成状态，不通过放宽超时掩盖问题。

只能对本次已编译并确认支持参数的测试目标执行上述命令。旧 EXE 可能忽略未知参数而启动生产服务，不以参数试探旧包。Debug 构建不会更新 `dist` 正式 Release 包。

## 性能与完整门槛

相同 Release 模式、相同固定样本比较键盘连续输入和画布拖拽。当前采样为 31 个自适应文本和 1 个可拖矩形，20 次键盘输入、10 次超过系统拖动阈值的交替拖动，每次断言位置确实变化。不能用阈值以下的鼠标移动代替真实拖拽，也不能与不同样本的历史数字直接比较。`saveEnqueue` 是 GUI 入队链的耗时；`saving` 保留仓库锁、读、备份、原子提交的耗时，不是端到端延迟，也不能混为同一次 GUI 阻塞。差量计算在后台执行，不为扩大计时范围重复采样。

```powershell
$previous = $env:SLEEKPR_DISPATCH_METRICS
try {
    $env:SLEEKPR_DISPATCH_METRICS = Join-Path $PWD 'build-release/save-dispatch-metrics.json'
    .\build-release\sleekpr_label_app_tests.exe designerEditDispatchMetrics -v1 -o build-release/save-dispatch.log,txt
    $result = $LASTEXITCODE
    Get-Content -LiteralPath build-release/save-dispatch.log -Encoding UTF8
    if ($result -ne 0) { throw 'Release 编辑采样失败' }
    Get-Content -LiteralPath $env:SLEEKPR_DISPATCH_METRICS -Encoding UTF8
} finally {
    if ($null -eq $previous) { Remove-Item Env:SLEEKPR_DISPATCH_METRICS -ErrorAction SilentlyContinue }
    else { $env:SLEEKPR_DISPATCH_METRICS = $previous }
}
```

定向通过后仍需完整 Debug/Release CTest、JS、已有预览慢工作池及源码/文档差异检查。记录当前实际套件数量、失败和跳过；显式性能采样默认跳过，需设置采样路径后实际执行，不能把跳过写成通过。截图需实际查看紧凑窗口及系统缩放下的退出按钮、长错误文本和状态布局。

MSVC 在受限环境出现 C1902 时先核对 cl/mspdbsrv 与缓存，确认是否环境/服务受限；需要沙箱外验证时走显式审批，不换调试格式掩盖问题、不终止用户进程。
