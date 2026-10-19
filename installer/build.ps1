param([string]$OutputRoot='work/phase28-installer',[string]$NativeRoot='work/phase27-native-rc',[string]$CompanionRoot='work/phase27-companion-rc/OBS-Playing-Companion-test-x64')
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath((Join-Path $workspace $OutputRoot))
if(-not $out.StartsWith((Join-Path $workspace 'work')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New work output required'}
New-Item -ItemType Directory $out | Out-Null
$native=(Resolve-Path (Join-Path $workspace $NativeRoot)).Path
$companion=(Resolve-Path (Join-Path $workspace $CompanionRoot)).Path
# Verify frozen RC inputs before selecting executable files; don't embed development sources.
foreach($root in @($native,$companion)){
 foreach($entry in (Import-Csv (Join-Path $root 'SHA256SUMS.csv'))){
  $file=[IO.Path]::GetFullPath((Join-Path $root $entry.Path))
  if(-not $file.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash $file).Hash -ne $entry.SHA256){throw 'RC manifest mismatch'}
 }
}
foreach($role in @('native','companion')){New-Item -ItemType Directory (Join-Path $out $role)|Out-Null}
Copy-Item "$native/obs-plugins" "$out/native/obs-plugins" -Recurse
Copy-Item "$native/docs" "$out/native/docs" -Recurse
Get-ChildItem "$native/source/shared" -File | Where-Object Name -Match '^(LICENSE|COPYING)' | ForEach-Object {Copy-Item $_.FullName (Join-Path "$out/native/docs" ('Shared-'+$_.Name))}
foreach($file in @('obs-playing-companion.exe','obs-playing-db.dll','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll','vcruntime140.dll','vcruntime140_1.dll','qt.conf','LICENSE','THIRD-PARTY-NOTICES.txt')){Copy-Item (Join-Path $companion $file) "$out/companion"}
foreach($dir in @('plugins','licenses','sbom','docs')){Copy-Item (Join-Path $companion $dir) "$out/companion/$dir" -Recurse}
foreach($role in @('native','companion')){Compress-Archive -Path "$out/$role/*" -DestinationPath "$out/$role.zip"}
$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$exe=Join-Path $out 'OBS-Playing-NEXUS-Setup-1.0.0-rc3.5.exe'
$icon=Join-Path $workspace 'playing-next/packaging/branding/nexus-app.ico'
$arguments=@('/nologo','/target:winexe','/platform:x64','/optimize+',"/win32icon:$icon","/resource:$icon,app.ico","/out:$exe",'/reference:System.dll','/reference:System.Core.dll','/reference:System.Drawing.dll','/reference:System.Windows.Forms.dll','/reference:System.Web.Extensions.dll','/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll',"/resource:$out/native.zip,native.zip","/resource:$out/companion.zip,companion.zip","/resource:$workspace/playing-next/packaging/branding/nexus-installer.png,branding.png",('/win32manifest:'+(Join-Path $PSScriptRoot 'app.manifest')),(Join-Path $PSScriptRoot 'Setup.cs'))
$arguments += (Join-Path $PSScriptRoot 'I18n.cs')
& $compiler @arguments
if($LASTEXITCODE -ne 0){throw 'Installer compile failed'}
# Corresponding sources are delivered separately beside the installer, not installed into OBS.
New-Item -ItemType Directory "$out/sources" | Out-Null
Copy-Item "$companion/source/*" "$out/sources" -Recurse -Force
Copy-Item "$native/source/*" "$out/sources" -Recurse -Force
Copy-Item "$companion/licenses" "$out/sources/qt-licenses" -Recurse
Copy-Item "$companion/sbom" "$out/sources/sbom" -Recurse
Copy-Item $PSScriptRoot "$out/sources/installer" -Recurse
Copy-Item "$PSScriptRoot/*" "$out/sources/playing-next/installer" -Recurse -Force
Copy-Item "$workspace/playing-next/CORRECCIONES.md" "$out/sources/playing-next/CORRECCIONES.md" -Force
Compress-Archive -Path "$out/sources/*" -DestinationPath "$out/OBS-Playing-NEXUS-1.0.0-rc3.5-Sources.zip"
Get-Item $exe,"$out/native.zip","$out/companion.zip","$out/OBS-Playing-NEXUS-1.0.0-rc3.5-Sources.zip" | ForEach-Object{[pscustomobject]@{File=$_.Name;Bytes=$_.Length;SHA256=(Get-FileHash $_.FullName).Hash}} | ConvertTo-Json | Set-Content "$out/build-manifest.json"
Get-Content "$out/build-manifest.json"
