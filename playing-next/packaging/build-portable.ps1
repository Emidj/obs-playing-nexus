param(
    [string]$BuildRoot='work/core-phase08h-build',
    [string]$QtRoot='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64',
    [string]$CrtRoot='C:/BuildTools/VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT',
    [string]$SourcesRoot='work/phase08j-sources',
    [string]$OutputRoot='work/phase08j-portable'
)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$allowed=(Join-Path $workspace 'work').TrimEnd('\')+'\'
$output=[IO.Path]::GetFullPath((Join-Path $workspace $OutputRoot))
if(-not $output.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $output)) {throw 'Output must be a new directory under workspace work'}
$BuildRoot=(Resolve-Path -LiteralPath $BuildRoot).Path
$QtRoot=(Resolve-Path -LiteralPath $QtRoot).Path
$CrtRoot=(Resolve-Path -LiteralPath $CrtRoot).Path
$SourcesRoot=(Resolve-Path -LiteralPath $SourcesRoot).Path
$expected=@{'qtbase-everywhere-src-6.8.3.zip'='992BF7766E214A341EF793EB3665FB784787D2FD666955F5F507F4C6F1F770DD';'qtimageformats-everywhere-src-6.8.3.zip'='30B81E4F641CDBD608AE8C01A48677338D1C69545117C5552063B63262C7EB1B'}
foreach($name in $expected.Keys) {if((Get-FileHash -LiteralPath (Join-Path $SourcesRoot $name)).Hash -ne $expected[$name]) {throw "Source mismatch: $name"}}
if((Get-FileHash -LiteralPath (Join-Path $workspace 'obs-playing/obs-playing-db.dll')).Hash -ne '5030DECC6D914539E3B9B7E28AA4F6DE1E7161DAC6FD4B21EB02E6754D9B175E') {throw 'Database dependency mismatch'}
$package=Join-Path $output 'OBS-Playing-Companion-test-x64'
New-Item -ItemType Directory -Path $package | Out-Null
function Copy-Payload([string]$source,[string]$relative) {
    $target=Join-Path $package $relative; New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target
}
Copy-Payload (Join-Path $BuildRoot 'RelWithDebInfo/obs-playing-companion.exe') 'obs-playing-companion.exe'
Copy-Payload (Join-Path $workspace 'obs-playing/obs-playing-db.dll') 'obs-playing-db.dll'
foreach($name in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll')) {Copy-Payload (Join-Path $QtRoot "bin/$name") $name}
foreach($relative in @('platforms/qwindows.dll','tls/qschannelbackend.dll','imageformats/qjpeg.dll','imageformats/qwebp.dll')) {Copy-Payload (Join-Path $QtRoot "plugins/$relative") "plugins/$relative"}
foreach($name in @('msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll','vcruntime140.dll','vcruntime140_1.dll')) {Copy-Payload (Join-Path $CrtRoot $name) $name}
@('[Paths]','Prefix=.','Plugins=plugins') | Set-Content -LiteralPath (Join-Path $package 'qt.conf') -Encoding ascii
@('@echo off','start "" "%~dp0obs-playing-companion.exe" --profile-dir "%~dp0perfil-PC-DJ"') | Set-Content -LiteralPath (Join-Path $package 'Abrir Companion PC DJ.cmd') -Encoding ascii
Copy-Payload (Join-Path $PSScriptRoot 'GUIA-DOS-PCS.txt') 'GUIA-DOS-PCS.txt'
foreach($name in @('README.md','README.html','README.en.md','README.en.html')){Copy-Payload (Join-Path $workspace "playing-next/docs/companion/$name") "docs/companion/$name"}
foreach($image in (Get-ChildItem (Join-Path $workspace 'playing-next/docs/companion/imagenes') -File)){Copy-Payload $image.FullName ('docs/companion/imagenes/'+$image.Name)}
Copy-Payload (Join-Path $workspace 'THIRD-PARTY-NOTICES.txt') 'THIRD-PARTY-NOTICES.txt'
Copy-Payload (Join-Path $workspace 'LICENSE') 'LICENSE'
Copy-Payload (Join-Path $workspace 'playing-next/THIRD-PARTY-NOTICES.md') 'licenses/Companion-notices.md'
foreach($name in $expected.Keys) {Copy-Payload (Join-Path $SourcesRoot $name) "source/$name"}
Copy-Payload (Join-Path $SourcesRoot 'obs-deps-2025-07-11.zip') 'source/obs-deps-2025-07-11.zip'
foreach($module in @('qtbase','qtimageformats')) {Copy-Payload (Join-Path $QtRoot "sbom/$module-6.8.3.spdx") "sbom/$module-6.8.3.spdx"}
# Full license texts and component attributions from the exact source archives.
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach($module in @('qtbase','qtimageformats')) {
    $zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $SourcesRoot "$module-everywhere-src-6.8.3.zip"))
    try {foreach($entry in $zip.Entries) {
        if($entry.Length -gt 0 -and ($entry.FullName -match '/LICENSES/[^/]+$' -or $entry.FullName -match '/qt_attribution\.json$' -or $entry.FullName -match '/(LICENSE[^/]*|COPYING[^/]*)$')) {
            $relative=$entry.FullName.Substring($entry.FullName.IndexOf('/')+1)
            if($relative.Contains('..')) {throw 'Unsafe source path'}
            $target=Join-Path $package "licenses/$module/$relative"; New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
        }
    }} finally {$zip.Dispose()}
}
Copy-Item -LiteralPath (Join-Path $workspace 'playing-next') -Destination (Join-Path $package 'source/playing-next') -Recurse
Copy-Payload (Join-Path $workspace 'LICENSE') 'source/LICENSE'
@'
Build own Companion: Visual Studio 2022 C++ x64 and CMake >=3.28.
cmake -S playing-next -B build -A x64 -DBUILD_TESTING=OFF -DPLAYING_BUILD_COMPANION=ON -DPLAYING_BUILD_OBS_PROBE=OFF -DQt6_DIR=<Qt6/lib/cmake/Qt6>
cmake --build build --config RelWithDebInfo
Use a Qt6 SDK with Core/Network/Gui/Widgets and Schannel. Sources and licenses of
Qt 6.8.3 modules are included in the adjacent ZIPs. OBS dependency build scripts,
configuration, original archive checksums and patches: obs-deps-2025-07-11.zip,
deps.qt/qt6.ps1 and Build-Dependencies.ps1. The Windows recipe uses these exact
source archive hashes. SBOM records the source revision metadata from the archive.
Qt is dynamically linked and its DLLs/plugins may be replaced with compatible
modified builds. Reverse engineering for debugging those modifications is allowed.
Microsoft runtime DLLs remain unmodified proprietary redistributables from
VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT; UCRT is supplied by Windows.
https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution
Do not redistribute a user's profile.pcs: DPAPI binds it to that Windows user.
This test package is not a final installer or certification on a second PC.
'@ | Set-Content -LiteralPath (Join-Path $package 'source/REBUILD.txt') -Encoding utf8
$manifest=Get-ChildItem -LiteralPath $package -File -Recurse | ForEach-Object {[pscustomobject]@{Path=$_.FullName.Substring($package.Length+1).Replace('\','/');Bytes=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}}
$manifest | Sort-Object Path | Export-Csv -LiteralPath (Join-Path $package 'SHA256SUMS.csv') -NoTypeInformation -Encoding utf8
$zipPath=Join-Path $output 'OBS-Playing-Companion-test-x64.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($package,$zipPath)
Get-FileHash -LiteralPath $zipPath | Select-Object Hash,Path
