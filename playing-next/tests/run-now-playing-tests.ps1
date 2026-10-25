param([Parameter(Mandatory=$true)][string]$ObsRuntimeRoot,[string]$BuildRoot='work/core-phase08h-build',[string]$Configuration='RelWithDebInfo')
$ErrorActionPreference='Stop'
$runtime=(Resolve-Path -LiteralPath $ObsRuntimeRoot).Path
$binary=Join-Path $runtime 'bin/64bit'
if(-not (Test-Path -LiteralPath (Join-Path $binary 'obs.dll'))){throw 'Select an OBS runtime root containing bin/64bit/obs.dll'}
$test=(Resolve-Path -LiteralPath (Join-Path $BuildRoot "$Configuration/playing-now-playing-publisher-tests.exe")).Path
$previousPath=$env:PATH
try {
 $env:PATH=$binary+';'+$previousPath
 & $test
 if($LASTEXITCODE -ne 0){throw "Public snapshot tests failed: $LASTEXITCODE"}
} finally {$env:PATH=$previousPath}
