param(
    [string]$BuildRoot = 'work/core-phase08-build',
    [string]$QtRoot = 'C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64'
)
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$build = (Resolve-Path -LiteralPath $BuildRoot).Path
$allowed = Join-Path $workspace 'work'
if (-not $build.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Preview build must be under this workspace work directory'
}
$qt = (Resolve-Path -LiteralPath $QtRoot).Path
$executable = Join-Path $build 'RelWithDebInfo/obs-playing-companion.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Build Companion first' }
if (-not (Test-Path -LiteralPath (Join-Path $qt 'plugins/platforms/qwindows.dll') -PathType Leaf)) { throw 'Qt Windows platform unavailable' }
$profile = Join-Path $build 'companion-preview'
$database = Join-Path $workspace 'obs-playing/obs-playing-db.dll'
$oldPath = $env:PATH
$oldPlatform = $env:QT_QPA_PLATFORM
$oldPluginPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
try {
    $env:PATH = (Join-Path $qt 'bin') + [IO.Path]::PathSeparator + $oldPath
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $qt 'plugins/platforms'
    & $executable --profile-dir $profile --db-library $database
    if ($LASTEXITCODE -ne 0) { throw 'Companion preview failed' }
} finally {
    $env:PATH = $oldPath
    $env:QT_QPA_PLATFORM = $oldPlatform
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $oldPluginPath
}
