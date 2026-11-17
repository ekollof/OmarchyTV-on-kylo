# Stock EPG timeline layout and gated screen — 2026-11-17

Status: current (layout, selection and chrome/synopsis screens; no stock EPG
capture)
Last verified: 2026-11-18 (firmware resource/instructions and host recorder
tests; selection, synopsis image, event options)
Evidence class: firmware resource/instructions and host/recorder tests. No TV
run, stock EPG capture, live EIT feed or scanout.

This recovers the multi-service EPG timeline from the shipped
`DialogEPG.svg` (BinSVG 101) and `MultiServiceTimelineScheduleDialog`, instead
of inventing a grid. Raw rectangles and the recorder candidate are in
[artifacts/analysis/epg-stock-2026-11-17](../../analysis/epg-stock-2026-11-17/README.md).

## Firmware contract

- Class: `MultiServiceTimelineScheduleDialog`, internal name `EPGTimeline`.
  `getEPGTitle` returns the `PROGRAM_GUIDE` key (dut "Programmagids").
- Five tab buttons are **text** buttons: `SVGButton::create` @0x010aea28
  looks the name up through `UIResourceManager::lookupString`, so these are
  localized labels, not glyph art. Labels `NOW_NEXT_SCHEDULE`, `HLP_FILTER`,
  `PREVIOUS_DAY`, `NEXT_DAY`, `EXTRAS` (dut "Nu/Volgende schema", "Filter",
  "Vorige dag", "Volgende dag", "Extra's"). The button states are
  TabButtonFA/UFA/FI/UFI (BinSVG 522..525): selected = solid white pill with
  `#811924` text, unselected = transparent with white text.
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

## Selection, focus and synopsis

- `MultiServiceTimelineScheduleDialog::onUpDownPress` @0x00dcd6a1 forwards
  Up/Down to the `ChannelNameListBox` and `ListTimelineBox::scrollUp/Down`, so
  it selects the service row; a changed row runs
  `ListTimelineBox::setFocusedEvent` with the current/last focus time.
- `onScrollTimeline` @0x00dcd531 maps Left/Right (ICE 0x20c/0x20d) to
  `scrollTimelineLeft/Right` -> `ListTimelineBox::scrollLeft/Right`
  @0x00dc7175/0x00dc7265 -> the focused
  `ServiceTimelineBox::scrollLeft/Right` @0x00dd500d/0x00dd5165. The service
  box moves the focused event index; when the index leaves the list or the row
  is not user-focused, the shipped body instead scrolls the time window and
  re-picks an event. This slice has no zoom/paging, so it implements the
  focused-index move within the fixed window and leaves the window-scroll
  branch open.
- `ServiceTimelineBox::setFocusedEvent` @0x00dd4c5c walks the list and takes
  the first event whose end is after the focus time; if none, the last event.
  `ListTimelineBox::setFocusTime` @0x00dc7098 uses the current time when it is
  inside the window and the window middle otherwise. `oa_epg_focus_event`
  encodes that rule.
- `onSelectPress` @0x00dce625 calls `showGuideEvent` @0x00dce401, which opens
  `GuideEventOptionsDialog` for the focused event (view channel/record/
  reminder/...); the option list and actions are not recovered.
- `onInfoKey` @0x00dcd234 toggles the synopsis flag and calls
  `showSynopsis`/`hideSynopsis` (resource 102/101). The shell's RC map has no
  INFO id, so the toggle is exposed as `ui.epg_info_key` (0 = disabled) rather
  than guessed.
- The record-102 event image comes from
  `ImageHelper::displayEventImage` @0x00de660c: `EnhancedServiceEvent::getImage`
  when available, else `displayServiceImage`. The project boundary is the
  optional `epg.image` v1 service (`oa_epg_image_iface.get`, borrowed RGBA);
  absent means the 62,299..382,479 box stays blank. A supplied image is scaled
  into the 320x180 rect.

## Event options dialog (record 151)

Select opens `GuideEventOptionsDialog` via `showGuideEvent` @0x00dce401. Its
`getDialogName` is `GuideEventOptions`; the base `OptionsListDialog` resolves
the theme resource `DialogOptionsList.svg` (BinSVG 151):

