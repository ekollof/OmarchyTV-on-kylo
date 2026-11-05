# Stock-parity follow-up — 2026-11-05

Interpretation: [STOCK-PARITY-2026-11-05](../../reports/STOCK-PARITY-2026-11-05.md).

## Blue-shift correction

`artifacts/protocol/p1/04-apps-stock-portal.png` is another pre-correction
BGRA capture and is red/blue swapped. `p1-portal-corrected.png` is the
corrected copy; `channel-check.txt` records raw/corrected samples. The
earlier `stock-corrected.png` was already correct and is unchanged.

`geometry.txt` records the corrected P1 portal chrome: 0..279 header,
478..572 card row at 190 px pitch, dark background. It is a second stock
portal state, not the missing full-screen apps-grid capture.

## Comparator

`openaurora/tools/capture_compare.py` compares a stock capture with a
candidate render. It supports explicit `--reference-order BGR` correction,
named region manifests and JSON output. Its `AE`/`RMSE16` definitions match
the earlier measurements:

- `portal-compare.txt` reproduces Netflix `AE=2784.68`, `RMSE16=24663.9`,
  Canal `AE=311.746` from the original stock-parity analysis.

The tool is covered by `openaurora/tests/capture_compare_test.py` in
`make check` (404 ok). Tests cover identical/changed images, BGR correction,
region bounds, size errors and the real stock-portal regression.

## Prepared fresh apps-grid comparison

- `apps-grid-regions.json`: candidate geometry from
  `DialogFullScreenPortal.svg` (top strip, grid, scrollbar, 5x3 310x230
  cells).
- `portal-cards.json`: the nine shared portal card rectangles.

Once a stock apps-grid capture is available at
`artifacts/protocol/stock-apps-grid.png` (decoded as RGB by `tools/tvshot.py`),
run:

```sh
make -C openaurora compare-stock-apps-grid
```

The target refuses with a clear message while the capture is absent. It must
not be treated as a parity result until the stock image exists and its
geometry is confirmed; the manifest is the candidate geometry, not a proven
stock layout.

No TV access occurred.
