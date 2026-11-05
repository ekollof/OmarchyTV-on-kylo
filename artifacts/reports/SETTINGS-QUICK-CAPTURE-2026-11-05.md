# Quick picture-settings offline capture — 2026-11-05

Real-asset, host-only capture of the quick picture-settings state
(`ui_vendor` portal screen 3). This closes the real-content composite/placement
part of the offline settings preview; the full settings panel and Mesa/
transformed frames remain open.

## What was added

- `tests/ui_settings_quick.c`: loads `nav_display`, `ui_engine` and
  `ui_vendor` with `ui.screen=settings`, the real workbench UI directory and
  the recorder display. Asserts screen 3 and the three captured blits at
  `0,180` (`settings_rail`), `0,240` (`settings_top`) and `0,440`
  (`settings_cat`).
- `make capture-settings-quick-real`: copies the recorder/UI modules, runs the
  test with `OA_NAV_CAPTURE`, then converts the 1920x1080 RGBA snapshot with
  `tools/rawrgba-to-png.py`.

The renderer is the shipped `paint_settings` path: clear `(13,14,14)`, then
the three firmware-captured assets. No TV, fb1, GPU or vendor renderer was
used.

## Comparison

The on-TV quick-settings frame captured during the earlier full-asset run is
`artifacts/analysis/live-tv-2026-11-05/ui-settings.png`. Compared with
`tools/capture_compare.py`:

```
full: AE=0 (0.000000) RMSE16=0 MAE8=0 RMSE8=0 max=0
changed=0/2073600 (0.000000)
```

The offline real-asset capture and the fb1 oracle are byte-identical. Evidence:
`artifacts/analysis/live-tv-2026-11-05/offline-settings-quick.png` and
`offline-settings-quick-compare.txt`.

## Limits

- This is the quick picture-settings state only (`settings_list` when the
  full 780x1080 panel is open is the other screen).
- The on-TV oracle is an fb1 capture with the full asset set, not
  HDMI/cold-start scanout.
- The recorder does not apply scene transforms; Mesa renderer integration and
  transformed-frame parity remain open.
- No TV access was used for this milestone; the capture is host-only.

## Command

```
make -j24 capture-settings-quick-real
ok quick picture-settings real-asset capture: rail/top/cat at 0,180/240/440
```
