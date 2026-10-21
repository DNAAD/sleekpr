param(
    [switch]$NoTests,

    [switch]$StopRunning,

    [string]$QtRoot
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot 'packaging\ExecutableBuildInfo.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'packaging\PortablePackage.psm1') -Force

function Resolve-ExistingPath {
    param(
        [string[]]$Candidates,
        [string]$ErrorMessage
    )

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw $ErrorMessage
}

function Find-VsDevCmd {
    $candidates = New-Object System.Collections.Generic.List[string]
    $programFilesX86 = ${env:ProgramFiles(x86)}

    if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
        $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vswhere) {
            $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if (-not [string]::IsNullOrWhiteSpace($installPath)) {
                $candidates.Add((Join-Path $installPath "Common7\Tools\VsDevCmd.bat"))
            }
        }

        $candidates.Add((Join-Path $programFilesX86 "Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"))
        $candidates.Add((Join-Path $programFilesX86 "Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"))
        $candidates.Add((Join-Path $programFilesX86 "Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"))
        $candidates.Add((Join-Path $programFilesX86 "Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"))
    }

    return Resolve-ExistingPath `
        -Candidates $candidates `
        -ErrorMessage "未找到 VS Build Tools。请安装 C++ 构建工具，或确认 VsDevCmd.bat 可被 vswhere 发现。"
}

function Find-QtRoot {
    param([string]$ExplicitQtRoot)

    $candidates = New-Object System.Collections.Generic.List[string]

    if (-not [string]::IsNullOrWhiteSpace($ExplicitQtRoot)) {
        $candidates.Add($ExplicitQtRoot)
    }

    if (-not [string]::IsNullOrWhiteSpace($env:QTDIR)) {
        $candidates.Add($env:QTDIR)
    }

    $knownQt = Join-Path $env:SystemDrive "Qt\6.8.3\msvc2022_64"
    $candidates.Add($knownQt)

    $qtBase = Join-Path $env:SystemDrive "Qt"
    if (Test-Path -LiteralPath $qtBase) {
        Get-ChildItem -LiteralPath $qtBase -Directory |
            Sort-Object Name -Descending |
            ForEach-Object {
                $candidates.Add((Join-Path $_.FullName "msvc2022_64"))
            }
    }

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $qtConfig = Join-Path $candidate "lib\cmake\Qt6\Qt6Config.cmake"
        $deployTool = Join-Path $candidate "bin\windeployqt.exe"
        if ((Test-Path -LiteralPath $qtConfig) -and (Test-Path -LiteralPath $deployTool)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "未找到 Qt msvc2022_64。可通过 -QtRoot 指定，例如：.\package-portable.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64"
}

function Quote-CmdArg {
    param([string]$Value)

    return '"' + $Value + '"'
}

function Invoke-DevCommand {
    param(
        [string]$VsDevCmd,
        [string]$Command
    )

    # 绿色包必须在 MSVC 开发环境中构建，否则 Release 构建和 windeployqt 可能找不到依赖。
    $fullCommand = '"' + $VsDevCmd + '" -arch=x64 -host_arch=x64 && ' + $Command
    & $env:ComSpec /d /s /c $fullCommand | Where-Object { $_ -notmatch 'Note: including file:|包含文件' } | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "命令执行失败，退出码：$LASTEXITCODE"
    }
}

function Stop-SleekprProcess {
    param([string]$RepoRoot)

    $expectedPaths = @(
        (Join-Path $RepoRoot "build\sleekpr.exe"),
        (Join-Path $RepoRoot "build-release\sleekpr.exe"),
        (Join-Path $RepoRoot "dist\sleekpr-release\sleekpr.exe")
    )

    Get-Process -Name "sleekpr" -ErrorAction SilentlyContinue | ForEach-Object {
        $processPath = $_.Path
        if ($expectedPaths -contains $processPath) {
            Write-Host "关闭正在运行的进程：$processPath"
            Stop-Process -Id $_.Id -Force
        }
    }
}

$repoRoot = $PSScriptRoot
$releaseBuildDir = Join-Path $repoRoot "build-release"

$vsDevCmd = Find-VsDevCmd
$resolvedQtRoot = Find-QtRoot -ExplicitQtRoot $QtRoot
$ctestPath = Join-Path (Split-Path -Parent (Split-Path -Parent $vsDevCmd)) "IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if (-not (Test-Path -LiteralPath $ctestPath)) {
    $ctestPath = "ctest"
}

Write-Host "项目目录：$repoRoot"
Write-Host "Qt 目录：$resolvedQtRoot"

if ($StopRunning) {
    Stop-SleekprProcess -RepoRoot $repoRoot
}

