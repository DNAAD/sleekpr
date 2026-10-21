# 绿色包只读核验命令

命令在仓库根目录执行，用于本次本机打包脚本的可信产物，不解压或替换目录。完整交付流程见 [Release 技能](../../native-release-delivery/SKILL.md)，环境与范围见 [验证技能](../SKILL.md)。

使用已获准加载仓库 PowerShell 模块的会话；执行策略拒绝时按 [Windows 环境说明](../../../wiki/windows-build.md) 处理，不修改永久策略。仅通过语法解析，不等于模块已能在该环境中实际执行。

## 隔离身份与设计器冒烟

```powershell
$ErrorActionPreference = 'Stop'
Import-Module .\packaging\ExecutableBuildInfo.psm1 -Force
$exe = Join-Path $PWD 'dist/sleekpr-release/sleekpr.exe'
$identity = Read-ExecutableBuildInfo -ExecutablePath $exe -Isolated
if ($null -eq $identity) { throw '缺少能力标记，不得对未知旧程序盲试参数' }
if ($identity.configuration -cne 'Release') { throw '目录内不是 Release 构建' }
$smoke = Invoke-ExecutableDiagnostic -ExecutablePath $exe -Argument '--smoke-test-designer' -Isolated $true -TimeoutMilliseconds 30000
if ($smoke.ExitCode -ne 0) { throw '隔离设计器启动验收失败' }
$identity | ConvertTo-Json -Depth 5
```

模块先检查 PE 能力标记，之后才允许版本命令；诊断不读取操作员设置、不启动 HTTP/托盘。隔离子进程使用包内前缀和本地 DLL，不能把普通双击启动当冒烟验证。

## 清单与 ZIP 摘要

先将 `$verificationPath` 设为本次打包输出的 `.zip.verification.json` 完整路径；不要按修改时间自动选文件。下面验证正式目录的构建身份、清单列出的文件及 ZIP 整体摘要，核对 ZIP 中的清单原文。它不替代打包脚本的目录树/ZIP 条目验收，SHA-256 也不是数字签名或不可信下载物的来源证明。

```powershell
$ErrorActionPreference = 'Stop'
if (-not (Get-Variable verificationPath -ErrorAction SilentlyContinue)) { throw '请先指定本次验证记录路径 verificationPath' }
Import-Module .\packaging\PortablePackage.psm1 -Force
Import-Module .\packaging\ExecutableBuildInfo.psm1 -Force
$repo = (Get-Location).Path
$packageDir = Assert-PortableOutputPath $repo (Join-Path $repo 'dist/sleekpr-release')
$recordPath = Assert-PortableOutputPath $repo $verificationPath
$manifestPath = Join-Path $packageDir 'manifest.json'
$manifestText = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8
$manifest = $manifestText | ConvertFrom-Json
$record = Get-Content -LiteralPath $recordPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1 -or $record.schemaVersion -ne 1 -or $record.status -ne 'verified') { throw '记录结构或正式状态不符' }
foreach ($validation in @($manifest.validation, $record.validation)) {
    if ($validation.status -ne 'verified' -or $validation.tests.status -ne 'passed' -or $validation.tests.total -le 0 -or $validation.tests.failures -ne 0 -or $validation.tests.skipped -ne 0) { throw '缺少完整且零失败零跳过的验收记录' }
}
$actual = Read-ExecutableBuildInfo -ExecutablePath (Join-Path $packageDir 'sleekpr.exe') -Isolated
if ($null -eq $actual) { throw '无法安全读取包内构建身份' }
foreach ($name in @('schemaVersion','version','revision','configuration','sourceDirty','workspaceDirty','sourceDigest')) {
    foreach ($info in @($manifest.buildInfo, $record.buildInfo)) {
        if ($null -eq $info.PSObject.Properties[$name] -or $actual.$name -cne $info.$name) { throw "构建身份不一致：$name" }
    }
}
if ($actual.configuration -cne 'Release' -or @($manifest.files).Count -eq 0) { throw '不是完整的正式构建清单' }
foreach ($file in $manifest.files) {
    $path = Assert-PortableOutputPath $repo (Join-Path $packageDir $file.path)
    if (-not $path.StartsWith($packageDir + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw '清单文件越出包目录' }
    if ((Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-PortableFileSha256 $path) -ne $file.sha256) { throw "文件摘要不一致：$($file.path)" }
}
if ([string]::IsNullOrWhiteSpace($record.archive) -or [IO.Path]::GetFileName($record.archive) -cne $record.archive) { throw '归档名不是单一文件名' }
$archive = Assert-PortableOutputPath $repo (Join-Path (Split-Path -Parent $recordPath) $record.archive)
if ((Get-PortableFileSha256 $archive) -ne $record.archiveSha256) { throw 'ZIP 摘要不一致' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entry = $zip.GetEntry('sleekpr-release/manifest.json')
    if ($null -eq $entry) { throw 'ZIP 缺少正式包清单' }
    $reader = [IO.StreamReader]::new($entry.Open(), [Text.Encoding]::UTF8)
    try {
        if ($reader.ReadToEnd() -cne $manifestText) { throw 'ZIP 清单与正式目录不同' }
    } finally { $reader.Dispose() }
} finally { $zip.Dispose() }
'构建身份、清单文件与 ZIP 摘要核对通过'
```

上述命令不修改或恢复操作员配置；恢复旧版本的目录替换需要另行明确目标和授权，不能用设置备份恢复代替。
