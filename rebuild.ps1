param(
    [ValidateSet("Debug", "Release")]
    [string]$Mode = "Debug",

    [switch]$NoTests,

    [switch]$NoDeploy,

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

    throw "未找到 Qt msvc2022_64。可通过 -QtRoot 指定，例如：.\rebuild.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64"
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

    # 统一从 VsDevCmd 进入 MSVC 环境，避免普通 PowerShell 找不到 cl、ninja 或 Windows SDK。
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
$vsDevCmd = Find-VsDevCmd
$resolvedQtRoot = Find-QtRoot -ExplicitQtRoot $QtRoot
$ctestPath = Join-Path (Split-Path -Parent (Split-Path -Parent $vsDevCmd)) "IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if (-not (Test-Path -LiteralPath $ctestPath)) {
    $ctestPath = "ctest"
}

Write-Host "项目目录：$repoRoot"
Write-Host "Qt 目录：$resolvedQtRoot"
Write-Host "构建模式：$Mode"

if ($StopRunning) {
    Stop-SleekprProcess -RepoRoot $repoRoot
}

$configureCommand = "cmake -S " + (Quote-CmdArg $repoRoot) + " -B " + (Quote-CmdArg $buildDir) + " -DCMAKE_PREFIX_PATH=" + (Quote-CmdArg $resolvedQtRoot)

if ($Mode -eq "Release") {
    $buildCommand = "cmake --build " + (Quote-CmdArg $buildDir) + " --target package_sleekpr_release --clean-first"
} elseif ($NoDeploy) {
    $buildCommand = "cmake --build " + (Quote-CmdArg $buildDir) + " --clean-first"
} else {
    $buildCommand = "cmake --build " + (Quote-CmdArg $buildDir) + " --target deploy_sleekpr --clean-first"
}

$commands = New-Object System.Collections.Generic.List[string]
$commands.Add($configureCommand)
$commands.Add($buildCommand)

if (-not $NoTests) {
    $testCommand = 'set PATH=' + $resolvedQtRoot + '\bin;%PATH% && ' + (Quote-CmdArg $ctestPath) + ' --test-dir ' + (Quote-CmdArg $buildDir) + ' --output-on-failure'
    $commands.Add($testCommand)
}

Invoke-DevCommand -VsDevCmd $vsDevCmd -Command ($commands -join ' && ')

if ($Mode -eq "Release") {
    Write-Host "Release 包已重新构建：$(Join-Path $repoRoot 'dist\sleekpr-0.1.0-windows-x64.zip')"
    exit 0
}

if ($NoDeploy) {
    Write-Host "Debug 构建已完成。"
    exit 0
}

Write-Host "Debug 构建和部署已完成：$(Join-Path $repoRoot 'build\sleekpr.exe')"

