param([switch]$Localization,[switch]$QuickConnection,[switch]$ProfileRetry,[string]$WorkRoot='work/phase10a-source',[string]$Version='32.2.2',[switch]$Render,[string]$EvidencePrefix='',[switch]$LoadOnly,[switch]$Baseline,[switch]$NormalReference,[switch]$NormalNative,[switch]$NormalMatrix,[switch]$GlyphReference,[string]$GlyphReferenceJson='',[switch]$GlyphUnclippedRoot,[switch]$TerminalSceneReference,[switch]$NormalRedesign,[switch]$NormalCatalogue,[switch]$Artwork,[switch]$LayerAnimation,[switch]$TrackTransitions,[switch]$WholeCatalogue,[switch]$TextOnly,[switch]$ProPilot,[switch]$VerticalGeometry,[switch]$PresetManager,[switch]$PresetManagerScene,[switch]$PresetManagerKeepOpen,[switch]$PresetManagerKeepSource,[switch]$DebugShutdown,[switch]$Customization,[switch]$PresetManagerDirectSave,[switch]$PresetManagerFileExchange,[switch]$Performance,[switch]$Optimization,[ValidateSet('Direct3D 11','OpenGL')][string]$GraphicsBackend='Direct3D 11',[ValidateSet('SDR','PQ','HLG')][string]$VideoMode='SDR',[switch]$BasicSharedSettings)
$ErrorActionPreference='Stop';if($ProfileRetry -or $QuickConnection){$Render=$true};if($VerticalGeometry){$ProPilot=$true};if(($PresetManagerDirectSave -or $PresetManagerFileExchange) -and -not $PresetManager){throw 'Direct Save As requires manager scenario'};if(($PresetManagerScene -or $PresetManagerKeepOpen -or $PresetManagerKeepSource) -and -not $PresetManager){throw 'Manager scenarios require -PresetManager'};if($PresetManagerKeepSource -and (-not $PresetManagerScene -or -not $PresetManagerKeepOpen)){throw 'Live exit requires scene and open manager'};if($BasicSharedSettings){$Render=$true};if($ProPilot){$TextOnly=$true};if($WholeCatalogue){$NormalCatalogue=$true}
if($PresetManager -and ($Render -or $LoadOnly -or $Baseline -or $NormalReference -or $NormalNative -or $NormalMatrix -or $GlyphReference -or $TerminalSceneReference -or $NormalRedesign -or $NormalCatalogue -or $Artwork -or $LayerAnimation -or $TrackTransitions -or $WholeCatalogue -or $TextOnly -or $ProPilot)){throw 'Choose preset manager test alone'}
if($LayerAnimation -and $TrackTransitions){throw 'Choose one animation test'};if($TextOnly -and ($LayerAnimation -or $TrackTransitions)){throw 'Choose one visual test'};if($LayerAnimation -or $TrackTransitions -or $TextOnly){$Artwork=$true}
if($Artwork -and ($Render -or $LoadOnly -or $Baseline -or $NormalReference -or $NormalNative -or $NormalMatrix -or $GlyphReference -or $NormalRedesign -or $NormalCatalogue)){throw 'Choose artwork test alone'}
if($NormalCatalogue -and ($Render -or $LoadOnly -or $Baseline -or $NormalReference -or $NormalNative -or $NormalMatrix -or $GlyphReference -or $TerminalSceneReference -or $NormalRedesign)){throw 'Choose catalogue or another source test'}
if($NormalRedesign -and ($Render -or $LoadOnly -or $Baseline -or $NormalReference -or $NormalNative -or $NormalMatrix -or $GlyphReference -or $TerminalSceneReference)){throw 'Choose redesign or another source test'}
if($TerminalSceneReference){if($GlyphReferenceJson -or $GlyphUnclippedRoot){throw 'Full Terminal scene uses its preserved template'};$GlyphReference=$true}
if($GlyphReference -and ($Render -or $LoadOnly -or $Baseline -or $NormalReference -or $NormalNative -or $NormalMatrix)){throw 'Choose CEF glyph reference or another source test'}
if($EvidencePrefix -and $EvidencePrefix -notmatch '^[a-zA-Z0-9.-]+$'){throw 'Invalid evidence prefix'}
if($Render -and $LoadOnly){throw 'Choose render test or load-only control'}
if($NormalReference -and ($Render -or $LoadOnly -or $Baseline)){throw 'Choose NORMAL reference or existing source tests'}
if($NormalNative -and ($Render -or $LoadOnly -or $Baseline -or $NormalReference)){throw 'Choose native NORMAL or other source tests'}
if($NormalMatrix -and ($Render -or $LoadOnly -or $Baseline -or $NormalReference -or $NormalNative)){throw 'Choose native matrix or other source tests'}
if($Optimization -and ($Performance -or $Customization -or $PresetManager -or $Render -or $LoadOnly -or $Baseline -or $Artwork -or $NormalCatalogue -or $NormalMatrix -or $NormalReference -or $NormalNative -or $GlyphReference -or $NormalRedesign -or $TerminalSceneReference -or $LayerAnimation -or $TrackTransitions -or $TextOnly)){throw 'Choose optimization test alone'}
if($Performance -and ($Customization -or $PresetManager -or $Render -or $LoadOnly -or $Baseline -or $Artwork -or $NormalCatalogue -or $NormalMatrix -or $NormalReference -or $NormalNative -or $GlyphReference -or $NormalRedesign -or $TerminalSceneReference -or $LayerAnimation -or $TrackTransitions -or $TextOnly)){throw 'Choose performance test alone'}
if($Customization -and ($PresetManager -or $Render -or $LoadOnly -or $Baseline -or $NormalReference -or $NormalNative -or $NormalMatrix -or $GlyphReference -or $NormalRedesign -or $NormalCatalogue -or $Artwork -or $LayerAnimation -or $TrackTransitions -or $TextOnly)){throw 'Choose customization test alone'}
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
$destination=[IO.Path]::GetFullPath((Join-Path $workspace $WorkRoot))
if(-not $destination.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $destination)){throw 'Require a new isolated work directory'}
if($Version -notmatch '^32\.\d+\.\d+$'){throw 'Require OBS32.x'}
$original=Join-Path $workspace "tests/obs32/$Version"
$exeOriginal=Join-Path $original 'bin/64bit/obs64.exe'
if((Get-Item -LiteralPath $exeOriginal).VersionInfo.ProductVersion -ne $Version){throw 'Runtime mismatch'}
$tests=Join-Path $destination 'tests/obs32'
$runtime=Join-Path $tests $Version
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing') -Destination $destination -Recurse
foreach($folder in @('bin','data','obs-plugins')) {Copy-Item -LiteralPath (Join-Path $original $folder) -Destination $runtime -Recurse}
foreach($file in @('prepare-portable.ps1','heartbeat.lua')) {Copy-Item -LiteralPath (Join-Path $workspace "work/audit-phase01/tests/obs32/$file") -Destination $tests}
Copy-Item -LiteralPath (Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/obs-playing-next.dll') -Destination $tests
Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing/obs-playing-db.dll') -Destination $tests
$qtPlugins=Join-Path $tests 'playing-qt-plugins/tls'
New-Item -ItemType Directory -Path $qtPlugins -Force | Out-Null
$qtBackend='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64/plugins/tls/qschannelbackend.dll'
Copy-Item -LiteralPath $qtBackend -Destination $qtPlugins
$script=if($Localization){'obs-localization.lua'}elseif($QuickConnection){'obs-quick-connect.lua'}elseif($ProfileRetry){'obs-profile-retry.lua'}elseif($BasicSharedSettings){'obs-basic-shared-settings.lua'}elseif($Optimization){'obs-optimization.lua'}elseif($Performance){'obs-performance.lua'}elseif($Customization){'obs-customization.lua'}elseif($PresetManager){'obs-preset-manager.lua'}elseif($ProPilot){'obs-pro-pilot.lua'}elseif($TextOnly){'obs-text-only.lua'}elseif($TrackTransitions){'obs-track-transitions.lua'}elseif($LayerAnimation){'obs-layer-animation.lua'}elseif($Artwork){'obs-artwork.lua'}elseif($NormalCatalogue){'obs-normal-catalogue.lua'}elseif($NormalRedesign){'obs-normal-redesign.lua'}elseif($GlyphReference){'obs-terminal-glyph-reference.lua'}elseif($NormalMatrix){'obs-normal-matrix.lua'}elseif($NormalNative){'obs-normal-native.lua'}elseif($NormalReference){'obs-normal-reference.lua'}elseif($Baseline){'obs-source-baseline.lua'}elseif($Render){'obs-source-render.lua'}elseif($LoadOnly){'obs-source-load-only.lua'}else{'obs-source-lifecycle.lua'}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination $tests
if($VerticalGeometry){[IO.File]::WriteAllText((Join-Path $tests 'vertical-geometry-only.flag'),'fixture')}
if($PresetManagerFileExchange){[IO.File]::WriteAllText((Join-Path $tests 'manager-file-exchange.flag'),'fixture')};if($PresetManagerDirectSave){[IO.File]::WriteAllText((Join-Path $tests 'manager-direct-save.flag'),'fixture')};if($PresetManagerScene){[IO.File]::WriteAllText((Join-Path $tests 'manager-scene.flag'),'fixture');Copy-Item -LiteralPath (Join-Path $workspace 'obs-playing/djemimas-logo.png') -Destination (Join-Path $tests 'private-photo.png')};if($PresetManagerKeepOpen){[IO.File]::WriteAllText((Join-Path $tests 'manager-keep-open.flag'),'fixture')};if($PresetManagerKeepSource){[IO.File]::WriteAllText((Join-Path $tests 'manager-keep-source.flag'),'fixture')};if($WholeCatalogue){[IO.File]::WriteAllText((Join-Path $tests 'whole-catalogue.flag'),'fixture')}
if($Artwork){
    & 'C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe' (Join-Path $PSScriptRoot 'prepare-artwork-fixtures.cjs') $tests
    if($LASTEXITCODE){throw 'Artwork fixture preparation failed'}
}
if($GlyphReference){
    $glyphArguments=@($tests);if($GlyphReferenceJson){$glyphArguments+=$GlyphReferenceJson}
    if($GlyphUnclippedRoot){if(-not $GlyphReferenceJson){$glyphArguments+=(Join-Path $workspace 'outputs/phase12l-terminal-glyphs/reference.json')};$glyphArguments+='--unclipped-root'}
    $glyphHelper=if($TerminalSceneReference){'prepare-terminal-scene-obs.cjs'}else{'prepare-terminal-glyph-obs.cjs'}
    & 'C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe' (Join-Path $PSScriptRoot $glyphHelper) @glyphArguments
    if($LASTEXITCODE -ne 0){throw 'CEF glyph page preparation failed'}
}
& (Join-Path $tests 'prepare-portable.ps1') -Version $Version
if($Optimization -or $Performance -or $Customization -or $PresetManager -or $Artwork -or $Render -or $NormalReference -or $NormalNative -or $NormalMatrix -or $GlyphReference -or $NormalRedesign -or $NormalCatalogue){
    $heartbeatPath=Join-Path $tests 'heartbeat.lua'
    $heartbeatText=[IO.File]::ReadAllText($heartbeatPath).Replace('ticks==24','ticks==48')
    [IO.File]::WriteAllText($heartbeatPath,$heartbeatText,[Text.UTF8Encoding]::new($false))
}
if($ProPilot){$heartbeatPath=Join-Path $tests 'heartbeat.lua';$heartbeatText=[IO.File]::ReadAllText($heartbeatPath).Replace('ticks==24',$(if($VerticalGeometry){'ticks==120'}else{'ticks==204'})).Replace('ticks==48',$(if($VerticalGeometry){'ticks==120'}else{'ticks==204'}));[IO.File]::WriteAllText($heartbeatPath,$heartbeatText,[Text.UTF8Encoding]::new($false))}
& (Join-Path $PSScriptRoot 'configure-obs-smoke.ps1') -PortableRoot $runtime
# Explicit graphics configurations apply only to this new isolated runtime.
$globalPath=Join-Path $runtime 'config/obs-studio/global.ini'
$globalText=[IO.File]::ReadAllText($globalPath)
if($globalText -match '(?m)^Renderer=.*$'){$globalText=[regex]::Replace($globalText,'(?m)^Renderer=.*$',('Renderer='+$GraphicsBackend))}
elseif($globalText -match '(?m)^\[Video\]'){$globalText=[regex]::Replace($globalText,'(?m)^\[Video\]','[Video]'+"`nRenderer=$GraphicsBackend")}
else{$globalText+="`n[Video]`nRenderer=$GraphicsBackend`n"}
[IO.File]::WriteAllText($globalPath,$globalText,[Text.UTF8Encoding]::new($false))
if($VideoMode -ne 'SDR'){
 $profilePath=Join-Path $runtime 'config/obs-studio/basic/profiles/Compatibility/basic.ini'
 $profileText=[IO.File]::ReadAllText($profilePath)
 $space=if($VideoMode -eq 'PQ'){'2100PQ'}else{'2100HLG'}
 $profileText=$profileText.Replace('[Video]',"[Video]`nColorFormat=P010`nColorSpace=$space`nColorRange=Partial")
 [IO.File]::WriteAllText($profilePath,$profileText,[Text.UTF8Encoding]::new($false))
}
$scenePath=Join-Path $runtime 'config/obs-studio/basic/scenes/Compatibility.json'
$scene=Get-Content -LiteralPath $scenePath -Raw | ConvertFrom-Json -AsHashtable
$scene.modules.'scripts-tool'+=@{path=(Join-Path $tests $script).Replace('\','/');settings=@{}}
[IO.File]::WriteAllText($scenePath,($scene|ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
$exe=Join-Path $runtime 'bin/64bit/obs64.exe'
$owned=$null
try {
    $started=[DateTime]::UtcNow
    if($DebugShutdown){
        $debugger=Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/playing-obs-debug-launcher.exe'
        $debugOutput=Join-Path $destination 'shutdown-debug'
        & $debugger $workspace $exe $debugOutput 150
        if($LASTEXITCODE -ne 0){throw "Debugged OBS failed; evidence retained at $debugOutput"}
        $debugEvents=[IO.File]::ReadAllText((Join-Path $debugOutput 'events.txt'))
        if($debugEvents -notmatch 'exit=0 timeout=0 exceptions=0'){throw 'Debugger did not confirm clean child exit'}
        $owned=[pscustomobject]@{ExitCode=0;HasExited=$true}
    }else{
        $owned=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -WindowStyle Hidden -PassThru -ArgumentList @('--portable','--multi','--disable-updater','--disable-missing-files-check','--only-bundled-plugins')
    }
    if(-not $DebugShutdown -and (-not $owned.WaitForExit($(if($ProPilot){117000}else{45000})) -or $owned.ExitCode -ne 0)){throw 'OBS did not close normally'}
    $result=Join-Path $tests $(if($Localization){'localization-result.txt'}elseif($QuickConnection){'quick-connect-result.txt'}elseif($ProfileRetry){'profile-retry-result.txt'}elseif($Optimization){'optimization-result.txt'}elseif($Performance){'performance-result.txt'}elseif($Customization){'customization-result.txt'}elseif($PresetManager){'preset-manager-result.txt'}elseif($ProPilot){'pro-pilot-result.txt'}elseif($TextOnly){'text-only-result.txt'}elseif($TrackTransitions){'track-transitions-result.txt'}elseif($LayerAnimation){'layer-animation-result.txt'}elseif($Artwork){'artwork-result.txt'}elseif($NormalCatalogue){'normal-catalogue-result.txt'}elseif($NormalRedesign){'normal-redesign-result.txt'}elseif($GlyphReference){'glyph-reference-result.txt'}elseif($NormalMatrix){'normal-matrix-result.txt'}elseif($NormalNative){'normal-native-result.txt'}elseif($NormalReference){'normal-reference-result.txt'}elseif($Render -and -not $Baseline){'source-render-result.txt'}else{'source-lifecycle-result.txt'})
    if(-not(Test-Path -LiteralPath $result) -or (Get-Item $result).LastWriteTimeUtc -lt $started){throw 'Missing fresh native source evidence'}
    if([IO.File]::ReadAllText($result) -notmatch '^PASS '){throw 'Native source test failed'}
    $baselineText=Get-Content -LiteralPath (Join-Path $tests 'heartbeat.txt') -Raw
    $expectedTicks=if($VerticalGeometry){120}elseif($ProPilot){204}elseif($Optimization -or $Performance -or $Customization -or $PresetManager -or $Artwork -or $Render -or $NormalReference -or $NormalNative -or $NormalMatrix -or $GlyphReference -or $NormalRedesign -or $NormalCatalogue){48}else{24}
    if($baselineText -notmatch "^$([regex]::Escape($Version)) ticks=$expectedTicks checked_sources=23"){throw 'Original sources baseline failed'}
    $label=if($EvidencePrefix){$EvidencePrefix}elseif($Render){'phase10c'}else{'phase10a'}
    $prefix=Join-Path $workspace "outputs/$label-$Version"
    Copy-Item -LiteralPath $result -Destination ($prefix+'-result.txt')
    if($Optimization){$gallery=$prefix+'-captures';New-Item -ItemType Directory -Path $gallery -Force | Out-Null;$images=@(Get-ChildItem -LiteralPath $tests -Filter 'optimization-*.png' -File);if($images.Count -ne 6){throw 'Missing optimization captures'};$images|Copy-Item -Destination $gallery}
    if($Performance){Copy-Item -LiteralPath (Join-Path $tests 'performance-eco.png') -Destination ($prefix+'-eco.png')}
    if($Customization){$gallery=$prefix+'-captures';New-Item -ItemType Directory -Path $gallery -Force | Out-Null;$images=@(Get-ChildItem -LiteralPath $tests -Filter 'custom-*.png' -File);if($images.Count -ne 12){throw 'Missing customization captures'};$images|Copy-Item -Destination $gallery}
    if($PresetManagerScene){foreach($name in @('manager-scene.png','manager-dialog.png')){Copy-Item -LiteralPath (Join-Path $tests $name) -Destination ($prefix+'-'+$name)}}
    if($Artwork){
        $gallery=$prefix+'-captures';New-Item -ItemType Directory -Path $gallery -Force | Out-Null
        $pattern=if($ProPilot){'pro-*.png'}elseif($TextOnly){'textonly-*.png'}elseif($TrackTransitions){'*-transition-*.png'}elseif($LayerAnimation){'motion-capture-*.png'}else{'artwork-capture-*.png'}
        if($ProPilot){Get-ChildItem -LiteralPath $tests -Filter 'pro-*-bounds.json' -File | Copy-Item -Destination $gallery}
        $images=@(Get-ChildItem -LiteralPath $tests -Filter $pattern -File)
        $expected=if($VerticalGeometry){35}elseif($ProPilot){95}elseif($TextOnly){5}elseif($TrackTransitions){18}elseif($LayerAnimation){10}else{7}
        if($images.Count -ne $expected){throw 'Missing image captures'}
        foreach($image in $images){Copy-Item -LiteralPath $image.FullName -Destination $gallery};if($TrackTransitions){$live=@(Get-ChildItem -LiteralPath $tests -Filter '*-live-final.png' -File);if($live.Count -ne 3){throw 'Missing live transition captures'};foreach($image in $live){Copy-Item -LiteralPath $image.FullName -Destination $gallery}}
    }
    if($GlyphReference){
        $glyphDestination=$prefix+'-captures';New-Item -ItemType Directory -Path $glyphDestination -Force | Out-Null
        $glyphImages=@(Get-ChildItem -LiteralPath $tests -Filter '*.png' -File)
        $glyphCases=Get-Content -LiteralPath (Join-Path $tests 'glyph-reference-cases.json') -Raw | ConvertFrom-Json
        if($glyphImages.Count -ne ($glyphCases.cases.Count*3)){throw 'Require every CEF glyph capture'}
        foreach($glyphImage in $glyphImages){Copy-Item -LiteralPath $glyphImage.FullName -Destination $glyphDestination}
        Copy-Item -LiteralPath (Join-Path $tests 'glyph-reference-cases.json') -Destination $glyphDestination
        if($TerminalSceneReference){Copy-Item -LiteralPath (Join-Path $tests 'terminal-reference-provenance.json') -Destination $glyphDestination}
    }
    if($NormalCatalogue){
        $gallery=$prefix+'-captures';New-Item -ItemType Directory -Path $gallery -Force | Out-Null
        $images=@(Get-ChildItem -LiteralPath $tests -Filter '*.png' -File)
        if($images.Count -ne $(if($WholeCatalogue){236}else{158})){throw 'Missing complete NORMAL gallery'}
        foreach($image in $images){Copy-Item -LiteralPath $image.FullName -Destination $gallery}
    }
    if($NormalRedesign){
        $redesignDestination=$prefix+'-captures';New-Item -ItemType Directory -Path $redesignDestination -Force | Out-Null
        $images=@(Get-ChildItem -LiteralPath $tests -Filter 'native-cantilever*.png' -File)
        if($images.Count -ne 6){throw 'Missing redesign captures'}
        foreach($image in $images){Copy-Item -LiteralPath $image.FullName -Destination $redesignDestination}
    }
    if($NormalNative){
        $nativeDestination=$prefix+'-captures';New-Item -ItemType Directory -Path $nativeDestination -Force | Out-Null
        $nativeImages=@(Get-ChildItem -LiteralPath $tests -Filter 'native-*.png' -File)
        if($nativeImages.Count -ne 11){throw 'Missing native NORMAL captures'}
        foreach($nativeImage in $nativeImages){Copy-Item -LiteralPath $nativeImage.FullName -Destination $nativeDestination}
    }
    if($NormalMatrix){
        $matrixDestination=$prefix+'-captures';New-Item -ItemType Directory -Path $matrixDestination -Force | Out-Null
        $matrixImages=@(Get-ChildItem -LiteralPath $tests -Filter '*.png' -File)
        if($matrixImages.Count -ne 54){throw 'Missing native GDI matrix captures'}
        foreach($matrixImage in $matrixImages){Copy-Item -LiteralPath $matrixImage.FullName -Destination $matrixDestination}
        Copy-Item -LiteralPath (Join-Path $tests 'normal-matrix-inputs.json') -Destination $matrixDestination
    }
    if($NormalReference){
        $referenceDestination=$prefix+'-references'
        New-Item -ItemType Directory -Path $referenceDestination -Force | Out-Null
        $referenceImages=@(Get-ChildItem -LiteralPath $tests -Filter '*.png' -File)
        if($referenceImages.Count -ne 54){throw 'Missing original GDI reference PNGs'}
        foreach($referenceImage in $referenceImages){Copy-Item -LiteralPath $referenceImage.FullName -Destination $referenceDestination}
        Copy-Item -LiteralPath (Join-Path $tests 'normal-reference-metrics.json') -Destination $referenceDestination
    }
    $log=Get-ChildItem -LiteralPath (Join-Path $runtime 'config/obs-studio/logs') -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $logText=[IO.File]::ReadAllText($log.FullName)
    $checkedLog=$logText;if($QuickConnection){$checkedLog=$checkedLog.Replace('[playing-next] Source configuration failed: No linked DJ; use pairing first','[playing-next] Expected unlinked state after forget')}
    if($checkedLog -match '\[playing-next\] (Source configuration failed|Render failed|Test capture failed|Remote fixture failed)'){throw 'Native source reported an error'}
    if($logText -match 'Failed to call timer_cb'){throw 'Unexpected Lua timer failure; log retained in isolated runtime'}
    Copy-Item -LiteralPath $log.FullName -Destination ($prefix+'.log')
    $allocations=if($logText -match 'Number of memory leaks: (\d+)'){[int]$Matches[1]}else{$null}
    [pscustomobject]@{Version=$Version;DllSHA256=(Get-FileHash -LiteralPath (Join-Path $tests 'obs-playing-next.dll')).Hash;Exit=$owned.ExitCode;OriginalSources=23;RenderTest=[bool]($Render -and -not $Baseline);LoadOnly=[bool]$LoadOnly;Baseline=[bool]$Baseline;FinalAllocationCounter=$allocations} | Export-Csv ($prefix+'.csv') -NoTypeInformation
    Write-Output "PASS OBS $Version; $([IO.File]::ReadAllText($result)); 23 originals; Exit=0"
}finally{if($null -ne $owned -and -not $owned.HasExited){Stop-Process -Id $owned.Id -Force}}
