# RP425 Bluetooth Native

A small x64 Windows print port monitor for printing to a Rongta RP425 over
classic Bluetooth Serial Port Profile (SPP). Use the printer from ordinary
Windows applications with an existing Rongta ZPL driver. No browser extension,
Python process, or sign-in task is required: Windows loads the monitor in its
print spooler.

This is an independent, experimental add-on, not an official Rongta driver.
It does not modify or redistribute Rongta's renderer, installer or catalog.

## Why this exists

Pairing the RP425 in Windows did not make Bluetooth printing reliable on the
setup that motivated this project. A queue using the installed Rongta-branded
Seagull driver could report a job as printed even when no label came out.
Correcting the outgoing COM port, restarting the spooler and disabling
bidirectional monitoring did not resolve that behavior.

The Chrome extension could print over its separate BLE connection, but required
manual reconnection and a browser-based workflow. The goal was to select the
printer in an ordinary Windows print dialog and let Windows manage the jobs.

The decisive test was sending the driver's exact rendered label directly to
the Bluetooth serial device: successive labels printed correctly. That showed
the existing driver could produce usable print data and that the printer could
receive it. The failing path was Windows' existing serial printing route; its
exact underlying defect was not established.

This monitor supplies an alternative delivery path inside the Windows spooler.
It keeps the existing driver for rendering, reconnects to the actual Bluetooth
serial device for each job, and reports transport failures back to the queue.
It replaces the temporary Python bridge used to prove that approach, so normal
printing no longer depends on a separate application running at sign-in.

Use it if your RP425 has this kind of Bluetooth delivery problem. If your
existing Windows Bluetooth queue already prints reliably, you do not need this
add-on. These findings come from one tested setup, not evidence that every
RP425 or Rongta driver has the same fault.

## Requirements

- Windows x64 with its classic Bluetooth stack and print spooler enabled.
- An RP425 paired in Windows with an **outgoing Bluetooth serial port**.
  Pair the classic device, rather than a name ending in `-BLE`.
- An installed v3 Rongta RP425 ZPL 203 DPI driver. Get vendor software from
  [Rongta's downloads](https://www.rongtatech.com/driver-download/2/).
- Administrator rights to install/remove a print monitor.
- Visual Studio 2022 C++ Build Tools and a Windows SDK to build from source.

One printer address is supported per monitor installation. v4/class drivers,
Windows Protected Print mode, BLE-only devices and ARM64 are not supported.
Managed security policy may prohibit custom unsigned user-mode monitor DLLs.
This project does not change signing policies or enable test-signing.

## Build and test

For installation without compiling, download a **windows-x64.zip** asset from
the repository's **Releases** page and follow [INSTALL.md](INSTALL.md). The asset
includes all required scripts and matching binaries. GitHub's **Source code**
ZIP and a DLL downloaded by itself are not ready-to-install packages.

To build from a source checkout, install Microsoft's C++ Build Tools with its x64 compiler and a Windows SDK,
then run from a normal 64-bit PowerShell terminal:

```powershell
./scripts/build.ps1
```

The script finds Build Tools using `vswhere`, builds with warnings treated as
errors, and runs native checks before recording `build/build-hash.json`.
Build outputs remain in the ignored `build/` directory. The DLL uses a static
C runtime and Windows system DLLs.

GitHub Actions builds and runs the same checks on Windows. CI does not install
the monitor or access Bluetooth hardware. Its artifact contains the DLL and
matching build hash and installation instructions, not a vendor-signed driver
package. Pushing a version tag builds/tests a new package and publishes an
experimental pre-release; see [release instructions](docs/RELEASING.md).

## Find your printer address and driver

These read-only PowerShell commands help identify the paired device and the
installed driver's exact name:

```powershell
Get-PnpDevice -Class Bluetooth |
    Where-Object FriendlyName -Like 'RP425*' |
    Select-Object FriendlyName, InstanceId

Get-PrinterDriver | Where-Object Name -Match 'RP425' |
    Select-Object Name, MajorVersion
```

The classic device instance contains `DEV_` followed by the 12-digit hexadecimal
Bluetooth address. Device Manager also exposes the hardware IDs. Do not use a
Bluetooth address belonging to headphones or another device.

## Install

Build locally first, or obtain a trusted DLL and its matching build hash. Place
them in `build/`. In **64-bit PowerShell as administrator**, replace the example
address below with your printer's address and use your installed driver's name:

```powershell
./scripts/install.ps1 -BluetoothAddress '001122AABBCC' `
    -DriverName 'RONGTA RP425(ZPL 203DPI)'
```

This creates **RP425 Bluetooth Native** on port **RP425-Bluetooth** and disables
bidirectional status handling for that queue. It registers the DLL through the
Windows AddMonitor API and saves the address under the monitor's protected HKLM
registry key. Existing printer queues and Python tasks are not changed.

Set the new queue's paper size and printing preferences to match your labels.
Print a label twice as separate jobs before relying on it. The optional
`-PrinterName` parameter selects a different name for the new queue.

The DLL runs in a privileged printing process. Review source and use trusted
builds; a defective monitor can affect the spooler and other printers.
The hash check detects a mismatched build, not publisher identity.

## Remove or upgrade

Wait for print jobs to finish, then run in an administrator terminal:

```powershell
./scripts/uninstall.ps1
```

If you used a different queue name, pass the same `-PrinterName`. Removal checks
for other queues using the port and unfinished jobs, unregisters the monitor,
and restarts the spooler to unload its DLL. That restart briefly interrupts
printer availability. Reinstall after removal to change the address or DLL.

## How it works

The signed printer driver renders ZPL. The monitor exposes a fixed port using
Windows' `MONITOR2` interface. At job start it resolves the configured address
to the current Bluetooth serial device, avoiding DOS COM names that the spooler
may redirect to its own pipes. It opens the actual device, uses a five-second
write timeout, sends paced chunks, and closes the connection at job end.

Successful writes and partial writes are counted. Transport failures return
failure to Windows rather than silently discarding the job. Ordinary Windows
queue error/retry handling applies. No background listener or network port is used.

## Validation and limits

The original fixed-address version printed successive text and normal PDF labels
on one Windows x64/RP425 setup. It also printed after a spooler restart and a
printer power cycle. The first PDF after the power cycle was delayed; the cause
of that delay remains unresolved.

This public configurable variant builds and passes native checks, but has not
yet been installed on hardware. Full PC reboot recovery, physical interruption
mid-job and other adapters/printer firmware remain unverified.

- No bidirectional paper/cover/status reporting.
- A job is limited to 32 MiB. Paced writes can make large raster jobs slow.
- Bluetooth write success is not proof that paper physically printed.
- Interrupted transmission or Windows retries can produce partial/duplicate
  labels. Inspect output before retrying or restarting the spooler.
- Native checks mock transmission; they verify partial writes, failure handling,
  enumeration, ABI layout and address validation, not RF or printer behavior.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Keep changes small and include meaningful
checks. Review transport changes on a separate printer queue before deployment.

License: [MIT](LICENSE). Windows SDKs and vendor drivers retain their own licenses
and are not included in this repository.

## Android companion and credits

For Android, use [RP425 RLabel Handoff](https://github.com/themegaphoenix/rongta-rp425-android-rlabel-handoff): it sends system print documents to RLabel without manually saving the PDF. RLabel handles cropping and printing.

Co-created by [themegaphoenix](https://github.com/themegaphoenix) and **ChatGPT**. See [AUTHORS.md](AUTHORS.md).
