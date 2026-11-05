# Full settings panel offline capture — 2026-11-05

Real-asset, host-only capture of the full `settings_list` 780x1080 panel. The
recorder crop is pixel-identical to the captured panel source.

## What was added

- `tests/ui_settings_panel.c` now accepts `OA_SETTINGS_REAL=1`: it skips the
  synthetic 4x4 blobs and uses the real workbench UI directory, while keeping
  the existing panel/quick-tile navigation assertions.
- `make capture-settings-panel-real` runs that mode, captures the full panel,
  and compares the 780x1080 crop with the OAI1
  `artifacts/workbench/source-stage-20261101/ui/settings_panel.rgba`.
- `tools/panel-source-compare.py` validates the OAI1 header/size, crops the
  1920x1080 recorder capture at the origin, compares RGB(A) bytes, and writes
  a PNG of the crop.

The screen is entered with `ui.screen=settings_list` and the default
`ui.sel=-1`, so no focus row is overlaid; the shipped `paint_settings_list`
path clears transparent, blits the panel at 0,0 and presents.

## Result

```
ok   full settings panel paints at 0,0; Exit returns to quick tiles,
     Select reopens, Exit to portal
panel: AE=0.000000 MAE8=0.000000 max=0 changed=0/842400
```

The panel source has opaque alpha255 pixels; the recorder crop matches every
pixel. Evidence:

- `artifacts/analysis/live-tv-2026-11-05/offline-settings-panel.png`
- `artifacts/analysis/live-tv-2026-11-05/offline-settings-panel-compare.txt`

## Focused-row follow-up

`OA_SETTINGS_SEL=0` selects row 0; `make capture-settings-focus-real` captures
the panel and compares the `20,186` 750x66 row region against
`settings_focus_0.rgba`:

```
panel: AE=0.000000 MAE8=0.000000 max=0 changed=0/49500
```

The captured focused-row composition is exact. The live-text path
(`ui.text=1`) and focus/animation behavior remain separate.

## Limits

- This compares against the captured panel source, not a new full-panel fb1
  oracle from the set.
- No focused row, live text, slider/selection overlay or disabled-row state is
  included.
- Recorder only: no Mesa renderer, transformed frames, GPU/Mali or
  HDMI/cold-start scanout.
- No TV access was used.

## Command

```
make -j24 capture-settings-panel-real
```
