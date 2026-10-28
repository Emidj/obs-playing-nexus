param([string]$Installer='work/phase28-release/OBS-Playing-Next-Setup-1.0.0-rc1.exe',[string]$WorkRoot='work/phase28-installed-obs',[string]$Version='32.2.2',[string]$RuntimeRoot='',[string]$EvidencePrefix='phase28-installed-obs')
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$root=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
if(-not $root.StartsWith((Join-Path $workspace 'work')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $root)){throw 'New work root required'}
$tests=Join-Path $root 'tests/obs32';$runtime=Join-Path $tests $Version
New-Item -ItemType Directory $runtime -Force|Out-Null
Copy-Item (Join-Path $workspace 'obs-playing') (Join-Path $root 'obs-playing') -Recurse
$original=Join-Path $workspace "tests/obs32/$Version"
if($RuntimeRoot){$original=(Resolve-Path -LiteralPath $RuntimeRoot).Path;if(-not $original.StartsWith((Join-Path $workspace 'work')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Study runtime must be under work'}}
if((Get-Item "$original/bin/64bit/obs64.exe").VersionInfo.ProductVersion -ne $Version){throw 'Runtime version mismatch'}
foreach($dir in @('bin','data','obs-plugins','core')){if(Test-Path (Join-Path $original $dir)){Copy-Item (Join-Path $original $dir) (Join-Path $runtime $dir) -Recurse}}

# Keep the modules observed in the prior --only-bundled-plugins control, plus
# their CEF dependencies. Move extra DLL copies aside; original runtime is intact.
$control=Get-Content (Join-Path $workspace 'outputs/phase27-native-final-32.2.2.log') -Raw
$section=[regex]::Match($control,'(?s)Loaded Modules:(.*?)---').Groups[1].Value
$allowed=@([regex]::Matches($section,'(?m)^.*?\s{5}([\w-]+\.dll)\s*$')|ForEach-Object{$_.Groups[1].Value})+@('libcef.dll','libEGL.dll','libGLESv2.dll','chrome_elf.dll')
if($allowed.Count -lt 20){throw 'Missing verified bundled-module list'}
$excluded=Join-Path $root 'excluded-extra-module-copies';New-Item -ItemType Directory $excluded|Out-Null
foreach($file in @(if(Test-Path (Join-Path $runtime 'obs-plugins/64bit')){Get-ChildItem (Join-Path $runtime 'obs-plugins/64bit') -File -Filter '*.dll'})){
 if($file.Name -notin $allowed){if(-not $file.FullName.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Out-of-fixture move'};Move-Item -LiteralPath $file.FullName -Destination (Join-Path $excluded $file.Name)}
}
foreach($file in @('prepare-portable.ps1','heartbeat.lua')){Copy-Item (Join-Path $workspace "work/audit-phase01/tests/obs32/$file") $tests}
& (Join-Path $tests 'prepare-portable.ps1') -Version $Version
& (Join-Path $workspace 'playing-next/tests/configure-obs-smoke.ps1') -PortableRoot $runtime
$script=Get-Content (Join-Path $workspace 'playing-next/tests/obs-source-lifecycle.lua') -Raw
$script=$script -replace '(?s)local ffi = require.*?local done = false','local done = false'
$script=$script -replace '(?s)    local module = ffi.new.*?    local source =','    local source ='
[IO.File]::WriteAllText((Join-Path $tests 'installed-source.lua'),$script,[Text.UTF8Encoding]::new($false))
$profile=Get-Content (Join-Path $workspace 'playing-next/tests/obs-profile.lua') -Raw
$profile=$profile.Replace('local loaded=false','local loaded=true')
[IO.File]::WriteAllText((Join-Path $tests 'installed-video.lua'),$profile,[Text.UTF8Encoding]::new($false))
Copy-Item (Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/playing-obs-profile-probe.dll') $tests
[IO.File]::WriteAllText((Join-Path $tests 'profile-owned.flag'),'isolated fixture')
@{cases=@(@{name='installed_neon';kind='static';profile=1;count=1;warmup=3;seconds=5;changes=$true;artwork=$false})}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $tests 'profile-config.json')
$scenePath=Join-Path $runtime 'config/obs-studio/basic/scenes/Compatibility.json'
$scene=Get-Content $scenePath -Raw|ConvertFrom-Json -AsHashtable
$scene.modules.'scripts-tool'+=@{path=(Join-Path $tests 'installed-source.lua').Replace('\','/');settings=@{}}
$scene.modules.'scripts-tool'+=@{path=(Join-Path $tests 'installed-video.lua').Replace('\','/');settings=@{}}
[IO.File]::WriteAllText($scenePath,($scene|ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
$sceneHash=(Get-FileHash $scenePath).Hash
$setup=Start-Process -FilePath (Resolve-Path $Installer).Path -ArgumentList @('/install','/role','plugin','/target',('"'+$runtime+'"'),'/no-registry','/log',('"'+(Join-Path $root 'setup-error.txt')+'"')) -WindowStyle Hidden -PassThru
$setup.WaitForExit();if($setup.ExitCode -ne 0){throw 'Installer failed'}
if((Get-FileHash $scenePath).Hash -ne $sceneHash){throw 'Installer modified scenes'}
$installedModulePath=Join-Path $runtime $(if([int]$Version.Split('.')[0] -ge 33){'plugins/obs-playing-next/obs-playing-next.dll'}else{'obs-plugins/64bit/obs-playing-next.dll'})
$installedModuleHash=(Get-FileHash $installedModulePath).Hash

$exe=Join-Path $runtime 'bin/64bit/obs64.exe'
$owned=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check') -WindowStyle Hidden -PassThru
if(-not $owned.WaitForExit(120000)){$owned.Kill();throw 'OBS timeout (not PASS)'}
if($owned.ExitCode -ne 0){throw 'OBS abnormal exit'}
$result=Get-Content (Join-Path $tests 'source-lifecycle-result.txt') -Raw
if(-not $result.StartsWith('PASS native source')){throw 'Installed source lifecycle missing'}
$videos=@(Get-ChildItem $tests -File -Filter 'video-*.json'|ForEach-Object{Get-Content $_.FullName -Raw|ConvertFrom-Json})
if(-not ($videos|Where-Object {$_.video.nonBlackSamples -gt 0})){throw 'Installed native scene did not produce visible RAW frames'}
if(-not (Get-Content (Join-Path $tests 'profile-result.txt') -Raw).StartsWith('PASS')){throw 'Installed video fixture failed'}
$log=Get-ChildItem (Join-Path $runtime 'config/obs-studio/logs') -File|Sort-Object LastWriteTime -Descending|Select-Object -First 1
$logText=Get-Content $log.FullName -Raw
if($logText -notmatch 'obs-playing-next.dll' -or $logText -notmatch 'Native DJ source registered'){throw 'Normal module discovery not confirmed'}
if($logText -match '\[playing-next\] (Source configuration failed|Render failed)'){throw 'Installed plugin configuration/render failure'}
Copy-Item $log.FullName (Join-Path $workspace ("outputs/$EvidencePrefix.log"))
$uninstall=Start-Process -FilePath (Resolve-Path $Installer).Path -ArgumentList @('/uninstall','/role','plugin','/target',('"'+$runtime+'"'),'/no-registry','/log',('"'+(Join-Path $root 'uninstall-error.txt')+'"')) -WindowStyle Hidden -PassThru
$uninstall.WaitForExit();if($uninstall.ExitCode -ne 0){throw 'Uninstaller failed'}
$installedModule=Join-Path $runtime $(if([int]$Version.Split('.')[0] -ge 33){'plugins/obs-playing-next/obs-playing-next.dll'}else{'obs-plugins/64bit/obs-playing-next.dll'})
if(Test-Path $installedModule){throw 'Installed DLL remained'}
@{InstallerSHA256=(Get-FileHash $Installer).Hash;ModuleSHA256=$installedModuleHash;Version=$Version;NormalDiscovery=$true;ExplicitModuleLoad=$false;Exit=0;VisibleRawFrames=$true;VideoSamples=$videos.Count;InstallPreservedScene=$true;UninstallRemovedModule=$true;Result=$result}|ConvertTo-Json|Set-Content (Join-Path $workspace ("outputs/$EvidencePrefix.json"))
Write-Output 'PASS installed OBS normal discovery, native lifecycle and uninstall'
