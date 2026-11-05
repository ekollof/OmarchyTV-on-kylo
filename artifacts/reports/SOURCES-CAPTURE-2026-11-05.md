# Bronlijst source-dialog offline capture — 2026-11-05

Host-only, real-asset capture of the source-selection dialog (portal screen 2).
The composited recorder frame is byte-identical to the on-TV fb1 capture.

## What was added

- `tests/ui_sources_quick.c`: loads `nav_display`, `ui_engine` and
  `ui_vendor` with `ui.screen=sources` and the workbench UI directory.
  Asserts screen 2, the 780x1080 `#BE1E2D` panel fill, and the focused row 0
  bar at `18,152` (748x66, `#FE9419`), then writes a recorder snapshot.
- `make capture-sources-real`: stages the recorder/UI modules, runs the test
  with the real `src_title`, `src_0..8` and `src_*_f` assets, and composites
  the straight-alpha capture over black with
  `tools/rawrgba-to-png.py --over-black`.

## Comparison

Reference: `artifacts/analysis/live-tv-2026-11-05/ui-sources.png` (the on-TV
Bronlijst capture from the full-asset run). Candidate:
`artifacts/analysis/live-tv-2026-11-05/offline-sources.png`.

```
full: AE=0 (0.000000) RMSE16=0 MAE8=0 RMSE8=0 max=0
changed=0/2073600 (0.000000)
```

## Limits

- Focused row 0 only. The other rows use the same captured assets but were
  not separately compared; live source names/capabilities are fixed assets,
  not decoded from the runtime service list.
- No animation, focus timing, remote interaction or transformed frames.
- Host recorder, not GPU/Mali or HDMI/cold-start scanout.
- No TV access was used for this capture; the reference is an earlier fb1
  oracle.

## Command

```
make -j24 capture-sources-real
ok Bronlijst real-asset capture: 780x1080 panel and focused row 0 at y152
```