$invoker = {
    param($stage, $context)
    Write-Host ('绿色包阶段：' + $stage)
    switch ($stage) {
        'build' {
            $configure = 'cmake -S ' + (Quote-CmdArg $repoRoot) + ' -B ' + (Quote-CmdArg $context.BuildDir) +
                ' -G Ninja -DCMAKE_BUILD_TYPE=Release -DSLEEKPR_ENABLE_RELEASE_ORCHESTRATOR=OFF -DSLEEKPR_ENABLE_TABLE_DESIGNER=OFF -DCMAKE_PREFIX_PATH=' + (Quote-CmdArg $resolvedQtRoot)
            Invoke-DevCommand -VsDevCmd $vsDevCmd -Command ($configure + ' && cmake --build ' + (Quote-CmdArg $context.BuildDir) + ' --parallel 4')
            return [pscustomobject]@{ ExitCode = 0 }
        }
        'test' {
            $reportPath = Join-Path $context.BuildDir 'package-ctest.xml'
            $command = 'set PATH=' + $resolvedQtRoot + '\bin;%PATH% && ' + (Quote-CmdArg $ctestPath) + ' --test-dir ' +
                (Quote-CmdArg $context.BuildDir) + ' --output-on-failure --parallel 1 --output-junit ' + (Quote-CmdArg $reportPath)
            Invoke-DevCommand -VsDevCmd $vsDevCmd -Command $command
            [xml]$report = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8
            $total = 0; $failures = 0; $skipped = 0
            foreach ($suite in $report.SelectNodes('//testsuite')) {
                $total += [int]$suite.GetAttribute('tests')
                $failures += [int]$suite.GetAttribute('failures')
                if ($suite.HasAttribute('errors')) { $failures += [int]$suite.GetAttribute('errors') }
                if ($suite.HasAttribute('skipped')) { $skipped += [int]$suite.GetAttribute('skipped') }
            }
            return [pscustomobject]@{ ExitCode = 0; TestsTotal = $total; Failures = $failures; Skipped = $skipped }
        }
        'deploy' {
            $exe = Join-Path $context.PayloadDir 'sleekpr.exe'
            Copy-Item -LiteralPath $context.BuildExe -Destination $exe
            $tool = Join-Path $resolvedQtRoot 'bin\windeployqt.exe'
            Invoke-DevCommand -VsDevCmd $vsDevCmd -Command ((Quote-CmdArg $tool) + ' --release --no-translations --compiler-runtime --dir ' + (Quote-CmdArg $context.PayloadDir) + ' ' + (Quote-CmdArg $exe))
            Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination (Join-Path $context.PayloadDir 'LICENSE')
            Copy-Item -LiteralPath (Join-Path $repoRoot 'packaging\README-PORTABLE.txt') -Destination (Join-Path $context.PayloadDir 'README-PORTABLE.txt')
            # 绿色目录优先带本地 CRT，避免仅依赖开发机已安装的 VS 运行时。
            $install = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $vsDevCmd))
            $redist = Join-Path $install 'VC\Redist\MSVC'
            $version = Get-ChildItem -LiteralPath $redist -Directory | Where-Object { $_.Name -match '^\d+(\.\d+)+$' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
            if ($null -eq $version) { throw '找不到可部署的 MSVC CRT' }
            $crt = Join-Path $version.FullName 'x64\Microsoft.VC143.CRT'
            if (-not (Test-Path -LiteralPath $crt -PathType Container)) { throw '缺少 x64 MSVC CRT 目录' }
            foreach ($dll in Get-ChildItem -LiteralPath $crt -Filter '*.dll' -File) { Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $context.PayloadDir $dll.Name) }
            return [pscustomobject]@{ ExitCode = 0 }
        }
        'identity' {
            $identity = Read-ExecutableBuildInfo -ExecutablePath (Join-Path $context.PayloadDir 'sleekpr.exe') -Isolated
            if ($null -eq $identity) { throw '暂存程序不支持安全构建诊断' }
            return [pscustomobject]@{ ExitCode = 0; BuildInfo = $identity }
        }
        'smoke' {
            return Invoke-ExecutableDiagnostic -ExecutablePath (Join-Path $context.PayloadDir 'sleekpr.exe') -Argument '--smoke-test-designer' -Isolated $true -TimeoutMilliseconds 30000
        }
        default { throw ('未知打包步骤：' + $stage) }
    }
}.GetNewClosure()

$result = Invoke-PortablePackage -RepoRoot $repoRoot -CommandInvoker $invoker -NoTests:$NoTests
Write-Host ('构建标识：' + ($result.BuildInfo | ConvertTo-Json -Compress))
if ($result.Status -eq 'verified') { Write-Host 'Release 本机自动验收通过，正式绿色包已生成。' }
else { Write-Warning '只生成未验收候选包，没有覆盖正式目录。' }
Write-Host ('绿色目录：' + $result.PackageDirectory)
Write-Host ('归档文件：' + $result.ArchivePath)
Write-Host ('包内清单：' + $result.ManifestPath)
Write-Host ('验证记录：' + $result.VerificationPath)
