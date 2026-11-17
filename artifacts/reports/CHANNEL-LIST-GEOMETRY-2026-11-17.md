# Stock channel-list geometry — 2026-11-17

Status: current (stock mode wired behind `ui.channels_stock=1`)
Last verified: 2026-11-17 (firmware SVG resource, read-only stock capture,
host/recorder tests and a capture comparison)
Evidence class: firmware resource, stock capture, recorder/host tests. No TV
run or scanout.

This recovers the stock channel-list screen geometry that the provisional
`SCREEN_CHANNELS` UI lacked. Sources are the shipped
`DialogAdvancedChannelList.svg` (BinSVG record 80) in
`artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_svg.{txt,bin}`
(aurora MD5 `64c252340d3af6c5d66072611b7bad7a`) and the read-only p0/04 capture
`artifacts/protocol/p0/04-channel-list.png`. Raw rectangle and capture dumps
are in
[artifacts/analysis/channel-list-geometry-2026-11-17](../../analysis/channel-list-geometry-2026-11-17/README.md).

## Screen rectangles (record 80)

| element | x | y | x1 | y1 |
|---|---:|---:|---:|---:|
| main_rectangle / panel | 0 | 0 | 780 | 1080 |
| banner | 0 | 0 | 780 | 162 |
| dialogTitle | 51 | 2 | 698 | 67 |
| filterInfo | 123 | 50 | 766 | 95 |
| watchButton (Bekijken) | 12 | 100 | 262 | 159 |
| editButton (Bewerken) | 264 | 100 | 514 | 159 |
| filterButton (Filter) | 516 | 100 | 766 | 159 |
| channelListBox | 19 | 179 | 769 | 1090 |
| logo | 610 | 21 | 766 | 66 |

## Colours

The SVG fills run through the stock theme; the p0 capture is BGRA-swapped, so
the corrected view swaps R/B. The values below are the corrected ones.

| role | value | source |
|---|---|---|
| panel | `#BE1E2D` (190,30,45) | record 80 fill |
| banner | (178,29,43) | `#811924` at opacity 2/10 over the panel |
| unselected tab | (193,71,83) | white at opacity 7/10 over the panel |
| selected tab | white | record 80 fill |
| focused row | `#FE9419` (254,148,25) | record 32 row fills + capture |
| normal text | white | capture |
| focused number/name | `#7E000A` (126,0,10) | record 36 textPos + capture |
| muted text | `#C6C6C6` (198,198,198) | record 80 filterInfo stroke |

## Rows (capture; not in record 80)

The first row surface is 19,195..767,261 (748x66), pitch 75, with 12 row
starts inside the 1080 px panel (row 11 is clipped). Focused number/name ink is
40 px high in `#7E000A`; normal row ink is 34 px white; the type marker is
white and its `ATV` ink ends at x=753. The focused row therefore also scales
its label, matching the list-row behaviour rather than a colour-only focus.
The font record and exact text anchors are not yet recovered.

## Implementation

- `include/openaurora/channel_layout.h`, `common/channel_layout.c`: pure
  layout/colour/row arithmetic. `oa_channel_tab_at` uses exclusive edges and
  the 2 px gaps; `oa_channel_row_rect` returns full rows and leaves the last
  partial row for the caller to clip; `oa_channel_visible_rows` returns 12.
- `tests/channel_layout.c` / `make check-channel-layout` assert every
  rectangle, colour, tab boundary, row and rejection above.
- **Stock mode (2026-11-17):** `ui.channels_stock=1` (default off) makes
  `ui_vendor` paint the stock panel/banner/tabs/rows with the model, the
  matching vendor font records and ink-cropped text. The provisional screen
  remains the default.
  - Fonts were identified by captured ink boxes: title "Zenders" 138x32 →
    record 7; subtitle 132x26 and selected tab 116x33 → 12; unselected tabs
    114x22 → 4; normal row 73x35 → 16; focused row 72x41 → 18; `ATV` 27x12 →
    1. `ui_vendor` loads 18/12/1 in stock mode and reuses the text-load fonts.
  - `tests/ui_channels_stock.c` / `make check-ui-channels-stock` uses 11
    synthetic analog rows and asserts the record-80 fills, tab gaps, 75 px
    focus rows and text blits.
  - `make check-ui-channels-stock-capture` renders the same rows with the real
    vendor fonts and the p0/04 names (C12, C07, BBC2, …), writes
    `stock-mode.png` and compares it with the corrected capture: panel MAE8
    **2.314**, banner **1.393**, tabs **0.132**, list **2.510**, focus row
    **1.510**, row 2 **2.581**. The header icon is the theme `channel_small.png`
    (60x60, ink 2,9..58,57) drawn at 30,14 and staged by `mk-ui.py`/the check.
    The tab labels use the captured ink anchors (Bekijken 80,116; Bewerken
    333,118; Filter 615,118), the subtitle uses font record 5 at 123,63, and
    the unselected tabs carry the captured 1 px `#CC6B74` border (interior
    `#C14753`, selected right edge `#FDFAFA`). The scrollbar is wired from
    `createScrollBars` (0x010bbd38) and the 20+16 px skin caps: 11 visible rows,
    3x816 window at 768,195, min bar 36; the 67-row fixture gives a 133 px bar
    by the shipped integer formula (capture white grip 134 px at y196, a 1 px
    edge/antialias difference). The stock screen's Up/Down/Select/Exit handling is
    tested (Select emits index 1 name `C07`). The stock portal sidebar capture
    (`shot-launcher.png`) orders the rail three-circle/source-arrow/home/
    monitor-Zenders/wrench; Select on rail 3 now opens `SCREEN_CHANNELS` only with `ui.channels_stock=1`; the default rail path is unchanged and is
    covered by the stock test. The header icon is `channel_small.png` (60x60)
    at 30,14.
  - Runtime order: both `paint_channels_stock` and the key handler prefer the
    ingested `OA_SVC_CHANNELS` active list. The stock test loads `channels`,
    ingests three runtime entries and asserts the rows render in that order and
    Select reports `Runtime A`. The live EASI/testtool feed remains owner-gated;
    only the ingest/render/selection path is tested offline.

