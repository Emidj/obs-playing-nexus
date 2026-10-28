param([switch]$Prepare,[switch]$Apply,[switch]$Rollback)
$ErrorActionPreference='Stop'
if(@($Prepare,$Apply,$Rollback | Where-Object {$_}).Count -gt 1){throw 'Choose one operation'}
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$shared='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays'
$folder=Join-Path $workspace 'outputs/phase12q-shared-group-srgb'
$lock=Join-Path $PSScriptRoot 'visual-audit/renderer.lock.json'
$allowed=@('obs-visual-renderer.hpp','obs-visual-renderer.cpp')
function Replace-Once([string]$Text,[string]$Before,[string]$After){
 if($Text.IndexOf($Before) -lt 0 -or $Text.IndexOf($Before) -ne $Text.LastIndexOf($Before)){throw 'Expected exact unique previous contract'}
 return $Text.Replace($Before,$After)
}
if($Prepare){
 if(Test-Path -LiteralPath $folder){throw 'Candidate folder already exists; inspect existing evidence'}
 $pins=@(Get-Content -LiteralPath $lock -Raw|ConvertFrom-Json)
 foreach($pin in $pins){if((Get-FileHash -LiteralPath (Join-Path $shared $pin.path)).Hash -ne $pin.sha256){throw 'External renderer changed before preparation'}}
 New-Item -ItemType Directory -Path $folder | Out-Null
 Copy-Item -LiteralPath $lock -Destination (Join-Path $folder 'renderer.lock.json.before')
 $manifest=@()
 foreach($name in $allowed){
  $source=Join-Path $shared ('src/adapters/'+$name)
  $backup=Join-Path $folder ($name+'.before');Copy-Item -LiteralPath $source -Destination $backup
  $text=[IO.File]::ReadAllText($source)
  if($name.EndsWith('.hpp')){
   $declaration='void drawGroup(uint64_t identity,float x=0,float y=0,float scale=1,float opacity=1,bool active=true);'
   $text=Replace-Once $text $declaration ($declaration+"`n`t// Final SDR output of an encoded-premultiplied group, matching Browser.`n`t// Never use inside another encoded group; existing drawGroup is unchanged.`n`tvoid drawGroupSrgb(uint64_t identity,float x=0,float y=0,float scale=1,float opacity=1,bool active=true);")
   $text=Replace-Once $text 'unsigned groupDepth_=0;' "unsigned groupDepth_=0;`n`tvoid drawGroupImpl(uint64_t identity,float x,float y,float scale,float opacity,bool active,bool srgbOutput);"
  }else{
   $signature='void ObsVisualRenderer::drawGroup(uint64_t identity,float x,float y,float scale,float opacity,bool active)'
   $wrappers=$signature+"`n{drawGroupImpl(identity,x,y,scale,opacity,active,false);}`nvoid ObsVisualRenderer::drawGroupSrgb(uint64_t identity,float x,float y,float scale,float opacity,bool active)`n{drawGroupImpl(identity,x,y,scale,opacity,active,true);}`nvoid ObsVisualRenderer::drawGroupImpl(uint64_t identity,float x,float y,float scale,float opacity,bool active,bool srgbOutput)"
   $text=Replace-Once $text $signature $wrappers
   $text=Replace-Once $text 'BlendScope blend;MatrixScope matrix(x,y);gs_matrix_scale3f(scale,scale,1);' "if(srgbOutput&&groupDepth_)throw std::logic_error(`"sRGB output cannot enter an encoded group`");`n`tBlendScope blend;if(srgbOutput)gs_enable_framebuffer_srgb(true);MatrixScope matrix(x,y);gs_matrix_scale3f(scale,scale,1);"
   $old='gs_effect_set_texture(gs_effect_get_param_by_name(groupEffect_,"image"),gs_texrender_get_texture(slot.target.get()));'
   $new='if(srgbOutput)gs_effect_set_texture_srgb(gs_effect_get_param_by_name(groupEffect_,"image"),gs_texrender_get_texture(slot.target.get()));else '+$old
   $text=Replace-Once $text $old $new
  }
  $candidate=Join-Path $folder $name;[IO.File]::WriteAllText($candidate,$text,[Text.UTF8Encoding]::new($false))
  $manifest+=[pscustomobject]@{File=$name;Before=(Get-FileHash -LiteralPath $backup).Hash;Candidate=(Get-FileHash -LiteralPath $candidate).Hash}
 }
 $manifest | Export-Csv (Join-Path $folder 'manifest.csv') -NoTypeInformation
 Write-Output 'Two-file candidate prepared; external engine unchanged';return
}
$rows=@(Import-Csv (Join-Path $folder 'manifest.csv'))
if($rows.Count -ne 2 -or @($rows.File|Sort-Object -Unique).Count -ne 2 -or @($rows|Where-Object {$_.File -notin $allowed}).Count){throw 'Only two exact renderer files permitted'}
$original=@(Get-Content -LiteralPath (Join-Path $folder 'renderer.lock.json.before') -Raw|ConvertFrom-Json)
$current=@(Get-Content -LiteralPath $lock -Raw|ConvertFrom-Json)
if($current.Count -ne $original.Count){throw 'Pin count changed'}
foreach($row in $rows){
 foreach($suffix in @('','.before')){
  $expected=if($suffix){$row.Before}else{$row.Candidate}
  if((Get-FileHash -LiteralPath (Join-Path $folder ($row.File+$suffix))).Hash -ne $expected){throw 'Candidate or rollback bytes changed'}
 }
 if(@($original|Where-Object {$_.path -eq ('src/adapters/'+$row.File) -and $_.sha256 -eq $row.Before}).Count -ne 1){throw 'Rollback pins do not match snapshot'}
}
foreach($pin in $current){
 $old=@($original|Where-Object {$_.path -eq $pin.path});if($old.Count -ne 1){throw 'Pin paths changed'}
 $row=@($rows|Where-Object {('src/adapters/'+$_.File) -eq $pin.path})
 $actual=(Get-FileHash -LiteralPath (Join-Path $shared $pin.path)).Hash
 if($row.Count){if($pin.sha256 -notin @($row[0].Before,$row[0].Candidate) -or $actual -notin @($row[0].Before,$row[0].Candidate)){throw 'Unrelated external renderer changes'}}
 elseif($pin.sha256 -ne $old[0].sha256 -or $actual -ne $pin.sha256){throw 'Untouched dependency changed'}
}
if(-not($Apply -or $Rollback)){Write-Output 'Candidate, rollback and external pins verified';return}
$desired=if($Rollback){'Before'}else{'Candidate'}
foreach($row in $rows){
 $path=Join-Path $shared ('src/adapters/'+$row.File)
 $input=Join-Path $folder ($row.File+$(if($Rollback){'.before'}else{''}))
 if((Get-FileHash -LiteralPath $path).Hash -ne $row.$desired){[IO.File]::WriteAllBytes($path,[IO.File]::ReadAllBytes($input))}
 if((Get-FileHash -LiteralPath $path).Hash -ne $row.$desired){throw 'Shared write verification failed; exact rollback preserved'}
 foreach($pin in $current){if($pin.path -eq ('src/adapters/'+$row.File)){$pin.sha256=$row.$desired}}
}
if($Rollback){[IO.File]::WriteAllBytes($lock,[IO.File]::ReadAllBytes((Join-Path $folder 'renderer.lock.json.before')))}
else{[IO.File]::WriteAllText($lock,($current|ConvertTo-Json -Depth 8)+"`n",[Text.UTF8Encoding]::new($false))}
foreach($pin in (Get-Content -LiteralPath $lock -Raw|ConvertFrom-Json)){if((Get-FileHash -LiteralPath (Join-Path $shared $pin.path)).Hash -ne $pin.sha256){throw 'Final external pins mismatch'}}
Write-Output "Two renderer files and pins verified: $desired"
