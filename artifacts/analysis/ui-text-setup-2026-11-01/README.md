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

## Not yet decoded

- Which literal each `getRectangle` in `renderBackground` resolves (the first
  stored rect at `+0x254..0x274` comes from a literal reached through a data
  pointer, e.g. "addition", and may miss for textbox resources). The
  transformed `textPos` rectangle, mask-transform scale factors and the final
  screen origin/baseline are therefore NOT established.
- The per-widget `+0x1b4` line count, `+0x1c4` type and `+0x220/+0x22c`
  alignment for the title, tab and row widgets. The fitted font7/font16
  origins remain capture fits, not firmware geometry.
- Focused-row FA type/geometry beyond the resource IDs (the animation target
  and transformed rect from `startSelectAnimation`).

Do not implement title/tab/focused labels from the fits. Next: resolve the
`renderBackground` rectangle literals per widget resource and trace one row's
`+0x1b4/+0x1c4/+0x220` setup through `initialisePictureSettingsShortcuts`.
