param([string]$Installer='work/installer110-dev.exe',[string]$PreviousInstaller='outputs/OBS-Playing-NEXUS-Setup-1.0.1.exe',[string]$WorkRoot='work/installer110-modern-tests',[string]$Obs33='work/obs-31-33-study-20261028/33.0.0/bin/64bit/obs64.exe',[string]$Obs32='tests/obs32/32.2.2/bin/64bit/obs64.exe',[string]$Obs31='work/obs-31-33-study-20261028/31.1.2/bin/64bit/obs64.exe')
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path "$PSScriptRoot/../..").Path
$root=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
if(-not $root.StartsWith((Join-Path $workspace 'work')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $root)){throw 'New fixture under work required'}
New-Item -ItemType Directory $root|Out-Null
$assembly=[Reflection.Assembly]::LoadFrom((Resolve-Path $Installer).Path);$engine=$assembly.GetType('Engine');$modern=$assembly.GetType('ModernInstaller');$flags=[Reflection.BindingFlags]'NonPublic,Static'
$common=Join-Path $root 'ProgramData fixture/obs-studio/plugins';$modern.GetField('CommonRoot',$flags).SetValue($null,$common)
function Call($type,$name,[object[]]$values){try{[object[]]$argsCopy=$values.Clone();for($i=0;$i -lt $argsCopy.Length;$i++){$argsCopy[$i]=$values[$i].PSObject.BaseObject};$type.GetMethod($name,$flags).Invoke($null,$argsCopy)}catch{throw ("Method "+$name+"; args="+$values.Count+"; "+$_.Exception.InnerException)}}
function E($name,[object[]]$values){Call $engine $name $values}
function M($name,[object[]]$values){Call $modern $name $values}
$checks=New-Object 'Collections.Generic.List[string]'
function Check($ok,$why){if(-not $ok){throw "FAIL $why"};$checks.Add("PASS $why")}
function Reject([scriptblock]$action,$why){$rejected=$false;try{&$action|Out-Null}catch{$rejected=$true};Check $rejected $why}
function Hash($file){(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}
function Fixture($name,$obs,[bool]$portable=$false){$dir=Join-Path $root $name;New-Item -ItemType Directory "$dir/bin/64bit","$dir/config/obs-studio" -Force|Out-Null;Copy-Item (Resolve-Path $obs).Path "$dir/bin/64bit/obs64.exe";[IO.File]::WriteAllText("$dir/config/obs-studio/user.txt",'keep scene/profile fixture');if($portable){[IO.File]::WriteAllText("$dir/portable_mode.txt",'')};return $dir}
$standard=Fixture 'OBS 33 standard' $Obs33
$portable=Fixture 'OBS 33 portátil ñ' $Obs33 $true
$legacy31=Fixture 'OBS 31' $Obs31 $true
$legacy32=Fixture 'OBS 32' $Obs32
E Validate @('plugin',$legacy31)|Out-Null;Check $true 'OBS 31 admitted'
E Validate @('plugin',$legacy32)|Out-Null;Check $true 'OBS 32 admitted'
Check (-not (M Applies @($legacy31)) -and -not (M Applies @($legacy32))) 'OBS 31/32 retain legacy layout'
$unsupported=Fixture 'Unsupported OBS' $Obs32
$futureSource=Join-Path $root 'future.cs';[IO.File]::WriteAllText($futureSource,'using System.Reflection;[assembly:AssemblyFileVersion("34.0.0.0")]class Future{static void Main(){}}')
& "$env:WINDIR/Microsoft.NET/Framework64/v4.0.30319/csc.exe" /nologo /target:winexe /platform:x64 ("/out:"+(Join-Path $unsupported 'bin/64bit/obs64.exe')) $futureSource
if($LASTEXITCODE){throw 'Future version fixture compile failed'}
Reject {E Validate @('plugin',$unsupported)} 'unvalidated future OBS major is rejected'

Check (M Applies @($standard)) 'OBS 33 detected from executable version'
Check ((M Root @($standard)) -eq (Join-Path $common 'obs-playing-next')) 'OBS 33 standard selects ProgramData fixture'
Check ((M Root @($portable)) -eq (Join-Path $portable 'plugins/obs-playing-next')) 'OBS 33 portable selects its own plugins folder'
$configPortable=Fixture 'OBS portable config without marker' $Obs33
[IO.File]::WriteAllText("$configPortable/config/obs-studio/global.ini",'[General]')
Check ((M Root @($configPortable)) -eq (Join-Path $configPortable 'plugins/obs-playing-next')) 'portable local config detected without marker'

$sceneHash=Hash "$standard/config/obs-studio/user.txt"
E Install @('plugin',$standard,$false,0)|Out-Null
$modernRoot=Join-Path $common 'obs-playing-next';$statePath=Join-Path $modernRoot 'setup/state.json';$state=Get-Content $statePath -Raw|ConvertFrom-Json
Check ($state.Layout -eq 'modern-v1' -and $state.Version -eq '1.1.0') 'modern state has version/layout'
foreach($file in $state.Files){Check ((Hash (Join-Path $modernRoot $file.Path)) -eq $file.Hash) ('modern installed hash '+$file.Path)}
Check (Test-Path "$modernRoot/obs-playing-db.dll") 'database dependency is next to module'
Check (Test-Path "$modernRoot/playing-qt-plugins/tls/qschannelbackend.dll") 'TLS dependency path retained'
Check (-not (Test-Path "$standard/obs-plugins/64bit/obs-playing-next.dll")) 'standard OBS 33 does not write legacy DLL'
Check ((Hash "$standard/config/obs-studio/user.txt") -eq $sceneHash) 'standard scenes/config untouched'
$beforeState=Hash $statePath;$beforeDll=Hash "$modernRoot/obs-playing-next.dll"
$stateBytes=[IO.File]::ReadAllBytes($statePath);$badState=Get-Content $statePath -Raw|ConvertFrom-Json;$badState.Files[0].Path='..\escape.dll';$badState|ConvertTo-Json -Depth 8|Set-Content $statePath -Encoding UTF8
Reject {E Uninstall @('plugin',$standard,$false)} 'modern state path traversal rejected before deletion'
Check ((Hash "$modernRoot/obs-playing-next.dll") -eq $beforeDll) 'invalid state leaves installed module untouched'
[IO.File]::WriteAllBytes($statePath,$stateBytes)

Reject {E Install @('plugin',$standard,$false,3)} 'modern injected rollback rejects'
Check ((Hash $statePath) -eq $beforeState -and (Hash "$modernRoot/obs-playing-next.dll") -eq $beforeDll) 'modern rollback restores DLL/state'
E Install @('plugin',$standard,$false,0)|Out-Null;Check $true 'modern reinstall succeeds'
E Install @('plugin',$portable,$false,0)|Out-Null
$portableRoot=Join-Path $portable 'plugins/obs-playing-next'
Check (Test-Path "$portableRoot/obs-playing-next.dll") 'portable modern module installed'
Check ((Hash "$modernRoot/obs-playing-next.dll") -eq $beforeDll) 'portable installation does not touch shared standard module'
[IO.File]::WriteAllText("$portableRoot/user-extra.txt",'foreign file')
E Uninstall @('plugin',$portable,$false)|Out-Null
Check (-not (Test-Path "$portableRoot/obs-playing-next.dll") -and (Test-Path "$portableRoot/user-extra.txt")) 'portable uninstall removes only owned files'
Check (Test-Path "$modernRoot/obs-playing-next.dll") 'portable uninstall leaves standard module intact'
E Uninstall @('plugin',$standard,$false)|Out-Null
Check (-not (Test-Path "$modernRoot/obs-playing-next.dll")) 'standard modern module uninstalled'
# Install the real prior release into a 32 fixture, then simulate an OBS upgrade.
$migration=Fixture 'OBS migration 32 to 33 ñ' $Obs32 $true
$old=[Reflection.Assembly]::LoadFrom((Resolve-Path $PreviousInstaller).Path);$oldEngine=$old.GetType('Engine')
Call $oldEngine Install @('plugin',$migration,$false,0)|Out-Null
$oldDll="$migration/obs-plugins/64bit/obs-playing-next.dll";$oldState="$migration/data/obs-plugins/obs-playing-next/setup/state.json";$oldUninstall="$migration/data/obs-plugins/obs-playing-next/setup/Uninstall.exe"
$oldHash=Hash $oldDll;$oldStateHash=Hash $oldState;$oldUninstallHash=Hash $oldUninstall
Copy-Item (Resolve-Path $Obs33).Path "$migration/bin/64bit/obs64.exe" -Force
$migrationRoot=Join-Path $migration 'plugins/obs-playing-next'
$originalBytes=[IO.File]::ReadAllBytes($oldDll);[IO.File]::AppendAllText($oldDll,'modified by user')
Reject {E Install @('plugin',$migration,$false,0)} 'modified registered legacy module blocks migration'
Check (-not (Test-Path "$migrationRoot/setup/state.json")) 'rejected migration has no committed modern state'
[IO.File]::WriteAllBytes($oldDll,$originalBytes)
$payload=M Payload @();$failAfter=$payload.Count+2
Reject {E Install @('plugin',$migration,$false,[int]$failAfter)} 'injected failure after legacy deletion rolls migration back'
Check ((Hash $oldDll) -eq $oldHash -and (Hash $oldState) -eq $oldStateHash -and (Hash $oldUninstall) -eq $oldUninstallHash) 'migration rollback restores previous module/state/uninstaller'
Check (-not (Test-Path "$migrationRoot/obs-playing-next.dll")) 'migration rollback removes new payload'
E Install @('plugin',$migration,$false,0)|Out-Null
Check (-not (Test-Path $oldDll) -and (Hash $oldState) -eq $oldStateHash -and (Hash $oldUninstall) -eq $oldUninstallHash) 'migration retires legacy module while retaining version metadata for a downgrade'
$state=Get-Content "$migrationRoot/setup/state.json" -Raw|ConvertFrom-Json
Check ($state.Migrated.Count -eq 3) 'migration records three recoverable originals'
foreach($file in $state.Migrated){Check ((Hash (Join-Path "$migrationRoot/setup" $file.Backup)) -eq $file.Hash) ('migration backup '+$file.Path)}
$backup=Join-Path "$migrationRoot/setup" $state.Migrated[0].Backup;$bytes=[IO.File]::ReadAllBytes($backup);[IO.File]::WriteAllText($backup,'tampered')
Reject {E Uninstall @('plugin',$migration,$false)} 'corrupt migration backup blocks uninstall'
Check (Test-Path "$migrationRoot/obs-playing-next.dll") 'corrupt-backup rejection preserves installed DLL'
[IO.File]::WriteAllBytes($backup,$bytes)
E Uninstall @('plugin',$migration,$false)|Out-Null
Check (-not (Test-Path "$migrationRoot/obs-playing-next.dll") -and -not (Test-Path $oldDll)) 'modern uninstall does not resurrect legacy module'
Check (Test-Path $backup) 'previous versions remain recoverable after uninstall'
Copy-Item (Resolve-Path $Obs32).Path "$migration/bin/64bit/obs64.exe" -Force
E Install @('plugin',$migration,$false,0)|Out-Null
Check (Test-Path $oldDll) 'returning to OBS 32 installs the legacy layout again'
E Uninstall @('plugin',$migration,$false)|Out-Null
# Old ProgramData bin/64bit layout also migrates with an exact backup.
$oldBin="$modernRoot/bin/64bit/obs-playing-next.dll";New-Item -ItemType Directory (Split-Path $oldBin) -Force|Out-Null;[IO.File]::WriteAllText($oldBin,'legacy ProgramData fixture');$binHash=Hash $oldBin
E Install @('plugin',$standard,$false,0)|Out-Null
$state=Get-Content $statePath -Raw|ConvertFrom-Json
Check (-not (Test-Path $oldBin)) 'old ProgramData bin/64bit module retired'
$record=$state.Migrated|Where-Object {$_.Location -eq 'plugin'}
Check ((Hash (Join-Path "$modernRoot/setup" $record.Backup)) -eq $binHash) 'old ProgramData bytes retained in backup'
E Uninstall @('plugin',$standard,$false)|Out-Null
# One shared registration across different standard OBS 33 roots.
E Install @('plugin',$standard,$true,0)|Out-Null
$second=Fixture 'Second standard OBS 33' $Obs33
E Install @('plugin',$second,$true,0)|Out-Null
[object[]]$hashArgs=New-Object object[] 1;$hashArgs[0]=[Text.Encoding]::UTF8.GetBytes($modernRoot.ToUpperInvariant())
$key='Software\Microsoft\Windows\CurrentVersion\Uninstall\OBSPlayingNext-plugin-'+(E Hash $hashArgs).Substring(0,12)
$entry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($key)
Check ($entry -ne $null -and $entry.GetValue('InstallLocation') -eq $modernRoot -and $entry.GetValue('DisplayVersion') -eq '1.1.0') 'shared registration uses physical plugin root'
Check ($entry.GetValue('UninstallString').Contains('/modern')) 'registered uninstaller preserves modern layout'
$entry.Dispose();E Uninstall @('plugin',$second,$true)|Out-Null
Check ([Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($key) -eq $null) 'modern registry entry removed'
# Real installed uninstaller relocates itself, using only this portable fixture.
E Install @('plugin',$portable,$false,0)|Out-Null
$process=Start-Process "$portableRoot/setup/Uninstall.exe" -ArgumentList @('/uninstall','/role','plugin','/target',('"'+$portable+'"'),'/no-registry','/log',('"'+$root+'/uninstall-error.txt"')) -WindowStyle Hidden -PassThru
$process.WaitForExit();Check ($process.ExitCode -eq 0) 'modern self-uninstaller bootstrap exits normally'
$deadline=[DateTime]::UtcNow.AddSeconds(15)
while((Test-Path "$portableRoot/setup/state.json") -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
if(Test-Path "$root/uninstall-error.txt"){throw (Get-Content "$root/uninstall-error.txt" -Raw)}
Check (-not (Test-Path "$portableRoot/setup/state.json") -and -not (Test-Path "$portableRoot/obs-playing-next.dll")) 'relocated modern uninstaller completes owned-file removal'
Check (Test-Path "$portableRoot/user-extra.txt") 'self-uninstall retains unrelated files'
$checks|Set-Content (Join-Path $root 'modern-installer-tests.txt')
Write-Output ("PASS modern installer checks="+$checks.Count)
