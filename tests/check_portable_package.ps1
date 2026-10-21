param([string]$RepoRoot, [string]$TestRoot)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $RepoRoot 'packaging\PortablePackage.psm1') -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Join-Path $TestRoot ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$suite = [pscustomobject]@{ Failures = 0 }
function Assert-True { param([bool]$Condition, [string]$Message) if (-not $Condition) { throw $Message } }
function Assert-Throws {
    param([scriptblock]$Action)
    $thrown = $false
    try { & $Action | Out-Null } catch { $thrown = $true }
    Assert-True $thrown '应在发布前拒绝此操作'
}
function Check-Case {
    param([string]$Name, [scriptblock]$Action)
    try { & $Action; Write-Host ('PASS ' + $Name) }
    catch { $suite.Failures++; Write-Host ('FAIL ' + $Name + ': ' + $_.Exception.Message) }
}
function New-Fixture {
    $directory = Join-Path $root ([Guid]::NewGuid().ToString('N'))
    $repo = Join-Path $directory 'repo'
    $old = Join-Path $repo 'dist\sleekpr-release'
    New-Item -ItemType Directory -Path $old -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $old 'old-release.txt'), 'old')
    return [pscustomobject]@{ Repo = $repo; Directory = $directory; Stages = (New-Object 'System.Collections.Generic.List[string]'); FailTests = $false; Skipped = 0; FailSmoke = $false; Debug = $false }
}
function New-Invoker {
    param($fixture)
    return {
        param($stage, $context)
        $fixture.Stages.Add($stage)
        switch ($stage) {
            'build' {
                New-Item -ItemType Directory -Path $context.BuildDir -Force | Out-Null
                [IO.File]::WriteAllText($context.BuildExe, 'fixture product binary')
                return [pscustomobject]@{ ExitCode = 0 }
            }
            'test' {
                return [pscustomobject]@{ ExitCode = $(if ($fixture.FailTests) { 1 } else { 0 }); TestsTotal = 3; Failures = $(if ($fixture.FailTests) { 1 } else { 0 }); Skipped = $fixture.Skipped }
            }
            'deploy' {
                Copy-Item -LiteralPath $context.BuildExe -Destination (Join-Path $context.PayloadDir 'sleekpr.exe')
                [IO.File]::WriteAllText((Join-Path $context.PayloadDir 'Qt6Core.dll'), 'runtime')
                [IO.File]::WriteAllText((Join-Path $context.PayloadDir 'debug.pdb'), '不得入包')
                [IO.File]::WriteAllText((Join-Path $context.PayloadDir 'test.log'), '不得入包')
                [IO.File]::WriteAllText((Join-Path $context.PayloadDir 'screenshot-preview.png'), '不得入包')
                return [pscustomobject]@{ ExitCode = 0 }
            }
            'identity' {
                return [pscustomobject]@{ ExitCode = 0; BuildInfo = [pscustomobject]@{ schemaVersion = 1; version = '1.2.3'; configuration = $(if ($fixture.Debug) { 'Debug' } else { 'Release' }); revision = ('a' * 40); sourceDirty = $true; workspaceDirty = $true; sourceDigest = ('b' * 64) } }
            }
            'smoke' { return [pscustomobject]@{ ExitCode = $(if ($fixture.FailSmoke) { 1 } else { 0 }) } }
            default { throw ('未知夹具步骤：' + $stage) }
        }
    }.GetNewClosure()
}

