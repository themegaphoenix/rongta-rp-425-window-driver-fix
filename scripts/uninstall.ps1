param([string]$PrinterName = 'RP425 Bluetooth Native')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'monitor-install-api.ps1')
$queue = Get-Printer -Name $PrinterName -ErrorAction SilentlyContinue
if ($queue) {
    if ($queue.PortName -ne 'RP425-Bluetooth') { throw 'The named queue does not use this monitor; it was not removed.' }
    if (Get-PrintJob -PrinterName $PrinterName) { throw 'Finish or cancel jobs before uninstalling.' }
    Remove-Printer -Name $PrinterName
}
if (Get-Printer | Where-Object PortName -eq 'RP425-Bluetooth') { throw 'Other queues still use this port; remove or move them first.' }
$unfinished = @(Get-Printer | ForEach-Object { Get-PrintJob -PrinterName $_.Name -ErrorAction SilentlyContinue } | Where-Object JobStatus -notmatch 'Printed')
if ($unfinished.Count) { throw 'Other jobs are unfinished; retry uninstall after they finish.' }
[RpMonitorInstall]::Uninstall()
Restart-Service Spooler
Remove-Item -LiteralPath (Join-Path $env:WINDIR 'System32\RongtaRp425Bluetooth.dll')
