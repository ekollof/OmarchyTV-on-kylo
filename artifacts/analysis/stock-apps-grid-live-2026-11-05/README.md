# Live parent-1 apps-grid parity evidence

Interpretation:
[STOCK-APPS-GRID-PARITY](../../reports/STOCK-APPS-GRID-PARITY-2026-11-05.md).

- `stock-apps-grid-live.png`: read-only fb1 capture of the stock Apps page,
  byte-identical to the saved oracle.
- `offline-apps-grid-live.png`: render using the live **parent-1** LauncherItem
  catalog (23 rows), original downloaded thumbnails, the Markt row and the
  OmarchyTV thumb900 tile.
- `compare-live.txt`: full/region metrics.
- `check.txt`, `full-host-summary.txt`: host suite and verifier.
- `identities.txt`: live DB, oracle and candidate hashes.
- `inputs.txt`: parent-1 row and thumbnail counts.

Result: full AE 10852.4 (0.52%), MAE8 1.335; top strip 0.36%, grid 0.87%,
scrollbar 0.12%. The 25-entry catalog (23 parent-1 rows + Markt + OmarchyTV)
matches the stock scrollbar length. The OmarchyTV parent-1 placement is
inferred from the 25-entry scrollbar; the DB stores it as a parent-0 row with
thumb900. No oracle tile crops are used.
