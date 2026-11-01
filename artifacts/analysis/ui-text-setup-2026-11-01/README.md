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

## Not yet decoded

- Transformed `textPos` scale/origin beyond row-position translation;
  per-widget `+0x1b4` line counts / `+0x1c4` types for the title, tab and row
  widgets; FA animation target geometry. Title/tab/focused text stays
  unimplemented.

Do not implement title/tab/focused labels from the fits. Next: resolve the
`renderBackground` rectangle literals per widget resource and trace one row's
`+0x1b4/+0x1c4/+0x220` setup through `initialisePictureSettingsShortcuts`.
