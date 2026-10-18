param([string[]]$PortableRoots,[string]$ControlScript='',[string]$ReportPrefix='phase09d-control',[switch]$KeepProbe)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
if($ReportPrefix -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid report prefix'}
if($ControlScript){$ControlScript=(Resolve-Path -LiteralPath $ControlScript).Path;if(-not $ControlScript.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Control script must be under work'}}
$rows=@()
foreach($inputRoot in $PortableRoots) {
    $root=(Resolve-Path -LiteralPath $inputRoot).Path
    if(-not $root.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Only work runtimes permitted'}
    $exe=Join-Path $root 'bin/64bit/obs64.exe';$version=(Get-Item $exe).VersionInfo.ProductVersion
    if(@(Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $exe}).Count){throw 'Do not change a running portable collection'}
    $scenePath=Join-Path $root 'config/obs-studio/basic/scenes/Compatibility.json'
    $original=[IO.File]::ReadAllBytes($scenePath);$owned=$null
    try {
        $scene=Get-Content -LiteralPath $scenePath -Raw | ConvertFrom-Json -AsHashtable
        if(-not $KeepProbe){$scene.modules.'scripts-tool'=@($scene.modules.'scripts-tool' | Where-Object {$_.path -notlike '*obs-visual-probe.lua'})}
        if($ControlScript){$scene.modules.'scripts-tool'+=@{path=$ControlScript.Replace('\','/');settings=@{}}}
        [IO.File]::WriteAllText($scenePath,($scene|ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
        $started=[DateTime]::UtcNow
        $owned=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru `
            -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
        if(-not $owned.WaitForExit(45000) -or $owned.ExitCode -ne 0){throw "Baseline OBS $version failed normal exit"}
        $heartbeat=Join-Path (Split-Path $root -Parent) 'heartbeat.txt'
        if((Get-Item $heartbeat).LastWriteTimeUtc -lt $started -or [IO.File]::ReadAllText($heartbeat) -notmatch 'checked_sources=23'){throw 'Baseline stale/failed'}
        $log=Get-ChildItem (Join-Path $root 'config/obs-studio/logs') -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        $output=Join-Path $workspace ('outputs/'+$ReportPrefix+'-'+$version+'.log')
        Copy-Item -LiteralPath $log.FullName -Destination $output
        if($KeepProbe) {
            $visual=Join-Path (Split-Path $root -Parent) 'visual-probe-result.txt'
            if((Get-Item $visual).LastWriteTimeUtc -lt $started -or [IO.File]::ReadAllText($visual) -notmatch '^PASS checks='){throw 'Native probe failed/stale'}
            Copy-Item -LiteralPath $visual -Destination (Join-Path $workspace ('outputs/'+$ReportPrefix+'-'+$version+'-probe.txt'))
        }
        $control=[regex]::Match([IO.File]::ReadAllText($output),'Number of memory leaks: (\d+)').Groups[1].Value
        $native=[regex]::Match([IO.File]::ReadAllText((Join-Path $workspace ('outputs/phase09c-obs-'+$version+'.log'))),'Number of memory leaks: (\d+)').Groups[1].Value
        if($control -eq '' -or $native -eq ''){throw 'Allocation counter absent'}
        $scenario=if($KeepProbe){'probe'}elseif($ControlScript){'control-script'}else{'original-baseline'}
        $rows+=[pscustomobject]@{Version=$version;Scenario=$scenario;Exit=$owned.ExitCode;ReferenceNativeFinalAllocations=$native;ObservedFinalAllocations=$control;DifferenceFromReference=([int]$control-[int]$native)}
        $csvName=if($ReportPrefix -eq 'phase09d-control'){'phase09d-allocation-controls.csv'}else{$ReportPrefix+'-allocations.csv'}
        $rows | Export-Csv (Join-Path $workspace ('outputs/'+$csvName)) -NoTypeInformation
        Write-Output "OBS $version scenario=$scenario observed_allocations=$control reference_native=$native; Exit=0, baseline23"
    }finally{
        if($null -ne $owned -and -not $owned.HasExited){Stop-Process -Id $owned.Id -Force}
        [IO.File]::WriteAllBytes($scenePath,$original)
    }
}
