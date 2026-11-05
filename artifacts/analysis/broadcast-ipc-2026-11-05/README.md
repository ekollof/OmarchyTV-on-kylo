# Broadcast CLI/socket evidence

Interpretation: [BROADCAST-IPC](../../reports/BROADCAST-IPC-2026-11-05.md).

- `check.txt`: fixture contract test plus the real Unix-socket IPC test.
- `full-host-summary.txt`: full host **413 ok**, `verify.cjs` **45 groups**.
- `identities.txt`: host binary and fixture module hashes.

The IPC test drives `tuner-*`, `epg-query`, `pvr-*`, reload, stop/unload and
shutdown over the control socket with the host fixture. No hardware or TV
operation is involved.