## Scrollbar contract (wired 2026-11-17)

`SVGListBoxBase::createScrollBars` (0x010bbd38) builds the scrollbar window
from the listbox fields: left `+0x214`, right `+0x21c`, top `+0x1fc`,
`bottom=(visible-1)*step+top+rowHeight`, and only when `visible<total`. For the
channel list that is x768..771, y195..1011 (3x816) with 11 visible rows.
`scrollbarFA.svg` record 61/62 give `bar_mTopM` 20 px and `bar_mBotM` 16 px, so
`calculateMinBarSize` gives `min_bar=36`. `SVGScrollBar::calculateBarSizeAndPosition`
(0x10c3af0) is implemented in `common/svg_scrollbar.c` (`bar=height*visible/
total`, clamped to `min_bar`, then the position formula). `paint_channels_stock`
now draws the white bar from those inputs. The 67-row fixture gives 133 px by
the shipped integer formula; the capture's white-threshold grip is 134 px at
y196 (1 px edge/antialias), and the capture check scrollbar region is MAE8
8.319. No fitted track/total.

## Stock key path

`AdvancedChannelListDialog` recovers the key handling: `onLeftRightPress`
@0x00e7e23c forwards Left/Right to the tab bar (with mirror swap), so
Left/Right move the watch/edit/filter focus; `onUpDownPress` @0x00e7ad28
forwards Up/Down to the list; `onSelectPress` @0x00e7e2e4 dispatches
`displayFilter` for the filter pane, `displayService` for the watch pane and
`displayEdit` for the edit pane; `onDigitPress` @0x00e7cb18 opens the modal
`GotoServiceDialog` and sets the list selection from its result; MENU key
0x210 ends the dialog to 0x29a (portal) and GUIDE sends dialog navigation
0x0b (EPG). The stock mode implements the tab focus and the watch-pane select
action; the edit/filter panes and the GotoService dialog are recorded but not
implemented, so Select on those panes logs and emits nothing.

## Edit-options pane (record 100)

`ChannelEditOptionsDialog` (internal name `ChannelEditOptions`) is opened by
`displayEdit` and uses `DialogEditOptionsList.svg` (BinSVG 100): panel
0,35,780,1080 white; left strip 0,160,142,1080 `#010101` at 6/10 (composed
103,103,103); content 142,160,782,1080 `#3F0B13`; list 152,211,768,1059;
dialogTitle is offscreen. `initialiseFields` @0x00e8decc adds the options in
this order:

| case | option | condition |
|---|---|---|
| 0 | ADD_TO_HOME | single selection and not already at home |
| 1 | MOVE_TO_POSITION | multiple selection |
| 2 | DELETE / REMOVE | not Rovi and deletable; DELETE single, REMOVE multiple |
| 3 | EDIT_NAME | name editable and single selection |
| 4 | HLP_LOCK_UNLOCK | always |
| 5 | HLP_FAVOURITE_ADD_REMOVE + " " + FAVOURITES | always |
| 6 | HLP_TAG | always |
| 7 | HLP_TAG_ALL | always |

Rows use the captured 70 px settings step and the 66 px row art (12 fit).
`ui.channels_edit=1` (with `ui.channels_stock=1`) opens the pane on Select over
the edit tab; Up/Down move the selection and Select/Exit close it. The option
actions (move/delete/lock/rename/favourite/tag) are not implemented.
`common/channel_edit.c` + `check-channel-edit` cover the layout, catalogue,
visibility and DELETE/REMOVE label; `check-ui-channels-stock` drives the pane
and `stock-edit.png` is the Dutch real-font candidate.

