# SVGTextBox line setup and FA row geometry — firmware decode (2026-11-01)

No implementation. This records what the decompile establishes so title/tab
and focused-label work does not proceed from fitted capture geometry.

Firmware: Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Raw decompiles:
artifacts/analysis/ui-text-layout-2026-11-01/text-widget-surface.txt
(SVGTextBox ctor/renderBackground/setFont),
/tmp/opencode/decomp-textfg.txt (renderForeground/drawTextBox/getTextWidth).

## State-indexed resources are exact (no fit)

`SettingItemsListBox::SettingItemsListBox` (0xd518f1) assigns resources by
state index with immediates that equal the BinSVG resource numbers:

| call | state | immediate | resource |
|---|---|---|---|
| setSVGName | 1 | 0x1db=475 | SettingListBoxFA |
| setSVGName | 3 | 0x1dc=476 | SettingListBoxFI |
| setSVGName | 0 | 0x1dd=477 | SettingListBoxUFA |
| setSVGName | 2 | 0x1de=478 | SettingListBoxUFI |
| setRowSVGName | 1 | 0x1df=479 | SettingListBoxRowFA |
| setRowSVGName | 3 | 0x1e0=480 | SettingListBoxRowFI |
| setRowSVGName | 0 | 0x1e1=481 | SettingListBoxRowUFA |
| setRowSVGName | 2 | 0x1e2=482 | SettingListBoxRowUFI |
| setRowItemsSVGName | — | 0x1cc=460 | SettingItemsListBoxRowItems |

State order 0=UFA/1=FA/2=UFI/3=FI (established in UI-GLYPH-DRAW via
makeRow/setRowSVGName forwarding). The names in
oa-svg-names.tsv confirm each number. This closes "which resource is the FA
row": 479, and the FA text/button/selection resources 488/496/504 already
used for the reconstruction.

## Line setup fields (renderForeground 0x10c7a01)

- Font: if `+0x21a==0` and `+0x219!=0`, `+0x21c =
  getFont(rectangleWidth, +0x1b4, +0x1c4)` and `+0x219` is cleared. A second
  site uses `getFont(+0x200-+0x1f8, 1, +0x1c4)`. So `+0x1b4` is the line-count
  argument, `+0x1c4` the font-type argument, `+0x21c` the selected font ID.
  This matches UI-FONT-SELECTION's getFont(height, lines, type) contract.
- Line height: if `+0x1b4 != 0` and positive, `+0x1c0 =
  FUN_01285478(rectangleWidth)` (signed division = available height per line);
  else `+0x1c0 = rectangleWidth`. `+0x1c0` is then passed as the draw height.
- `drawText` receives, in order: text (`+0xa4`), font ID (`+0x21c`), flags
  (`+0x1b1`, `+0x1b0`), height (`+0x1c0`), colors, alignment (`uVar16` from
  `+0x220`, overridden by `+0x22c`), line count (`+0x1b4`), dimensions.
- Alignment `uVar16` is remapped for mirror/RTL (`getOSDHorizontalMirror`,
  Arabic/Hebrew shaping checks) before drawing.
- `+0x1b0` gates the scrolling-slide path (`AnimationHelper::
  calculateSlidingMargin` when `+0x2b0` set); otherwise the static path draws.

## Row object and FA widget map (SettingItemsListBoxRow ctor 0xd53d0d)

Each row owns fixed widget slots:

| offset | widget | role |
|---|---|---|
| +0x104 | SVGTextBox | label (normal rows) |
| +0x3c4 | SVGTextBox | second label (row type 1) |
| +0x684 | SVGButton | value button |
| +0x1014 | SVGSelectionBox | selection box |
| +0x13c0/+0x1498 | SVGSlideBar | sliders |
| +0x1570/+0x1724 | SVGProgressBar | progress bars |
| +0x18d8 | SVGTextEntryBox | text entry |
| +0x1ca8/+0x1f68/+0x2228/+0x24e8 | SVGTextBox | extra text (text2/3/4, value/caption) |
| +0x27a8 | SVGCheckBox | checkbox |
| +0x2b28/+0x2c0c/+0x2cf0 | SVGImage | icons |

