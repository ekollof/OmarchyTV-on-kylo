# Row rect: firmware data vs the reconstruction, and what is still open

Read after owner observation (2026-11-02) that the focused row looked wrong:
the yellow bar read as "not a rect" with red widgets on it, and the numbers
were misaligned.

## What the shipped resources say (verified with build/svg-rects)

Resource 179 (`DialogSettings_tab`, the picture-settings dialog) contains:

| name | rect (1920x1080 space) |
|---|---|
| background | (0,0,1920,1080) |
| `main_rectangle` | (0,162,780,1080) |
| unnamed panel fill | (0,0,780,1080), `#BE1E2D` |
| `listBox` | **(20,170,770,996)** |

Records 475-478 (setting-list boundary slices) give the list metrics already
used elsewhere: `mTopM` height 16 (top inset), `mBotM` height 10 (bottom
inset), `mRowL` height 70 (row height / vertical step).

With `SVGListBoxBase::getRowPosition` (0x10bbaa8) — left=leftInset,
top=topInset+step*i, right=window width-rightInset, bottom=top+rowHeight — and
zero horizontal insets, row 0 is **(20, 186, 750, 70)**. Rows are created
with these rects (`createListItems` → row `setWindowRect`), so the row
surfaces are 750x70 and the vendor bar frame is (20,186+70i,750,70).

## Resolution: adopt the firmware frame (2026-11-02)

The bar is now drawn at (20,186+70i,750,66): row frame from the chain above,
66 px content height from record 479's slices and the decoded vertical
mapping (below). Verified on fb1: the fill spans exactly y256..321 for sel=1
(`tv-fb1-edited-row-1-y186.png`, sha256 `e3fd5209...`).

## Vertical mapping (decoded, not guessed)

`HorizontalMaskScaleTransform::transformRect` maps x through the width ratio
but translates y only (`y0'=y0+C`, height preserved) — no vertical scaling
exists. The 66 px slice content therefore sits top-aligned in the 70 px row;
the bottom 4 px strip is whatever the cleared-then-unpainted surface region
shows (resizeUISurface clears handles, not pixels — indeterminate statically,
consistent with panel showing through). Row height 70 is verified three
independent ways: (1) raw rowPos rects in records 475-478 are all
(244,178,319,248), exactly 70 tall; (2) the rowHeight computation
(`[rowPos+0x14]-[rowPos+0x0c]` at asm 10bda5a–10bda60) reads record fields
with no scaling; (3) container rects pass through the height-preserving
transform above. Our flat 66 px bar + unpainted strip matches all of this.

## What WAS fixed (no geometry claim)

The reconstructed `settings_focus_N` assets are not used when live text is
active. Measured content of `settings_focus_1.rgba` (750x66, RGBA):

- 46,718 px bar `#FE9419`;
- 432 px `#7E000A` plus antialiasing (baked row text);
- **816 px `#9B9B9B` (baked grey widget shapes)**.

Blitting that band and then drawing live text produced exactly the reported
artifacts: grey/red shapes on the bar and two overlapping sets of digits.
The bar is now a solid `#FE9419` fill at the existing placement and live text
carries the row content. Test `tests/ui_settings_text.c` asserts the fill and
asserts the baked asset is not blitted when text is on.
