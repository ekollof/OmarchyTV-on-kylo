# Stock EPG timeline — 2026-11-17

Offline recovery from `DialogEPG.svg` (BinSVG 101), its synopsis (102), the
`MultiServiceTimelineScheduleDialog` / `ListTimelineBox` /
`ServiceTimelineBox` bodies and a recorder render. No TV operation and no stock
EPG screen capture exists.

## Sources

- Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`.
- `telefunken18_svg.{txt,bin}` records 101 (`DialogEPG.svg`), 102
  (`DialogEPG_synopsis.svg`), 346/348/352 (`EPGlistbox*` row/text slices);
  `rects.txt` is the raw dump.
- `MultiServiceTimelineScheduleDialog`: `getDialogName` -> `EPGTimeline`,
  `createTabButtons` @0x00dc9354 (five glyph actions), `createServiceList`
  @0x00dc9ce4 (list split), `updateTimelineTitleBar` @0x00dc9950 (date and
  start/middle/end labels), `setStartOfSchedule` @0x00dc8390 (local midnight
  minus `86400*n` seconds for day navigation), `getEPGTitle` -> key
  `PROGRAM_GUIDE`.
- `ServiceTimelineBox::createButton` @0x00dd4914 and `setInterval`
  @0x00dd4fc0: event bar `x = boxWidth*seconds/interval`, `x1` clamped to the
  box width; interval in seconds at `+0x80`; `ListTimelineBox::zoom`
  @0x00dc6f1c steps 3600 s between 3600 and 14400 s and the initialized global
  (0x178d628) is **7200 s**.
- `TimeDateServices::getDateString` @0x00eaec00: `<weekday-short> %02u/%02u`
  plus `/%u` when the year flag is set; `getTimeString` @0x00eaea94:
  `%02u:%02u`.

## Screen rectangles (record 101)

| element | x | y | x1 | y1 |
|---|---:|---:|---:|---:|
| panel | 0 | 0 | 1920 | 1080 |
| dialogTitle | 164 | 21 | 560 | 75 |
| channelName | 590 | 9 | 1434 | 81 |
| eventTitle | 593 | 84 | 1437 | 144 |
| serviceLogo | 204 | 43 | 354 | 193 |
| channelNumber | 358 | 65 | 538 | 137 |
| eventTime | 290 | 149 | 551 | 194 |
| clockTextBox | 1175 | 149 | 1434 | 194 |
| red under-list band | 0 | 319 | 1920 | 1080 |
| date bar | 0 | 264 | 1920 | 318 |
| date | 44 | 266 | 368 | 311 |
| timeL / timeM / timeR | 677 / 1196 / 1713 | 266 | 927 / 1446 / 1868 | 311 |
| iconLeft / iconRight | 620 / 1868 | 265 | 674 / 1922 | 319 |
| EPGListBox | 3 | 319 | 1920 | 1043 |
| EPGServiceListBox | 0 | 319 | 640 | 1043 |
| tabButton1..5 | 206+302*i | 198 | 506+302*i | 257 |

Colours: list `#C5C9CD`, service column white with `#231F20`, red band
`#BE1E2D` at 9/10, date bar `#010101` at 5/10, event title `#F7941E`, arrows
`#009F93`. The tab rects use a red placeholder stroke and are **not** painted:
the five buttons are glyph actions (`NOW_NEXT_SCHEDULE`, `HLP_FILTER`,
`PREVIOUS_DAY`, `NEXT_DAY`, `EXTRAS`), whose glyph assets are not identified.

The top band 0..319 is left on the resource's white canvas with white text
strokes; the runtime theme must supply a header. This slice uses the
full-screen dialog family header `#481730` (DialogPlayList resource) and the
date bar composed over it, flagged as a family inference pending a stock
capture. Do not treat it as record-101 evidence.

## List split and event geometry

`createServiceList` splits the list at
`services.x1 = list.x + EPGServiceListBox.width + 1`, giving
service column **3..644** and timeline **644..1920, 319..1043** (width 1276).
`ServiceTimelineBox::createButton` computes:

```
x  = trunc(width * (eventStart - windowStart) / interval)   clipped at 0
x1 = trunc(width * (eventEnd   - windowStart) / interval)   clamped to width
```

with seconds. `zoom` provides 1/2/3/4-hour windows and the default is 2 h.
The `now` indicator uses the same ratio.

The **row pitch is not pinned**: `ChannelNameListBox::getTimelineRowHeight`
just returns `SVGListBoxBase::rowHeight`, which is derived from the scaled row
art at runtime. The model therefore takes an explicit pitch; the screen paints
rows only when `ui.epg_row_pitch` is set (default 0 = chrome only). The
candidate uses 76 px, the height of `EPGlistboxRowFA.svg`'s `mRowM` at its own
artboard, as an explicit test input — not a recovered runtime step.

## Screen wiring and evidence

- `common/epg_layout.c` + `include/openaurora/epg_layout.h`: rectangle/colour
  model, list split, event span, instant x, explicit-pitch visible rows and
  tab hit-testing; `check-epg-layout` covers all of them.
- `ui.epg_stock=1` (default off) + `ui.screen=epg` / `open:epg` adds
  `SCREEN_EPG`: recovered chrome, `PROGRAM_GUIDE` title, `Di 14/11/2023`-style
  date and start/middle/end times through `i18n`, service rows from the
  runtime channel list or stored DB, event bars from `OA_SVC_EPG`, and the now
  indicator when it falls inside the window. Up/Down scroll the service
  window; Exit returns to the portal.
- `tests/ui_epg_stock.c` publishes a fixture EPG schedule (three 30-minute
  events on service 1, one clipped 2-hour event on service 2), sets
  `ui.epg_row_pitch=76` and a fixed `ui.epg_window_start`, and asserts the
  chrome fills, the split border, the bars at their computed x/width and the
  scroll; `check-ui-epg-stock` passes.
- `tools/epg-stock-check.sh` renders `epg-stock.png` with the real vendor fonts
  and the Dutch langprofile (title "Programmagids", date "Di 14/11/2023",
  23:13/00:13/01:13, NPO 1/NPO 2/RTL 4, Journaal/NOS Sport/Film and the
  clipped "Lang programma"). No stock EPG capture exists, so this is recorder
  candidate evidence, not parity or scanout.

## Limits

The header fill is a family inference; the tab glyph art, channel logo/HD/
encrypted icons, event button art and colours, selection/focus, synopsis
(record 102), day navigation, filters, reminders/record actions, the stock
entry path, the runtime row pitch and scanout remain open. Bars use a
provisional four-colour palette because the vendor event-button resource is
not recovered.
