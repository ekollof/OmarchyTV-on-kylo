# Startpagina portal offline capture — 2026-11-05

Host-only, real-asset capture of the Startpagina portal (portal screen 0). The
composited recorder frame is **byte-identical** to the on-TV fb1 capture from
the staged UI run.

## What was added

- `tests/ui_portal_quick.c`: loads `nav_display`, `ui_engine` and `ui_vendor`
  with the workbench UI directory, `ui.focus=2`, the default nine-card row and
  the recorder display. Asserts screen 0, the title blit at `29,80`, the first
  rail fill at `24,211`, then writes a recorder snapshot.
- `make capture-portal-real`: runs the test with the real `title`, `rail1..5`
  and `card_*` assets and converts the 1920x1080 snapshot.
- `tools/rawrgba-to-png.py --over-black`: composites the recorder's straight
  alpha over opaque black, matching what the fb1/OSD scanout shows. Without it
  the transparent portal background reads as `(36,38,40)` instead of the
  composited `(22,23,24)`.

## Comparison

Reference: `artifacts/analysis/live-tv-2026-11-05/ui-vendor-live.png` (the
stock-shaped portal captured from fb1 during the earlier full-asset run).
Candidate: `artifacts/analysis/live-tv-2026-11-05/offline-portal.png`.

```
full: AE=0 (0.000000) RMSE16=0 MAE8=0 RMSE8=0 max=0
changed=0/2073600 (0.000000)
```

Before compositing, the only difference was the background: every asset pixel,
rail, title and card was already identical. The recorder's straight-alpha
`(36,38,40,153)` over black yields `(22,23,24)`, exactly the TV frame.

## Focused-card state

`make capture-portal-card-real` sets `OA_PORTAL_CARD=1`, sends two Right key
events through the shipped portal path (rail -> cards -> Prime Video) and
asserts the focused-card accent fill at `394,473`. Compared with the on-TV
`ui-card-focus.png`:

```
full: AE=0 (0.000000) RMSE16=0 MAE8=0 RMSE8=0 max=0
changed=0/2073600 (0.000000)
```

Evidence: `artifacts/analysis/live-tv-2026-11-05/offline-portal-card.png` and
`offline-portal-card-compare.txt`.

## Limits

- Default nine-card row and the Prime Video focused-card state only; the live
  parent-0 database with extra off-screen cards is not part of this capture.
- Rail-highlight animation frames, title slide/animation, live text and the
  other portal overlays are not covered.
- Host recorder, not GPU/Mali or HDMI/cold-start scanout.
- No TV access was used for this capture; the references are earlier fb1
  oracles.

## Commands

```
make -j24 capture-portal-real
ok Startpagina real-asset capture: title 29,80 rail 24,211

make -j24 capture-portal-card-real
ok Startpagina focused-card capture: title 29,80 rail 24,211 accent 394,473
```
