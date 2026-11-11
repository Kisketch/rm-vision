# Security policy

## Reporting a vulnerability

Please report suspected vulnerabilities privately through GitHub's
**Report a vulnerability** button (Security → Advisories) on this
repository, rather than opening a public issue. Include the affected
component, a minimal reproduction, and the version/commit you tested.

## Scope notes

This is competition robot-control software. The safety-relevant
surfaces are the serial protocol handling (untrusted bytes from the
MCU link), configuration parsing, and the detection-cache format.
Known protocol limitations (CRC off by default pending MCU firmware,
no MCU timestamp) are documented engineering constraints tracked in
`docs/` and the on-robot trial checklist — they are known issues, not
undisclosed vulnerabilities, but reports of *new* parsing or memory
safety defects in those paths are very welcome. The CI runs an
ASan+UBSan job on every push for exactly this reason.

## Supported versions

Only the current `main` branch is maintained.
