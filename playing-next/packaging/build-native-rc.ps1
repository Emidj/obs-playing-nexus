param([string]$OutputRoot='work/phase27-native-rc')
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath((Join-Path $workspace $OutputRoot))
if(-not $out.StartsWith((Join-Path $workspace 'work')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)){throw 'New output under work required'}
New-Item -ItemType Directory -Path $out | Out-Null
function Payload($from,$relative){$to=Join-Path $out $relative;New-Item -ItemType Directory -Path (Split-Path $to) -Force | Out-Null;Copy-Item -LiteralPath $from -Destination $to}
$bin='obs-plugins/64bit'
Payload (Join-Path $workspace 'work/core-phase08h-build/RelWithDebInfo/obs-playing-next.dll') "$bin/obs-playing-next.dll"
Payload (Join-Path $workspace 'obs-playing/obs-playing-db.dll') "$bin/obs-playing-db.dll"
$qt='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64'
Payload "$qt/plugins/tls/qschannelbackend.dll" "$bin/playing-qt-plugins/tls/qschannelbackend.dll"
foreach($file in @('LICENSE','THIRD-PARTY-NOTICES.txt','playing-next/THIRD-PARTY-NOTICES.md','playing-next/COMPATIBILITY.md','playing-next/HARDENING.md','playing-next/TRAKTOR.md','playing-next/RELEASE-1.0.0.md')){Payload (Join-Path $workspace $file) ('docs/'+(Split-Path $file -Leaf))}
foreach($name in @('README.md','README.html','README.en.md','README.en.html')){Payload (Join-Path $workspace "playing-next/docs/companion/$name") "docs/companion/$name"}
foreach($image in (Get-ChildItem (Join-Path $workspace 'playing-next/docs/companion/imagenes') -File)){Payload $image.FullName ('docs/companion/imagenes/'+$image.Name)}
Copy-Item -LiteralPath (Join-Path $workspace 'playing-next') -Destination (Join-Path $out 'source/playing-next') -Recurse
$shared='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays'
foreach($lock in @('sources.lock.json','renderer.lock.json')){
 foreach($entry in (Get-Content (Join-Path $workspace "playing-next/tests/visual-audit/$lock") -Raw|ConvertFrom-Json)){
 $from=Join-Path $shared $entry.path
 if((Get-FileHash -LiteralPath $from).Hash -ne $entry.sha256){throw "Shared hash mismatch $($entry.path)"}
 Payload $from ('source/shared/'+$entry.path)
 }
}
foreach($file in (Get-ChildItem -LiteralPath $shared -File | Where-Object Name -Match '^(LICENSE|COPYING)')){Payload $file.FullName ('source/shared/'+$file.Name)}
@'
OBS Playing NEXUS 1.0.0 — Windows x64 / OBS 32.x.
No es instalador. Payload plugin: obs-plugins/64bit; no incluye Qt de OBS.
Companion separado en PC DJ; fuente nativa en PC streaming.
Conservar versiones previas y configuración. No copiar herramientas de prueba.
Consulte docs/RELEASE-1.0.0.md para pruebas realizadas y seguimiento posterior.
El cierre estable no certifica todas las versiones, equipos ni redes.
Código combinado del renderer GPL-2.0-or-later; código propio MIT.
Fuentes Playing y motor auditado incluidas. Qt/licencias/receta se suministran
junto al paquete Companion asociado. Libobs/Qt SDK del OBS elegido para rebuild.
CMake: PLAYING_BUILD_OBS_SOURCE=ON, PLAYING_BUILD_SHARED_VISUAL=ON,
PLAYING_SHARED_ROOT=<source/shared>, libobs_DIR y Qt6_DIR explícitos.
PLAYING_SOURCE_TEST_HOOKS=OFF. Consulte source/playing-next/README.md.
'@ | Set-Content (Join-Path $out 'LEEME.txt') -Encoding utf8
$rows=Get-ChildItem $out -Recurse -File|ForEach-Object{[pscustomobject]@{Path=$_.FullName.Substring($out.Length+1);Bytes=$_.Length;SHA256=(Get-FileHash $_.FullName).Hash}}
$rows|Sort-Object Path|Export-Csv (Join-Path $out 'SHA256SUMS.csv') -NoTypeInformation
Compress-Archive -Path "$out/*" -DestinationPath ($out+'.zip')
Get-FileHash ($out+'.zip')
