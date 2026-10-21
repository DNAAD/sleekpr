---
name: verification
description: 在验证原生 Qt 客户端改动、排查构建或启动问题、检查标签界面，或准备交付和 Git 提交时使用。
---

# 验证与交付

## 环境前提

命令在仓库根目录运行。先读 [Windows 构建知识](../../wiki/windows-build.md)，确认 MSVC x64 开发者环境、CMake/CTest、Node.js 已可用，`QTDIR` 指向与当前构建匹配的 Qt 根目录。不要把下面命令直接用于尚未初始化的普通 PowerShell。

示例对应当前 `build` 中的单配置 Ninja/Debug 构建，不是正式打包流程。若使用多配置生成器，先核对构建缓存，补齐所选配置参数和测试程序路径，不能直接沿用示例的 exe 路径。

普通 PowerShell 可在同一次 `cmd /c` 中调用已发现的 `VsDevCmd.bat`、构建和 CTest；环境发现复用 `quick-start.ps1` 的逻辑。不能在子进程初始化后假定父 PowerShell 已获得开发者环境。

## 完整验证

每个原生命令后检查退出码。Windows PowerShell 的 `$ErrorActionPreference` 不会自动把所有原生命令非零退出当成终止错误；不能用最后一次成功覆盖前面的失败。

```powershell
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($env:QTDIR)) { throw '请先设置当前 Qt 根目录 QTDIR' }
$qtBin = Join-Path $env:QTDIR 'bin'
if (-not (Test-Path -LiteralPath (Join-Path $qtBin 'Qt6Core.dll'))) { throw 'QTDIR 不是可用的 Qt 根目录' }
$env:PATH = $qtBin + ';' + $env:PATH

cmake --build build --parallel 4
if ($LASTEXITCODE -ne 0) { throw '构建失败，停止验证' }
ctest --test-dir build --output-on-failure --parallel 1 --output-junit (Join-Path $PWD 'build/debug-ctest.xml')
if ($LASTEXITCODE -ne 0) { throw 'CTest 失败，停止验证' }
node --test tests/js/sleekpr-print-client.test.mjs
if ($LASTEXITCODE -ne 0) { throw '客户端脚本测试失败，停止交付' }
git diff --check
if ($LASTEXITCODE -ne 0) { throw '工作区差异检查失败' }
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw '暂存内容检查失败' }
```

按 `ctest --test-dir build -N` 核对当前测试集合，不固定要求某个历史数量。完整构建不能替换成仅构建单个测试目标；CTest 已注册实际客户端启动冒烟和条件性的 MSVC/Ninja 依赖检查，不能只跑重新编译的 UI 测试就宣称启动安全。

记录实际测试总数、失败与跳过；退出码零不能用来掩盖跳过项。JUnit 汇总便于核对数量，但 Qt GUI 失败可能没有完整控制台断言，仍需下面的逐用例日志。

Qt GUI 测试必须串行，包括另行启动的测试程序；不要同时运行多个 GUI 套件。测试仅使用临时配置和模拟打印。链接遇到程序占用时按知识页处理，不擅自强杀或真实打印。

## 单项回归与截图

确认构建已更新，并在同一验证环境中运行；真实程序不支持把普通启动当成隔离测试。

```powershell
$env:SLEEKPR_BINDINGS_SCREENSHOT = Join-Path (Get-Location).Path 'build\binding-review.png'
.\build\sleekpr_label_app_tests.exe unifiedReferencesFollowContentWithoutMovingCursor -o build/binding-review.log,txt
$result = $LASTEXITCODE
Get-Content -Encoding UTF8 build/binding-review.log
if ($result -ne 0) { throw '绑定回归失败' }
```

截图变量必须指向 `.png` 文件，并实际查看生成图片，核对紧凑窗口、系统缩放下的文本、列宽、焦点和布局；文件存在不等于视觉验收通过。绑定边界可单独运行 `arrayReferenceRefreshKeepsTemplateCursor`、`partialKeyCollisionDoesNotRemoveEditor`、`clearedLegacyQrStillAcceptsBindingKeyChange`。单项通过不能替代完整回归。

## 慢工作池与逐用例日志

