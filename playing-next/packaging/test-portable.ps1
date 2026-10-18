param(
    [string]$Archive='work/phase08j-portable/OBS-Playing-Companion-test-x64.zip',
    [string]$BuildRoot='work/core-phase08h-build',
    [string]$QtRoot='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64',
    [string]$Dumpbin='C:/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe',
    [string]$Report='outputs/phase08j-package-smoke.txt'
)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
$Archive=(Resolve-Path -LiteralPath $Archive).Path
if(-not $Archive.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) {throw 'Only workspace work archives can be tested'}
$testRoot=Join-Path $workspace ('work/phase08j-extracted-'+[Guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath $Archive -DestinationPath $testRoot
$manifest=Import-Csv -LiteralPath (Join-Path $testRoot 'SHA256SUMS.csv')
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($entry in $manifest) {
    $file=[IO.Path]::GetFullPath((Join-Path $testRoot $entry.Path))
    if(-not $file.StartsWith($testRoot+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Unsafe manifest path'}
    if(-not $seen.Add($file)) {throw 'Duplicate manifest path'}
    if((Get-FileHash -LiteralPath $file).Hash -ne $entry.SHA256 -or (Get-Item -LiteralPath $file).Length -ne [long]$entry.Bytes) {throw "Payload mismatch: $($entry.Path)"}
}
$actual=Get-ChildItem -LiteralPath $testRoot -Recurse -File
if($actual.Count -ne $manifest.Count+1) {throw 'Unexpected package files'}
if($actual | Where-Object {$_.Extension -in @('.pcs','.pdb','.pem') -or $_.Name -eq 'playing-core-probe.dll'}) {throw 'Private profile, debug or probe payload bundled'}
$imports=0
foreach($pe in ($actual | Where-Object {$_.Extension -in @('.exe','.dll')})) {
    $text=& $Dumpbin /dependents $pe.FullName
    if($LASTEXITCODE -ne 0) {throw 'PE inspection failed'}
    foreach($line in $text) {if($line -match '^\s+([\w.-]+\.dll)\s*$') {
        $name=$Matches[1]; ++$imports
        if(Test-Path -LiteralPath (Join-Path $testRoot $name)) {continue}
        if($name -match '^(Qt6|MSVCP140|VCRUNTIME140)') {throw "Missing local dependency: $name"}
        if($name -match '^(api-ms-win-|ext-ms-win-)' -or (Test-Path -LiteralPath (Join-Path "$env:SystemRoot/System32" $name))) {continue}
        throw "Unresolved dependency: $name"
    }}
}
$oldPath=$env:PATH
try {
    # Harness uses SDK; children use System32-only PATH and no Qt overrides.
    $env:PATH=(Join-Path $QtRoot 'bin')+[IO.Path]::PathSeparator+$oldPath
    $harness=Join-Path (Resolve-Path -LiteralPath $BuildRoot).Path 'RelWithDebInfo/playing-companion-executable-tests.exe'
    $result=& $harness (Join-Path $testRoot 'obs-playing-companion.exe') (Join-Path $testRoot 'obs-playing-db.dll') --portable 2>&1
    if($LASTEXITCODE -ne 0) {$result | Set-Content -LiteralPath $Report; throw 'Portable test failed; fixture retained'}
    @("Package SHA256=$((Get-FileHash -LiteralPath $Archive).Hash)","Verified files=$($manifest.Count); inspected imports=$imports",$result,"Extracted=$testRoot","PASS: local Qt/CRT/platform/TLS; sanitized child PATH; normal exits; loopback only") | Set-Content -LiteralPath $Report -Encoding utf8
    Get-Content -LiteralPath $Report
} finally {$env:PATH=$oldPath}