Default row type `+0x2dec = 0x1b` (27). Default per-state resource IDs stored
in the row object (immediates in the ctor):

| field | value | resource |
|---|---|---|
| +0x2e8c | 0x1e9=489 | textbox UFA |
| +0x2e90 | 0x1e8=488 | textbox FA |
| +0x2e94 | 0x1eb=491 | textbox UFI |
| +0x2e98 | 0x1ea=490 | textbox FI |
| +0x2eac/0x2eb0/0x2eb8/0x2eb4 | 0x1f1/0x1f0/0x1f2/499 | button UFA/FA/FI/UFI |
| +0x2e9c/0x2ea0/0x2ea8/0x2ea4 | 0x1fa/0x1f8/0x1f9/0x1fb | selbox UFA/FA/FI/UFI |
| +0x2e6c/0x2e70/0x2e78/0x2e74 | 0x1f1/0x1f0/0x1f2/499 | button set 2 (type 5) |
| +0x2e80/0x2e88/0x2e7c/0x2e84 | 0x1f0/0x1f2/0x1f1/499 | button set 1 (type 6) |

(resources: 496 ButtonFA, 497 ButtonUFA, 498 ButtonFI, 499 ButtonUFI,
504 SelBoxFA, 505 SelBoxFI, 506 SelBoxUFA, 507 SelBoxUFI.)

## renderItems branches (0xd54401)

- The label textbox rect comes from a resource literal (`DAT_00d54750`), then
  `setWindowRect(rect offset by row position)` — i.e. the transformed text
  rect is the resource rect translated by the row's position. The text is set
  from the row label at `+0x2dd8`.
- If the first resource literal resolves (`iVar2==0`, literal `DAT_00d54748`
  = "alexa"… i.e. the row resource does NOT carry that marker), the label
  gets FA 488 / UFA 489 / FI 490 / UFI 491 by state. Otherwise all states get
  fallback 0x26d=621 plus `setFont(0x1a=26)`.
- Row type 1 uses the second textbox (+0x3c4) with fixed caption resources
  0x1ec–0x1ef = 492–495 (caption FA/UFA/FI/UFI), no fallback.
- Button states get 496–499 by state (type 5/6 sets differ only in which
  stored field supplies state 2); fallback 0x269=617 plus `setFont(0x1e=30)`.
- SelectionBox (types 2/3) gets 504–507 by state; fallback 0x26b=619 plus
  `setFont(0x1e=30)`.
- Mirror handling: `+0x2e4c/0x2e50` image IDs switch to 0x78/0x77 when
  `getOSDHorizontalMirror` is set.

The `DAT_00d5474c/50/54`, `DAT_00d54e30`, `DAT_00d54b24/28/2c` literals live in
.data as runtime UString state (not plain rodata strings like "alexa"), so
their exact names are not established from a static dump. Do not assign them.

## Picture shortcut rows (initialisePictureSettingsShortcuts 0xee0ed0)

The function builds exactly three static `SettingItem`s in a loop
(`iVar15` = 0, 1, 2), all with SettingType 2. Labels are global UStrings
(first word = key pointer), resolved from the binary:

| id | label key | values (SettingValue index → key) |
|---|---|---|
| 0 | MODE | dynamic picture-mode list from `PictureModeNameProvider` (id 0 item; the "Modus/Natuurlijk" row) |
| 1 | ENERGY_SAVING | 0 OFF, 1 MINIMUM_STR, 2 MEDIUM, [3 MAXIMUM_STR if mode≠3/0], 4 AUTO, 6 SCREEN_OFF, 5 CUSTOM (note 6 added before 5 in the instruction order) |
| 2 | PICTURE_ZOOM | includes 1 ZOOM_MODE_AUTO, 5 ZOOM_MODE_16_9, 6 ZOOM_MODE_SUBTITLE (partial enumeration; full list not yet traced) |