Check-Case 'rejectsEscapingOrReparseOutputPath' {
    $fixture = New-Fixture
    Assert-Throws { Assert-PortableOutputPath -RepoRoot $fixture.Repo -Path (Join-Path $fixture.Repo '..\outside') }
    $target = Join-Path $fixture.Directory 'target'
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    $link = Join-Path $fixture.Repo 'dist\link'
    New-Item -ItemType Junction -Path $link -Target $target | Out-Null
    Assert-Throws { Assert-PortableOutputPath -RepoRoot $fixture.Repo -Path (Join-Path $link 'output') }
}
Check-Case 'failedReleaseTestsDoNotPublish' {
    $fixture = New-Fixture
    $fixture.FailTests = $true
    Assert-Throws { Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture) }
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture.Repo 'dist\sleekpr-release\old-release.txt')) '失败覆盖了旧发布目录'
    Assert-True (-not $fixture.Stages.Contains('deploy')) '测试失败后仍部署了候选包'
}
Check-Case 'manifestMatchesExecutable' {
    $fixture = New-Fixture
    $result = Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture)
    Assert-True ($result.Status -eq 'verified') '未生成已验收结果'
    $manifest = Get-Content -LiteralPath $result.ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-True ($manifest.buildInfo.version -eq '1.2.3' -and $manifest.buildInfo.sourceDirty) '清单未采用真实构建标识'
    $exe = @($manifest.files | Where-Object { $_.path -eq 'sleekpr.exe' })
    Assert-True ($exe.Count -eq 1) '清单缺少产品 exe'
    $hash = Get-PortableFileSha256 (Join-Path $result.PackageDirectory 'sleekpr.exe')
    Assert-True ($exe[0].sha256 -eq $hash) '清单摘要与 exe 不同'
    Assert-True (@($manifest.files | Where-Object { $_.path -eq 'manifest.json' }).Count -eq 0) '清单不能计算自身摘要'
    $verification = Get-Content -LiteralPath $result.VerificationPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-True ($verification.archiveSha256 -eq (Get-PortableFileSha256 $result.ArchivePath)) '包外归档摘要无效'
}
Check-Case 'skippedReleaseTestsDoNotPublish' {
    $fixture = New-Fixture
    $fixture.Skipped = 1
    Assert-Throws { Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture) }
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture.Repo 'dist\sleekpr-release\old-release.txt')) '跳过测试覆盖了旧正式包'
    Assert-True (-not $fixture.Stages.Contains('deploy')) '有跳过项仍进入正式部署'
}
Check-Case 'noTestsIsMarkedUnverified' {
    $fixture = New-Fixture
    $result = Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture) -NoTests
    Assert-True ($result.Status -eq 'unverified') '跳过测试不能报告正式验收通过'
    Assert-True (-not $fixture.Stages.Contains('test')) '未遵守跳过测试参数'
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture.Repo 'dist\sleekpr-release\old-release.txt')) '候选包不能覆盖正式目录'
    $manifest = Get-Content -LiteralPath $result.ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-True ($manifest.validation.status -eq 'unverified') '清单缺少未验收状态'
    Assert-True ($result.ArchivePath -match 'unverified') '归档名未标明候选包'
}
Check-Case 'archiveContainsNoDebugArtifacts' {
    $fixture = New-Fixture
    $result = Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture)
    Assert-True ($result.Status -eq 'verified') '未生成已验收结果'
    $zip = [IO.Compression.ZipFile]::OpenRead($result.ArchivePath)
    try {
        $names = @($zip.Entries | ForEach-Object { $_.FullName })
        Assert-True (@($names | Where-Object { $_ -match '\.(pdb|log)$|screenshot' }).Count -eq 0) '归档包含调试产物'
        Assert-True (@($names | Where-Object { $_ -eq 'sleekpr-release/manifest.json' }).Count -eq 1) '归档缺少清单'
    } finally { $zip.Dispose() }
}
Check-Case 'failedSmokeDoesNotPublish' {
    $fixture = New-Fixture
    $fixture.FailSmoke = $true
    Assert-Throws { Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture) }
    Assert-True ($fixture.Stages.Contains('smoke')) '夹具未到达启动验收，不能算此项验证成功'
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture.Repo 'dist\sleekpr-release\old-release.txt')) '启动失败覆盖了正式目录'
}
Check-Case 'publicationFailureRestoresPreviousArchive' {
    $fixture = New-Fixture
    $oldArchive = Join-Path $fixture.Repo 'dist\sleekpr-1.2.3-aaaaaaaa-bbbbbbbbbbbb-windows-x64.zip'
    [IO.File]::WriteAllText($oldArchive, 'old archive')
    $target = Join-Path $fixture.Directory 'blocked-target'
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $fixture.Repo 'dist\sleekpr-release\blocked') -Target $target | Out-Null
    Assert-Throws { Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture) }
    Assert-True ($fixture.Stages.Contains('smoke')) '未到达发布步骤'
    Assert-True ([IO.File]::ReadAllText($oldArchive) -eq 'old archive') '发布失败未恢复旧归档'
    Assert-True (Test-Path -LiteralPath (Join-Path $fixture.Repo 'dist\sleekpr-release\old-release.txt')) '发布失败损坏旧目录'
}
Check-Case 'debugIdentityCannotBePublishedAsRelease' {
    $fixture = New-Fixture
    $fixture.Debug = $true
    Assert-Throws { Invoke-PortablePackage -RepoRoot $fixture.Repo -CommandInvoker (New-Invoker $fixture) }
    Assert-True ($fixture.Stages.Contains('identity') -and -not $fixture.Stages.Contains('smoke')) '夹具未在构建模式校验阶段拒绝'
}
if ($suite.Failures -gt 0) { throw ('绿色包回归失败：' + $suite.Failures) }
