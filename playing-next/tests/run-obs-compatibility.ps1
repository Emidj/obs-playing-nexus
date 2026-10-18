param([string]$EvidencePrefix='phase25')
$ErrorActionPreference='Stop'
if($EvidencePrefix -notmatch '^[a-zA-Z0-9.-]+$'){throw 'Invalid evidence prefix'}
$workspace=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$node='C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe'
$rows=@()
$dll=Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/obs-playing-next.dll'
$sha=(Get-FileHash -LiteralPath $dll).Hash
foreach($version in @('32.0.0','32.0.4','32.1.2','32.2.2')){
 foreach($scenario in @('catalogue','pro','manager')){
  if((Get-FileHash -LiteralPath $dll).Hash -ne $sha){throw 'DLL changed during matrix'}
  $root="work/$EvidencePrefix-$scenario-$version"
  $arguments=@{WorkRoot=$root;Version=$version;EvidencePrefix="$EvidencePrefix-$scenario"}
  if($scenario -eq 'catalogue'){$arguments.WholeCatalogue=$true}
  elseif($scenario -eq 'pro'){$arguments.ProPilot=$true}
  else{$arguments.PresetManager=$true;$arguments.PresetManagerScene=$true;$arguments.PresetManagerKeepOpen=$true;$arguments.PresetManagerKeepSource=$true;$arguments.PresetManagerFileExchange=$true}
  & (Join-Path $PSScriptRoot 'run-source-lifecycle.ps1') @arguments
  $tests=Join-Path $workspace "$root/tests/obs32"
  $gate='lifecycle'
  if($scenario -ne 'manager'){
   $checker=if($scenario -eq 'pro'){'check-pro-pilot-captures.cjs'}else{'check-transition-catalogue.cjs'}
   $result=Join-Path $workspace "outputs/$EvidencePrefix-$scenario-$version-pixels.json"
   & $node (Join-Path $PSScriptRoot $checker) $tests $result
   if($LASTEXITCODE){throw "Pixel checks failed: $version $scenario"}
   if($scenario -eq 'catalogue'){
    & $node (Join-Path $PSScriptRoot 'check-normal-gallery.cjs') $tests (Join-Path $workspace "outputs/$EvidencePrefix-$scenario-$version-motion.json")
    if($LASTEXITCODE){throw "Catalogue motion checks failed: $version"}
   }
   $gate='lifecycle+pixels'
  }
  $rows+=[pscustomobject]@{OBS=$version;Scenario=$scenario;SHA256=$sha;Gate=$gate;Exit=0;Status='PASS';WorkRoot=$root}
  $rows | Export-Csv (Join-Path $workspace "outputs/$EvidencePrefix-compatibility.csv") -NoTypeInformation
 }
}
