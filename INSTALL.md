# Install the downloaded release

This experimental Windows x64 add-on needs administrator rights and an existing
v3 Rongta RP425 ZPL driver. It is unsigned and does not change signing policies.
No compiler, SDK, Python or browser extension is needed to use the release ZIP.

## 1. Prepare Windows and the printer

Install the vendor's RP425 ZPL 203 DPI driver. Pair the powered-on printer in
Windows Bluetooth settings using its classic device name, not a `-BLE` entry.
Verify that Windows created an outgoing Bluetooth COM port. A BLE-only connection
is not sufficient.

This project supports one printer address per installation. ARM64, v4/class
drivers and Windows Protected Print mode are unsupported.

## 2. Download and extract

Download the `rp425-bluetooth-native-vX.Y.Z-windows-x64.zip` asset from the
repository's **Releases** page, plus its `.sha256` file. Use this asset rather
than GitHub's automatically generated **Source code** ZIP.

Optionally compare the ZIP's hash with the downloaded checksum:

```powershell
Get-FileHash './rp425-bluetooth-native-vX.Y.Z-windows-x64.zip' -Algorithm SHA256
```

The checksum detects a damaged/mismatched download; it is not a publisher
signature. Review source and obtain builds from a trusted source.

In the downloaded ZIP's Properties dialog, select **Unblock** if shown before
extracting it. Alternatively, use `Unblock-File` on that specific ZIP. Extract
the entire archive, preserving its `build/` and `scripts/` folders. Do not copy
only the DLL or run scripts from inside the ZIP.

## 3. Find the address and driver name

Open **64-bit Windows PowerShell as administrator**, change to the extracted
folder, then run these read-only commands:

```powershell
Get-PnpDevice -Class Bluetooth |
    Where-Object FriendlyName -Like 'RP425*' |
    Select-Object FriendlyName, InstanceId

Get-PrinterDriver | Where-Object Name -Match 'RP425' |
    Select-Object Name, MajorVersion
```

The classic printer instance contains `DEV_` followed by its 12 hexadecimal
address digits. Use those digits without colons or hyphens. Choose the exact
installed **ZPL 203 DPI** driver name; its `MajorVersion` must be 3.

## 4. Install and print

Replace the example address and driver name with your own:

```powershell
./scripts/install.ps1 -BluetoothAddress '001122AABBCC' `
    -DriverName 'RONGTA RP425(ZPL 203DPI)'
```

Installation creates **RP425 Bluetooth Native**. It does not change existing
queues. In Printing Preferences, set the paper size to your actual labels.
Select this new queue from your usual PDF app and print one label, then send
a second separate job. Windows loads the monitor automatically through its
spooler; no sign-in application needs to be started.

If PowerShell or organizational security policy blocks the scripts/DLL, inspect
that policy with your administrator. Do not disable signature enforcement or
enable test-signing to install this add-on.

## Errors, upgrades and removal

Opening/writing failures are returned to Windows. Check the queue for errors,
ensure the printer is on and nearby, and close other applications using its
Bluetooth connection. Paper/cover status is not reported. A successful write
does not prove physical printing, and large raster jobs can be slow. Printing
after a power cycle was observed with a delay on the original test setup.

Inspect labels before retrying an interrupted job: partial or duplicate output
is possible. Cancel unwanted retries in Windows' print queue.

To remove the add-on, finish or cancel its jobs and wait for other queues to be
idle, then run as administrator:

```powershell
./scripts/uninstall.ps1
```

This removes the new queue/monitor and briefly restarts the spooler to unload
the DLL. Uninstall the old build before installing an upgrade. If you installed
with a custom `-PrinterName`, pass that same name to uninstall.

The public configurable build passes automated checks but still needs broader
hardware validation. Full Windows reboot and interrupted physical transmission
remain unverified. Keep a working alternative available during testing.
