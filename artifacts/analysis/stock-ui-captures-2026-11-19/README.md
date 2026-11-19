# Live stock-UI capture pass — 2026-11-19

Read-only evidence for
[STOCK-UI-LIVE-CAPTURES-2026-11-19](../../reports/STOCK-UI-LIVE-CAPTURES-2026-11-19.md).
Stock Aurora 1778 ran throughout; no Aurora stop, device write, watchdog
change or `/conf` write. DIAL virtual RC keys were verified by capture diff
(never by the POST status); screens were captured with `tools/tvshot.py`
(BGRA→RGB) and the ARM `fb-region-sampler` (read-only fb1, sha256
`8e4278cc…`, staged under `/buffer/oa-stockcap-20261119`, hash-verified and
removed afterwards).

- `channel-filter-popup.png` — live filter popup from the populated channel
  screen: panel #3F0B13 at (142,160)–(778,1079), 75 px row step, focused
  "Zendertype" bar (152,228) 614x65, and all ten rows (see the report).
- `goto-dialog-live.png` — pulse frame 20 (t≈658 ms): the real GotoService
  Dialog, a flat 167×167 `#481730` box at (41,45)..(207,211) with a centred
  white "1" over the populated channel screen.
- `panel2.manifest` / `panel1.manifest` — sampler timing/hash/yoffset records
  (dialog present t=87..1467 ms, gone 1527 ms; the second panel run targeted
  the empty-list state and found no panel, confirming the firmware guard).
- `channel-populated.png` — Zenders/Alleen TV list, 11 ATV services.
- `channel-edit-pane.png` — Bewerken options with ADD_TO_HOME focused.
- `epg-empty.png`, `sources-screen.png`, `portal-home.png`,
  `portal-tv-rail.png`, `channel-banner.png`, `launcher-remove-confirm.png`
  — the other states listed in the report.
- `settingsDB-before.db` / `settingsDB-after.db` — vendor settingsDB copies
  bracketing the accidental "Toevoegen aan Thuis" action; the LauncherItem
  diff is recorded in the report. No DB write was performed.

TV restored to standby; no staging/helper left; stock PIDs and `/conf` flags
unchanged.
