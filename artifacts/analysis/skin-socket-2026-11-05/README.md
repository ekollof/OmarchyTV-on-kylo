# Skin socket evidence

Interpretation: [SKIN-SOCKET](../../reports/SKIN-SOCKET-2026-11-05.md).

- `check.txt`: real Unix-socket skin selection/fallback output.
- `full-host-check.txt`: full strict host `make check` (399 ok).
- `identities.txt`: host, ui_engine, ui_vendor, ui_omarchy and recorder DSOs.

The test starts with missing/invalid configured skins, verifies vendor fallback,
switches to Omarchy and back through the `skin` command, exercises an enter
failure fallback, and quits cleanly. It uses the recorder display, not the TV.
