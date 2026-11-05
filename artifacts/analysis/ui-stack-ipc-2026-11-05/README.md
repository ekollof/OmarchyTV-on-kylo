# Non-HAL UI stack IPC evidence

Interpretation: [UI-STACK-IPC](../../reports/UI-STACK-IPC-2026-11-05.md).

- `check.txt`: real Unix-socket + stdin stack lifecycle result.
- `full-host-check.txt`: full strict host `make check` (397 ok).
- `identities.txt`: host, settings, image_decode, ui_engine, ui_vendor and
  recorder display DSO hashes.

The test copies the existing read-only settingsDB fixture and exercises the
real service dependency chain: settings -> launcher.db, nav_display -> display,
ui_engine -> ui_vendor. It reloads image_decode, settings, ui_engine,
nav_display and ui_vendor in dependency-safe order. No TV/HAL/framebuffer or
persistent file is touched.