| element | rect |
|---|---|
| panel | 0,0,1920,1080 |
| bgTextbox | 610,806,1310,909 (white at 7/10) |
| itemPos (first row) | 610,844,1310,920 (#010101) |
| dialogTitle | 634,750,1320,825 |

`calculateItemPosition` @0x00ea963c puts item i at
`itemPos.y + i*(bgTextbox.y1 - itemPos.y)` (pitch 65, height 76);
`calculateHeight` @0x00ea9700 is
`(bgTextbox.y1-itemPos.y)*count + (itemPos.y-bgTextbox.y) +
(bgTextbox.y1-itemPos.y1)*2` = 65*count + 16 for this record.
`reposition` @0x00ea9894 shifts the dialog by
`base.y1 - (height + dialogTitle.y1)`, but the base rect is a
runtime-initialized screen rect (BSS pointer 0x1a1a464), so the absolute anchor
is not pinned offline; the screen takes an explicit
`ui.epg_options_offset_y` (0 = resource coordinates).

`GuideEventOptionsDialog::init` @0x00ea5919 builds the buttons in order:
`VIEW_CHANNEL` always; with an enhanced event `RESTART` when
`getRestartURL() != 0` else `HLP_WATCH` when `onDemandAvailable() == 2`, and
`MORE_INFO` when `getNoInformationAvailable() == 0`; without one it adds
`MORE_INFO` from the first vtable call; `addServicePlayerButton` @0x00ea572c
then adds `LAUNCH_SERVICE_PLAYER` when not in OnDemandProgram mode and the
metadata provider reports a player. The OnDemandProgram branch is
`HLP_WATCH` (if available), `MORE_INFO`, then the player.
Labels resolve through `UIResourceManager::lookupString` (dut "Zender kijken",
"Restart", "Bekijken", "Meer informatie", "Player lanceren"); the selected row
is `#FE9419` with `#7E000A` text, unselected rows `#010101` with white text.

`ui.epg_options=1` (default off) opens the overlay on Select and paints the
recovered title/bgTextbox/rows; Up/Down move the focus, Select logs the
unrecovered action and closes, Exit closes. The available runtime inputs are
explicit: an event is enhanced when present, `no_information` when its
description is empty, and restart/on-demand/player checks have no backend here
(0). The full-screen white `background`/`main_rectangle` are not repainted over
the EPG, and the anchor is 0 by default, both flagged open pending a capture.
`check-guide-options` covers the geometry/height/option rules and
`check-ui-epg-stock` drives the overlay (`epg-stock-options.png`).

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

## Event-button styles and synopsis

`SVGServiceEventButton::resetSVGNames` @0x00dd5404 selects
`serviceBFA/UFA/FI/UFI` (BinSVG 456/457/458/459) for the default path. Fills:
FA row `#F7941E` / time `#D37B1C` / text `#7E000A`; UFA row `#7E000A` / time
`#6D010F` / white text; FI row `#EDEDED` / time `#D8D8D8` / `#7E000A`; UFI is
UFA's fill. Unfocused A/I share one fill, so A/I only changes the focused
colour. The bars now use these recovered styles (`active` = now inside the
event; focused false until a selection model exists).

`showSynopsis` @0x00dca1b0 selects `SVGDialog::setSVGName 0x66` (record 102);
`hideSynopsis` returns to 0x65. Record 102: header 0,0,1920,264 (family
inference), event bar 0,264,1920,571 (`#010101` 5/10), event image
62,299,382,479, event text 429,273,1882,512, duration 76,472,396,517, date
44,518,368,562, timeM 1196,519,1446,563, list 0,570,1920,1077 and service
column 0,572,640,994 `#C5C9CD`. `ui.epg_synopsis=1` renders the selected
event title, `HH:MM-HH:MM` time, vendor duration `%dh %dm`/`%dm %s` and one
clipped description line, and the bottom area keeps the timeline: the
synopsis reuses `createServiceList` on EPGListBox 0,570..1920,1077, split at
641, so the service names and event bars are painted there with the same
7200 s ratio. Both views paint the five tab labels from the i18n keys with the
TabButton state colours.

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
  `OA_SVC_EPG`, the focused service/event (Up/Down, Left/Right; focused style
  FA/FI when the event is running, UFA/UFI otherwise), the now indicator and
  Exit. Rows paint only when `ui.epg_row_pitch` is explicitly set; the default
  is chrome only because the runtime row step is art-derived and not pinned
  offline. `ui.epg_info_key` toggles the record-102 synopsis; the optional
  `epg.image` provider supplies the event image.
- `tests/ui_epg_stock.c` publishes a fixture schedule (three 30-minute events
  on service 1, one 2-hour event clipped at the window end on service 2), sets
  `ui.epg_row_pitch=76` and `ui.epg_window_start`, and asserts the chrome
  fills, split border, bar x/width, scroll and Exit; `check-ui-epg-stock`
  passes.
- `tools/epg-stock-check.sh` renders the real-font candidates `epg-stock.png`
  and `epg-stock-synopsis.png` with the Dutch langprofile: "Programmagids",
  "Di 14/11/2023", 23:13/00:13/01:13, NPO 1/NPO 2/RTL 4,
  Journaal/NOS Sport/Film, the clipped "Lang programma" and the synopsis
  "Journaal / 23:13-23:43 / 30m 0s".

## Limits and next step

No stock EPG screen capture exists, so the candidate is recorder evidence, not
pixel parity or scanout. The header fill is a family inference; the exact
vendor tab font record is not pinned (`ui.tab_font_id` defaults to 4 because
font7 overflows the 300 px tabs), channel logo/HD/encrypted icons, the
`GuideEventOptionsDialog` actions, the real event-image backend (the optional
provider has no producer), the service-image fallback, the options anchor and
background treatment, text wrapping,
day navigation, filters, reminders/record actions, the stock entry path, the
runtime row pitch and scanout remain open. Event-bar colours, selection/focus
styles and the INFO synopsis toggle are recovered. Live EIT/EPG data still
requires the owner-authorized TV/tuner window; only the ingest/query path is
offline-tested.

Report: EPG-STOCK-LAYOUT-2026-11-17. Related: EPG-EIT-MODULE, EPG-NOWNEXT,
EPG-PERSISTENCE, BROADCAST-IPC, PVR-STOCK-LAYOUT, CHANNEL-LIST-GEOMETRY.
