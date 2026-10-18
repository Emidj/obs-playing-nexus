param([string]$Installer='work/phase28-release/OBS-Playing-Next-Setup-1.0.0-rc1.exe',[string]$WorkRoot='work/phase28-installed-companion',[string]$EvidencePrefix='phase28-installed-companion')
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$root=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
if(-not $root.StartsWith((Join-Path $workspace 'work')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $root)){throw 'New fixture under work required'}
New-Item -ItemType Directory $root|Out-Null
$exe=(Resolve-Path $Installer).Path
$app=Join-Path $root 'Companion DJ'
$install=Start-Process $exe -ArgumentList @('/install','/role','companion','/target',('"'+$app+'"'),'/no-registry','/log',('"'+$root+'/install-error.txt"')) -PassThru -WindowStyle Hidden
$install.WaitForExit();if($install.ExitCode -ne 0){throw 'Companion install failed'}
$oldPath=$env:PATH
try{
 $env:PATH='C:/NEXUS-Development/Desktop/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64/bin;'+$oldPath
 $result=& (Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/playing-companion-executable-tests.exe') (Join-Path $app 'obs-playing-companion.exe') (Join-Path $app 'obs-playing-db.dll') --portable 2>&1
 if($LASTEXITCODE -ne 0){throw 'Installed Companion lifecycle failed'}
 $result|Set-Content (Join-Path $workspace "outputs/$EvidencePrefix.txt")
}finally{$env:PATH=$oldPath}
# Invoke the installed uninstaller, including relocation and parent-exit wait.
$remove=Start-Process (Join-Path $app 'setup/Uninstall.exe') -ArgumentList @('/uninstall','/role','companion','/target',('"'+$app+'"'),'/no-registry','/log',('"'+$root+'/uninstall-error.txt"')) -PassThru -WindowStyle Hidden
$remove.WaitForExit();if($remove.ExitCode -ne 0){throw 'Uninstaller bootstrap failed'}
$deadline=[DateTime]::UtcNow.AddSeconds(15)
while((Test-Path (Join-Path $app 'setup/state.json')) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
if(Test-Path (Join-Path $root 'uninstall-error.txt')){throw (Get-Content (Join-Path $root 'uninstall-error.txt') -Raw)}
if((Test-Path (Join-Path $app 'obs-playing-companion.exe')) -or (Test-Path (Join-Path $app 'setup/Uninstall.exe')) -or (Test-Path (Join-Path $app 'setup/state.json'))){throw 'Installed uninstaller failed to remove owned executables/state'}
Add-Content (Join-Path $workspace "outputs/$EvidencePrefix.txt") 'PASS installed uninstaller relocation and owned-file removal'
Write-Output 'PASS installed Companion and self-uninstaller'
