# Security

The DLL runs inside Windows' privileged print spooler. Administrator-controlled
installation and registry settings select the printer. The monitor only resolves
the paired classic Bluetooth serial service and validates device-path prefixes;
it does not expose a TCP listener or download/execute printer-provided code.

Report a reproducible issue with affected functions and a minimal sanitized test.
Do not post an exploit or private label data in a public issue. Before publishing,
the repository owner should enable GitHub private vulnerability reporting so
security reports can be sent privately. No public-repository reporting endpoint
exists yet.

Use trusted builds. Local hashes do not establish publisher identity. Do not
disable Windows signing/security policies to work around an installation failure.
