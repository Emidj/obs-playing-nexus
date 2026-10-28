param([string]$Installer='work/phase28-final/OBS-Playing-Next-Setup-1.0.0-rc1.exe',[string]$WorkRoot='work/phase28-tests',[string]$ObsExe='work/phase27-native-smoke/tests/obs32/32.2.2/bin/64bit/obs64.exe')
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$root=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
if(-not $root.StartsWith((Join-Path $workspace 'work')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $root)){throw 'New work root required'}
New-Item -ItemType Directory $root|Out-Null
$assembly=[Reflection.Assembly]::LoadFrom((Resolve-Path $Installer).Path)
$engine=$assembly.GetType('Engine')
$desktop=Join-Path $root 'Desktop fixture';New-Item -ItemType Directory $desktop|Out-Null
$engine.GetField('DesktopRoot',[Reflection.BindingFlags]'NonPublic,Static').SetValue($null,$desktop)
function Invoke-Engine($name,[object[]]$parameters){$m=$engine.GetMethod($name,[Reflection.BindingFlags]'NonPublic,Static');try{[object[]]$raw=$parameters.Clone();for($i=0;$i -lt $raw.Length;$i++){$raw[$i]=$parameters[$i].PSObject.BaseObject};$m.Invoke($null,$raw)}catch{throw $_.Exception.InnerException}}
function Get-FileHash($Path){[pscustomobject]@{Hash=(Invoke-Engine HashFile @((Resolve-Path $Path).Path))}}
$checks=New-Object 'Collections.Generic.List[string]'
function Check($condition,$label){if(-not $condition){throw "FAIL $label"};$checks.Add("PASS $label")}
function MustReject([scriptblock]$action,$label){$failed=$false;try{& $action|Out-Null}catch{$failed=$true};Check $failed $label}
$elevation=$assembly.GetType('Elevation')
$command=$elevation.GetMethod('Command',[Reflection.BindingFlags]'NonPublic,Static')
function Invoke-Elevation([object[]]$parameters){[object[]]$raw=$parameters.Clone();for($i=0;$i -lt $raw.Length;$i++){$raw[$i]=$parameters[$i].PSObject.BaseObject};$command.Invoke($null,$raw)}
$request=Invoke-Elevation @('plugin',(Join-Path $root 'OBS prueba'),$false,(Join-Path $root 'reply.txt'))
Check ($request.UseShellExecute -and $request.Verb -eq 'runas') 'automatic elevation requests Windows UAC'
Check ($request.FileName -eq (Resolve-Path $Installer).Path) 'elevation uses this exact installer'
Check ($request.Arguments.Contains('/install /role plugin') -and $request.Arguments.Contains('/elevated') -and $request.Arguments.Contains('"'+(Join-Path $root 'OBS prueba')+'"')) 'elevation preserves action and quoted destination with retry marker'
$request=Invoke-Elevation @('plugin',(Join-Path $root 'OBS prueba'),$true,(Join-Path $root 'reply.txt'))
Check ($request.Arguments.StartsWith('/uninstall /role plugin')) 'plugin uninstall supports automatic elevation'
MustReject {Invoke-Elevation @('companion',(Join-Path $root 'Companion'),$false,(Join-Path $root 'reply.txt'))} 'Companion does not request administrator by default'
Add-Type -TypeDefinition 'public static class Rc3FileLease { public static System.Threading.Timer ReleaseAfter(System.IO.FileStream stream) { return new System.Threading.Timer(delegate(object unused) { stream.Dispose(); }, null, 150, System.Threading.Timeout.Infinite); } }'
$lockedPath=Join-Path $root 'locked-fixture.txt'
[IO.File]::WriteAllText($lockedPath,'before')
$locked=[IO.File]::Open($lockedPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
$releaseTimer=[Rc3FileLease]::ReleaseAfter($locked)
try{
 [object[]]$atomicParameters=New-Object object[] 2
 $atomicParameters[0]=$lockedPath;$atomicParameters[1]=[Text.Encoding]::UTF8.GetBytes('after')
 Invoke-Engine Atomic $atomicParameters|Out-Null
 Check ([IO.File]::ReadAllText($lockedPath) -eq 'after') 'atomic replace retries a temporary file lock'
}finally{$releaseTimer.Dispose();$locked.Dispose()}
$locked=[IO.File]::Open($lockedPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try{MustReject {Invoke-Engine Atomic $atomicParameters} 'persistent file lock fails after bounded retries';Check ([IO.File]::ReadAllText($lockedPath) -eq 'after') 'persistent lock preserves original file'}finally{$locked.Dispose()}
$plugin=Join-Path $root 'OBS portátil áé'
New-Item -ItemType Directory "$plugin/bin/64bit","$plugin/obs-plugins/64bit","$plugin/config/obs-studio/basic/scenes" -Force|Out-Null
Copy-Item (Resolve-Path $ObsExe) "$plugin/bin/64bit/obs64.exe"
$scene="$plugin/config/obs-studio/basic/scenes/Usuario.json";[IO.File]::WriteAllText($scene,'{"sources":[{"name":"usuario"}]}')
$sceneHash=(Get-FileHash $scene).Hash
$db="$plugin/obs-plugins/64bit/obs-playing-db.dll";[IO.File]::WriteAllText($db,'previous plugin db')
$previousHash=(Get-FileHash $db).Hash
$user="$plugin/obs-plugins/64bit/user-plugin.dll";[IO.File]::WriteAllText($user,'user plugin')
Invoke-Engine Install @('plugin',$plugin,$false,0)|Out-Null
$statePath="$plugin/data/obs-plugins/obs-playing-next/setup/state.json"
$state=Get-Content $statePath -Raw|ConvertFrom-Json
Check ($state.Files.Count -ge 10) 'fresh plugin installation Unicode path'
foreach($f in $state.Files){Check ((Get-FileHash (Join-Path $plugin $f.Path)).Hash -eq $f.Hash) ('installed hash '+$f.Path)}
Check ((Get-FileHash $scene).Hash -eq $sceneHash) 'scenes untouched'
Check (Test-Path $user) 'unrelated plugin preserved'
$firstBackups=@(Get-ChildItem "$plugin/data/obs-plugins/obs-playing-next/setup/backups" -Directory).Count
Invoke-Engine Install @('plugin',$plugin,$false,0)|Out-Null
Check (@(Get-ChildItem "$plugin/data/obs-plugins/obs-playing-next/setup/backups" -Directory).Count -gt $firstBackups) 'reinstall preserves old version backup'
$dll="$plugin/obs-plugins/64bit/obs-playing-next.dll";$dllHash=(Get-FileHash $dll).Hash
$stateHash=(Get-FileHash $statePath).Hash
MustReject {Invoke-Engine Install @('plugin',$plugin,$false,3)} 'injected install failure rolls back'
Check ((Get-FileHash $dll).Hash -eq $dllHash -and (Get-FileHash $statePath).Hash -eq $stateHash) 'rollback restores payload and state'
$state=Get-Content $statePath -Raw|ConvertFrom-Json
$original=$state.Files|Where-Object {$_.Path -eq 'obs-plugins\64bit\obs-playing-db.dll'}
$backup=Join-Path "$plugin/data/obs-plugins/obs-playing-next/setup" $original.Original
$backupBytes=[IO.File]::ReadAllBytes($backup)
[IO.File]::WriteAllText($backup,'tampered')
MustReject {Invoke-Engine Uninstall @('plugin',$plugin,$false)} 'corrupt original backup refuses uninstall before removing payload'
Check ((Get-FileHash $dll).Hash -eq $dllHash) 'backup rejection leaves module intact'
[IO.File]::WriteAllBytes($backup,$backupBytes)
Invoke-Engine Uninstall @('plugin',$plugin,$false)|Out-Null
Check ((Get-FileHash $db).Hash -eq $previousHash) 'uninstall restores previous DB'
Check (-not (Test-Path $dll)) 'owned native module removed'
Check ((Get-FileHash $scene).Hash -eq $sceneHash -and (Test-Path $user)) 'uninstall preserves scenes and other plugin'
Invoke-Engine Uninstall @('plugin',$plugin,$false)|Out-Null
Check $true 'repeated uninstall'
MustReject {Invoke-Engine Install @('plugin',(Join-Path $root 'invalid-obs'),$false,0)} 'invalid OBS rejected'
MustReject {Invoke-Engine Child @($plugin,'../outside.txt')} 'path traversal rejected'
$companion=Join-Path $root 'Companion DJ ñ'
Invoke-Engine Install @('companion',$companion,$false,0)|Out-Null
Check (Test-Path "$companion/obs-playing-companion.exe") 'Companion installed'
Check (-not (Test-Path "$companion/Abrir receptor de pruebas.cmd")) 'receiver test launcher absent'
Check (Test-Path "$companion/Abrir OBS Playing Companion.cmd") 'final Companion launcher'
$profile="$companion/profile.pcs";[IO.File]::WriteAllText($profile,'private user fixture')
$gui="$companion/GUIA.txt";[IO.File]::AppendAllText($gui,'user customization')
MustReject {Invoke-Engine Install @('companion',$companion,$false,0)} 'modified owned file prevents overwrite'
$kept=Invoke-Engine Uninstall @('companion',$companion,$false)
Check ($kept.Contains('GUIA.txt') -and (Test-Path $gui)) 'modified file kept on uninstall'
Check (Test-Path $profile) 'profile preserved'
Check (-not (Test-Path "$companion/obs-playing-companion.exe")) 'Companion executable removed'
$registered=Join-Path $root 'Registered Companion'
Invoke-Engine Install @('companion',$registered,$true,0)|Out-Null
Check (Test-Path (Join-Path $desktop 'OBS Playing Companion.lnk')) 'desktop shortcut created'
$shellCheck=New-Object -ComObject WScript.Shell
$linkCheck=$shellCheck.CreateShortcut((Join-Path $desktop 'OBS Playing Companion.lnk'))
Check ($linkCheck.TargetPath -eq (Join-Path $registered 'obs-playing-companion.exe') -and $linkCheck.WorkingDirectory -eq $registered) 'desktop shortcut opens installed Companion'
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($linkCheck)|Out-Null
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($shellCheck)|Out-Null
[object[]]$keyArguments=New-Object object[] 1
$keyArguments[0]=[Text.Encoding]::UTF8.GetBytes($registered.ToUpperInvariant())
$keySuffix=Invoke-Engine Hash $keyArguments
$key='Software\Microsoft\Windows\CurrentVersion\Uninstall\OBSPlayingNext-companion-'+$keySuffix.Substring(0,12)
$entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($key)
Check ($entry -ne $null -and $entry.GetValue('InstallLocation') -eq $registered) 'per-user uninstall entry correct'
Check ($entry.GetValue('DisplayName') -eq 'OBS Playing NEXUS Companion') 'NEXUS product name registered'
if($entry){$entry.Dispose()}
Invoke-Engine Uninstall @('companion',$registered,$true)|Out-Null
Check (-not (Test-Path (Join-Path $desktop 'OBS Playing Companion.lnk'))) 'owned desktop shortcut removed'
$entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($key)
Check ($entry -eq $null) 'own uninstall registry entry removed'
if($entry){$entry.Dispose()}
$checks|Set-Content (Join-Path $root 'installer-tests.txt')
Write-Output ("PASS installer checks="+$checks.Count)
