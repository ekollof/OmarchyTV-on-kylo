# ui_vendor/recorder display IPC evidence

Interpretation: [UI-VENDOR-IPC](../../reports/UI-VENDOR-IPC-2026-11-05.md).

- `check.txt`: real Unix-socket + stdin lifecycle result.
- `full-host-check.txt`: full strict host `make check` (396 ok).
- `identities.txt`: host, ui_vendor, ui_engine and recorder display DSO hashes.

The test loads `mod_nav_display` (recorder display), `ui_engine` and
`ui_vendor` only. It uses `ui-state` to prove the portal service is live,
reloads ui_vendor and the display provider, and exits cleanly. No TV/HAL
module, framebuffer or persistent file is touched.