对当前已配置的单配置 Release 缓存执行下面命令；先确认模式及 Qt `bin` 在 PATH。其他生成器需按缓存调整配置和 EXE 路径。`-v1 -o` 保存用例进入点和断言，原生命令结束后先保存退出码再读日志；失败不靠反复重试或增加固定睡眠时间掩盖。

```powershell
cmake --build build-release --parallel 4 --target sleekpr_print_preview_tests
if ($LASTEXITCODE -ne 0) { throw '预览测试构建失败' }
$previousDelay = [Environment]::GetEnvironmentVariable('SLEEKPR_TEST_PREVIEW_DELAY_MS', 'Process')
$log = Join-Path $PWD 'build-release/preview-slow-pool.log'
try {
    $env:SLEEKPR_TEST_PREVIEW_DELAY_MS = '300'
    & .\build-release\sleekpr_print_preview_tests.exe -v1 -o "$log,txt"
    $result = $LASTEXITCODE
    Get-Content -LiteralPath $log -Encoding UTF8
    if ($result -ne 0) { throw '慢工作池预览回归失败' }
} finally {
    if ($null -eq $previousDelay) {
        Remove-Item Env:SLEEKPR_TEST_PREVIEW_DELAY_MS -ErrorAction SilentlyContinue
    } else {
        $env:SLEEKPR_TEST_PREVIEW_DELAY_MS = $previousDelay
    }
}
```

该环境变量仅由测试夹具读取，当前延迟上限 1000ms，用于在代表性预览提交前占用两个工作槽，不是生产性能开关或耗时断言。用例仍等待当前图像就绪并验证像素/定位，不改成“多等 300ms 就通过”。普通故障可替换目标和用例名称，沿用 `-v1 -o` 日志方式；所有 GUI 套件继续串行。

## 后台保存与退出回归

涉及共享保存服务、控制屏障、窗口关闭、正常退出或取消退出时，使用 [后台保存与退出验证](references/background-save-exit.md)。覆盖可控慢写入、旧结果、撤销回原值、仅应用、非法原文、控制操作、已接受打印提交和真实 EXE 隔离冒烟；不得把普通生产启动当成验证。

## 正式打包与产物核对

正式交付先使用 [原生 Release 交付技能](../native-release-delivery/SKILL.md)。下面命令复用脚本发现 MSVC/CTest/部署工具的逻辑；`QTDIR` 已按环境前提设置。没有发布需求的文档改动不要执行此块。

```powershell
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\package-portable.ps1 -QtRoot $env:QTDIR
if ($LASTEXITCODE -ne 0) { throw 'Release 打包失败，不能交付正式包' }
```

默认流程完整执行 Release CTest，要求总数大于零、零失败、零跳过，再暂存部署、隔离验证身份和设计器、校验 ZIP 并发布；不传 `-NoTests` 或 `-StopRunning`。跳过测试的包是 `unverified` 候选，不覆盖正式目录。

按需使用 [产物只读核验命令](references/portable-artifacts.md)：安全读取包内 EXE 的真实标识、隔离冒烟、逐文件及 ZIP SHA-256。输入采用本次打包实际输出的验证记录路径，不硬编码旧版本号，不按文件时间猜“最新包”。这些检查不启动正常托盘/HTTP，也不执行真实打印；只核对现有包不能宣称重新打包。

## 提交与交付检查

- 只在用户明确要求时提交或推送，Harness 本身不授予权限。先检查分支、远程地址和所有已修改及未跟踪文件，确认范围，不撤销用户改动。
- 暂存源码、测试及相关文档，不暂存 exe、PDB、日志、截图或生成目录；检查实际暂存快照。未跟踪文件不受普通 `git diff --check` 覆盖，需要纳入检查。
- 提交备注使用中文，描述实际改动和已完成的验证，不宣称未运行的测试或真实设备结果。
- 推送前读取远程状态；遇到分叉或冲突先处理，不使用强制推送。成功后核对提交号、远程跟踪状态及工作区，不能仅凭本地提交成功宣称已推送。
- 明确更新的是 Debug 还是 Release。没有重新打包就不能宣称正式包已更新；本次只改 Harness 时进行文档、技能格式、引用及命令检查，不冒充重新执行了业务全量测试。
