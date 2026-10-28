param([string]$EvidencePrefix='phase25-graphics',[string]$Version='32.2.2')
$ErrorActionPreference='Stop'
if($EvidencePrefix -notmatch '^[a-zA-Z0-9.-]+$'){throw 'Invalid evidence prefix'}
$rows=@()
foreach($case in @(
 @{Id='opengl';GraphicsBackend='OpenGL';VideoMode='SDR';Expected='Initializing OpenGL'},
 @{Id='pq';GraphicsBackend='Direct3D 11';VideoMode='PQ';Expected='Rec. 2100 (PQ)'},
 @{Id='hlg';GraphicsBackend='Direct3D 11';VideoMode='HLG';Expected='Rec. 2100 (HLG)'}
)){
 $root="work/$EvidencePrefix-$($case.Id)"
 try{
  & "$PSScriptRoot/run-source-lifecycle.ps1" -WorkRoot $root -Version $Version -Optimization -EvidencePrefix "$EvidencePrefix-$($case.Id)" -GraphicsBackend $case.GraphicsBackend -VideoMode $case.VideoMode
  $log=Get-Content "outputs/$EvidencePrefix-$($case.Id)-$Version.log" -Raw
  if($log -notmatch [regex]::Escape($case.Expected)){throw 'Requested graphics configuration was not observed in OBS log'}
  if($case.VideoMode -ne 'SDR' -and $log -notmatch 'format:\s+P010'){throw 'HDR P010 output was not observed'}
  $rows+=[pscustomobject]@{Case=$case.Id;Status='PASS';Scope='Current static/animated source draw and lifecycle; output color fidelity not measured';Reason='';WorkRoot=$root}
 }catch{
  $rows+=[pscustomobject]@{Case=$case.Id;Status='FAIL';Scope='Not certified';Reason=$_.Exception.Message;WorkRoot=$root}
 }
 $rows | Export-Csv "outputs/$EvidencePrefix.csv" -NoTypeInformation
}
if(@($rows|Where-Object Status -eq 'FAIL').Count){throw 'Graphics compatibility cases failed; inspect recorded evidence'}
