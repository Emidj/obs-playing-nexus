param([string]$WorkRoot='work/phase18b-builtin-close',[ValidateRange(1,40)][int]$Rounds=12,[switch]$Legacy)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
$destination=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
if(-not $destination.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $destination)){throw 'Require new isolated directory under work'}
$runtime=Join-Path $destination '32.1.2'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
foreach($folder in @('bin','data','obs-plugins')){Copy-Item -LiteralPath (Join-Path $workspace "tests/obs32/32.1.2/$folder") -Destination $runtime -Recurse}
# No Playing Next DLL in either independent control.
if($Legacy){Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing') -Destination $destination -Recurse}
$config=Join-Path $runtime 'config/obs-studio'
New-Item -ItemType Directory -Path (Join-Path $config 'basic/scenes'),(Join-Path $config 'basic/profiles/Compatibility') -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $runtime 'portable_mode.txt'),'')
$ini="[General]`nFirstRun=true`nEnableAutoUpdates=false`nConfirmOnExit=false`n[Basic]`nSceneCollection=Compatibility`nSceneCollectionFile=Compatibility`nProfile=Compatibility`nProfileDir=Compatibility`n"
foreach($name in @('global.ini','user.ini')){[IO.File]::WriteAllText((Join-Path $config $name),$ini)}
[IO.File]::WriteAllText((Join-Path $config 'basic/profiles/Compatibility/basic.ini'),"[General]`nName=Shutdown isolation`n[Video]`nBaseCX=1280`nBaseCY=720`nOutputCX=1280`nOutputCY=720`nFPSCommon=30`n[Audio]`nSampleRate=48000`nChannelSetup=Stereo`n")
$sources=@();$items=@()
$presets=@('studio','minimal','oblique','aura','orbit','terminal','rupture','dune','chrome','monolith','portal','cantilever','beacon','laser','matrix','lotus','tide','silk','obsidian','lens','gum','sintonia','trazo')
for($i=0;$i -lt 23;$i++){$name="Native control $i";$id=if($Legacy){'obs_playing_v01'}else{'color_source_v3'};$settings=if($Legacy){@{demo=$true;preset=$presets[$i];program='virtualdj';vdj_mode='auto'}}else{@{width=1280;height=720;color=4294901760}};$sources+=@{name=$name;id=$id;versioned_id=$id;settings=$settings;mixers=0;enabled=$true;flags=0};$items+=@{name=$name;visible=$true;id=$i;pos=@{x=0.0;y=0.0};scale=@{x=1.0;y=1.0};rot=0.0;align=5;bounds_type=0}}
$sources+=@{name='Compatibility';id='scene';versioned_id='scene';settings=@{items=$items;id_counter=23}}
$scene=@{name='Compatibility';current_scene='Compatibility';current_program_scene='Compatibility';scene_order=@(@{name='Compatibility'});sources=$sources;modules=@{}}
if($Legacy){$scene.modules=@{'scripts-tool'=@(@{path=(Join-Path $destination 'obs-playing/obs-playing.lua').Replace('\','/');settings=@{}})}}
$scenePath=Join-Path $config 'basic/scenes/Compatibility.json'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class PlayingShutdownControl {
 [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr window,uint message,IntPtr wparam,IntPtr lparam);
 public delegate bool Callback(IntPtr window,IntPtr parameter);
 [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback,IntPtr parameter);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window,StringBuilder text,int count);
 public static IntPtr FindMainWindow(uint process){
  IntPtr found=IntPtr.Zero;
  EnumWindows((window,parameter)=>{uint owner;GetWindowThreadProcessId(window,out owner);if(owner==process){var title=new StringBuilder(256);GetWindowText(window,title,256);if(title.ToString().StartsWith("OBS "))found=window;}return found==IntPtr.Zero;},IntPtr.Zero);
  return found;
 }
}
'@
$rows=[Collections.Generic.List[object]]::new();$exe=Join-Path $runtime 'bin/64bit/obs64.exe'
function Read-OpenLog([string]$path){
    $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $reader=[IO.StreamReader]::new($stream)
    try{return $reader.ReadToEnd()}finally{$reader.Dispose()}
}
for($round=1;$round -le $Rounds;$round++){
    [IO.File]::WriteAllText($scenePath,($scene|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
    $owned=$null;$started=[DateTime]::UtcNow;$normal=$false;$timedOut=$false;$closeSent=$false;$handle=[IntPtr]::Zero
    try{
        $owned=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
        $deadline=[DateTime]::UtcNow.AddSeconds(20);$loaded=$false;$log=$null
        while(-not $owned.HasExited -and [DateTime]::UtcNow -lt $deadline){
            $owned.Refresh();$log=Get-ChildItem (Join-Path $config 'logs') -File -ErrorAction SilentlyContinue | Where-Object LastWriteTimeUtc -ge $started | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
            if($log){$text=Read-OpenLog $log.FullName;$sourceId=if($Legacy){'obs_playing_v01'}else{'color_source_v3'};$loaded=([regex]::Matches($text,"source: 'Native control \d+' \($sourceId\)").Count -eq 23)}
            $handle=[PlayingShutdownControl]::FindMainWindow($owned.Id)
            if($loaded -and $handle -ne [IntPtr]::Zero){break};Start-Sleep -Milliseconds 100
        }
        if($loaded -and $handle -ne [IntPtr]::Zero){
            Start-Sleep -Milliseconds 300
            $closeSent=[PlayingShutdownControl]::PostMessageW($handle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
            if(-not $closeSent){throw 'Close message failed'}
            $normal=$owned.WaitForExit(15000);$timedOut=-not $normal;$normal=$normal -and $owned.ExitCode -eq 0
        }
        $exit=if($owned.HasExited){$owned.ExitCode}else{$null}
        $text=if($log){Read-OpenLog $log.FullName}else{''}
        $clean=$text -notmatch 'Native DJ source registered|obs-playing-next.dll'
        $luaLoaded=$text -match 'Loaded lua script: obs-playing.lua'
        $clean=$clean -and ($luaLoaded -eq [bool]$Legacy)
        if($log){Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $destination "round-$round.log")}
        $crash=Get-ChildItem (Join-Path $config 'crashes') -File -ErrorAction SilentlyContinue | Where-Object LastWriteTimeUtc -ge $started | Select-Object -First 1
        $signature='';if($crash){$body=[IO.File]::ReadAllText($crash.FullName);Copy-Item -LiteralPath $crash.FullName -Destination (Join-Path $destination "round-$round-crash.txt");if($body -match 'signal_handler_disconnect' -and $body -match 'OBSSourceLabel'){$signature='OBSSourceLabel/signal_handler_disconnect'}else{$signature='other'}}
        $row=[pscustomobject]@{Round=$round;PID=$owned.Id;SourcesLoaded=$loaded;IndependentControl=$clean;Legacy=[bool]$Legacy;LuaLoaded=$luaLoaded;CloseSent=$closeSent;NormalExit=$normal;Exit=$exit;Timeout=$timedOut;Crash=$signature}
        $rows.Add($row);$rows | Export-Csv (Join-Path $destination 'results.csv') -NoTypeInformation
        Write-Output ($row|ConvertTo-Json -Compress)
    }finally{if($owned -and -not $owned.HasExited){Stop-Process -Id $owned.Id -Force}}
}
if($rows | Where-Object {-not $_.SourcesLoaded -or -not $_.IndependentControl -or -not $_.CloseSent}){throw 'Invalid isolation control'}
Write-Output "Isolation complete: $Rounds rounds; failures=$(@($rows | Where-Object {-not $_.NormalExit}).Count)"
