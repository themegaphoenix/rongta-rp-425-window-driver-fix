$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$output = Join-Path $root 'build'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ Build Tools and a Windows SDK first.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Microsoft x64 C++ Build Tools were not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
$commands = @"
@echo off
call "$vcvars"
if errorlevel 1 exit /b 1
cd /d "$output"
cl /nologo /std:c++17 /O2 /W4 /WX /MT /guard:cf /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 /LD "$root\src\rp425_monitor.cpp" /link /OUT:RongtaRp425Bluetooth.dll /EXPORT:InitializePrintMonitor2 /DYNAMICBASE /NXCOMPAT /GUARD:CF advapi32.lib spoolss.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /O2 /W4 /WX /MT /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 /I"$root\src" "$root\tests\monitor_tests.cpp" /Fe:monitor_tests.exe /link advapi32.lib spoolss.lib
if errorlevel 1 exit /b 1
monitor_tests.exe
if errorlevel 1 exit /b 1
dumpbin /exports RongtaRp425Bluetooth.dll
"@
$batch = Join-Path $output 'build.cmd'
Set-Content -LiteralPath $batch -Value $commands
& $env:ComSpec /d /c "`"$batch`""
if ($LASTEXITCODE) { throw "Native build/test failed: $LASTEXITCODE" }
Get-FileHash -LiteralPath (Join-Path $output 'RongtaRp425Bluetooth.dll') | Select-Object Hash | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'build-hash.json')