## Goto-service dialog (record 116)

`AdvancedChannelListDialog::onDigitPress` @0x00e7cb18 opens a modal
`GotoServiceDialog` (dialog name `GotoService`) and, when the result is not -1,
calls `SVGListBoxBase::setSelectedItem(result)`. Layout from
`DialogGoToService.svg` (BinSVG 116): background 0,0,1918,1080 white;
main_rectangle 0,0,540,239 white; serviceNumber -133,98,377,298 (the authored
pre-transform text box); animation_slide_down_fade_in 0,308,100,408.

`init` @0x00d2296c looks up `dash`, `bigFont` and `serviceNumber`, sets the
text entry mode to 0xb (11), font 0xf (15), maps the entry states to resources
0x18b..0x18e (BinSVG 395..398, gotoServiceTextEntryBoxFA/FI/UFA/UFI) and
creates the box from the serviceNumber rect. `onDigit` @0x00d2262c forwards the
key and restarts a 1500 ms timer (3000 ms with the keys-shortcut dialog open);
`handleMessage` @0x00d226c8 maps digits 0x200..0x209 to onDigit, Select 0x20e
and timer 0x1006/1 to `findServiceAndEndDialog` @0x00d223f8, and 0x21f/0x28b to
onStandby. `findServiceAndEndDialog` reads `getNumericContents` (decimal walk,
leading zeros accepted) and looks the number up in the service list.

`common/goto_service.c` + `check-goto-service` encode the record rects, the
numeric entry (max digits explicit, 4 in the screen), value/lookup and the
timeout constants. `ui.channels_goto=1` opens the overlay on a digit; our shell
has no digit key map and Select is id 53 (ASCII '5'), so the digit range is
explicit (`ui.goto_digit_base`, 0 = disabled; the fixture uses 100..109).
Up/Down are not part of the vendor handler; Select resolves the number against
the rendered order (runtime RSN, else lcn/channel_no) and moves the list focus,
Exit cancels. The authored serviceNumber x=-133 is a pre-transform coordinate
and the entry-state ink is not pinned, so the digits are centred in the authored
box (122,198) in #231F20 on the white panel; the timer and the exact dialog
animation/position remain open. `check-ui-channels-stock` drives 109 -> Runtime B
and `stock-goto.png` is the candidate.

## Filter/sort popup (record 636)

`ChannelListFilterSortDialog` (internal name `ChannelListFilter`) is opened by
`displayFilter` and uses `DialogFilterSortSelector.svg` (BinSVG 636): panel
60,117,666,467 fill `#494949` with a `#DCDCDC` border, `filterSortTypeList`
covering the same rect. `initialiseFields` @0x00e8f1a4 supplies the recovered
language keys: `CHANNEL_TYPE` (ALL/DVB_AERIAL_ONLY/DVB_CABLE_ONLY/
DVB_SATELLITE_ONLY/IPTV_ONLY/ANALOG_ONLY), `FILTER_TV_RADIO_LABEL`
(TV_ONLY/RADIO_ONLY/TEXT_ONLY), `FREE_PAY_LABEL`
(FILTER_FREE/FILTER_ENCRYPTED), `FILTER_LETTER_LABEL` (A-Z) and `SORT_LABEL`
(SORT_NUMERIC/SORT_ALPHABETIC). The HD values UHD/HD/SD, HD/SD, SD, HD, UHD are
literal but their item label is not in the langprofile, so that item stays
out. Rows use the captured 70 px settings step (66 px `SettingListBoxRowFA`
art); the 350 px list shows 5 rows.

`ui.channels_filter=1` (with `ui.channels_stock=1`) opens the popup on Select
over the filter tab. Up/Down move the item, Left/Right change its value
(wrapping), the selected row is `#FE9419` with `#7E000A` text, and Select/Exit
close it; applying the CA/filter view to the list is not recovered, so Select
currently only logs. `common/channel_filter.c` + `check-channel-filter` cover
the catalogue, navigation and layout; `check-ui-channels-stock` drives the
popup and `stock-filter.png` is the Dutch real-font candidate.

The provisional `SCREEN_CHANNELS` remains the default; the portal entry path
(rail 3), runtime-order ingest/selection, scrollbar and the tab focus/Select
dispatch are wired and tested.
The live EASI/LCN visibility feed and scanout remain open; the remaining
capture error is row text antialias and the scrollbar's 1 px white edge, not
fitted offsets. The capture comparison is
recorder/CPU, not fb1/scanout proof.

Report: CHANNEL-LIST-GEOMETRY-2026-11-17. Related: CHANNEL-LIST-UI,
CHANNEL-RUNTIME-ORDER, P0-BROWSER-PROBE, UI-THEME-PROVIDER.
