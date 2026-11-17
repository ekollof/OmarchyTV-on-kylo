# Stock EPG timeline layout and gated screen — 2026-11-17

Status: current (layout/timeline and chrome screen; no stock EPG capture)
Last verified: 2026-11-17 (firmware resource/instructions and host recorder
tests)
Evidence class: firmware resource/instructions and host/recorder tests. No TV
run, stock EPG capture, live EIT feed or scanout.

This recovers the multi-service EPG timeline from the shipped
`DialogEPG.svg` (BinSVG 101) and `MultiServiceTimelineScheduleDialog`, instead
of inventing a grid. Raw rectangles and the recorder candidate are in
[artifacts/analysis/epg-stock-2026-11-17](../../analysis/epg-stock-2026-11-17/README.md).

## Firmware contract

- Class: `MultiServiceTimelineScheduleDialog`, internal name `EPGTimeline`.
  `getEPGTitle` returns the `PROGRAM_GUIDE` key (dut "Programmagids").
- Five tab buttons are glyph actions, not text tabs: `NOW_NEXT_SCHEDULE`,
  `HLP_FILTER`, `PREVIOUS_DAY`, `NEXT_DAY`, `EXTRAS` (dut "Nu/Volgende
  schema", "Filter", "Vorige dag", "Volgende dag", "Extra's").
- `createServiceList` @0x00dc9ce4 splits the list at
  `list.x + EPGServiceListBox.width + 1` -> service column 3..644, timeline
  644..1920, y 319..1043 (width 1276).
- `ServiceTimelineBox::createButton` @0x00dd4914:
  `x = width*(eventStart-windowStart)/interval` and
  `x1 = width*(eventEnd-windowStart)/interval`, clipped at 0 and clamped to
  the width, in seconds. `ListTimelineBox::zoom` @0x00dc6f1c steps the interval
  by 3600 s between 3600 and 14400 s; the initialized default is **7200 s**.
- `updateTimelineTitleBar` @0x00dc9950 sets the date and the start/middle/end
  labels. `TimeDateServices` formats `<weekday-short> %02u/%02u/%u` and
  `%02u:%02u`; weekday keys are `*_SHORT` (dut Za/Zo/Ma/Di/Woe/Do/Vr).
- `setStartOfSchedule` @0x00dc8390 is local midnight minus `86400*n` seconds
  for day navigation, not the initial window start.

## Screen rectangles (record 101)

Panel 0,0,1920,1080; title 164,21,560,75; channel name 590,9,1434,81; event
title 593,84,1437,144; logo 204,43,354,193; channel number 358,65,538,137;
event time 290,149,551,194; clock 1175,149,1434,194; date bar 0,264,1920,318;
date 44,266,368,311; timeL/M/R 677/1196/1713 at y266..311; arrows 620/1868;
list 3,319,1920,1043; service column 0,319,640,1043; tab buttons
206+302*i,198,506+302*i,257.

Colours: list `#C5C9CD`, service column white with `#231F20`, red under-list
band `#BE1E2D` at 9/10, date bar `#010101` at 5/10, event title `#F7941E`,
arrows `#009F93`. The top band is left on the resource's white canvas with
white text; this slice uses the full-screen dialog family header `#481730`
and composes the date bar over it, flagged as a family inference pending a
stock capture.

## Implementation

- `common/epg_layout.c` + `include/openaurora/epg_layout.h`: pure model for the
  rectangles/colours, the service/timeline split, `oa_epg_event_span`
  (clip/clamp/truncate), `oa_epg_instant_x` for the now line,
  `oa_epg_visible_rows` with an explicit pitch and tab hit-testing.
  `check-epg-layout` asserts record-101 rects, the split (3..644 / 644..1920),
  7200 s spans (e.g. 0..30 min -> x644 w319; 60..90 min -> x1282 w319; a
  2-hour event clipped to w1276), the instant x, explicit pitch and invalid
  inputs.
- `ui.epg_stock=1` (default off), `ui.screen=epg` or `open:epg`: a
  `SCREEN_EPG` chrome with `PROGRAM_GUIDE`/date/times through `i18n`, service
  names from the runtime channel list or stored DB, event bars from
  `OA_SVC_EPG`, the now indicator and Up/Down/Exit. Rows paint only when
  `ui.epg_row_pitch` is explicitly set; the default is chrome only because the
  runtime row step is art-derived and not pinned offline.
- `tests/ui_epg_stock.c` publishes a fixture schedule (three 30-minute events
  on service 1, one 2-hour event clipped at the window end on service 2), sets
  `ui.epg_row_pitch=76` and `ui.epg_window_start`, and asserts the chrome
  fills, split border, bar x/width, scroll and Exit; `check-ui-epg-stock`
  passes.
- `tools/epg-stock-check.sh` renders the real-font candidate `epg-stock.png`
  with the Dutch langprofile: "Programmagids", "Di 14/11/2023",
  23:13/00:13/01:13, NPO 1/NPO 2/RTL 4, Journaal/NOS Sport/Film and the clipped
  "Lang programma".

## Limits and next step

No stock EPG screen capture exists, so the candidate is recorder evidence, not
pixel parity or scanout. The header fill is a family inference; tab glyph art,
channel logo/HD/encrypted icons, event-button art/colours (bars use a
provisional four-colour palette), selection/focus, synopsis (record 102), day
navigation, filters, reminders/record actions, the stock entry path, the
runtime row pitch and scanout remain open. Live EIT/EPG data still requires the
owner-authorized TV/tuner window; only the ingest/query path is offline-tested.

Report: EPG-STOCK-LAYOUT-2026-11-17. Related: EPG-EIT-MODULE, EPG-NOWNEXT,
EPG-PERSISTENCE, BROADCAST-IPC, PVR-STOCK-LAYOUT, CHANNEL-LIST-GEOMETRY.
