# Stock-parity evidence

Interpretation: [STOCK-PARITY](../../reports/STOCK-PARITY-2026-11-04.md).

> **Correction 2026-11-05:** the P1 portal capture is also pre-correction and
> blue-shifted. See
> [stock-parity-2026-11-05](../stock-parity-2026-11-05/README.md) for its
> corrected copy and the reusable comparator.

- `stock-corrected.png`: `shot-launcher-right.png` with RGB channels swapped
  to correct the old BGRA/RGBA capture defect.
- `channel-swap.txt`: before/after sample pixels versus current downloaded
  sources, plus background/rail samples.
- `portal-card-metrics.txt`: stock corrected vs offline portal at identical
  171x95 card rectangles (AE/RMSE per card).
- `stock-vs-apps-card-metrics.txt`: stock portal vs certified visible
  apps-grid crops (different screen geometry; informational only).

The original capture is left untouched. No TV was accessed. This analysis
shows the old capture cannot certify stock apps-grid pixel parity: card art,
background color and status chrome differ from the current assets.
