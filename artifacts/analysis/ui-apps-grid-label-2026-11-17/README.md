# Apps-grid label origin — 2026-11-17

Question: the live-DB focused app label was 2 px left of the stock capture
(STOCK-APPS-GRID-PARITY). The firmware centre path uses the physical text
width plus the first-bearing adjustment (UI-HORIZONTAL-ORIGIN), not the ink
bbox; `common/apps_label.c` now uses that rule.

Reproduction (offline, real dump DB + original thumbnails):

```sh
cd openaurora
python3 tools/stage-apps-grid.py --out /tmp/apps/ui \
  --db ../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db \
  --thumbs ../artifacts/workbench/thumbs
OA_FONT_INDEX=../artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.txt \
OA_FONT_PACK=../artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.bin \
OA_APPS_GRID_SEL=22 build/tests/apps_grid_stock build/tests/ui-live-cards/modules \
  /tmp/apps/ui /tmp/apps/out.rgba \
  ../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db
```

Measured on the focused Twitter label, same luminance threshold (<60) for all:

| image | ink bbox | position vs stock |
|---|---|---|
| stock `live-tv-2026-11-05/stock-apps-twitter-label.png` | 919..999, 829..850 | — |
| old ink-centred (`offline-apps-twitter-label.png`) | 917..997 | 2 px left |
| fixed physical-width rule | 918..998 | 1 px left |

The remaining 1 px is not yet reduced. The `rowItems` resource (BinSVG 377)
defines `mRowM` (210,36,570,216) and `text` (236,214,564,254); the button mask
geometry is in resource 373 (`mRowM` 187,0,507,180). The
HorizontalMaskScaleTransform mapping from that 360-wide mask/text box onto the
310 px cell is not reduced to an exact text origin, so do not shift the label
by a fitted +1. `rects.txt` is the raw rectangle dump.

The offline live-DB render as a whole is not re-baselined here: the current
ui_vendor tile/focus geometry differs from the 2026-11-05 capture, so only the
label-strip bbox comparison above is meaningful.
