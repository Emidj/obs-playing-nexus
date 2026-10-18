param([switch]$Apply,[switch]$Rollback)
$ErrorActionPreference='Stop'
if($Apply -and $Rollback){throw 'Choose apply or rollback'}
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$folder=Join-Path $workspace 'outputs/phase12h-shared-group-candidate'
$shared='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays'
$rows=@(Import-Csv (Join-Path $folder 'manifest.csv'))
$allowed=@('obs-visual-renderer.hpp','obs-visual-renderer.cpp')
if($rows.Count -ne 2 -or @($rows.File|Sort-Object -Unique).Count -ne 2 -or @($rows|Where-Object{$_.File -notin $allowed}).Count){throw 'Only the two exact renderer files are permitted'}
foreach($row in $rows){
 if((Get-FileHash -LiteralPath (Join-Path $folder $row.File)).Hash -ne $row.Candidate -or (Get-FileHash -LiteralPath (Join-Path $folder ($row.File+'.before'))).Hash -ne $row.Before){throw 'Candidate or rollback bytes changed'}
 $actual=(Get-FileHash -LiteralPath (Join-Path $shared ('src/adapters/'+$row.File))).Hash
 if($actual -ne $row.Before -and $actual -ne $row.Candidate){throw 'External renderer has unrelated changes; refuse to overwrite'}
}
$lock=Join-Path $PSScriptRoot 'visual-audit/renderer.lock.json'
$backup=Join-Path $folder 'renderer.lock.json.before'
if(-not(Test-Path -LiteralPath $backup)){
 $entries=@(Get-Content -LiteralPath $lock -Raw|ConvertFrom-Json)
 foreach($row in $rows){if(@($entries|Where-Object{$_.path -eq ('src/adapters/'+$row.File) -and $_.sha256 -eq $row.Before}).Count -ne 1){throw 'Initial pins are not the verified previous contract'}}
 Copy-Item -LiteralPath $lock -Destination $backup
}
$originalEntries=@(Get-Content -LiteralPath $backup -Raw|ConvertFrom-Json)
foreach($row in $rows){if(@($originalEntries|Where-Object{$_.path -eq ('src/adapters/'+$row.File) -and $_.sha256 -eq $row.Before}).Count -ne 1){throw 'Rollback pin snapshot invalid'}}
$currentEntries=@(Get-Content -LiteralPath $lock -Raw|ConvertFrom-Json)
foreach($entry in $currentEntries){
 $old=@($originalEntries|Where-Object{$_.path -eq $entry.path});if($old.Count -ne 1){throw 'Renderer pin paths changed'}
 $row=@($rows|Where-Object{('src/adapters/'+$_.File) -eq $entry.path})
 if($row.Count){if($entry.sha256 -ne $row[0].Before -and $entry.sha256 -ne $row[0].Candidate){throw 'Renderer pins contain unrelated changes'}}
 elseif($entry.sha256 -ne $old[0].sha256 -or (Get-FileHash -LiteralPath (Join-Path $shared $entry.path)).Hash -ne $entry.sha256){throw 'Untouched renderer dependency changed'}
}
if($currentEntries.Count -ne $originalEntries.Count){throw 'Renderer pin count changed'}
if(-not($Apply -or $Rollback)){Write-Output 'Candidate and exact rollback verified; external files and pins not changed';return}
$desired=if($Rollback){'Before'}else{'Candidate'}
foreach($row in $rows){
 $path=Join-Path $shared ('src/adapters/'+$row.File)
 $actual=(Get-FileHash -LiteralPath $path).Hash
 if($actual -ne $row.$desired){
  $source=if($Rollback){Join-Path $folder ($row.File+'.before')}else{Join-Path $folder $row.File}
  [IO.File]::WriteAllBytes($path,[IO.File]::ReadAllBytes($source))
 }
 if((Get-FileHash -LiteralPath $path).Hash -ne $row.$desired){throw 'Renderer write verification failed; rollback snapshot preserved'}
}
if($Rollback){[IO.File]::WriteAllBytes($lock,[IO.File]::ReadAllBytes($backup))}
else{
 foreach($row in $rows){foreach($entry in $currentEntries){if($entry.path -eq ('src/adapters/'+$row.File)){$entry.sha256=$row.Candidate}}}
 [IO.File]::WriteAllText($lock,($currentEntries|ConvertTo-Json -Depth 8)+"`n",[Text.UTF8Encoding]::new($false))
}
foreach($entry in (Get-Content -LiteralPath $lock -Raw|ConvertFrom-Json)){if((Get-FileHash -LiteralPath (Join-Path $shared $entry.path)).Hash -ne $entry.sha256){throw 'Final shared renderer pin mismatch'}}
Write-Output "Two renderer files and pins verified: $desired"
