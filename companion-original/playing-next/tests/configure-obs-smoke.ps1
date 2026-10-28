param([Parameter(Mandatory=$true)][string]$PortableRoot)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$allowed = [IO.Path]::GetFullPath((Join-Path $workspace 'work')).TrimEnd('\') + '\'
$resolved = [IO.Path]::GetFullPath($PortableRoot)
if (-not $resolved.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Only an isolated OBS runtime under this project work directory is allowed.'
}
foreach ($name in @('global.ini','user.ini')) {
    $path = Join-Path $resolved ('config\obs-studio\' + $name)
    if (-not (Test-Path -LiteralPath $path)) { throw 'Prepare the isolated portable configuration first.' }
    $text = [IO.File]::ReadAllText($path)
    if ($text -match '(?m)^ConfirmOnExit=.*$') {
        $text = [regex]::Replace($text, '(?m)^ConfirmOnExit=.*$', 'ConfirmOnExit=false')
    } else {
        $text = [regex]::Replace($text, '(?m)^\[General\]\r?\n', "[General]`nConfirmOnExit=false`n")
    }
    [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
}
