# image.decode socket/CLI evidence

Interpretation: [IMAGE-DECODE-IPC](../../reports/IMAGE-DECODE-IPC-2026-11-04.md).

- `check.txt`: real Unix-socket + stdin lifecycle test result.
- `full-host-check.txt`: full strict host `make check` (395 ok).
- `identities.txt`: host binary and image_decode module SHA256.

The test starts no display/HAL/TV module. It writes no persistent files and
removes the socket on clean exit. This is host modular-reload evidence, not a
display or independent-startup claim.
