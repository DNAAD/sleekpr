function Assert-PortableOutputPath {
    param([string]$RepoRoot, [string]$Path)
    $root = [IO.Path]::GetFullPath($RepoRoot).TrimEnd([char[]]'\/')
    $full = [IO.Path]::GetFullPath($Path).TrimEnd([char[]]'\/')
    $prefix = $root + [IO.Path]::DirectorySeparatorChar + 'dist' + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw ('发布路径越界：' + $full) }
    foreach ($part in $full.Split([char[]]'\/')) { if ($part -match '[ .]$') { throw '发布路径包含不安全尾随字符' } }
    # 校验目标及所有已有祖先，不能经目录联接清理或移动到工作区之外。
    $current = $full
    while (-not [string]::IsNullOrWhiteSpace($current)) {
        if (Test-Path -LiteralPath $current) {
            if (((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw ('发布路径不能经过重解析点：' + $current) }
        }
        $parent = [IO.Path]::GetDirectoryName($current)
        if ($parent -eq $current) { break }
        $current = $parent
    }
    return $full
}

function Assert-PortableTree {
    param([string]$RepoRoot, [string]$Directory)
    $null = Assert-PortableOutputPath $RepoRoot $Directory
    $pending = New-Object 'System.Collections.Generic.Stack[string]'
    $pending.Push($Directory)
    while ($pending.Count -gt 0) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw ('包内包含重解析点：' + $item.FullName) }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Move-PortableItem {
    param([string]$RepoRoot, [string]$Source, [string]$Destination)
    $sourcePath = Assert-PortableOutputPath $RepoRoot $Source
    $targetPath = Assert-PortableOutputPath $RepoRoot $Destination
    if (Test-Path -LiteralPath $targetPath) { throw ('移动目标已存在，未覆盖：' + $targetPath) }
    if ((Get-Item -LiteralPath $sourcePath -Force).PSIsContainer) { Assert-PortableTree $RepoRoot $sourcePath }
    Move-Item -LiteralPath $sourcePath -Destination $targetPath -ErrorAction Stop
}

function Test-PortableDebugArtifact {
    param([string]$Relative)
    return $Relative -match '(?i)\.(pdb|ilk|obj|log)$|(^|/)(Testing|tests|screenshots?)(/|$)|(^|/)(CMakeCache\.txt|screenshot[^/]*\.png)$'
}

function Get-PortableFileSha256 {
    param([string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose(); $stream.Dispose() }
}

function Get-PortableFiles {
    param([string]$Directory)
    $prefix = [IO.Path]::GetFullPath($Directory).TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
    foreach ($file in Get-ChildItem -LiteralPath $Directory -File -Recurse -Force | Sort-Object FullName) {
        $relative = $file.FullName.Substring($prefix.Length).Replace('\','/')
        if ($relative -eq 'manifest.json') { continue }
        [pscustomobject]@{ path = $relative; bytes = [long]$file.Length; sha256 = (Get-PortableFileSha256 $file.FullName) }
    }
}

function Write-PortableJson {
    param([string]$Path, $Value)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
}

function Invoke-PortableStage {
    param([scriptblock]$Invoker, [string]$Stage, $Context)
    $result = & $Invoker $Stage $Context
    if ($null -eq $result -or $null -eq $result.PSObject.Properties['ExitCode'] -or $result.ExitCode -ne 0) { throw ('绿色包阶段失败：' + $Stage) }
    return $result
}

function New-PortableArchive {
    param([string]$Directory, [string]$ArchivePath, [string]$FolderName)
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    Add-Type -AssemblyName System.IO.Compression
    $zip = [IO.Compression.ZipFile]::Open($ArchivePath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -LiteralPath $Directory -File -Recurse -Force | Sort-Object FullName) {
            # .NET Framework 默认可能写反斜杠，显式规范 ZIP 条目，避免跨解压工具出现目录差异。
            $relative = $file.FullName.Substring($Directory.Length + 1).Replace('\','/')
            $entry = $zip.CreateEntry($FolderName + '/' + $relative, [IO.Compression.CompressionLevel]::Optimal)
            $input = [IO.File]::OpenRead($file.FullName)
            $output = $entry.Open()
            try { $input.CopyTo($output) } finally { $output.Dispose(); $input.Dispose() }
        }
    } finally { $zip.Dispose() }
}

function Assert-PortableArchive {
    param([string]$ArchivePath, [string]$FolderName, $Files)
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        foreach ($entry in $zip.Entries) { if (Test-PortableDebugArtifact $entry.FullName) { throw '归档混入调试产物' } }
        if ($null -eq $zip.GetEntry($FolderName + '/manifest.json')) { throw '归档缺少清单' }
        foreach ($file in $Files) {
            $entry = $zip.GetEntry($FolderName + '/' + $file.path)
            if ($null -eq $entry -or $entry.Length -ne $file.bytes) { throw ('归档文件不一致：' + $file.path) }
            $stream = $entry.Open()
            $hash = [Security.Cryptography.SHA256]::Create()
            try { $actual = [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
            finally { $hash.Dispose(); $stream.Dispose() }
            if ($actual -ne $file.sha256) { throw ('归档摘要不一致：' + $file.path) }
        }
    } finally { $zip.Dispose() }
}

function Assert-PortableClientStopped {
    param([string]$Directory)
    $exe = [IO.Path]::GetFullPath((Join-Path $Directory 'sleekpr.exe'))
    foreach ($process in Get-Process -Name sleekpr -ErrorAction SilentlyContinue) {
        if ($process.Path -and $process.Path.Equals($exe, [StringComparison]::OrdinalIgnoreCase)) { throw '客户端仍在正式目录运行，请保存并从托盘退出后再发布。' }
    }
}

function Invoke-PortablePackage {
    param([string]$RepoRoot, [scriptblock]$CommandInvoker, [switch]$NoTests)
    if ($null -eq $CommandInvoker) { throw '缺少验收命令器' }
    $repo = [IO.Path]::GetFullPath($RepoRoot)
    $dist = Join-Path $repo 'dist'
    $lockPath = Assert-PortableOutputPath $repo (Join-Path $dist '.sleekpr-publish.lock')
    [IO.Directory]::CreateDirectory($dist) | Out-Null
    $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $stageRoot = $null
    $published = $false
    try {
        $folder = if ($NoTests) { 'sleekpr-candidate' } else { 'sleekpr-release' }
        $stageRoot = Assert-PortableOutputPath $repo (Join-Path $dist ('.sleekpr-stage-' + [Guid]::NewGuid().ToString('N')))
        $payload = Join-Path $stageRoot $folder
        $context = [pscustomobject]@{ RepoRoot = $repo; BuildDir = (Join-Path $repo 'build-release'); BuildExe = (Join-Path $repo 'build-release\sleekpr.exe'); PayloadDir = $payload; Isolated = $true }
        $null = Invoke-PortableStage $CommandInvoker 'build' $context
        if (-not (Test-Path -LiteralPath $context.BuildExe -PathType Leaf)) { throw 'Release 未产生产品 exe' }
        $tests = [pscustomobject]@{ status = 'skipped_by_option'; total = 0; failures = 0; skipped = 0 }
        if (-not $NoTests) {
            $result = Invoke-PortableStage $CommandInvoker 'test' $context
            if ($result.TestsTotal -le 0 -or $result.Failures -ne 0 -or $null -eq $result.Skipped -or $result.Skipped -ne 0) { throw '没有完整执行且全绿的测试记录，不能发布正式包' }
            $tests = [pscustomobject]@{ status = 'passed'; total = $result.TestsTotal; failures = $result.Failures; skipped = $result.Skipped }
        }
        [IO.Directory]::CreateDirectory($payload) | Out-Null
        $null = Invoke-PortableStage $CommandInvoker 'deploy' $context
        Assert-PortableTree $repo $payload
        foreach ($file in Get-ChildItem -LiteralPath $payload -File -Recurse -Force) {
            $relative = $file.FullName.Substring($payload.Length + 1).Replace('\','/')
            if (Test-PortableDebugArtifact $relative) {
                $null = Assert-PortableOutputPath $repo $file.FullName
                Remove-Item -LiteralPath $file.FullName -Force
            } elseif ($relative -match '(?i)(^|/)(Qt6[^/]*d|msvcp[0-9_]*d|vcruntime[0-9_]*d|ucrtbased)\.dll$|^platforms/(qwindowsd|qminimald|qoffscreend)\.dll$|^imageformats/[^/]+d\.dll$') { throw ('混入调试运行时：' + $relative) }
        }
        $stagedExe = Join-Path $payload 'sleekpr.exe'
        if (-not (Test-Path -LiteralPath $stagedExe -PathType Leaf)) { throw '部署缺少产品 exe' }
        if ((Get-PortableFileSha256 $stagedExe) -ne (Get-PortableFileSha256 $context.BuildExe)) { throw '部署 exe 与构建产物不同' }
        # 固定本地 Qt 前缀，隔离启动不能回退到开发机 Qt 安装目录。
        [IO.File]::WriteAllText((Join-Path $payload 'qt.conf'), "[Paths]`nPrefix=.`nPlugins=.`nTranslations=translations`n", [Text.UTF8Encoding]::new($false))
        $identity = (Invoke-PortableStage $CommandInvoker 'identity' $context).BuildInfo
        if ($null -eq $identity -or $identity.configuration -ne 'Release' -or $identity.version -notmatch '^[0-9A-Za-z][0-9A-Za-z._-]{0,63}$' -or
            $identity.sourceDigest -notmatch '^[0-9a-fA-F]{64}$') { throw '不能发布未知或非 Release 构建' }
        $null = Invoke-PortableStage $CommandInvoker 'smoke' $context
        $status = if ($NoTests) { 'unverified' } else { 'verified' }
        $files = @(Get-PortableFiles $payload)
        $manifest = [ordered]@{ schemaVersion = 1; buildInfo = $identity; generatedAtUtc = [DateTime]::UtcNow.ToString('o'); validation = [ordered]@{ status = $status; tests = $tests; buildInfoProbe = 'passed'; designerSmoke = 'passed'; dependencyIsolation = 'local-runtime' }; files = $files }
        Write-PortableJson (Join-Path $payload 'manifest.json') $manifest
        $revision = if ($identity.revision -eq 'unknown') { 'unknown' } else { $identity.revision.Substring(0,8) }
        $suffix = if ($NoTests) { '-unverified' } else { '' }
        $name = 'sleekpr-' + $identity.version + '-' + $revision + '-' + $identity.sourceDigest.Substring(0,12) + '-windows-x64' + $suffix + '.zip'
        $archive = Assert-PortableOutputPath $repo (Join-Path $dist $name)
        $directory = Assert-PortableOutputPath $repo (Join-Path $dist $folder)
        $verificationPath = Assert-PortableOutputPath $repo ($archive + '.verification.json')
        $stagedArchive = Join-Path $stageRoot 'archive.zip'
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        New-PortableArchive $payload $stagedArchive $folder
        Assert-PortableArchive $stagedArchive $folder $files
        $stagedVerification = Join-Path $stageRoot 'verification.json'
        Write-PortableJson $stagedVerification ([ordered]@{ schemaVersion = 1; status = $status; buildInfo = $identity; archive = $name; archiveSha256 = (Get-PortableFileSha256 $stagedArchive); validation = $manifest.validation })
        Assert-PortableClientStopped $directory
        # 发布只移动已经验收的暂存数据；每一步记录逆操作，失败时保留或恢复旧产物。
        $moves = New-Object 'System.Collections.Generic.List[object]'
        $move = { param($from,$to) Move-PortableItem $repo $from $to; $moves.Add([pscustomobject]@{ Source = $from; Destination = $to }) }
        try {
            if (Test-Path -LiteralPath $archive) { & $move $archive (Join-Path $stageRoot 'previous.zip') }
            if (Test-Path -LiteralPath $verificationPath) { & $move $verificationPath (Join-Path $stageRoot 'previous-verification.json') }
            & $move $stagedArchive $archive
            & $move $stagedVerification $verificationPath
            Assert-PortableClientStopped $directory
            if (Test-Path -LiteralPath $directory) { & $move $directory (Join-Path $stageRoot 'previous-directory') }
            & $move $payload $directory
        } catch {
            for ($i = $moves.Count - 1; $i -ge 0; $i--) {
                $operation = $moves[$i]
                try {
                    if ((Test-Path -LiteralPath $operation.Destination) -and -not (Test-Path -LiteralPath $operation.Source)) { Move-PortableItem $repo $operation.Destination $operation.Source }
                } catch { Write-Warning ('请核对保留的旧产物：' + $operation.Destination) }
            }
            throw
        }
        $published = $true
        return [pscustomobject]@{ Status = $status; PackageDirectory = $directory; ArchivePath = $archive; ManifestPath = (Join-Path $directory 'manifest.json'); VerificationPath = $verificationPath; BuildInfo = $identity }
    } finally {
        if ($published -and $stageRoot -and (Test-Path -LiteralPath $stageRoot)) {
            try {
                $null = Assert-PortableOutputPath $repo $stageRoot
                Assert-PortableTree $repo $stageRoot
                Remove-Item -LiteralPath $stageRoot -Recurse -Force
            } catch { Write-Warning ('已发布，旧暂存备份未清理：' + $stageRoot) }
        }
        $lock.Dispose()
    }
}

Export-ModuleMember -Function Assert-PortableOutputPath, Invoke-PortablePackage, Get-PortableFileSha256
