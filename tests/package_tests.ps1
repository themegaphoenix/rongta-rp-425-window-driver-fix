param([Parameter(Mandatory)][string]$Version)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$name = "rp425-bluetooth-native-$Version-windows-x64.zip"
$path = Join-Path $root "build\release\$name"
$checksum = (Get-Content -LiteralPath ($path + '.sha256') -Raw).Trim() -split '\s+'
if ($checksum.Count -ne 2 -or $checksum[1] -ne $name -or $checksum[0] -ne (Get-FileHash -LiteralPath $path).Hash) { throw 'Release ZIP checksum mismatch.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($path)
try {
    $expected = @('build/RongtaRp425Bluetooth.dll','build/build-hash.json',
        'scripts/install.ps1','scripts/uninstall.ps1','scripts/monitor-install-api.ps1',
        'INSTALL.md','README.md','LICENSE','SECURITY.md','CONTRIBUTING.md')
    $actual = @($archive.Entries.FullName)
    if ($actual.Count -ne $expected.Count -or (Compare-Object $expected $actual)) { throw 'Unexpected or missing package files.' }
    $reader = [IO.StreamReader]::new($archive.GetEntry('build/build-hash.json').Open())
    try { $hash = ($reader.ReadToEnd() | ConvertFrom-Json).Hash } finally { $reader.Dispose() }
    $dllStream = $archive.GetEntry('build/RongtaRp425Bluetooth.dll').Open()
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $dllHash = [BitConverter]::ToString($sha.ComputeHash($dllStream)).Replace('-', '') }
    finally { $dllStream.Dispose(); $sha.Dispose() }
    if ($dllHash -ne $hash) { throw 'Packaged DLL does not match its build hash.' }
} finally { $archive.Dispose() }
$rejected = $false
try { & (Join-Path $root 'scripts\package.ps1') -Version '../escape' }
catch {
    if ($_.FullyQualifiedErrorId -notlike 'ParameterArgumentValidationError*') { throw }
    $rejected = $true
}
if (-not $rejected) { throw 'Unsafe version input was accepted.' }
'Package checks passed: layout, ZIP checksum, DLL hash and version validation.'
