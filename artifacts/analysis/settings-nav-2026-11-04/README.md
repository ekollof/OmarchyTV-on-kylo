# Live DIAL navigation and sublauncher state — 2026-11-04

Owner authorized that recorded DIAL session; every key verified by screenshot.
No value-adjusting keys were sent and no visible value change was observed.
No DB differential proves zero writes. Session reports staging/helpers removed;
this is not a current live poll.

## DIAL key injection works with per-step verification

`POST :56789/apps/SmartCenter` `<remote><key code='…'/></remote>`,
HTTP 201 each time; every 201 below produced the observed on-screen effect
(201 alone is not proof — each step was screenshot-verified):

| key | code | observed result |
|---|---|---|
| Menu | 1048 | launcher portal opens (Startpagina, 5-rail) |
| Down | 1019 | rail focus 3rd(home) → 4th(TV row) |
| Down | 1019 | focus → 5th (wrench); "Instellingen" tab row appears |
| Select | 1053 | shortcut tiles open (Modus/Energiebesparing/Beeld zoomen) |
| Down | 1019 | no visible change (already in tiles) |
| Select | 1053 | toggles back to tabs-only view |
| Up | 1020 | enters shortcut tiles, Modus tile focused |
| Right | 1022 | tile focus Modus → Energiebesparing (Beeld tab stays focused) |
| Select | 1053 | Energiebesparing value editable in place (`< Maximaal >`, orange) |
| Exit | 1010 | edit cancelled, value unchanged (still Maximaal) |

## Sublauncher structure (observed, not the full slider list)

The reached screen shows shortcut tiles (the 3 static type-2 rows:
MODE/ENERGY_SAVING/PICTURE_ZOOM with live values Natuurlijk/Maximaal/16:9)
plus the tab button row (Beeld focused, Geluid/Netwerk/Installatie/Systeem
visible) — but NO dialog title and NO full slider list. Live tab labels
match the decoded keys (PICTURE→Beeld, SOUND→Geluid, NETWORK→Netwerk,
INSTALLATION→Installatie, SYSTEM→Systeem).

## Live heap scan of this state (read-only /proc/1785/mem)

`live-scan-sublauncher.txt`: 143 candidates, 30 created. No font-18 title
textbox, no type-9 row or PictureSettingsDialog vptr in eligible heaps.
TabDialog was NOT in the identity table. This scan cannot prove its absence,
process-wide absence of the full list or ownership of the tab strip.
Created widgets are wide tile windows (e.g. (199,271,439,467),
(449,271,689,467)) in list boxes with count=9/count=7, step=140,
row_items=463/464 — the sublauncher grid, a different composition path from
the settings list (step 70). The v2/v3 scanner fields (flags1/flags2/
override/slide/slidermargin, key/ustring) all print sane values; no
overread (ASan-clean patterns retained).

## Consequences for the slider-indent question

The +30 slider-row indent needs the `DialogSettings_tab` list screen, which
this navigation path did not reach (shortcuts instead of the dynamic
slider rows from `update*`). The extended scanner is staged-ready
(`/tmp` copy removed after use); use the reviewed scanner only with the actual
screen visible. Stored lookup keys can be stale; retain current text evidence.

Captures: `tv-tabs-beeld.png`, `tv-shortcut-tiles-modus-focused.png`,
`tv-shortcut-edit-maximaal.png`, `tv-shortcut-editexit.png` (all fb1 layer 1,
R/B-corrected RGB).
