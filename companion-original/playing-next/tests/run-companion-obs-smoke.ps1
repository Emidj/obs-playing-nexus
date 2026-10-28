param(
    [string]$PortableTests='work/audit-phase01/tests/obs32',
    [string]$BuildRoot='work/core-phase08-build',
    [string]$QtRoot='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64',
    [string]$ReportPrefix='phase08d'
)
$ErrorActionPreference='Stop'
if($ReportPrefix -notmatch '^[a-zA-Z0-9_-]+$') {throw 'Invalid report prefix'}
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
$PortableTests=(Resolve-Path -LiteralPath $PortableTests).Path
$BuildRoot=(Resolve-Path -LiteralPath $BuildRoot).Path
foreach($target in @($PortableTests,$BuildRoot)) {if(-not $target.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) {throw 'Only isolated workspace work directories permitted'}}
$runtime=Join-Path $PortableTests '32.2.2'
$fixture=Join-Path $BuildRoot ('obs-companion-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$oldPath=$env:PATH; $env:PATH=(Join-Path $QtRoot 'bin')+[IO.Path]::PathSeparator+$oldPath
$orchestrator=$null; $obsProcess=$null
try {
    $tool=Join-Path $BuildRoot 'RelWithDebInfo/playing-companion-process-tests.exe'
    $orchestrator=Start-Process -FilePath $tool -WindowStyle Hidden -PassThru -WorkingDirectory $fixture `
        -ArgumentList ('--obs "'+$fixture+'"') -RedirectStandardOutput (Join-Path $fixture 'stdout.txt') -RedirectStandardError (Join-Path $fixture 'stderr.txt')
    $deadline=[DateTime]::UtcNow.AddSeconds(12)
    while(-not (Test-Path -LiteralPath (Join-Path $fixture 'ready.txt'))) {
        if($orchestrator.HasExited -or [DateTime]::UtcNow -ge $deadline) {throw 'Companion process fixture failed startup'}
        Start-Sleep -Milliseconds 25
    }
    & (Join-Path $PortableTests 'prepare-portable.ps1') -Version '32.2.2'
    & (Join-Path $PSScriptRoot 'configure-obs-smoke.ps1') -PortableRoot $runtime
    Copy-Item -LiteralPath (Join-Path $BuildRoot 'RelWithDebInfo/playing-core-probe.dll') -Destination $PortableTests
    foreach($script in @('obs-core-probe.lua','obs-companion-probe.lua')) {Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination $PortableTests}
    [IO.File]::WriteAllText((Join-Path $PortableTests 'companion-probe-root.txt'),$fixture.Replace('\','/'),[Text.UTF8Encoding]::new($false))
    $scenePath=Join-Path $runtime 'config/obs-studio/basic/scenes/Compatibility.json'
    $scene=Get-Content -LiteralPath $scenePath -Raw | ConvertFrom-Json -AsHashtable
    foreach($script in @('obs-core-probe.lua','obs-companion-probe.lua')) {$scene.modules.'scripts-tool'+=@{path=(Join-Path $PortableTests $script).Replace('\','/');settings=@{}}}
    [IO.File]::WriteAllText($scenePath,($scene|ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
    $exe=Join-Path $runtime 'bin/64bit/obs64.exe'; $started=[DateTime]::UtcNow
    $obsProcess=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru `
        -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
    if(-not $obsProcess.WaitForExit(45000) -or $obsProcess.ExitCode -ne 0) {throw 'OBS did not close normally Exit=0'}
    if(-not $orchestrator.WaitForExit(5000) -or $orchestrator.ExitCode -ne 0) {throw 'Companion orchestrator did not close normally Exit=0'}
    $text=[IO.File]::ReadAllText((Join-Path $fixture 'obs-result.txt'))
    if($text -notmatch 'result=0; stages=0,1,2,;') {throw 'Companion OBS live bridge failed'}
    $heartbeat=[IO.File]::ReadAllText((Join-Path $PortableTests 'heartbeat.txt')).Trim()
    if($heartbeat -notmatch 'ticks=24 checked_sources=23' -or (Get-Item (Join-Path $PortableTests 'heartbeat.txt')).LastWriteTimeUtc -lt $started) {throw 'Original 23-source baseline missing/stale'}
    $outputs=Join-Path $workspace 'outputs'
    foreach($entry in @('core-probe-result.txt','provider-probe-result.txt','history-probe-result.txt')) {
        $path=Join-Path $PortableTests $entry
        if((Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $started -or [IO.File]::ReadAllText($path) -notmatch 'result=0') {throw "Failed/stale $entry"}
        Copy-Item -LiteralPath $path -Destination (Join-Path $outputs ($ReportPrefix+'-'+$entry))
    }
    Copy-Item -LiteralPath (Join-Path $fixture 'obs-result.txt') -Destination (Join-Path $outputs ($ReportPrefix+'-obs-probe.txt'))
    Copy-Item -LiteralPath (Join-Path $fixture 'process-result.txt') -Destination (Join-Path $outputs ($ReportPrefix+'-process-probe.txt'))
    $log=Get-ChildItem -LiteralPath (Join-Path $runtime 'config/obs-studio/logs') -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $outputs ($ReportPrefix+'-obs.log'))
    "OBS Exit=0; Companion sender/receiver exits=0,0; $heartbeat; $text; fixture=$fixture; test-only atomic frame bridge" | Set-Content (Join-Path $outputs ($ReportPrefix+'-obs-result.txt'))
    Write-Output "OBS Exit=0; Companion processes Exit=0,0; $heartbeat; $text"
} catch {
    "FAIL $($_.Exception.Message); fixture=$fixture; forced cleanup is not PASS" | Set-Content (Join-Path $workspace ('outputs/'+$ReportPrefix+'-obs-failed.txt'))
    throw
} finally {
    # Ask owned children to exit before forced cleanup of the orchestrator.
    [IO.File]::WriteAllText((Join-Path $fixture 'sender.command'),'stop')
    [IO.File]::WriteAllText((Join-Path $fixture 'receiver.command'),'stop')
    if($null -ne $orchestrator -and -not $orchestrator.HasExited) { $null=$orchestrator.WaitForExit(2000) }
    foreach($owned in @($obsProcess,$orchestrator)) {if($null -ne $owned -and -not $owned.HasExited) {Stop-Process -Id $owned.Id -Force}}
    $env:PATH=$oldPath
}