`setType(2)` (0xd51e01) shows only the label textbox (+0x104) and the
selection box (+0x1014); button/sliders/progress/entry/check/images are
hidden. So the static shortcut list is three selection-box rows — the
slider rows (Contrast/Helderheid/Scherpte/Kleur with font16 digits) are NOT
created here; they come from the dynamic `update*` paths
(`updatePictureMode` 0xee3bc9 / `updateSettingsList` 0xedf46d), still untraced.
Each created shortcut row gets `setSelectionDisabled(row, true)` after
insertion (0xee11b0 site).

This matches the panel: Modus/Energiebesparing/Beeld-zoomen are text-value
selection rows (font7 values), while 95/50/50/60 are slider digits (font16)
owned by the dynamic rows.

## PictureSettingsDialog rows (initialiseFields 0xe0891d)

Item inventory (`SettingItem(framework, id, label, type, values, ...)`):

| id | label key | type | widget (setType) |
|---|---|---|---|
| 1 | CONTRAST (+suffix) | 9 | slider + value textbox |
| 2 | BRIGHTNESS (+suffix) | 9 | slider + value textbox |
| 3 | (00e09500 area) | 9 | slider + value textbox |
| 4 | COLOUR_STR | 9 | slider + value textbox |
| 5 | ENERGY_SAVING | 2 | label + selection box |
| 6 | PICTURE_ZOOM | 2 | label + selection box |
| 7 | PC_POSITION_SETTINGS_TITLE | 4 | button row |
| 8 | PIP_SETTINGS_TITLE | 4 | button row |
| 9 | (built label) | 2 | label + selection box |
| 10 | (built label) | 4 | button row |
| 0x120 | OLED Settings | 4 | button row |
| 0x6d | ADVANCE_SETTINGS | 4 | button row |
| 0xd | TV_TYPE | 2 | label + selection box |
| 0xe | DISPLAY_FORMAT | 2 | label + selection box |
| 0xf | HDMI_RESOLUTION_OPTION_LABEL | 2 | label + selection box |
| 0 | MODE | 2 | label + selection box |

Slider min/max/current come from the provider at `+0x5e8` (`+0x1e8`/`+0x1ec`
for bounds). `drawRow` (0xd50f61) maps item type 9→row 0x17, 10→0x18,
0xb→0x19 under the progressbar-in-place flag; `setType` case 9 shows the
slidebar (+0x13c0) and the value textbox (+0x1ca8) — the font16 digit rows.

## TextBox render branches (asm-verified, renderForeground 0x10c7a01)

`+0x1b4` (line count; ctor default 0, no writer found in row/text paths):

- `< 0`: return, nothing drawn (10c8258 `ble` taken).
- `== 0`: `+0x1c0 = full rect height`, single-line draw (count 1).
- `> 0`: `+0x1c0 = FUN_01285478(height, +0x1b4)` (signed division = height
  per line), then scroll/slide or static multi-line draw.

