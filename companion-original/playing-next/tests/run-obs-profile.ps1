param([string]$WorkRoot='work/phase24-profile',[string]$Version='32.2.2',[ValidateRange(1,3)][int]$Rounds=3,[ValidateRange(1,30)][int]$WarmSeconds=5,[ValidateRange(2,60)][int]$MeasureSeconds=15,[switch]$Soak,[switch]$Coverage,[switch]$Latency,[ValidateRange(2,20)][int]$LatencySamples=20,[ValidateRange(60,7200)][int]$SoakSeconds=180,[switch]$SoakArtwork,[string]$ProductionDllPath='work/phase24-production-dll/obs-playing-next.dll')
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\';$destination=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
if(-not $destination.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $destination)){throw 'Require new isolated work folder'}
if($Version -notmatch '^32\.\d+\.\d+$'){throw 'OBS32 required'}
$original=Join-Path $workspace "tests/obs32/$Version";$runtime=Join-Path $destination $Version
if((Get-Item (Join-Path $original 'bin/64bit/obs64.exe')).VersionInfo.ProductVersion -ne $Version){throw 'Runtime version mismatch'}
New-Item -ItemType Directory -Path $runtime -Force|Out-Null
foreach($name in @('bin','data','obs-plugins')){Copy-Item -LiteralPath (Join-Path $original $name) -Destination $runtime -Recurse}
$sourceDll=if($Latency){'work/core-phase08h-build/RelWithDebInfo/obs-playing-next.dll'}else{$ProductionDllPath}
$resolvedDll=[IO.Path]::GetFullPath((Join-Path $workspace $sourceDll));if(-not $resolvedDll.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Production DLL must be under isolated work'}
Copy-Item -LiteralPath (Join-Path $workspace $sourceDll),(Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/playing-obs-profile-probe.dll'),(Join-Path $workspace 'obs-playing/obs-playing-db.dll') -Destination $destination
Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing/djemimas-logo.png') -Destination (Join-Path $destination 'profile-artwork.png')
$luaName=if($Latency){'obs-profile-latency.lua'}else{'obs-profile.lua'}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot $luaName) -Destination $destination
if($Latency){[IO.File]::WriteAllText((Join-Path $destination 'latency-count.txt'),[string]$LatencySamples)}
$tls=Join-Path $destination 'playing-qt-plugins/tls';New-Item -ItemType Directory -Path $tls -Force|Out-Null
Copy-Item -LiteralPath 'C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64/plugins/tls/qschannelbackend.dll' -Destination $tls
[IO.File]::WriteAllText((Join-Path $destination 'profile-owned.flag'),'isolated fixture')
$environment=[ordered]@{OBS=$Version;DllSHA256=(Get-FileHash (Join-Path $destination 'obs-playing-next.dll')).Hash;ProbeSHA256=(Get-FileHash (Join-Path $destination 'playing-obs-profile-probe.dll')).Hash;Cpu=(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);Gpu=(Get-CimInstance Win32_VideoController|Select-Object Name,DriverVersion);OS=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber,TotalVisibleMemorySize);Video='1280x720 60fps D3D11 SDR, RAW NV12 sink, no encoder';PowerPlan=(& powercfg /getactivescheme|Out-String).Trim()}
[IO.File]::WriteAllText((Join-Path $destination 'environment.json'),($environment|ConvertTo-Json -Depth 6))
if(-not('PlayingGpuCounters' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'WindowsGpuCounters.cs')}
$logical=[int]($environment.Cpu|Measure-Object NumberOfLogicalProcessors -Sum).Sum
function Export-ProfileSamples($data,$path){
 $normalized=@(foreach($row in $data){$copy=[ordered]@{};foreach($property in $row.PSObject.Properties){$value=$property.Value;if($value -is [double]){$value=$value.ToString('R',[Globalization.CultureInfo]::InvariantCulture)};$copy[$property.Name]=$value};[pscustomobject]$copy})
 $normalized|Export-Csv -LiteralPath $path -NoTypeInformation
 [IO.File]::WriteAllText([IO.Path]::ChangeExtension($path,'json'),($data|ConvertTo-Json -Depth 5))
}
$exe=Join-Path $runtime 'bin/64bit/obs64.exe';$all=@()
for($round=1;$round -le $Rounds;$round++){
 foreach($pattern in @('phase-*.json','video-*.json','profile-result.txt','profile-error.txt','latency-result.txt','latency-error.txt','latency-samples.json')){Get-ChildItem -LiteralPath $destination -Filter $pattern -File|Remove-Item}
 $caseList=@(@{name='baseline_empty';kind='baseline';count=0;profile=1},@{name='loaded_idle';kind='idle';count=1;profile=1},@{name='static_demo';kind='static';count=1;profile=1})
 $order=if($round -eq 1){0,1,2}elseif($round -eq 2){2,1,0}else{1,0,2}
 foreach($p in $order){$caseList+=@{name=('vinyl_'+@('eco','balanced','maximum')[$p]);kind='vinyl';count=1;profile=$p}}
 $caseList+=@(@{name='mixed_six';kind='mixed';count=6;profile=1},@{name='history_balanced';kind='history';count=1;profile=1},@{name='recovery_idle';kind='recovery';count=0;profile=1})
 if($Soak){$caseList=@(@{name='loaded_idle';kind='idle';count=1;profile=1},@{name='soak_mixed_six';kind='mixed';count=6;profile=1;changes=$true;seconds=$SoakSeconds;artwork=[bool]$SoakArtwork},@{name='recovery_idle';kind='recovery';count=0;profile=1})}
 if($Coverage){$caseList=@(@{name='baseline_empty';kind='baseline';count=0;profile=1},@{name='loaded_idle';kind='idle';count=1;profile=1},@{name='history_waiting';kind='waiting';count=1;profile=1},@{name='static_artwork';kind='static';count=1;profile=1},@{name='mixed_six_artwork';kind='mixed';count=6;profile=1},@{name='history_shared_four';kind='history';count=4;profile=1},@{name='recovery_idle';kind='recovery';count=0;profile=1});foreach($c in $caseList){$c.artwork=$true}}
 foreach($c in $caseList){$c.warmup=$WarmSeconds;if(-not $c.ContainsKey('seconds')){$c.seconds=$MeasureSeconds}}
 [IO.File]::WriteAllText((Join-Path $destination 'profile-config.json'),(@{round=$round;cases=$caseList}|ConvertTo-Json -Depth 5))
 $config=Join-Path $runtime 'config/obs-studio';New-Item -ItemType Directory -Path (Join-Path $config 'basic/scenes'),(Join-Path $config 'basic/profiles/Profile') -Force|Out-Null
 $ini="[General]`nFirstRun=true`nConfirmOnExit=false`nEnableAutoUpdates=false`n[Basic]`nSceneCollection=Profile`nSceneCollectionFile=Profile`nProfile=Profile`nProfileDir=Profile`n[BasicWindow]`nWarnBeforeStartingStream=true`n"
 foreach($name in @('global.ini','user.ini')){[IO.File]::WriteAllText((Join-Path $config $name),$ini)}
 [IO.File]::WriteAllText((Join-Path $runtime 'portable_mode.txt'),'')
 [IO.File]::WriteAllText((Join-Path $config 'basic/profiles/Profile/basic.ini'),"[General]`nName=Profile`n[Video]`nBaseCX=1280`nBaseCY=720`nOutputCX=1280`nOutputCY=720`nFPSCommon=60`nColorFormat=NV12`nColorSpace=709`nColorRange=Partial`n[Audio]`nSampleRate=48000`nChannelSetup=Stereo`n")
 $scene=@{name='Profile';current_scene='Profile';current_program_scene='Profile';scene_order=@(@{name='Profile'});sources=@(@{name='Profile';id='scene';versioned_id='scene';settings=@{items=@();id_counter=0}});modules=@{'scripts-tool'=@(@{path=(Join-Path $destination $luaName).Replace('\','/');settings=@{}})}}
 [IO.File]::WriteAllText((Join-Path $config 'basic/scenes/Profile.json'),($scene|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
 $counter=[PlayingGpuCounters]::new();$owned=$null;$rows=@();$clock=[Diagnostics.Stopwatch]::StartNew();$previous=$null;$seen=0
 try{
  $owned=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
  while(-not $owned.HasExited){
   $limit=if($Latency){($LatencySamples+1)*12+70}else{($caseList|ForEach-Object{$_.warmup+$_.seconds}|Measure-Object -Sum).Sum+70}
   if($clock.Elapsed.TotalSeconds -gt $limit){throw 'Profiling timeout'}
   if(Test-Path -LiteralPath (Join-Path $destination 'latency-error.txt')){throw ([IO.File]::ReadAllText((Join-Path $destination 'latency-error.txt')))}
   if(Test-Path -LiteralPath (Join-Path $destination 'profile-error.txt')){throw ([IO.File]::ReadAllText((Join-Path $destination 'profile-error.txt')))}
   $owned.Refresh();$time=$clock.Elapsed.TotalSeconds;$cpu=$owned.TotalProcessorTime.TotalSeconds
   $phaseFile=Get-ChildItem -LiteralPath $destination -Filter 'phase-*.json' -File|Sort-Object LastWriteTimeUtc|Select-Object -Last 1
   $phase=$null;if($phaseFile){try{$phase=Get-Content -LiteralPath $phaseFile.FullName -Raw|ConvertFrom-Json}catch{}}
   $engines=$counter.Read($owned.Id);$gpu=$null;if($engines.Count){$gpu=($engines.Values|Measure-Object -Maximum).Maximum}
   $videoFile=Get-ChildItem -LiteralPath $destination -Filter 'video-*.json' -File|Sort-Object Name|Select-Object -Last 1;$video=$null;if($videoFile){try{$v=Get-Content -LiteralPath $videoFile.FullName -Raw|ConvertFrom-Json;if($phase -and $v.index -eq $phase.index -and $v.phase -eq $phase.phase){$video=$v.video}}catch{}}
   if($phase -and $previous -and $phase.phase -eq 'measure' -and $previous.phase -eq 'measure' -and $previous.index -eq $phase.index -and $video){
    $core=100*($cpu-$previous.cpu)/($time-$previous.time)
    $rows+=[pscustomobject]@{Round=$round;Scenario=$phase.name;ElapsedSeconds=$time;SampleIntervalSeconds=($time-$previous.time);CpuCorePercent=$core;CpuMachinePercent=$core/$logical;WorkingMiB=$owned.WorkingSet64/1MB;PrivateMiB=$owned.PrivateMemorySize64/1MB;Handles=$owned.HandleCount;Threads=$owned.Threads.Count;GpuEngineMaxPercent=$gpu;GpuEngineCount=$engines.Count;NonBlackSamples=$video.nonBlackSamples;ActiveFps=$video.activeFps;AverageFrameMs=$video.averageFrameMs;VideoElapsedSeconds=$v.elapsedNs/1e9;OutputFrames=$video.outputFrames;TotalFrames=$video.totalFrames;LaggedFrames=$video.laggedFrames;GpuEngines=($engines|ConvertTo-Json -Compress)}
   }
   if($phase){$previous=@{cpu=$cpu;time=$time;index=$phase.index;phase=$phase.phase};if($phase.index -ne $seen -and $phase.phase -eq 'measure'){$seen=$phase.index;Write-Output "Round ${round}/${Rounds}: $($phase.name)"}}
   Start-Sleep -Milliseconds 900;$owned.Refresh()
  }
  $owned.WaitForExit();if($owned.ExitCode -ne 0){throw "OBS abnormal exit $($owned.ExitCode)"}
  if($Latency){
   $result=Join-Path $destination 'latency-result.txt';if(-not(Test-Path $result) -or (Get-Content $result -Raw) -notmatch '^PASS '){throw 'Missing latency result'}
   $latencies=Get-Content (Join-Path $destination 'latency-samples.json') -Raw|ConvertFrom-Json
   if($latencies.Count -ne 4*($LatencySamples+1) -or @($latencies|Where-Object{$_.rawMs -lt $_.renderMs -or $_.renderMs -lt 0}).Count){throw 'Invalid latency measurements'}
   $roundFolder=Join-Path $destination "round-$round";New-Item -ItemType Directory -Path $roundFolder -Force|Out-Null
   Copy-Item -LiteralPath $result,(Join-Path $destination 'latency-samples.json') -Destination $roundFolder
   $log=Get-ChildItem (Join-Path $config 'logs') -File|Sort-Object LastWriteTimeUtc|Select-Object -Last 1;Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $roundFolder 'obs.log')
   Write-Output "PASS latency round $round; samples=$($latencies.Count); normal Exit0";continue
  }
  if(-not(Test-Path (Join-Path $destination 'profile-result.txt'))){throw 'Missing profile result'}
  $roundFolder=Join-Path $destination "round-$round";New-Item -ItemType Directory -Path $roundFolder -Force|Out-Null;Get-ChildItem -LiteralPath $destination -File|Where-Object{$_.Name -match '^(phase-|video-|profile-(result|config))'}|Copy-Item -Destination $roundFolder
  Export-ProfileSamples $rows (Join-Path $roundFolder 'samples.csv');$all+=$rows
  $log=Get-ChildItem (Join-Path $config 'logs') -File|Sort-Object LastWriteTimeUtc|Select-Object -Last 1;Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $roundFolder 'obs.log')
  foreach($c in $caseList){$samples=@($rows|Where-Object{$_.Scenario -eq $c.name});if($samples.Count -lt $(if($Soak -and $SoakSeconds -gt 180){[math]::Max(1,[math]::Floor($c.seconds*.6))}else{[math]::Max(1,$c.seconds-4)})){throw "Insufficient samples $($c.name): $($samples.Count)"};if(@($samples|Where-Object{$_.GpuEngineCount -eq 0}).Count){throw "GPU counter unavailable $($c.name)"};if(@($samples.VideoElapsedSeconds|Select-Object -Unique).Count -lt 2){throw "No independent video samples $($c.name)"};if(($samples[-1].OutputFrames-$samples[0].OutputFrames) -lt (($samples[-1].VideoElapsedSeconds-$samples[0].VideoElapsedSeconds)*54)){throw "RAW output not rendering at 60fps $($c.name)"}}
  foreach($c in $caseList|Where-Object{$_.kind -notin @('baseline','idle','recovery','waiting')}){if(@($rows|Where-Object{$_.Scenario -eq $c.name -and $_.NonBlackSamples -gt 0}).Count -eq 0){throw "Visible sources absent $($c.name)"}}
  Write-Output "PASS round $round; samples=$($rows.Count); normal Exit0"
 }finally{$counter.Dispose();if($owned -and -not $owned.HasExited){Stop-Process -Id $owned.Id -Force}}
}
if(-not $Latency){Export-ProfileSamples $all (Join-Path $destination 'samples.csv')}
$label=Split-Path $destination -Leaf;$evidence=Join-Path $workspace "outputs/$label";New-Item -ItemType Directory -Path $evidence -Force|Out-Null
Copy-Item -LiteralPath (Join-Path $destination 'environment.json') -Destination $evidence
if(-not $Latency){Copy-Item -LiteralPath (Join-Path $destination 'samples.csv'),(Join-Path $destination 'samples.json') -Destination $evidence}
foreach($r in Get-ChildItem -LiteralPath $destination -Directory -Filter 'round-*'){Copy-Item -LiteralPath $r.FullName -Destination $evidence -Recurse}
Write-Output "PASS profiling completed; evidence=$evidence"
