param(
    [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{12}$')][string]$BluetoothAddress,
    [Parameter(Mandatory)][string]$DriverName,
    [string]$PrinterName = 'RP425 Bluetooth Native'
)
$ErrorActionPreference = 'Stop'
if (-not [Environment]::Is64BitProcess) { throw 'Run 64-bit PowerShell as administrator.' }
. (Join-Path $PSScriptRoot 'monitor-install-api.ps1')
$root = Split-Path $PSScriptRoot
$dll = Join-Path $root 'build\RongtaRp425Bluetooth.dll'
$destination = Join-Path $env:WINDIR 'System32\RongtaRp425Bluetooth.dll'
$key = 'HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\RP425 Bluetooth Native'
$address = $BluetoothAddress.ToUpperInvariant()
$driver = Get-PrinterDriver -Name $DriverName -ErrorAction Stop
if ($driver.MajorVersion -ne 3) { throw 'This monitor requires an existing v3 printer driver.' }
if (Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue) { throw 'That printer name already exists; choose a new name.' }
$expected = (Get-Content -LiteralPath (Join-Path $root 'build\build-hash.json') -Raw | ConvertFrom-Json).Hash
if ((Get-FileHash -LiteralPath $dll).Hash -ne $expected) { throw 'DLL differs from the tested build.' }
if (Test-Path -LiteralPath $key) { throw 'This monitor is already installed; uninstall the previous build before replacing it.' }
if (Test-Path -LiteralPath $destination) {
    if ((Get-FileHash -LiteralPath $destination).Hash -ne $expected) { throw 'A different DLL already exists in System32; resolve the previous installation first.' }
} else { Copy-Item -LiteralPath $dll -Destination $destination }
$added = $false
$queueAdded = $false
try {
    [RpMonitorInstall]::Install(); $added = $true
    New-ItemProperty -LiteralPath $key -Name BluetoothAddress -PropertyType String -Value $address -Force | Out-Null
    Add-Printer -Name $PrinterName -DriverName $DriverName -PortName 'RP425-Bluetooth'
    $queueAdded = $true
    [void][RpMonitorInstall]::Bidirectional($PrinterName, $false)
    Get-Printer -Name $PrinterName | Select-Object Name,DriverName,PortName,PrinterStatus
} catch {
    if ($queueAdded) { Remove-Printer -Name $PrinterName }
    if ($added) { [RpMonitorInstall]::Uninstall() }
    throw
}