Vendor `getFont(height, lines, type)` (0xeeedc0) calls the same division
**before** the type switch (after only the empty-finder check), and
`FUN_01285478` raises SIGFPE on divisor 0. So a zero line count reaching
`getFont` with a populated finder would fault — yet row text renders. The
writer that makes `+0x1b4` nonzero for live rows is still unidentified
(`setText` only clears `+0x1bc`; `setLabel`/`setButtonText`/`appendItem`
don't touch it; neither do `makeRow`/row ctor/drawRow). Do not assume a
default of 1: the ternary above is the observed behavior.

`setTextAlignment` (0x10c72a9) writes `+0x220` (`+0x224` explicit flag; value
5 also clears the bold flag `+0x1b1`). Alignment in renderForeground is a
bitmask on `+0x220`: `0x92` → left-ish, `0x248` → right, `0x124` → center,
else left+width.

## Tab composition (TabDialog::showSubMenu 0xd6f141)

TabDialog owns 5 tab `SVGButton`s (+0x4fc/+0xe8c/+0x181c/+0x21ac/+0x2b3c =
`+0x34cc..+0x34dc`), the title textbox (+0x34e0) and the Left/Right `UIGroup`
(+0x4cc, keys 0x20d/0x20c). `updateTitles` sets each created tab button's text
from localized keys (default literal "tab_text", `DAT_00d6fdb4` → 0x14ddb58);
the key table itself (`DAT_00d6fdbc` → 0x1a0f1bc) lives in .bss, so the five
keys are not statically resolvable — do not invent them. Tab labels are
button texts at the 191 resource positions, not separate textboxes.

`showSubMenu` creates the focused tab's content dialog from the UIGroup focus
id (`+0x4ac`):

| tab | content dialog |
|---|---|
| 0 | PictureSettingsDialog (Beeld — the captured panel) |
| 1 | SoundSettingsDialog (Geluid) |
| 2 | IPMultipleDeviceSettingsDialog (Netwerk, if wifi available) |
| 3 | InstallationSettingsListDialog |
| 4 | SettingsListMenuDialog |

So the vertical Beeld panel is `PictureSettingsDialog` hosted as tab 0's
submenu. Tab switching = destroy/create the content dialog; `setSelectedItem`
only stores the index (`+0x4a8`). Left/Right with the submenu flag set routes
through the tab UIGroup then `updateGlyphAndTitle` + `showSubMenu`; Up/Down
with flag set focuses the submenu list.

## Value/tab interaction (no implementation yet)

`onRightLeftPress` (0xe0ad0d): stores the selected id; id 6 with dual view
shows a modal prompt; key-repeat is enabled on 0x2c8/0x2c9. Ids 0–5 go
through `updateViaPictureElementSetting` (0→0x16 plus mode-value refresh,
1→1, 2→2, 3→3, 4→4, 5→0x22) after an energy-design check; id 0xd logs and
refreshes. All other ids (except 0x6d/8) remap 0x2c8→0x20c / 0x2c9→0x20d and
forward to `SettingsDialog::onRightLeftPress` — the base value adjust.

`onSelectPress` (0xe0a81d): 7 opens PCPicturePositionDialog; 0x6d opens
AdvancedPictureSettingsDialog; 0x120 forwards to handleSettingsMenuResult;
8 posts navigation message 0x19 (or a prompt); 10 opens the demo-mode dialog.
`TabDialog::onRightLeftPress` switches tabs (hide → UIGroup 0x20c/0x20d →
updateGlyphAndTitle → show) when the submenu flag is set.

## Line count writer found (SVGTextBox::create 0x10c8889)

`create(id, text, rect, param_4, parent)` writes `*(this + 0x1b4) = param_4`
— param_4 **is** the line count — and stores the client-offset rect at
`+0x284..+0x2a0` with `setWindowRect`. `renderItems` creates the label/caption
boxes with literal `1`, so row text is single-line; `getFont(h/1, type)` can
never divide by zero there. The `+0x1b4` writer question is closed for rows.
(`+0x1c4` type writer in row paths is still unidentified; ctor default 0.)

`setLabel` routes by row type (1 → caption box `+0x3c4`, else `+0x104`) and
stores `+0x2dd8`; `setButtonText` → button `+0x684` / `+0x2de0`;
`setTextValue`/`setNumericTextValue` target only the text *entry* box
(`+0x18d8`, not a display widget).

## Value display path (setSlideValue 0xd52efd)

Slider rows display their value in the value textbox `+0x1ca8`: type 9
drives slidebar `+0x13c0` (other types `+0x1498`), stores `+0x2df8`, and
integer-formats the value into `+0x1ca8` via `operator<<`. Those are the
font16 digits at the fitted origins. Type-2 selection rows show values via
`appendItem` → selection box (`+0x1014`) with `setSelectedItemWithId`.

## Still open (no implementation)

Do not implement value/tab/slider-graphics/title/focused labels from the fits.
Next: resolve the `renderBackground` rectangle literals per widget resource,
then implement from firmware geometry.
