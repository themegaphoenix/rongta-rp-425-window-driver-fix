# Contributing

Run `./scripts/build.ps1` before submitting changes. The native checks must pass
with compiler warnings treated as errors. Test altered transport behavior using
a separate queue; keep a known-working printer configuration available.

For hardware reports, include Windows version/architecture, Bluetooth adapter,
RP425 firmware if known, exact driver name/version, whether it failed before or
after power-off/on, and the Windows queue error. Say whether the same rendered
data prints when sent directly. Report observed behavior separately from guesses.

Remove Bluetooth addresses, usernames, machine names, label contents and local
paths from logs. Do not attach shipping labels, spool files, proprietary driver
packages or vendor binaries. Microsoft SDK headers/libraries and build tools
also belong outside the repository.

Useful validation includes separate successive jobs, printer power cycles,
spooler reload, Windows reboot, printer-off retries, cancellation and partial
transmission. Check for duplicate output when testing errors.
