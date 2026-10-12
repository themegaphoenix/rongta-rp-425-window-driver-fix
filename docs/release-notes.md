Experimental, unsigned x64 Windows Bluetooth port monitor for the Rongta RP425.

Download the **windows-x64.zip** asset, extract the complete archive, and follow
**INSTALL.md**. The ZIP includes the DLL, matching build hash, install/uninstall
scripts and instructions. A compiler or Windows SDK is not needed to install it.

Requires an existing v3 RP425 ZPL driver, classic Bluetooth SPP pairing and
administrator rights. No vendor binaries are modified or redistributed.

Automated build/native checks passed. These checks do not validate Bluetooth
hardware. The original fixed-address implementation printed successive PDFs,
survived spooler reload and printed after a printer power cycle with an initial
delay. The configurable public variant, full PC reboot recovery and interrupted
physical transmission still need hardware validation. Bidirectional status is
unsupported, and interrupted/retried jobs can produce duplicate labels.
