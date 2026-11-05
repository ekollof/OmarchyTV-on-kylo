# Hardware-free module matrix IPC evidence

Interpretation: [MODULE-MATRIX-IPC](../../reports/MODULE-MATRIX-IPC-2026-11-05.md).

- `check.txt`: real Unix-socket lifecycle result.
- `full-host-check.txt`: full strict host `make check` (398 ok).
- `identities.txt`: host and matrix module DSO hashes.

The test starts ticker, ui_clock, ui_anim, ui_navigation, ui_list, power and
cec with no display/HAL module, exercises nav/list queries, and reloads each
provider in dependency-safe order. CEC's asynchronous cleanup is retried as
documented. No TV/framebuffer or persistent file is touched.
