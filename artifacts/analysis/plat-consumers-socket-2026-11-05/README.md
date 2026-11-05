# Plat-consumer socket evidence

Interpretation: [PLAT-CONSUMERS-SOCKET](../../reports/PLAT-CONSUMERS-SOCKET-2026-11-05.md).

- `check.txt`: real Unix-socket lifecycle for the plat fixture and consumers.
- `repeated.txt`: five consecutive passing runs.
- `full-host-check.txt`: full strict host `make check` (403 ok).
- `identities.txt`: host, plat fixture, audio, source, ir and panel hashes.

The test publishes a host-only `plat:v1`, starts audio/source/ir/panel with
hardware paths disabled, proves the provider cannot reload while consumers are
active, reloads it after stopping them, reloads each consumer, and repeats a
fresh unload/load/start cycle. No HAL, device, framebuffer or TV is used.
