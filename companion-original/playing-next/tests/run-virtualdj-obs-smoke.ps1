param(
    [Parameter(Mandatory=$true)][string]$PortableTests,
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [string]$Version='32.2.2',
    [string]$Report='phase05c5-virtualdj-20261010'
)
$ErrorActionPreference='Stop'
$workspace=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$workRoot=[IO.Path]::GetFullPath((Join-Path $workspace 'work')).TrimEnd('\')+'\'
foreach($target in @($PortableTests,$BuildRoot)) {
    if(-not [IO.Path]::GetFullPath($target).StartsWith($workRoot,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Only isolated directories under project work are permitted.'
    }
}
if($Version -notmatch '^32\.\d+\.\d+$' -or $Report -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Invalid version/report.' }
$PortableTests=[IO.Path]::GetFullPath($PortableTests); $BuildRoot=[IO.Path]::GetFullPath($BuildRoot)
$runtime=Join-Path $PortableTests $Version
$exe=Join-Path $runtime 'bin\64bit\obs64.exe'
$tool=Join-Path $BuildRoot 'RelWithDebInfo\playing-virtualdj-probe-fixture.exe'
$fixture=Join-Path $BuildRoot ('obs-vdj-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$resultPath=Join-Path $fixture 'result.txt'; $outputs=Join-Path $workspace 'outputs'
$obsProcess=$null; $fixtureProcess=$null
try {
    $fixtureProcess=Start-Process -FilePath $tool -WorkingDirectory $fixture -WindowStyle Hidden -PassThru `
        -ArgumentList ('"'+$fixture+'"') -RedirectStandardOutput (Join-Path $fixture 'stdout.txt') `
        -RedirectStandardError (Join-Path $fixture 'stderr.txt')
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    $readyPath=Join-Path $fixture 'ready.json'
    while(-not (Test-Path -LiteralPath $readyPath)) {
        if($fixtureProcess.HasExited -or [DateTime]::UtcNow -ge $deadline) { throw 'Memory fixture startup failed.' }
        Start-Sleep -Milliseconds 25
    }
    $ready=Get-Content -LiteralPath $readyPath -Raw | ConvertFrom-Json
    if($ready.pid -ne $fixtureProcess.Id -or $ready.module -lt 65536) { throw 'Invalid fixture identity.' }
    & (Join-Path $PortableTests 'prepare-portable.ps1') -Version $Version
    & (Join-Path $PSScriptRoot 'configure-obs-smoke.ps1') -PortableRoot $runtime
    Copy-Item -LiteralPath (Join-Path $BuildRoot 'RelWithDebInfo\playing-core-probe.dll') -Destination $PortableTests
    foreach($script in @('obs-core-probe.lua','obs-virtualdj-probe.lua')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination $PortableTests
    }
    @{pid=$ready.pid;module=$ready.module;root=$fixture.Replace('\','/');report=$resultPath.Replace('\','/')} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PortableTests 'virtualdj-probe-config.json') -Encoding utf8NoBOM
    $scenePath=Join-Path $runtime 'config\obs-studio\basic\scenes\Compatibility.json'
    $scene=Get-Content -LiteralPath $scenePath -Raw | ConvertFrom-Json -AsHashtable
    foreach($script in @('obs-core-probe.lua','obs-virtualdj-probe.lua')) {
        $scene.modules.'scripts-tool'+=@{path=(Join-Path $PortableTests $script).Replace('\','/');settings=@{}}
    }
    [IO.File]::WriteAllText($scenePath,($scene | ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
    $started=[DateTime]::UtcNow
    $obsProcess=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru `
        -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
    if(-not $obsProcess.WaitForExit(45000) -or $obsProcess.ExitCode -ne 0) { throw 'OBS did not close normally with ExitCode 0.' }
    $text=[IO.File]::ReadAllText($resultPath)
    if($text -notmatch 'OBS result=0;' -or $text -notmatch 'stages=0,1,2,3,4;') { throw 'Memory provider probe failed.' }
    'stop' | Set-Content -LiteralPath (Join-Path $fixture 'stop.txt')
    if(-not $fixtureProcess.WaitForExit(5000) -or $fixtureProcess.ExitCode -ne 0) { throw 'Memory fixture did not close normally.' }
    $heartbeatPath=Join-Path $PortableTests 'heartbeat.txt'
    $heartbeat=[IO.File]::ReadAllText($heartbeatPath).Trim()
    if($heartbeat -notmatch 'ticks=24 checked_sources=23' -or (Get-Item -LiteralPath $heartbeatPath).LastWriteTimeUtc -lt $started) { throw 'Original preset compatibility result missing/stale.' }
    foreach($entry in @('core-probe-result.txt','provider-probe-result.txt','history-probe-result.txt')) {
        $path=Join-Path $PortableTests $entry
        if((Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $started -or [IO.File]::ReadAllText($path) -notmatch 'result=0') { throw "Failed/stale probe $entry" }
        Copy-Item -LiteralPath $path -Destination (Join-Path $outputs ($Report+'-'+$entry))
    }
    $log=Get-ChildItem -LiteralPath (Join-Path $runtime 'config\obs-studio\logs') -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $outputs ($Report+'.log'))
    Copy-Item -LiteralPath $resultPath -Destination (Join-Path $outputs ($Report+'-probe.txt'))
    "OBS Exit=0; fixture Exit=0; $heartbeat; fixture=$fixture; synthetic layout, no live DJ claim" | Set-Content (Join-Path $outputs ($Report+'-result.txt'))
    Write-Output "OBS Exit=0; fixture Exit=0; $heartbeat; $($text.Trim())"
} catch {
    $failure=$Report+'-failed-'+(Split-Path $fixture -Leaf)
    "FAIL: $($_.Exception.Message); fixture=$fixture; forced cleanup is not PASS" | Set-Content (Join-Path $outputs ($failure+'-result.txt'))
    $log=Get-ChildItem -LiteralPath (Join-Path $runtime 'config\obs-studio\logs') -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if($log) { Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $outputs ($failure+'.log')) }
    throw
} finally {
    foreach($ownedProcess in @($obsProcess,$fixtureProcess)) {
        if($null -ne $ownedProcess -and -not $ownedProcess.HasExited) { Stop-Process -Id $ownedProcess.Id -Force }
    }
}
