# Provisional channel-list UI — 2026-11-05

Status: current (stock geometry and a gated stock mode 2026-11-17; the
provisional screen remains the default)
Last verified: 2026-11-17 (geometry, recorder test and capture comparison; no
TV run)

Host/recorder only. No TV scanout or runtime EASI access.

## What was added

`ui_vendor` now has a `SCREEN_CHANNELS` screen (`ui.screen=channels`) over
`service.list`. It uses the stored `channel_no`/name query, not runtime EASI
LCN/visibility. The screen deliberately shows:

```
PROVISIONAL STORED ORDER - NOT RUNTIME EASI
```

on a red strip, so it cannot be mistaken for vendor channel-list parity.

Rows show the stored channel number and name; the focused row uses the
existing #F7941E selection color. Up/Down changes selection, Select emits the
usual select action, and Exit returns to the portal. Text uses the existing
vendor font pack path (`ui.text=1`); the automated test uses a synthetic font
and the readable evidence capture uses the real font7 pack.

`ui.screen=channels` is a lab/test entry point only. The stock remote path into
the vendor channel list is not recovered, and the runtime EASI active-list
LCN/visibility is not available offline.

No production `system()` calls remain. The display module's mdev node repair
now uses `posix_spawn("/sbin/mdev", "-s")` and `waitpid`, and the channel-list
test cleans its temporary tree with `nftw` instead of a shell command.

## Evidence

- `channels-real.png`: recorder capture with the real font7 pack; rows
  `1 Alpha`, `2 Beta`, `3 Gamma`, Beta focused.
- `channels.png`: the same screen with the synthetic test font.
- `check.txt`: recorder test output.
- `real-font.txt`: real-font capture run.
- `full-host-summary.txt`: full host **410 ok**, `verify.cjs` **45 groups**.
- `focused.txt`: display/channel focused runs after the no-`system()` change.
- `system-calls.txt`: production `system()` scan; test wrappers only.
- `arm-build.txt`: fresh ARM build of display and ui_vendor.
- `identities.txt`: host, service_db, ui_vendor and display hashes (host+ARM).

The test verifies the top strip, provisional marker strip, focused row, text
blits, Down selection change and Exit back to portal 0. It uses a synthetic
three-row service DB and the recorder display, not scanout.

## Limits

This is an explicit provisional UI, not vendor parity:

- runtime EASI LCN/ordering and visibility/HD-simulcast filtering are missing;
  **partly superseded:** [CHANNEL-RUNTIME-ORDER](CHANNEL-RUNTIME-ORDER-2026-11-05.md)
  adds an ACTIVECHANNELLIST ingest/order path and a runtime UI mode, but no
  live feed, LCN or visibility fields;
- the normal stock remote entry path is not recovered;
- **partly superseded 2026-11-17:** `artifacts/protocol/p0/04-channel-list.png`
  is a stock channel-list capture. CHANNEL-LIST-GEOMETRY-2026-11-17 recovers
  the `DialogAdvancedChannelList.svg` (record 80) rectangles, theme colours
  and capture row metrics into `common/channel_layout.c` +
  `check-channel-layout`. 2026-11-17 adds a default-off stock mode
  (`ui.channels_stock=1`) with matching vendor fonts and a recorder/capture
  check (`check-ui-channels-stock`, `check-ui-channels-stock-capture`; panel
  MAE8 2.750). The channel_small header icon is drawn/staged; Up/Down/Select/
  Exit are tested (Select index 1 `C07`), Select on the portal rail-3 monitor/Zenders icon opens the screen only with
  `ui.channels_stock=1` (sidebar order from `shot-launcher.png`); the default
  rail path is unchanged, and the stock screen/input prefer the ingested runtime
  active list (3-entry ingest test reports `Runtime A`). The provisional screen
  remains the default; the live EASI feed, scrollbar track/total, exact tab
  anchors, scrolling/paging and scanout remain open;
- tuner acquisition/scan, EPG, PVR, media/network and hardware ownership remain
  open;
- recorder evidence is not physical fb1/scanout evidence.

No TV operation was performed for this UI milestone.
