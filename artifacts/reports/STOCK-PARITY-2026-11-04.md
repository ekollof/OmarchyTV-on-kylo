# Stock-parity analysis — corrected old capture — 2026-11-04

Offline analysis only. No TV operation.

## The old capture is channel-swapped

`artifacts/protocol/shot-launcher-right.png` is a pre-correction tvshot capture.
Decoding its BGRA bytes as RGB swaps red and blue. Direct samples:

| sample | old capture | after R/B swap | current downloaded source |
|---|---|---|---|
| Prime card | `(249,148,25)` orange | `(25,148,249)` blue | `(26,152,255)` blue |
| Canal Digitaal card | `(0,33,254)` blue | `(254,33,0)` red | `(224,0,0)` red |

`artifacts/analysis/stock-parity-2026-11-04/stock-corrected.png` is the
channel-corrected copy (the original is left unchanged). This confirms the
"blue-shifted" appearance is the known BGRA/RGBA capture defect, not a vendor
blue theme.

## Layout comparison

The corrected stock portal and the offline recorded portal share the same card
geometry: cards at `x=209+190*i, y=478, 171x95`; the rail focus color samples
`(247,148,30)` in both. The apps-grid candidate uses a different screen
geometry (`183,180,1554x720`, 5x3 `310x230` cells), so the portal capture cannot
serve as an apps-grid pixel oracle.

## Card-content differences

Even after channel correction, the stock portal and the offline portal are not
pixel-identical at the same card rectangles:

| card | AE | RMSE |
|---|---:|---:|
| Netflix | 2784.68 (17.1%) | 0.3763 |
| Prime Video | 1187.50 (7.3%) | 0.2163 |
| YouTube | 388.09 (2.4%) | 0.1040 |
| tabii | 219.42 (1.4%) | 0.0662 |
| Canal Digitaal | 311.75 (1.9%) | 0.0833 |
| TV Vlaanderen | 279.86 (1.7%) | 0.0766 |
| Télésat | 242.70 (1.5%) | 0.0742 |
| Digiturk Play | 322.87 (2.0%) | 0.0725 |
| Gain | 342.38 (2.1%) | 0.0827 |

The old stock Netflix card is a large zoomed wordmark; the current downloaded
400x225 asset is a smaller wordmark on a white tile. `uiraw2/card_netflix`
versus `launcher_app_thumb61` already differs (RMSE 0.1348), including
transparent rounded corners. This is asset/crop version drift and alpha
composition, not proof that the decoder is wrong. The stock background is
`(13,14,14)` while the reconstructed offline portal uses `(36,38,40)`; the
stock portal also shows right-side status/gear/refresh icons that the offline
portal does not.

## Conclusion

- The saved stock screenshot is useful for geometry and channel-order
  validation after correction.
- It is **not** a stock apps-grid or pixel-parity oracle for the current
  downloaded asset set. Card artwork, background color and right-side status
  chrome differ.
- A fresh, same-session stock capture with the corrected BGRA decoder and an
  actual stock apps-grid screen is still required before claiming stock visual
  parity. That requires TV access and is not part of this offline step.

Evidence: `artifacts/analysis/stock-parity-2026-11-04/`.
