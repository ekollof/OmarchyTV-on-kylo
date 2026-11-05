# Broadcast service foundation evidence

Interpretation: [BROADCAST-FOUNDATION](../../reports/BROADCAST-FOUNDATION-2026-11-05.md).

- `check.txt`: host fixture test for tuner/EPG/PVR contracts.
- `full-host-summary.txt`: full host **412 ok**, `verify.cjs` **45 groups**.
- `identities.txt`: fixture module, test binary and host hashes.

The fixture is test-only and publishes the project-internal `tuner`, `epg` and
`pvr` services. No hardware, vendor EPG/EIT, recording engine or TV operation
is involved.
