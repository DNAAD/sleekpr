function Test-ExecutableBuildInfoCapability {
    param([string]$ExecutablePath)
    if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { return $false }
    $version = [Diagnostics.FileVersionInfo]::GetVersionInfo([IO.Path]::GetFullPath($ExecutablePath))
    return $version.PrivateBuild -ceq 'sleekpr-build-info-json-v1'
}

function Invoke-ExecutableDiagnostic {
    param([string]$ExecutablePath, [ValidateSet('--build-info-json','--smoke-test-designer')][string]$Argument = '--build-info-json', [bool]$Isolated, [int]$TimeoutMilliseconds = 20000)
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = [IO.Path]::GetFullPath($ExecutablePath)
    $start.Arguments = $Argument
    $start.WorkingDirectory = Split-Path -Parent $start.FileName
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
    $start.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)
    if ($Isolated) {
        foreach ($name in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QTDIR','Qt6_DIR')) {
            $start.EnvironmentVariables.Remove($name)
        }
        $start.EnvironmentVariables['PATH'] = $start.WorkingDirectory + ';' + (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
    }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw '无法启动构建诊断进程' }
        $output = $process.StandardOutput.ReadToEndAsync()
        $errors = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            # 只结束自己创建且声明支持无副作用诊断的子进程，不能按名称关闭操作员客户端。
            $process.Kill()
            $process.WaitForExit()
            throw '构建诊断子进程超时'
        }
        return [pscustomobject]@{ ExitCode = $process.ExitCode; Stdout = $output.GetAwaiter().GetResult(); Stderr = $errors.GetAwaiter().GetResult() }
    } finally { $process.Dispose() }
}

function Read-ExecutableBuildInfo {
    param([string]$ExecutablePath, [scriptblock]$CapabilityResolver, [scriptblock]$ProcessRunner, [switch]$Isolated)
    if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw ('程序不存在：' + $ExecutablePath) }
    if ($null -eq $CapabilityResolver) { $CapabilityResolver = { param($path) Test-ExecutableBuildInfoCapability -ExecutablePath $path } }
    # 必须先检查 PE 能力标记，否则旧 exe 会忽略参数并意外启动第二个托盘服务。
    if (-not (& $CapabilityResolver $ExecutablePath)) { return $null }
    if ($null -eq $ProcessRunner) { $ProcessRunner = { param($path, $isolated) Invoke-ExecutableDiagnostic -ExecutablePath $path -Isolated $isolated } }
    $result = & $ProcessRunner $ExecutablePath ([bool]$Isolated)
    if ($null -eq $result -or $result.ExitCode -ne 0) { throw '构建诊断子进程失败' }
    try { $identity = $result.Stdout | ConvertFrom-Json -ErrorAction Stop } catch { throw '构建诊断不是有效 JSON' }
    foreach ($name in @('schemaVersion','version','revision','configuration','sourceDirty','workspaceDirty','sourceDigest')) {
        if ($null -eq $identity -or $null -eq $identity.PSObject.Properties[$name]) { throw ('构建诊断缺少字段：' + $name) }
    }
    if ($identity.schemaVersion -ne 1 -or [string]::IsNullOrWhiteSpace($identity.version) -or [string]::IsNullOrWhiteSpace($identity.configuration) -or
        $identity.sourceDigest -notmatch '^[0-9a-fA-F]{64}$' -or
        ($identity.revision -ne 'unknown' -and $identity.revision -notmatch '^[0-9a-fA-F]{40,64}$')) { throw '构建诊断字段无效' }
    foreach ($name in @('sourceDirty','workspaceDirty')) {
        if ($null -ne $identity.$name -and $identity.$name -isnot [bool]) { throw ('构建诊断状态无效：' + $name) }
    }
    return $identity
}

Export-ModuleMember -Function Test-ExecutableBuildInfoCapability, Read-ExecutableBuildInfo, Invoke-ExecutableDiagnostic
