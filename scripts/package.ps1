param([ValidatePattern('^(local|ci|v\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?)$')][string]$Version = 'local')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$build = Join-Path $root 'build'
$expected = (Get-Content -LiteralPath (Join-Path $build 'build-hash.json') -Raw | ConvertFrom-Json).Hash
if ((Get-FileHash -LiteralPath (Join-Path $build 'RongtaRp425Bluetooth.dll')).Hash -ne $expected) { throw 'DLL differs from the tested build.' }
$files = @('build/RongtaRp425Bluetooth.dll','build/build-hash.json',
    'scripts/install.ps1','scripts/uninstall.ps1','scripts/monitor-install-api.ps1',
    'INSTALL.md','README.md','LICENSE','SECURITY.md','CONTRIBUTING.md')
foreach ($file in $files) { if (-not (Test-Path -LiteralPath (Join-Path $root $file) -PathType Leaf)) { throw "Missing package input: $file" } }
$release = Join-Path $build 'release'
New-Item -ItemType Directory -Path $release -Force | Out-Null
$name = "rp425-bluetooth-native-$Version-windows-x64.zip"
$path = Join-Path $release $name
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Open($path, [IO.FileMode]::Create)
$archive = $null
try {
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
    foreach ($file in $files) { [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, (Join-Path $root $file), $file, [IO.Compression.CompressionLevel]::Optimal) | Out-Null }
} finally { if ($archive) { $archive.Dispose() }; $stream.Dispose() }
$hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($path + '.sha256', "$hash  $name`n", [Text.UTF8Encoding]::new($false))
Get-Item -LiteralPath $path,($path + '.sha256') | Select-Object Name,Length
