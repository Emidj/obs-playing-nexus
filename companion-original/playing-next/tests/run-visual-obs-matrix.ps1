param(
    [string]$RuntimeRoot='tests/obs32',
    [string]$BuildRoot='work/core-phase08h-build',
    [string]$WorkRoot='work/phase09-visual-matrix',
    [string[]]$Versions=@('32.0.0','32.0.4','32.1.2','32.2.2'),
    [ValidateSet('playing-visual-probe','playing-normal-group-probe','playing-normal-shared-group-probe','playing-shared-group-control-probe','playing-shared-group-applied-probe','playing-group-srgb-candidate-probe','playing-native-glyph-obs-probe')][string]$ProbeName='playing-visual-probe',
    [ValidatePattern('^[a-zA-Z0-9.-]+$')][string]$EvidencePrefix='phase09c',
    [string]$GlyphReferenceJson=''
)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
$destination=[IO.Path]::GetFullPath($WorkRoot)
$build=(Resolve-Path -LiteralPath $BuildRoot).Path
$runtimes=(Resolve-Path -LiteralPath $RuntimeRoot).Path
foreach($path in @($destination,$build)) {if(-not $path.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Only isolated work paths permitted'}}
if(Test-Path -LiteralPath $destination){throw 'WorkRoot must be new; existing configurations are never overwritten'}
$dll=Join-Path $build ("RelWithDebInfo/$ProbeName.dll")
if(-not (Test-Path -LiteralPath $dll)){throw 'Compile the opt-in visual probe first'}
foreach($version in $Versions) {
    if($version -notmatch '^32\.\d+\.\d+$'){throw 'Only explicit OBS32.x versions permitted'}
    $exe=Join-Path $runtimes "$version/bin/64bit/obs64.exe"
    if(-not (Test-Path -LiteralPath $exe) -or (Get-Item $exe).VersionInfo.ProductVersion -ne $version){throw "Runtime version unavailable: $version"}
}
New-Item -ItemType Directory -Path $destination | Out-Null
$tests=Join-Path $destination 'tests/obs32';New-Item -ItemType Directory -Path $tests | Out-Null
Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing') -Destination $destination -Recurse
Copy-Item -LiteralPath (Join-Path $workspace 'work/audit-phase01/tests/obs32/prepare-portable.ps1') -Destination $tests
Copy-Item -LiteralPath (Join-Path $workspace 'work/audit-phase01/tests/obs32/heartbeat.lua') -Destination $tests
Copy-Item -LiteralPath $dll -Destination (Join-Path $tests 'playing-visual-probe.dll')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'obs-visual-probe.lua') -Destination $tests
$rows=@();$owned=$null
$previousGlyphOutput=$env:PLAYING_GLYPH_GPU_OUTPUT
$previousGlyphReference=$env:PLAYING_GLYPH_REFERENCE_JSON
if($GlyphReferenceJson){$env:PLAYING_GLYPH_REFERENCE_JSON=(Resolve-Path -LiteralPath $GlyphReferenceJson).Path}
try {
    foreach($version in $Versions) {
        $runtime=Join-Path $tests $version;New-Item -ItemType Directory -Path $runtime | Out-Null
        foreach($folder in @('bin','data','obs-plugins')) {
            Copy-Item -LiteralPath (Join-Path $runtimes "$version/$folder") -Destination $runtime -Recurse
        }
        & (Join-Path $tests 'prepare-portable.ps1') -Version $version
        & (Join-Path $PSScriptRoot 'configure-obs-smoke.ps1') -PortableRoot $runtime
        $scenePath=Join-Path $runtime 'config/obs-studio/basic/scenes/Compatibility.json'
        $scene=Get-Content -LiteralPath $scenePath -Raw | ConvertFrom-Json -AsHashtable
        $scene.modules.'scripts-tool'+=@{path=(Join-Path $tests 'obs-visual-probe.lua').Replace('\','/');settings=@{}}
        [IO.File]::WriteAllText($scenePath,($scene|ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
        $exe=Join-Path $runtime 'bin/64bit/obs64.exe'
        $originalHash=(Get-FileHash -LiteralPath (Join-Path $runtimes "$version/bin/64bit/obs64.exe")).Hash
        if((Get-FileHash -LiteralPath $exe).Hash -ne $originalHash){throw 'Copied runtime hash mismatch'}
        $started=[DateTime]::UtcNow
        if($ProbeName -eq 'playing-native-glyph-obs-probe'){$env:PLAYING_GLYPH_GPU_OUTPUT=Join-Path $destination "$version-glyph-captures"}
        $owned=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru `
            -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
        if(-not $owned.WaitForExit(45000) -or $owned.ExitCode -ne 0){throw "OBS $version did not close normally"}
        $visual=Join-Path $tests 'visual-probe-result.txt';$heartbeat=Join-Path $tests 'heartbeat.txt'
        foreach($evidence in @($visual,$heartbeat)){if(-not(Test-Path -LiteralPath $evidence) -or (Get-Item $evidence).LastWriteTimeUtc -lt $started){throw "Missing/stale result $evidence"}}
        $result=[IO.File]::ReadAllText($visual).Trim();$baseline=[IO.File]::ReadAllText($heartbeat).Trim()
        if($result -notmatch '^PASS checks=' -or $baseline -notmatch "^$([regex]::Escape($version)) ticks=24 checked_sources=23"){throw "Native/baseline failed: $result; $baseline"}
        $prefix=Join-Path $workspace ("outputs/$EvidencePrefix-obs-"+$version)
        Copy-Item -LiteralPath $visual -Destination ($prefix+'-probe.txt')
        Copy-Item -LiteralPath $heartbeat -Destination ($prefix+'-baseline.txt')
        $log=Get-ChildItem -LiteralPath (Join-Path $runtime 'config/obs-studio/logs') -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        Copy-Item -LiteralPath $log.FullName -Destination ($prefix+'.log')
        $rows+=[pscustomobject]@{Version=$version;Qt=(Get-Item (Join-Path $runtime 'bin/64bit/Qt6Gui.dll')).VersionInfo.ProductVersion;ObsSHA256=$originalHash;ProbeSHA256=(Get-FileHash $dll).Hash;Exit=$owned.ExitCode;Baseline=$baseline;Visual=$result}
        $rows | Export-Csv (Join-Path $workspace "outputs/$EvidencePrefix-obs-matrix.csv") -NoTypeInformation
        Write-Output "PASS OBS $version; Exit=0; 23 sources; $result"
    }
}catch{
    $_ | Out-String | Set-Content (Join-Path $workspace "outputs/$EvidencePrefix-obs-matrix-failed.txt")
    throw
}finally{
    $env:PLAYING_GLYPH_GPU_OUTPUT=$previousGlyphOutput
    $env:PLAYING_GLYPH_REFERENCE_JSON=$previousGlyphReference
    # Only the process launched by this run. Forced cleanup is always FAIL.
    if($null -ne $owned -and -not $owned.HasExited){Stop-Process -Id $owned.Id -Force}
}
