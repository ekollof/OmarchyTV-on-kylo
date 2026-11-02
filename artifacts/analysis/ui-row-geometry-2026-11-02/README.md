# FA/UFA row widget geometry from shipped resources (2026-11-02)

`openaurora/tools/re/svg-rects.c` (`make build/svg-rects`) dumps named
rectangles of BinSVG records from the shipped `telefunken18_svg` pack. All
values below are resource data, not capture fits.

## Record 479 (SettingListBoxFA): the focus bar is a 3-slice, all #FE9419

| slice | mask rect | fill rect |
|---|---|---|
| mRowL (left cap) | (18,61,51,127) | same, #FE9419 |
| mRowM (middle, stretched) | (54,61,174,127) | same, #FE9419 |
| mRowR (right cap) | (178,61,211,127) | same, #FE9419 |

66 px tall, 193 px wide; left/right caps 33 px stay fixed while the 120 px
middle stretches (HorizontalMaskScaleTransform). Because every painted fill
is solid #FE9419, replacing the *correctly composed background* with a flat
fill is equivalent for this resource. That does NOT by itself validate the
fitted 750x66 framebuffer placement, clipping or the captured overlay as a
whole — those remain reconstruction choices. Non-solid fills would need the
slice structure.

## Label/caption/button/selbox FA vs UFA textPos (firmware, not fitted)

| widget | FA rec | FA textPos (h) | UFA rec | UFA textPos (h) | marker |
|---|---|---|---|---|---|
| label | 488 | (9,2,49,44) = 42 | 489 | (9,3,49,43) = 40 | fontBold only in FA |
| caption | 492 | (9,2,49,48) = 46 | 493 | (7,3,47,49) = 46 | fontBold only in FA |
| button | 496 | (279,102,366,170) = 68 | 497 | (279,102,366,168) = 66 | fontBold only in FA |
| selbox | 504 | (286,94,367,162) = 68 | 505 | (286,96,367,162) = 66 | fontBold only in FA |

Alignment rects are identical within each pair. Differences are per-widget:
label shifts top/bottom by 1; caption shifts x by 2 (same height 46);
button/selbox lose 2 px of height. This confirms the earlier row-label
finding (FA 488 fontBold/height42 vs UFA 489 no-marker/height40) from the
resources themselves.

Reproduce:

```
cd openaurora && make build/svg-rects
./build/svg-rects <index.txt> <binary.bin> 479 488 489 492 493 496 497 504 505
```

Remaining for live focused text: the runtime mask/surface/row-offset
composition around these rects (decoded in principle — mask positioning,
height rescale, getRowPosition — but not yet wired to a live row instance),
and FA slider/button graphics. Do not render from these until that chain
is integrated.
