# Provisional channel-list UI evidence

Interpretation: [CHANNEL-LIST-UI](../../reports/CHANNEL-LIST-UI-2026-11-05.md).

- `channels-real.png`: recorder capture with the real font7 pack; Beta focused.
- `channels.png`: same layout with the synthetic test font.
- `check.txt`: recorder test pass.
- `real-font.txt`: real-font capture pass.
- `full-host-summary.txt`: full host **410 ok**, `verify.cjs` **45 groups**.
- `focused.txt`: display/channel focused runs after removing `system()`.
- `system-calls.txt`: no production `system()` calls; test wrappers remain.
- `arm-build.txt`: fresh ARM build of display and ui_vendor.
- `identities.txt`: host/ARM ui_vendor, service_db and display hashes.

The screen uses stored `channel_no`/name order, not runtime EASI LCN or
visibility, and displays a red "PROVISIONAL STORED ORDER - NOT RUNTIME EASI"
strip. Recorder only; no TV scanout or vendor UI capture comparison.
