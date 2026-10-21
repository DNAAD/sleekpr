param([string]$RepoRoot, [string]$TestRoot, [string]$ExecutablePath)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $RepoRoot 'packaging\ExecutableBuildInfo.psm1') -Force
$caseRoot = Join-Path $TestRoot ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null
$fakeExe = Join-Path $caseRoot 'fixture.exe'
[IO.File]::WriteAllText($fakeExe, '不得执行的临时夹具')
$state = [pscustomobject]@{ Calls = 0; Failures = 0 }
function Check-Case {
    param([string]$Name, [scriptblock]$Action)
    try { & $Action; Write-Host ('PASS ' + $Name) }
    catch { $state.Failures++; Write-Host ('FAIL ' + $Name + ': ' + $_.Exception.Message) }
}
function Assert-True { param([bool]$Condition, [string]$Message) if (-not $Condition) { throw $Message } }
function Assert-Throws {
    param([scriptblock]$Action)
    $thrown = $false
    try { & $Action | Out-Null } catch { $thrown = $true }
    Assert-True $thrown '应拒绝无效诊断结果'
}
$capable = { param($path) return $true }
$legacy = { param($path) return $false }
$identity = [pscustomobject]@{ schemaVersion = 1; version = '1.2.3'; revision = ('a' * 40); configuration = 'Release'; sourceDirty = $true; workspaceDirty = $true; sourceDigest = ('b' * 64) }
$validJson = $identity | ConvertTo-Json -Compress
$runner = { param($path, $isolated) $state.Calls++; return [pscustomobject]@{ ExitCode = 0; Stdout = $validJson; Stderr = '' } }.GetNewClosure()

Check-Case 'legacyExecutableIsNotStartedForMetadata' {
    $value = Read-ExecutableBuildInfo -ExecutablePath $fakeExe -CapabilityResolver $legacy -ProcessRunner $runner
    Assert-True ($null -eq $value -and $state.Calls -eq 0) '旧程序不能为版本探测被启动'
}
Check-Case 'modernExecutableReadsAndValidatesIdentity' {
    $value = Read-ExecutableBuildInfo -ExecutablePath $fakeExe -CapabilityResolver $capable -ProcessRunner $runner
    Assert-True ($null -ne $value -and $value.version -eq '1.2.3' -and $state.Calls -eq 1) '未读取受支持程序的构建标识'
}
Check-Case 'failedDiagnosticIsRejected' {
    Assert-Throws { Read-ExecutableBuildInfo -ExecutablePath $fakeExe -CapabilityResolver $capable -ProcessRunner { return [pscustomobject]@{ ExitCode = 7; Stdout = '{}'; Stderr = '失败' } } }
}
Check-Case 'malformedDiagnosticIsRejected' {
    Assert-Throws { Read-ExecutableBuildInfo -ExecutablePath $fakeExe -CapabilityResolver $capable -ProcessRunner { return [pscustomobject]@{ ExitCode = 0; Stdout = 'not-json'; Stderr = '' } } }
}
Check-Case 'builtExecutableHasSafeMetadataCapability' {
    Assert-True (Test-ExecutableBuildInfoCapability -ExecutablePath $ExecutablePath) '真实程序缺少安全的版本探测能力标记'
}
if ($state.Failures -gt 0) { throw ('构建标识回归失败：' + $state.Failures) }
