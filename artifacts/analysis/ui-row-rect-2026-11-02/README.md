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
zero horizontal insets, row 0 would be **(20, 186, 750, 70)**.

## Why that value is NOT adopted yet

The focused bar comes from `SVGListBoxRow::renderBackground` (0x10bec08),
which resizes a row surface to the row rect and then composes three mask
slices through `MaskPositioningTransform` and
`HorizontalMaskScaleTransform`. Record 479's slices are authored in a
1280x720 canvas (mRowL (18,61,51,127), mRowM (54,61,174,127), mRowR
(178,61,211,127) = 66 px tall), while the list slices are authored so that
mRowL is 70 px tall. The mapping from those mask canvases into the row
surface (vertical scale/offset, and which rect the caps keep) is NOT decoded.
Assuming "row rect == bar rect" would be a guess, and the two candidate
heights (66 authored vs 70 row) differ.

The current UI still draws the bar at the pre-existing reconstruction
placement (20, 183+70*sel, 750, 66). That placement was never vendor-verified
either; it is retained only so this change does not silently introduce a new
guess. Open task: decode `resizeUISurface` + the two transforms to decide
between (20,186,750,70) and a 66-px bar inside that row.

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
