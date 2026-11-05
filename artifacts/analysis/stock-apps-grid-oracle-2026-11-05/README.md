# Stock apps-grid oracle and parity comparison

Interpretation:
[STOCK-APPS-GRID-PARITY](../../reports/STOCK-APPS-GRID-PARITY-2026-11-05.md).

- `stock-apps-grid.png`: read-only fb1 capture of the stock full-screen Apps
  page (1920x1080 RGB, BGRA decoded by `tools/tvshot.py`). Obtained after DIAL
  virtual-remote navigation: MENU 1048, UP 1020 to the Apps rail tile, OK 1053,
  OK 1053 on "Meer toevoegen", then capture.
- `offline-apps-grid-stock.png`: offline recorder render using the stock theme
  background/title, real downloaded launcher thumbnails and stock-derived
  third-row tiles.
- `compare-before.txt`: old synthetic candidate vs the oracle.
- `compare-after.txt`: stock-layout candidate vs the oracle.

Before: full AE 612938 (29.56%), MAE8 75.38.
After: full AE 20506.8 (0.99%), MAE8 2.52, changed pixels 8.64%.

No TV display state was changed by the capture; DIAL only navigated the stock
UI to the requested screen.
