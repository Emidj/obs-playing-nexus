param(
    [Parameter(Mandatory=$true)][string]$PortableTests,
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [string]$Version='32.2.2',
    [string]$Report='phase05b5-rekordbox-20261010'
)
$ErrorActionPreference='Stop'
$workspace=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$workRoot=[IO.Path]::GetFullPath((Join-Path $workspace 'work')).TrimEnd('\')+'\'
foreach($target in @($PortableTests,$BuildRoot)) {
    if(-not [IO.Path]::GetFullPath($target).StartsWith($workRoot,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Build and portable tests must be isolated under this project work directory.'
    }
}
if($Version -notmatch '^32\.\d+\.\d+$' -or $Report -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Invalid report/version.' }
$PortableTests=[IO.Path]::GetFullPath($PortableTests)
$BuildRoot=[IO.Path]::GetFullPath($BuildRoot)
$runtime=Join-Path $PortableTests $Version
$exe=Join-Path $runtime 'bin\64bit\obs64.exe'
$fixtureTool=Join-Path $BuildRoot 'RelWithDebInfo\playing-rekordbox-probe-fixture.exe'
$library=Join-Path $workspace 'obs-playing\obs-playing-db.dll'
if((Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash -ne
    '5030DECC6D914539E3B9B7E28AA4F6DE1E7161DAC6FD4B21EB02E6754D9B175E') { throw 'Audited DLL hash mismatch.' }
$fixture=Join-Path $BuildRoot ('obs-rb-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$database=Join-Path $fixture 'probe.db'
$ready=Join-Path $fixture 'ready.txt'
$probeResult=Join-Path $fixture 'result.txt'
& $fixtureTool create $database $library | Set-Content (Join-Path $fixture 'create.txt')
if($LASTEXITCODE -ne 0) { throw 'Creating isolated encrypted fixture failed.' }
& (Join-Path $PortableTests 'prepare-portable.ps1') -Version $Version
& (Join-Path $PSScriptRoot 'configure-obs-smoke.ps1') -PortableRoot $runtime
Copy-Item -LiteralPath (Join-Path $BuildRoot 'RelWithDebInfo\playing-core-probe.dll') -Destination $PortableTests
foreach($script in @('obs-core-probe.lua','obs-rekordbox-probe.lua')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination $PortableTests
}
@{database=$database;library=$library;ready=$ready;report=$probeResult} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PortableTests 'rekordbox-probe-config.json') -Encoding utf8NoBOM
$scenePath=Join-Path $runtime 'config\obs-studio\basic\scenes\Compatibility.json'
$scene=Get-Content -LiteralPath $scenePath -Raw | ConvertFrom-Json -AsHashtable
foreach($script in @('obs-core-probe.lua','obs-rekordbox-probe.lua')) {
    $scene.modules.'scripts-tool'+=@{path=(Join-Path $PortableTests $script).Replace('\','/');settings=@{}}
}
[IO.File]::WriteAllText($scenePath,($scene | ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
$obsProcess=$null; $writerProcess=$null
function Read-SharedWriterLog([string]$Path) {
    if(-not (Test-Path -LiteralPath $Path)) { return '' }
    $stream=$null; $reader=$null
    try {
        $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        $reader=[IO.StreamReader]::new($stream)
        return $reader.ReadToEnd()
    } catch [IO.IOException] { return '' }
    finally { if($null -ne $reader) { $reader.Dispose() } elseif($null -ne $stream) { $stream.Dispose() } }
}
function Get-SharedSha256([string]$Path) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash($stream)) }
    finally { $sha.Dispose(); $stream.Dispose() }
}
try {
    $started=[DateTime]::UtcNow
    $obsProcess=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru `
        -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
    $deadline=[DateTime]::UtcNow.AddSeconds(30)
    while(-not (Test-Path -LiteralPath $ready)) {
        if($obsProcess.HasExited -or [DateTime]::UtcNow -ge $deadline) { throw 'OBS initial encrypted history probe timed out.' }
        Start-Sleep -Milliseconds 50
    }
    $before=Get-SharedSha256 $database
    $writerOut=Join-Path $fixture 'writer.txt'
    $writerProcess=Start-Process -FilePath $fixtureTool -WorkingDirectory $fixture -WindowStyle Hidden -PassThru `
        -ArgumentList @('append',('"'+$database+'"'),('"'+$library+'"')) `
        -RedirectStandardOutput $writerOut -RedirectStandardError (Join-Path $fixture 'writer-error.txt')
    $deadline=[DateTime]::UtcNow.AddSeconds(3)
    while((Read-SharedWriterLog $writerOut) -notmatch 'APPENDED_WAL_WRITER_OPEN') {
        if($writerProcess.HasExited -or [DateTime]::UtcNow -ge $deadline) { throw 'External WAL writer did not commit.' }
        Start-Sleep -Milliseconds 25
    }
    $after=Get-SharedSha256 $database
    $wal=Get-Item -LiteralPath ($database+'-wal')
    if($before -ne $after -or $wal.Length -le 0 -or $writerProcess.HasExited) {
        throw 'WAL commit was not isolated from main database checkpoint.'
    }
    "WAL commit verified: main SHA256 unchanged=$after; WAL bytes=$($wal.Length); writer alive=True" |
        Set-Content (Join-Path $fixture 'wal-evidence.txt')
    if(-not $obsProcess.WaitForExit(25000) -or $obsProcess.ExitCode -ne 0) {
        throw 'OBS failed to close normally with exit code zero.'
    }
    if(-not $writerProcess.WaitForExit(6000) -or $writerProcess.ExitCode -ne 0) { throw 'External fixture writer failed.' }
    $probeText=[IO.File]::ReadAllText($probeResult)
    if($probeText -notmatch 'OBS result=0;') { throw 'Native rekordbox probe failed.' }
    $heartbeat=[IO.File]::ReadAllText((Join-Path $PortableTests 'heartbeat.txt')).Trim()
    if($heartbeat -notmatch 'ticks=24 checked_sources=23' -or
        (Get-Item -LiteralPath (Join-Path $PortableTests 'heartbeat.txt')).LastWriteTimeUtc -lt $started) {
        throw 'Original sources compatibility probe failed or is stale.'
    }
    $outputs=Join-Path $workspace 'outputs'
    $log=Get-ChildItem -LiteralPath (Join-Path $runtime 'config\obs-studio\logs') -File |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $outputs ($Report+'.log'))
    Copy-Item -LiteralPath $probeResult -Destination (Join-Path $outputs ($Report+'-probe.txt'))
    Copy-Item -LiteralPath (Join-Path $fixture 'wal-evidence.txt') -Destination (Join-Path $outputs ($Report+'-wal.txt'))
    "Exit=0; $heartbeat; fixture=$fixture" | Set-Content (Join-Path $outputs ($Report+'-result.txt'))
    Write-Output "Exit=0; $heartbeat; $($probeText.Trim())"
} catch {
    $outputs=Join-Path $workspace 'outputs'
    $failedReport=$Report+'-failed-'+(Split-Path $fixture -Leaf)
    "FAIL: $($_.Exception.Message); fixture=$fixture; any forced cleanup is not PASS" |
        Set-Content (Join-Path $outputs ($failedReport+'.txt'))
    $log=Get-ChildItem -LiteralPath (Join-Path $runtime 'config\obs-studio\logs') -File |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if($null -ne $log) { Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $outputs ($failedReport+'.log')) }
    throw
} finally {
    # Only processes started by this invocation may be stopped. Timeout/kill is never PASS.
    foreach($owned in @($writerProcess,$obsProcess)) {
        if($null -ne $owned -and -not $owned.HasExited) { $owned.Kill(); $owned.WaitForExit() }
    }
}
