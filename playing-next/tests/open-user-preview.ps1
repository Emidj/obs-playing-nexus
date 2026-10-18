param([string]$WorkRoot='work/phase17-user-preview',[ValidateSet(1,4,5,6,7,8,9,10,11,12,13,14,15,16)][int]$ProStyle=1,[switch]$PresetManager)
$ErrorActionPreference='Stop'
if($PresetManager -and $ProStyle -ne 1){throw 'Manager review uses the Neon Club scene'}
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$destination=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
if(-not $destination.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $destination)){throw 'Require a new isolated directory under work'}
$version='32.2.2'
$tests=Join-Path $destination 'tests/obs32'
$runtime=Join-Path $tests $version
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
foreach($folder in @('bin','data','obs-plugins')){Copy-Item -LiteralPath (Join-Path $workspace "tests/obs32/$version/$folder") -Destination $runtime -Recurse}
Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing') -Destination $destination -Recurse
Copy-Item -LiteralPath (Join-Path $workspace 'work/audit-phase01/tests/obs32/prepare-portable.ps1') -Destination $tests
Copy-Item -LiteralPath (Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/obs-playing-next.dll') -Destination $tests
Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing/obs-playing-db.dll') -Destination $tests
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'obs-user-preview.lua') -Destination $tests
if($PresetManager){
    $managerPreview=@'

ffi.cdef[[
typedef struct obs_source obs_source_t;typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
obs_source_t* obs_get_source_by_name(const char*);obs_properties_t* obs_source_properties(const obs_source_t*);
obs_property_t* obs_properties_get(obs_properties_t*,const char*);bool obs_property_button_clicked(obs_property_t*,void*);
void* obs_obj_get_data(void*);void obs_properties_destroy(obs_properties_t*);void obs_source_release(obs_source_t*);
]]
local function managerReview()
 obs.timer_remove(managerReview)
 local source=native.obs_get_source_by_name('Neon Club - piloto editable')
 assert(source~=nil,'Manager review source unavailable')
 local props=native.obs_source_properties(source)
 local button=native.obs_properties_get(props,'preset_manager')
 assert(button~=nil,'Manager review button unavailable')
 native.obs_property_button_clicked(button,native.obs_obj_get_data(source))
 native.obs_properties_destroy(props);native.obs_source_release(source)
end
local previousLoad=script_load
function script_load()previousLoad();obs.timer_add(managerReview,2000)end
local previousUnload=script_unload
function script_unload()obs.timer_remove(managerReview);previousUnload()end
'@
    [IO.File]::AppendAllText((Join-Path $tests 'obs-user-preview.lua'),$managerPreview,[Text.UTF8Encoding]::new($false))
}
if($ProStyle -eq 4){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1","add(scene,'Vinyl Classic - piloto editable',4,1");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 5){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367","add(scene,'Minimal - piloto editable',5,1,960,280");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 6){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367","add(scene,'Glassmorph - piloto editable',6,1,960,280");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 7){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367","add(scene,'Urban - piloto editable',7,1,960,320");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 8){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367","add(scene,'Waveform - piloto editable',8,1,960,320");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 9){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367","add(scene,'HUD - piloto editable',9,1,960,320");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 16){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367,40,35)","add(scene,'Vertical Liquid - piloto editable',16,2,480,720,40,0)").Replace("add(scene,'Solo texto horizontal - editable',0,1,960,180,40,440)","").Replace("add(scene,'Solo texto vertical - editable',0,2,320,560,1080,50)","");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 15){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367,40,35)","add(scene,'Vertical Urban - piloto editable',15,2,384,720,40,0)").Replace("add(scene,'Solo texto horizontal - editable',0,1,960,180,40,440)","").Replace("add(scene,'Solo texto vertical - editable',0,2,320,560,1080,50)","");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 14){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367,40,35)","add(scene,'Vertical Vinyl - piloto editable',14,2,400,720,40,0)").Replace("add(scene,'Solo texto horizontal - editable',0,1,960,180,40,440)","").Replace("add(scene,'Solo texto vertical - editable',0,2,320,560,1080,50)","");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 13){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367,40,35)","add(scene,'Vertical Glass - piloto editable',13,2,416,720,40,0)").Replace("add(scene,'Solo texto horizontal - editable',0,1,960,180,40,440)","").Replace("add(scene,'Solo texto vertical - editable',0,2,320,560,1080,50)","");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 12){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367,40,35)","add(scene,'Vertical Retro - piloto editable',12,2,480,720,40,0)").Replace("add(scene,'Solo texto horizontal - editable',0,1,960,180,40,440)","").Replace("add(scene,'Solo texto vertical - editable',0,2,320,560,1080,50)","");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 11){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367","add(scene,'Liquid - piloto editable',11,1,960,320");[IO.File]::WriteAllText($previewPath,$script)}
if($ProStyle -eq 10){$previewPath=Join-Path $tests 'obs-user-preview.lua';$script=[IO.File]::ReadAllText($previewPath).Replace("add(scene,'Neon Club - piloto editable',1,1,960,367","add(scene,'Particles - piloto editable',10,1,960,320");[IO.File]::WriteAllText($previewPath,$script)}
Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing/djemimas-logo.png') -Destination (Join-Path $tests 'preview-profile.png')
Copy-Item -LiteralPath (Join-Path $workspace 'playing-next/presets/pro-assets/neon-demo-artwork-v1.png') -Destination (Join-Path $tests 'preview-demo-cover.png')
[IO.File]::WriteAllText((Join-Path $tests 'preview-history.txt'),"21:00 : Sonny Fodera - Mind Still (Original Mix)`r`n",[Text.UTF8Encoding]::new($false))
& (Join-Path $tests 'prepare-portable.ps1') -Version $version
$scenePath=Join-Path $runtime 'config/obs-studio/basic/scenes/Compatibility.json'
$scene=Get-Content -LiteralPath $scenePath -Raw | ConvertFrom-Json -AsHashtable
$scene.modules.'scripts-tool'=@(@{path=(Join-Path $tests 'obs-user-preview.lua').Replace('\','/');settings=@{}})
# This preview has no heartbeat or auto-close script.
[IO.File]::WriteAllText($scenePath,($scene|ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
$profile=Join-Path $runtime 'config/obs-studio/basic/profiles/Compatibility/basic.ini'
[IO.File]::WriteAllText($profile,"[General]`nName=Phase17 Review`n[Video]`nBaseCX=1440`nBaseCY=720`nOutputCX=1440`nOutputCY=720`nFPSCommon=30`n[Audio]`nSampleRate=48000`nChannelSetup=Stereo`n")
$exe=Join-Path $runtime 'bin/64bit/obs64.exe'
# Visible launch is explicitly requested by the user.
$owned=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -PassThru -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
[pscustomobject]@{PID=$owned.Id;Executable=$exe;DllSHA256=(Get-FileHash (Join-Path $tests 'obs-playing-next.dll')).Hash;StartedUtc=[DateTime]::UtcNow} | ConvertTo-Json | Set-Content (Join-Path $destination 'preview-process.json')
Write-Output "Preview launched: PID=$($owned.Id); root=$destination"
