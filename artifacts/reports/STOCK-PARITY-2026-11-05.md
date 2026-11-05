# Stock parity follow-up and fresh apps-grid comparison prep — 2026-11-05

Offline only. No TV operation.

## Blue-shifted P1 capture

The user-visible P1 stock portal screenshot
(`artifacts/protocol/p1/04-apps-stock-portal.png`) is another pre-correction
BGRA capture and is red/blue swapped. It was captured before the 2026-10-31
BGRA correction. Raw and corrected samples:

| sample | raw | corrected |
|---|---|---|
| Prime card | `(128,77,14)` | `(14,77,128)` blue |
| Canal card | `(28,47,128)` | `(128,47,28)` red |
| header strip | `(38,26,140)` | `(140,26,38)` red |
| background | `(13,12,12)` | `(12,12,13)` neutral |

`artifacts/analysis/stock-parity-2026-11-05/p1-portal-corrected.png` is the
corrected copy. The earlier `stock-corrected.png` was already correct and is
not changed. Do not use the raw P1 file for colour parity.

The corrected P1 capture shows a 0..279 header, the card row at y=478..572
with 190 px pitch, and a dark neutral background. The earlier corrected
capture has the same card row geometry but no header. They are different stock
portal states (the card rectangles differ by up to about 0.48 normalized MAE),
so P1 is not a full-screen apps-grid oracle either.

## Apps-grid reachability correction

The earlier APPS-GRID report suspected the full-screen portal might be dead on
this model. A firmware reachability audit now shows that navigation code
`0x80` constructs `FullScreenPortalDialog`; two shipped paths emit it:

- `DialogNavigator::doNavigateFullScreenMarket` at `0xd1aa2c`, only after
  `FullScreenMarketDialog::doModal` returns `0x29a`;
- `OperationFinder::handleOperation` case `0x79` at `0xd3e6a0`, after its
  launcher-item and internet checks.

The screen is therefore reachable, not proven dead. This does **not** resolve
the `+0x1fc/+0x200` grid runtime-inset contradiction, and no stock capture of
the screen exists. The candidate `183,180,1554x720` / 5x3 `310x230` rectangles
still assume left=right=0 and remain unconfirmed.

## Reusable comparator

`openaurora/tools/capture_compare.py` compares a stock capture with a candidate
render. It supports:

- explicit `--reference-order BGR` to correct an old red/blue-swapped capture;
- an optional named-region manifest;
- text and JSON metrics;
- size and region-bounds validation.

Its metric definitions match the original stock-parity measurements:
`AE = mean absolute error / 255 * pixels` and `RMSE16 = RMSE * 257`.
`portal-compare.txt` reproduces the earlier values exactly, for example
Netflix `AE=2784.68`, `RMSE16=24663.9`, and Canal `AE=311.746`.

`openaurora/tests/capture_compare_test.py` covers identical and changed images,
BGR correction, region metrics, out-of-bounds/size errors and the real
stock-portal regression. It is wired into `make check`:

```
ok stock capture compare: exact/changed metrics, BGR correction, regions, bounds/size errors and STOCK-PARITY regression
```

Full host `make check`: **404 ok**. `verify.cjs`: **45 groups**.

## Fresh apps-grid comparison prepared

Two manifests are ready:

- `portal-cards.json` — nine shared portal cards.
- `apps-grid-regions.json` — candidate apps-grid geometry from
  `DialogFullScreenPortal.svg`: top strip `0,0,1920,162`, grid
  `183,180,1554x720`, scrollbar `1750,180,8x720`, and 5x3 `310x230` cells.

When a corrected stock apps-grid capture exists at
`artifacts/protocol/stock-apps-grid.png`, run:

```sh
make -C openaurora compare-stock-apps-grid
```

The target deliberately refuses to run while the file is absent; it prints
`missing ../artifacts/protocol/stock-apps-grid.png; capture the stock apps
grid first`. The manifest encodes our candidate geometry, not a proven stock
layout. A real parity claim still requires the stock capture and confirmation
that its geometry matches.

## Evidence

- `artifacts/analysis/stock-parity-2026-11-05/channel-check.txt`
- `geometry.txt`
- `portal-compare.txt`
- `p1-vs-old-portal.txt`
- `apps-grid-regions.json`, `portal-cards.json`
- `apps-grid-reachability.txt` (nav code 0x80 senders and unresolved insets)
- `capture-compare-test.txt`, `full-host-check.txt`, `identities.txt`

## Limits

No stock apps-grid capture exists in the repository. No apps-grid pixel parity,
scanout, live fb1 ownership or cold startup is claimed. The comparator is
offline tooling and the P1 correction is a second portal state, not a parity
oracle.
