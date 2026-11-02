# SVGTextBox line setup and FA row geometry — firmware decode (2026-11-01)

No implementation. This records what the decompile establishes so title/tab
and focused-label work does not proceed from fitted capture geometry.

Firmware: Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Raw decompiles:
artifacts/analysis/ui-text-layout-2026-11-01/text-widget-surface.txt
(SVGTextBox ctor/renderBackground/setFont),
artifacts/analysis/ui-text-geometry-2026-11-02/rect-mask-draw.txt contains a
fresh durable renderForeground/create/mask extract. The originally referenced
/tmp/opencode/decomp-textfg.txt is gone; do not search for it as required state.
UI-TEXT-GEOMETRY-2026-11-02 corrects the scale axis/rounding and closes the
single-line textPos-to-draw path below. Other unsupported older paragraphs
remain dated research, not API contracts.

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

`+0x1b4` (line count; ctor default0; create's4th argument writes it, as
resolved in the later "Line count writer found" section):

- `< 0`: return, nothing drawn (10c8258 `ble` taken).
- `== 0`: `+0x1c0 = full rect height`, single-line draw (count 1).
- `> 0`: `+0x1c0 = FUN_01285478(height, +0x1b4)` (signed division = height
  per line), then scroll/slide or static multi-line draw.

Vendor `getFont(height, lines, type)` (0xeeedc0) calls the same division
**before** the type switch (after only the empty-finder check), and
`FUN_01285478` raises SIGFPE on divisor 0. So a zero line count reaching
`getFont` with a populated finder would fault — yet row text renders. The
writer was initially unidentified (the following negative search is historical;
create's4th argument and row literal1 resolve it below)
(`setText` only clears `+0x1bc`; `setLabel`/`setButtonText`/`appendItem`
don't touch it; neither do `makeRow`/row ctor/drawRow). Do not assume a
default of1: the ctor stays0, while row creation explicitly supplies1.

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
Title TEXT per tab is closed (analysis/ui-tab-titles-2026-11-02): static
literal keys give PICTURE/SOUND/NETWORK/INSTALLATION_STR/SYSTEM_STR
(default SETTINGS), i.e. Beeld/Geluid/Netwerk/Installatie/Systeem
(Instellingen), verified through the shipped profile; glyph ids
0x335/0x336/0x337/0x32f/0x338. Tab button keys stay runtime-only.

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

`onRightLeftPress` (0xe0ad0d, full decompile 2026-11-02): stores the selected
id; id 6 with dual view shows a modal prompt; key-repeat is armed on vendor
key codes 0x2c8/0x2c9 (our project-local key topic uses different ids —
21/22 for Left/Right). Ids 0–5 go through `updateViaPictureElementSetting`
(0→0x16 plus mode-value refresh, 1→1, 2→2, 3→3, 4→4, 5→0x22) after an
energy-design check — and that function (0xe0aca1) OPENS A SUB-DIALOG via
`showPictureElementSettingDialog`, then restores the selected item and
refreshes values. So Left/Right on the main-list slider rows does NOT adjust
in place: the slider lives in the per-setting sub-dialog. Our lab in-place
digit editing is therefore a functional placeholder only, honestly marked as
such; real parity needs the sub-dialog composition. Id 0xd logs and
refreshes. All other ids (except 0x6d/8) remap 0x2c8→0x20c / 0x2c9→0x20d and
forward to `SettingsDialog::onRightLeftPress` — the base list-navigation
adjust path.

`onSelectPress` (0xe0a81d): 7 opens PCPicturePositionDialog; 0x6d opens
AdvancedPictureSettingsDialog; 0x120 forwards to handleSettingsMenuResult;
8 posts navigation message 0x19 (or a prompt); 10 opens the demo-mode dialog.
`TabDialog::onRightLeftPress` switches tabs (hide → UIGroup 0x20c/0x20d →
updateGlyphAndTitle → show) when the submenu flag is set.

## Widget-name table mechanism (getWidgetStandardName 0xeee798)

**Withdrawn2026-11-02 (UI-LIST-METRICS):** the following expanded name table
and alleged four-entry variant tables are incorrect. The switch selects a
PC-relative literal then follows a global char* to a string, with explicit
mirror branches. Adjacent globals are not four variants of one widget. Do not
use the old names below to select slices; only the numeric call-site sequence
is retained as historical research. Verified normal/non-mirrored names include
2=gTopL,14=mTopL,17=mRowL,18=mRowM,22=mRowR,23=mBotL. Pointer chains are
preserved in analysis/ui-list-metrics-2026-11-02/widget-pointer-chains.txt.

Historical incorrect expansion:

```
47 background, 17 mBotR, 5 gRowR, 53 alignment, 22 bar_mTopM, 10 gRowR,
53 alignment, 18 mBotM, 6 gRowM, 53 alignment, 42 textPos,
65 fontRegular, 66 fontBold, 68 fontItalic, 67 fontLight,
70 fontLightItalic, 69 fontBoldItalic,
42 textPos, 14 mTopR, 2 gTopL, 53 alignment, 16 mRowR, 4 gTopL,
53 alignment, 23 mRowMYellow, 11 mRowR, 53 alignment, 25 gRowMRed,
13 mRowMGreen, 53 alignment, 15 mRowMRed, 3 gTopM, 53 alignment,
24 title_gRowR, 12 gBotM, 53 alignment, ... (second half repeats the
mask/font/textPos pattern for the remaining states)
```

Findings: `textPos` (id 42) is requested twice; ids 65–70 are the font-marker
names scanned in priority order (consistent with UI-GLYPH-DRAW types 0–5);
`alignment` (53) interleaves every slice. The second half repeats the pattern
for the remaining widget states.

The scale found last turn applies to the looked-up text rect on the
single-line branch: `x1' = x0 + (surfaceW/maskW) * rectW`. Which looked-up
rect becomes `+0x254`, and how the found font marker maps to the `+0x1c4`
getFont type, need register-level dataflow not completed here — plus the FA
animation target. No implementation from this yet.

## Ctor defaults + unconditional text-rect store (SVGTextBox)

Ctor: `+0x254` = `""` (literal `DAT_010c5df4` → 0x12f5cda), rect `+0x25c..`
empty; `+0x219 = 1` (font lookup pending), `+0x21c = 0`, `+0x220 = 4`,
`+0x1c4 = 0`, `+0x1b4 = 0`, `+0x224/5 = 0`.

In `renderBackground`, BEFORE the line-count branch, the looked-up record
(here literal `"addition"`) is stored to `+0x254..+0x274` when found;
otherwise the ctor defaults stand. Ghidra's types at this site are
unreliable (`UString::operator=` from an `EASIRect*`), so the natural reading
— record = {name; coords}, `+0x254` = name, `+0x25c..` = coords — is a
hypothesis, not a fact. What is firm: the store is unconditional and
pre-branch; row textboxes (488/489) have no `addition` rect, so for rows the
defaults stand unless another path writes them.

## Marker→type mapping, asm-verified (10c62ac–10c63c6)

The font-marker scan requests ids 65,66,68,67,70,69 but CHECKS in priority
order, first present wins; `sl` is overwritten by the id-66 (fontBold) result
(the earlier textPos value is consumed by the rescale first):

| check order | register | marker id | name | +0x1c4 if present |
|---|---|---|---|---|
| 1st | fp (10c62ba) | 65 | fontRegular | 0 |
| 2nd | sl (10c62da) | 66 | fontBold | 1 |
| 3rd | r9 (10c631a) | 67 | fontLight | 2 |
| 4th | r8 (10c62fa) | 68 | fontItalic | 3 |
| 5th | r6 (10c635a) | 69 | fontBoldItalic | 4 |
| 6th | [sp,#20] (10c633a; r7=sp+8) | 70 | fontLightItalic | 5 |
| none | — | — | — | 0 (default) |

Correction 2026-11-02 (independent adversarial review): the slot is
`[r7,#12]` = `[sp,#20]`, not `[sp,#12]` as first transcribed; verified against
`4104285` (`add r7,sp,#8`) and `4104663`/`4104708`. The stale "sl/textPos"
wording is also corrected: `sl` holds the id-66 (fontBold) result at chain
time, the textPos value having been consumed earlier by the rescale.

So `+0x1c4` = first-present font marker in type order 0..5; the writer question
is closed for this path (a separate unknown-suspend path at 10c5b98 also writes
it — not traced). Row label boxes carry only the FA/UFA markers relevant to
their state; e.g. FA 488 has fontBold, UFA 489 has none (falls through to the
sl/textPos or default arm per the chain above).

`create(id, text, rect, param_4, parent)` writes `*(this + 0x1b4) = param_4`
— param_4 **is** the line count — and stores the client-offset rect at
`+0x284..+0x2a0` with `setWindowRect`. `renderItems` creates the label/caption
boxes with literal `1`, so row text is single-line; `getFont(h/1, type)` can
never divide by zero there. The `+0x1b4` writer question is closed for rows.
`+0x1c4` is written by renderBackground's font-marker scan; the single-line
branch reaches it at10c636c/637c/63aa..63c4. It is not necessary to find a
separate row-specific writer. Marker mapping is in UI-GLYPH-DRAW.

Rect-to-draw consumption for textbox widgets (asm-verified, 2026-11-02):
`create` (0x10c8888) copies the passed rect to stack, and when the parent is
non-null calls `parent->getClientRect()` (0x1152974) and offsets the rect by
the parent client origin (`EASIRect::offset`). It then `stmia`s the result to
BOTH `+0x284` and `+0x294`, calls base `SVGWindow::setWindowRect` (+0x28),
and assigns id/parent. `SVGTextBox::setWindowRect` override (0x10c8ae8)
refreshes both copies from the hook-adjusted rect (vtable +40 call first),
writes the base rect, and calls `resizeUISurface` on the +0x1c8 surface with
the new rect. `drawTextBox` (0x10c8808) `ldmia`s +0x284 into the draw args and
dispatches vtable +80; it sets +0x219. `animRectDecode` (0x10c8c4c) reads the
second copy (+0x294/+0x298). So: resource rect + row offset (renderItems) →
parent-origin offset (create) → +0x284 → drawTextBox → surface draw, with
+0x294 feeding animation decode. The earlier "stores the client-offset rect"
note is superseded by this full chain; the UIWindow base rect itself lives at
+0x28 (verified from setWindowRect/getWindowRect decompiles), not +0x284.

Same dual-copy convention in SVGSelectionBox (asm-verified, 2026-11-02),
offsets +0x368/+0x378. `commonCreateStep` (0x10b5bf4) does parent
`getClientRect` + `EASIRect::offset`, then `stmia` to both copies and base
`setWindowRect` (10b5c38–10b5c5e, identical shape to textbox create).
`setWindowRect` override (0x10b5e60) writes both copies when created, then
redraws via `drawSelectionBox`. `drawSelectionBox` (0x10b5b44) refreshes copy
2 from copy 1 (`ldmia` +0x368 → `stmia` +0x378), resizes the +0xa4 surface
with the rect, then renders background/displayed item/animated rectangle
(animation start gated on enabled + duration > 1.0). `animRectDecode`
(0x10b6038) reads the second copy (+0x378/+0x37c).

Button verified, slidebar bounded (asm, 2026-11-02). `SVGButton::drawButton`
(0x10b07a4) reads +0x148..+0x154, refreshes copy 2 (`ldmia` +0x148 →
`stmia` +0x158), resizes the +0xa4 surface and dispatches render (vtable
+76) + renderForeground — identical to the selection-box draw path, with
copies at +0x148/+0x158 (written by the setWindowRect override at
0x10ade58–10ade64 after the base call + hook). The slidebar does NOT follow
this convention: `commonCreateStep` (0x10c4638) adds the parent client origin
inline (10c467c–10c468e, no `EASIRect::offset` call) and calls only base
`setWindowRect`; the override (0x10c4790) does base + `drawSliderBar`
redraw with no rect-copy stores. The slidebar works off the base +0x28 rect
(its value display is the already-decoded +0x1ca8 textbox path). Image
widget offsets remain unverified.

Image widget bounded too (asm, 2026-11-02): no dual-copy stores anywhere.
`SVGImage::imageCreate` (0x10c9430) contains zero `stmia` — it calls only
base `setWindowRect`; the `setWindowRect` override (0x10c96c0) re-invokes
`create(id, rect, parent)` instead of storing copies. The parent-origin
offset itself is shared (`getClientRect` + `EASIRect::offset` in create,
0x10c968c–10c9696). Final tally: dual-copy rects in textbox (+0x284/+0x294),
selection box (+0x368/+0x378) and button (+0x148/+0x158); base-rect-only in
slidebar and image.

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

## Row rect literals resolved (renderItems 0xd54401, asm + file bytes)

The `.data` literals are `char*` variables (double indirection at asm level:
`ldr rX,[pc]` then `ldr rX,[rX,#0]`); following both levels in the file gives:

| code literal | resolves to | 460-template rect |
|---|---|---|
| `DAT_00d54748` (single deref) | `alexa` | presence test: if the row resource has an `alexa` rect, use alexa fallback names (0x269/0x26b/0x26d + setFont 30/26); else FA names |
| `DAT_00d5474c` | `text1` | label box (11,-16,382,50); also the type-1 caption box (+0x3c4) |
| `DAT_00d54750` | `labelLong` | label box (11,-16,489,50) when type==4 and flag `+0x2e45` |
| `DAT_00d54b24` | `button` | button (429,-17,348,64) |
| `DAT_00d54b28` | `image1` | glyph image (478,0,42,40) |
| `DAT_00d54b2c` | `image2` | glyph image (692,0,42,40) |
| `DAT_00d54e30` | `selectionbox` | selection box (412,-9,393,60) |
| `DAT_00d54e2c` | `` (empty) | SelectionBox::create name arg |

(The `DAT_00d54744` slot is the stack-canary, not a literal.)

Translation part of the transform is established: each widget rect =
resource rect offset by the row position (`setWindowRect` after `offset` in
renderItems; row positions from `getRowPosition`). The scale part (mask
transforms inside `SVGTextBox::renderBackground` and FA animation target)
remains partly open. The type writer and text-height rescale are now recovered;
see UI-TEXT-GEOMETRY-2026-11-02.

## Text-rect scale mechanism (SVGTextBox::renderBackground, +0x1b4==1 branch)

Row textboxes (created with line count 1) take the first branch: background
slices via `MaskPositioningTransform` (alignments 1/2), the middle slice via
`HorizontalMaskScaleTransform`, then a rect looked up by widget-standard-name
has its **height** rescaled in place (corrected2026-11-02):

```
bottom' = top + trunc(float(surfaceHeight) / float(maskHeight) * float(rectHeight))
```

The previous width/double/round-half-away interpretation was incorrect.
The code uses float32 VFP division/multiplication and signed conversion toward
zero. HorizontalMaskScaleTransform itself separately uses a double **width**
ratio; that is not the manual text-height correction. The rect keeps its top,
left and right. The looked-up name is textPos (ID42 at10c621c..622e).
Translation still comes from mask alignment and row offset. Resolved literals:
`DAT_010c607c` → `addition` (stored text-rect source on one path); the
`DAT_010c6xx` canary-looking slots are not strings — do not read them as
literals.

## Text-rect rescale, asm-verified (10c6244–10c6296, +0x1b4==1 branch)

Applied to the id-42 (`textPos`) lookup result `sl` when non-null:

```
A    = [r7,#0x234] - [r7,#0x22c]
B    = [r7,#0x26c] - [r7,#0x264]
span = [sl,#20] - [sl,#12]
[sl,#20] = [sl,#12] + int(float(A) / float(B) * float(span))
```

Float32 operations, signed conversion toward zero. The old round-half-away
claim is withdrawn. B has no firmware zero guard; do not infer valid geometry
from an invalid denominator. RectHandler::handleSVGElement ef6128 and the
EASIRect constructors establish corners {left,top,right,bottom}; record+12 is
top and record+20 is bottom. Therefore A/B/span are heights here. Fresh
instruction excerpts and49,728 ARM VFP comparisons are in UI-TEXT-GEOMETRY.

## Record layout confirmed (10c6b9e–10c6bd6)

`r1 = fp` (looked-up record) → `UString::operator=` into `this+0x1ec`,
then `ldmia fp+8 / stmia +0x1f4` (16-byte coord copy) plus `+0x204/8/c`
tail fields. So the container record is `{UString name; coords...}` and both
`+0x254` (preamble, literal `"addition"`) and `+0x1ec` (else branch, standard
name) receive looked-up rect NAMES with coords following. Resolved2026-11-02:
single-line path10c6396 creates name42=textPos, branches10c63a2→10c6b7a,
then copies the looked-up record to+0x1ec. renderForeground10c7a18..7a26 copies
that RectangleDescription and uses its geometry. +0x254 is the separate
addition rectangle. This is no longer an open question.

Do not implement value/tab/slider-graphics/title/focused labels from the fits.
Next: remaining mask slice positioning, row allocation and text origin/alignment,
then replace fitted UI geometry only when that whole chain is established.

## Mirror-branch verification (independent re-derivation 2026-11-02)

The switch selects different string tables per OSD-mirror flag; the first
`puVar3 = DAT...` per case is the mirrored arm, the `if (iVar2 == 0)` arm the
non-mirrored one. Both DATs resolved through the file (literal pool → .data
table → rodata string):

| id | mirrored arm | non-mirrored (iVar2==0) arm |
|---|---|---|
| 17 (0x11) | mRowR (00eee9e8) | mRowL (00eee9fc) |
| 22 (0x16) | mRowL (00eee9fc) | mRowR (00eee9e8) |
| 23 (0x17) | mBotR (00eeea00) | mBotL (00eeea08) |

Font markers have single (mirror-independent) arms: 65 fontRegular,
66 fontBold, 67 fontLight, 68 fontItalic, 69 fontBoldItalic,
70 fontLightItalic — confirming the UI-TEXT-GEOMETRY correction
(65 is fontRegular, not fontLight).

The original "four-entry variant table" reading is withdrawn: the tables are
per-branch string selections, not per-state variants, and the first-pass
case→DAT association mixed mirrored arms. Entry[0] of the *selected* table is
the name. Non-mirrored (stock) names match UI-LIST-METRICS' pointer-chain file.
The rescale formula and row-literal findings are unaffected.

## Slider adjust semantics (SVGSlideBar, asm-verified 2026-11-02)

Field map: +0xb0 min, +0xb4 max, +0xb8 value, +0xbc step.
`onKeyRight` (0x10c487d): `value += step; if (value > max) value = max`,
then vtable+0x50 redraw. `onKeyLeft` (0x10c482d): `value -= step;
if (value < min) value = min`, then vtable+0x50. One-sided clamps, no
wrap. `setSliderValue` (0x10c493d) stores +0xb8 directly with NO clamp;
`setSliderRangeValues(min, max)` (0x10c4975) stores the bounds;
`setSliderStep` (0x10c49b1) stores +0xbc.

`SettingItemsListBoxRow::setSlideValue` (0xd52efd): unless progress mode,
picks the slidebar (+0x13c0 for type 9, +0x1498 otherwise),
`setSliderValue` if created, stores +0x2df8, and formats the value textbox
(+0x1ca8) with integer `operator<<` — plain `%d`. Our lab's `snprintf %d`
matches firmware formatting exactly; only the range remains invented.
SUPERSEDED (2026-11-02, "Range/step wiring" below): the "no static callers"
note was wrong — an address-width bug in the scan pattern hid them.
setSliderRangeValues is called from setBarRangeValues and renderItems;
setSliderStep has no caller (ctor default 1). Decompiles in
/tmp/opencode/decomp-slide.txt and decomp-sval.txt (ephemeral — re-run
DumpFirmware.java with the addresses above to reproduce).

## Sub-dialog composition (PictureElementSettingDialog, decompile-verified)

`showPictureElementSettingDialog` (0xe0a46d) hides the tab dialogs
(vtable+0x1c/+0x34 with 0), constructs a stack
`PictureElementSettingDialog` for the setting type, runs it with
`doModal`, then restores visibility and destructs. The slider lives here,
not on the main list.

`initialiseFields` (0xe120e0) switches on the setting type (`+0x574`).
Slider cases build type-9 `SettingItem`s with the current value and two
provider-filled ints; each type calls a DIFFERENT provider virtual with the
same out-param shape — +0x1e8 (type 1), +0x1ec (type 2), +0x1f4 (type 3),
+0x1f8, +0x208, +0x204, … — on the provider at +0x57c. So min/max-style
bounds are per-setting RUNTIME provider (picture-pipeline) data, not
firmware constants: no static range table exists to decode. The lab 0..100
placeholder stands until a live provider query path exists (needs native
provider ownership, out of scope). SUPERSEDED (2026-11-02): the ctor param
roles, the +0xb0/+0xb4/+0xbc wiring and the step source are decoded in
"Range/step wiring" below. Decompiles in /tmp/opencode/decomp-showdlg.txt and decomp-subdlg.txt
(ephemeral — re-run DumpFirmware.java @0x00e0a46d @0x00e120e1).

`startSelectAnimation` (0x10bbedd) snapshots the current selected-row rect
(`+0x25c..+0x268`) to `+0x24c..+0x258` (old) and stores the new row rect at
`+0x25c..`, then starts the row-named animation. `onAnimationProgress(p, _)`
(0x10b6c5) linearly interpolates per component and stores the moving rect at
`+0x9b..+0x9e`, then calls vtable slot +100 (redraw with the moving rect):

```
new = B*p + A*(1-p),  A=[0x93..0x96], B=[0x97..0x9a]
```

No easing inside the callback (list path is linear per UI-ANIMATIONS; easing
lives in the controller progress). The +100 consumer question is now closed;
remaining open: what `SVGListBoxBase::renderBackground` (0x10bd305) draws from
+0x26c, and any pixel acceptance. No implementation.

## FA animation wiring closed (vtable + asm, 2026-11-02)

`onAnimationProgress` (0x10bb6c4) interpolates with `vcvt.s32.f32`
(truncation, matching the VFP finding), builds the EASIRect and stores it at
byte +0x26c, then calls vtable slot +100 (index 25) with `(this, &rect)` —
r1 points at the fresh rect (the earlier "no args" reading was wrong).
The vtable pointer is read from the ctor literal at 10bd2fc → 0x153b3ec;
slot 25 = 0x10be16d =
`SVGListBoxBase::repositionSelectionIndicator(EASIRect const&)`.
That function ignores its argument and calls
`SVGListBoxBase::renderBackground()` (0x10bd304) on `this`; the moving rect
is consumed from +0x26c.

`startSelectAnimation` (0x10bbedc) is gated on `getAnimationEnabled` and
skips the snapshot when the new selected-row rect equals +0x25c. Otherwise it
copies +0x25c..+0x268 to +0x24c..+0x258, stores the new row rect at +0x25c,
then searches animation container +0x280 for the animation whose name matches
widget-name id 55 and starts it on the UIAnimationTarget subobject at +0x98
(ctor builds the target at +0x98; the `Thn152` thunk adjusts by -152).
Id 55 resolves (same ELF translation, single-arm case 0x37) to
`selectionIndicator`, so the animation driving the interpolation IS the
selectionIndicator-named animation. Duration = atof(animation delay string)
× float at 0x10bc04c. Decompiles: /tmp/opencode/decomp-repos.txt is
ephemeral; re-run DumpFirmware.java @0x010be16d @0x010bbedd to reproduce.

## What list renderBackground draws from +0x26c (0x10bd305, asm-verified)

Tail of the function (aurora.dis past file-line 4091800; decompile saved at
/tmp/opencode/decomp-listbg.txt, ephemeral — re-run DumpFirmware.java
@0x010bd305): after the whole-list mask/row passes, it resolves one rectangle
record by the name from widget-name id 55 (`selectionIndicator`, the 22nd and
last lookup in this function) out of the rectangle set built from the list
resource + animation container (+0x280), caches the record pointer at
+0x27c, brings the list surface to front and calls:

```
SVGSurface::drawRectangle(surface, record+0x1c /*mode*/,
    record+0x18, record+0x20 /*colors*/,
    +0x26c - listX0, +0x270 - listY0,   /* moving rect, list-relative */
    +0x274 - +0x26c, +0x278 - +0x270)   /* moving size */
```

Asm (10bdd6c–10bddac): `bringToFront`; loads +0x270/+0x278/+0x26c/+0x274;
`subs` differences on stack; `str fp,[r4,#0x27c]`; `bl drawRectangle`.
So the FA indicator is a firmware-filled rectangle using the moving
interpolated rect — not a stretched slice. The row FA slice stretch
(HorizontalMaskScaleTransform in the row's own renderBackground) and this
list-level indicator fill are distinct mechanisms.

Full widget-name id sequence in this function (asm, in order):
14, 2, 16, 4, 15, 3, 17, 5, 44, 22, 10, 44, 18, 6, 43, 23, 11, 25,
13, 24, 12, 55. Stock names (same translation): mTopL, gTopL, mTopR, gTopR,
mTopM, gTopM, mRowL, gRowL, scrollPos, mRowR, gRowR, scrollPos, mRowM, gRowM,
rowPos, mBotL, gBotL, mBotR, gBotR, mBotM, gBotM, selectionIndicator.
New names: gTopL/gTopM/gTopR/gBotL/gBotM/gBotR (g edge sets), rowPos,
scrollPos. Swapped mirror pairs: 2/4, 11/13, 14/16, 23/25 (same pattern as
5/10 and 17/22); the rest single-arm.

Bonus from the same function: list-level mode selector at +0x244.
When +0x248 == 0 it resolves six triples (font marker + two slice names) by
first-present priority — the same scrambled check order as the textbox
marker scan — and stores 0..5:
0: fontRegular + gBotL + mBotR; 1: fontBold + gRowL + gTopL;
2: fontLight + gRowR + gTopR; 3: fontItalic + gRowM + gTopM;
4: fontBoldItalic + gRowMRed + title_gRowL;
5: fontLightItalic + mRowMGreen + title_gRowR; else 0.
(Triple tables live at file VAs 0x10bd5b8..0x10bd5cc, each entry a string
pointer; resolved through the same segment-correct translation.)
New slice names: gRowMRed, mRowMGreen, title_gRowL, title_gRowR.
SUPERSEDED (2026-11-02): the reader question is answered below ("Readers of
list +0x244/+0x27c"); per-widget rect consumption is answered later in this
file. Pixel acceptance remains open. No implementation.

## Readers of list +0x244/+0x27c: none found by direct scan (2026-11-02)

CLAIM SCOPE (corrected after adversarial review 2026-11-02): this is a
negative result for *immediate-offset* accesses, not a proof of universality.
The scan covers `ldr/str [rN, #580]` and `[rN, #636]` in aurora.dis; it does
NOT exclude adjusted-base or computed-offset reads, overlapping loads,
aggregate struct copies, or a pointer taken to the field. Do not treat
"no readers" as absolute.

Direct accesses found:

- +0x244 writes: ctor 10bd1fe (zeroing) and renderBackground 10bd57c (mode
  store). The only other object-base read at this offset is 10c5ca4 in the
  SVGTextBox ctor — a different class layout, not the list field.
- +0x27c writes: ctor 10bd220 (zeroing) and renderBackground 10bdd7e
  (record-pointer cache). Other object-base reads belong to other classes
  (e.g. SVGTextBox::animRectDecode, LSCSession comms, and
  PrivacySettingsDialog::cleanupAndEndDialog at d4ba6e).

No SVGListBoxBase method was found reading either field directly; the
indicator paint uses the record pointer in-register at the drawRectangle
call. Parity consequence: do not invent consumers for these fields on the
strength of this scan alone. The +0x248 flag guarding the selector is a
separate field and is unaffected by this finding.

## Row-background name sequence (SVGListBoxRow::renderBackground 0x10bec08)

### Two-block mechanism (2026-11-02)

The function has TWO mask/scale blocks, not one (the second starts at asm
10bf0f2):

- Block 1 (surface this+0x98, resized to the row window rect from vtable
  +0x28): mask mRowL → render gRowL; mask mRowR → render gRowR; then
  HorizontalMaskScaleTransform with surface rect
  `((d0-d8)+f8, f4, f0-(c0-c8), ec)` built from mask-rect fields plus the
  row-rect origin (f8/f4/f0/ec), mask mRowM → render gRowM.
- Block 2 (new surface this+0xc4, resized to the vtable+0x28 rect at
  r7+40): mask mRowM (id 18) → render **rowItems** (id 48).
- Row resource mapping verified in SettingItemsListBox ctor asm: list
  backgrounds setSVGName(1→475, 3→476, 0→477, 2→478); row backgrounds
  setRowSVGName(1→479 FA, 0→481 UFA, 3→480 FI, 2→482 UFI); widget ids
  488-491/496-499/504-507 in row ctor immediates (matches the Ghidra table).

OPEN PUZZLE, refined 2026-11-02: the renderer IS BinSVGRenderer and vtable+0xc
is `renderGroup` (index 3, verified via the ctor-literal vtable at 0x1501b38;
it takes facade/resource/name/container/transform). renderGroup CLEARS the
container on every call and renders only the FIRST group matching the name.
So at each `getRectangle(container, "glyph")` (id 54) the container holds
exactly the last-rendered group's rects: {mRowL,""}, then {mRowR,""}, then
{mRowM,""} for record 479 — none named "glyph". Record 479 itself has no
"glyph" rect (verified with build/svg-rects). Coherent reading (hypothesis,
not yet proven): for this resource the miss path destroys the +0xe0 array
and the children created with the empty local_e8 stay invisible, so the
visible bar is SURFACE content only — the mask renders composited into the
row surface (this+0x98, sized to the row window rect). The "glyph" children
would serve resources that do carry a "glyph" rect. Still to verify: whether
MaskPositioningTransform::getMaskRectangle records anything into the
container (and under what name), the vertical anchoring of alignments 1/2,
and the surface→screen composition that decides 66 vs 70 px.

### Name call order (asm-verified)

Asm-verified call order of `getWidgetStandardName` in the function
(aurora.dis file lines 4093381–4093798), with stock (non-mirrored, `iVar2==0`)
names resolved through the ELF with segment-correct VA→file translation and
cross-checked against the four known m-names:

| # | asm site | id | stock name |
|---|---|---|---|
| 1 | 10bec98 | 54 (0x36) | glyph |
| 2 | 10becd8 | 17 (0x11) | mRowL |
| 3 | 10bed0a | 5 | gRowL |
| 4 | 10bed9c | 22 (0x16) | mRowR |
| 5 | 10bedcc | 10 (0xa) | gRowR |
| 6 | 10bee66 | 18 (0x12) | mRowM |
| 7 | 10bee96 | 6 | gRowM |
| 8 | 10bf0f6 | 18 (0x12) | mRowM |
| 9 | 10bf140 | 48 (0x30) — CORRECTION 2026-11-02: the earlier "r2 carried" note was wrong; asm has `movs r2,#48` at 10bf134 | rowItems |

New names recovered: `gRowL`/`gRowM`/`gRowR` (a `g` set alongside the known
`m` set) and `glyph` (54). Branch structure: 5/10 are a swapped mirror pair
(stock 5=gRowL, 10=gRowR; mirrored arms swap), the same pattern as 17/22;
6, 18 and 54 are single-arm. Decompile source:
artifacts/analysis/ui-mask-layout-2026-11-02/list-row-layout.txt;
getWidgetStandardName switch: /tmp/opencode/decomp-wstd.txt (ephemeral,
re-runnable with DumpFirmware.java @0xeee798).

Structural facts read directly from that decompile (no implementation):
the first name fills the UString slot (r7+12, 8 bytes below the r7+20 slot
of call #2, matching the 8-byte UString pitch) that the row's own
`getRectangle(container, ...)` lookup consumes; when that lookup misses, the
row destroys its child windows (+e0..+f0). The gRowL/gRowR names go to the
renderer calls inside the two mask passes (alignments 1 and 2); the mRowM
lookups surround the `HorizontalMaskScaleTransform` surface setup. SUPERSEDED
(2026-11-02): the +100 consumer linkage is closed later in this file ("FA
animation wiring closed"); still open are the mask-pass rects for a live row
and the scale surface-rect field mapping.

## Range/step wiring (drawRow → row → slidebar, asm-verified 2026-11-02)

CORRECTION: the previous "no static callers / virtual-only" claim was wrong —
an address-width bug in the scan pattern (`{7,8}` missed 6-digit `0xdxxxxx`
code) hid the direct callers. With the fixed pattern: `setSliderRangeValues`
is called from `setBarRangeValues` (d53376) and `renderItems` (d55a04);
`setSliderValue` from `setSlideValue` (2 sites), `renderItems` (d556f4) and
two other dialogs. `setSliderStep` has no callers anywhere — step stays at
the ctor default.

The full path: `drawRow` reads item +0x24/+0x28 → `setBarRangeValues(row,
min, max)` → row +0x2df0/+0x2df4 cache (+ slidebar if created); `renderItems`
pushes the row cache into the slidebar via `setSliderRangeValues`;
`setSlideValue` takes the value from item +0x20. So SettingItem
+0x20/+0x24/+0x28 = current/min/max. Slidebar ctor defaults: min 0, max 0,
value 0, step **1** (+0xbc) — our lab step=1 matches the firmware default;
only the range stays invented (provider-fed at runtime).

SettingItem ctor param map (all three overloads decoded): param_7→+0x20
(current), param_8→+0x24, param_9→+0x28, param_10→+0x30, param_11→+0x34,
bools→+0x2c/+0x2d; the first two overloads zero +0x30/+0x34. (For type
0xb/0x19 progress rows +0x30/+0x34 are bar image ids; for sub-dialog slider
items +0x30 is a flag and +0x34 is 0.) Row widget inventory re-verified at
asm level in the row ctor (0xd53d0c): label +0x104, caption +0x3c4, button
+0x684, selbox +0x1014, slidebars +0x13c0/+0x1498, textentry +0x18d8, value
textbox +0x1ca8, label store +0x2dd8.
