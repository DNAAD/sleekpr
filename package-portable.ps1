param(
    [switch]$NoTests,

    [switch]$StopRunning,

    [string]$QtRoot
)

$ErrorActionPreference = "Stop"

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
    & $env:ComSpec /d /s /c $fullCommand
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
$buildDir = Join-Path $repoRoot "build"
$releaseBuildDir = Join-Path $repoRoot "build-release"
$distDir = Join-Path $repoRoot "dist\sleekpr-release"
$archivePath = Join-Path $repoRoot "dist\sleekpr-0.1.0-windows-x64.zip"
$releaseExe = Join-Path $distDir "sleekpr.exe"

$vsDevCmd = Find-VsDevCmd
$resolvedQtRoot = Find-QtRoot -ExplicitQtRoot $QtRoot
$ctestPath = Join-Path (Split-Path -Parent (Split-Path -Parent $vsDevCmd)) "IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if (-not (Test-Path -LiteralPath $ctestPath)) {
    $ctestPath = "ctest"
}

Write-Host "项目目录：$repoRoot"
Write-Host "Qt 目录：$resolvedQtRoot"
Write-Host "输出目录：$distDir"
Write-Host "绿色包：$archivePath"

if ($StopRunning) {
    Stop-SleekprProcess -RepoRoot $repoRoot
}

$configureCommand = "cmake -S " + (Quote-CmdArg $repoRoot) + " -B " + (Quote-CmdArg $buildDir) + " -DCMAKE_PREFIX_PATH=" + (Quote-CmdArg $resolvedQtRoot)
$packageCommand = "cmake --build " + (Quote-CmdArg $buildDir) + " --target package_sleekpr_release"
$packageWorkflowCommand = @($configureCommand, $packageCommand) -join " && "

Invoke-DevCommand -VsDevCmd $vsDevCmd -Command $packageWorkflowCommand

if (-not $NoTests) {
    $testCommand = 'set PATH=' + $resolvedQtRoot + '\bin;%PATH% && ' + (Quote-CmdArg $ctestPath) + ' --test-dir ' + (Quote-CmdArg $releaseBuildDir) + ' --output-on-failure'
    Invoke-DevCommand -VsDevCmd $vsDevCmd -Command $testCommand
}

if (-not (Test-Path -LiteralPath $releaseExe)) {
    throw "绿色包目录中缺少 sleekpr.exe：$releaseExe"
}

if (-not (Test-Path -LiteralPath $archivePath)) {
    throw "绿色包 zip 未生成：$archivePath"
}

Write-Host ""
Write-Host "绿色免安装包已生成：$archivePath"
Write-Host "解压目录已同步生成：$distDir"
Write-Host "生产机器解压后双击 sleekpr.exe 即可运行。"
