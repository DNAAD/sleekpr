param(
    [ValidateSet("Release", "Debug")]
    [string]$Mode = "Release",

    [switch]$Rebuild,

    [switch]$NoLaunch,

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

    if (-not [string]::IsNullOrWhiteSpace($env:Qt6_DIR)) {
        $qt6Dir = $env:Qt6_DIR
        if ($qt6Dir.EndsWith("lib\cmake\Qt6", [System.StringComparison]::OrdinalIgnoreCase)) {
            $candidates.Add((Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qt6Dir))))
        }
    }

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

    throw "未找到 Qt msvc2022_64。可通过 -QtRoot 指定 Qt 根目录，例如：.\quick-start.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64"
}

function Invoke-DevCommand {
    param(
        [string]$VsDevCmd,
        [string]$Command
    )

    # 通过 VsDevCmd 注入 MSVC、Windows SDK 和 Ninja/CMake 环境，避免用户手动打开开发者命令行。
    $fullCommand = '"' + $VsDevCmd + '" -arch=x64 -host_arch=x64 && ' + $Command
    & $env:ComSpec /d /s /c $fullCommand
    if ($LASTEXITCODE -ne 0) {
        throw "命令执行失败，退出码：$LASTEXITCODE"
    }
}

function Quote-CmdArg {
    param([string]$Value)

    return '"' + $Value + '"'
}

$repoRoot = $PSScriptRoot
$vsDevCmd = Find-VsDevCmd
$resolvedQtRoot = Find-QtRoot -ExplicitQtRoot $QtRoot

Write-Host "项目目录：$repoRoot"
Write-Host "Qt 目录：$resolvedQtRoot"
Write-Host "启动模式：$Mode"

if ($Mode -eq "Release") {
    $exePath = Join-Path $repoRoot "dist\sleekpr-release\sleekpr.exe"
    if ($Rebuild -or -not (Test-Path -LiteralPath $exePath)) {
        Write-Host "正在构建并打包 Release 绿色包..."
        $buildDir = Join-Path $repoRoot "build"
        $configureCommand = "cmake -S " + (Quote-CmdArg $repoRoot) + " -B " + (Quote-CmdArg $buildDir) + " -DCMAKE_PREFIX_PATH=" + (Quote-CmdArg $resolvedQtRoot)
        $packageCommand = "cmake --build " + (Quote-CmdArg $buildDir) + " --target package_sleekpr_release"
        Invoke-DevCommand -VsDevCmd $vsDevCmd -Command ($configureCommand + " && " + $packageCommand)
    }
} else {
    $exePath = Join-Path $repoRoot "build\sleekpr.exe"
    if ($Rebuild -or -not (Test-Path -LiteralPath $exePath)) {
        Write-Host "正在构建 Debug 并部署 Qt 运行时..."
        $buildDir = Join-Path $repoRoot "build"
        $configureCommand = "cmake -S " + (Quote-CmdArg $repoRoot) + " -B " + (Quote-CmdArg $buildDir) + " -DCMAKE_PREFIX_PATH=" + (Quote-CmdArg $resolvedQtRoot)
        $deployCommand = "cmake --build " + (Quote-CmdArg $buildDir) + " --target deploy_sleekpr"
        Invoke-DevCommand -VsDevCmd $vsDevCmd -Command ($configureCommand + " && " + $deployCommand)
    }
}

if (-not (Test-Path -LiteralPath $exePath)) {
    throw "未找到可启动程序：$exePath"
}

if ($NoLaunch) {
    Write-Host "已验证可启动程序存在：$exePath"
    exit 0
}

Write-Host "正在启动：$exePath"
Start-Process -FilePath $exePath -WorkingDirectory (Split-Path -Parent $exePath)

