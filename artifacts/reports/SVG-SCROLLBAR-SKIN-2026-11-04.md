# Scrollbar skin minimum — 2026-11-04

`common/svg_scrollbar.c` adds the shipped `SVGScrollBar::calculateMinBarSize`
(`0x10c397c`, 0x174 bytes, CRC `0xd9fdda32`) as
`oa_svg_scrollbar_min_bar`. The differential has **16** native/C cases; the
full `verify.cjs` now has **41** ok lines. Host, ASan/UBSan, ARM/qemu, full
`make check` (265 ok) and a focused real-TV CPU run pass.

## Recovered skin contract

The widget standard names for IDs `0x20..0x23` were decoded from the shipped
`SVGWindow::getWidgetStandardName` jump table and verified against the BinSVG
pack (`svg-animations.c --elements`):

| ID | standard name | element rect (x,y,w,h) |
|---|---|---|
| 0x20 | `bar_gTopM` | 91,37,12,20 |
| 0x21 | `bar_gBotM` | 91,93,12,16 |
| 0x22 | `bar_mTopM` | 91,37,12,20 |
| 0x23 | `bar_mBotM` | 91,93,12,16 |

The matching scrollbar resources are `scrollbarFA.svg`/`scrollbarUFA.svg`/
`scrollbarFI.svg`/`scrollbarUFI.svg` (IDs 61–64). The background caps carry
the dark red `#811924` inner rectangle; the moving caps are black with a
`#FFFFFF` focused (or `#969696` unfocused) inner rectangle. The apps-grid
scrollbar element is `1750,180,8x720` (APPS-GRID).

`calculateMinBarSize`:

1. Read the cached supplier image at object `+0x74 + orientation*4`; if zero,
   fetch it with the image id at `+0x84 + orientation*4` through the SVG
   supplier vtable slot `+8`, and cache the result.
2. If the image is still null, leave `+0xb8` unchanged and return.
3. Reset `min = 0`.
4. For pair (0x20,0x22): resolve the background name, call the renderer
   vtable slot `+0x14` with the image/name/container, resolve the moving-cap
   name, get its rectangle, and add its height.
5. Repeat with (0x21,0x23).
6. Store the sum at `+0xb8`.

The two outer moving-cap heights are 20 and 16, so the apps-grid scrollbar
minimum is **36 px**. That value is now used by the geometry test for a
20-item/15-visible grid: bar size 540, position 0 for the first page and 180
for the second page.

## Verification

`tests/svg_scrollbar_skin.c` covers the fresh supplier, cached image,
supplier-null (min unchanged), orientation 1, and missing-bottom-cap cases,
and asserts the full supplier/name/background/rect trace order.
`tests/svg_scrollbar_skin_native.h` copies the body verbatim, redirects its
stack guard, and installs a fake supplier/renderer vtable,
`getWidgetStandardName`, `RectangleContainer`, `getRectangle` and UString
destructor stubs. The 16 cases vary orientation, cache, supplier return and
bottom-cap presence, comparing the min, the cached image and the full trace.

| executable | bytes | MD5 |
|---|---:|---|
| tests/svg_scrollbar_skin | 26996 | 94e26f3959cb13ace4ef905a10d44af3 |
| tests/tab_policy_firmware | — | see artifacts/analysis/svg-scrollbar-skin-2026-11-04/native-final.txt |

The focused real-TV CPU test ran the self-contained skin ARM unit: exit 0,
staging hash-verified and removed, no helper left, no-reset flag retained. No
device/screen/input/settings/network/service/boot change.

## Limits

- The `renderBackground` (`0x10c3658`) and `renderBar` (`0x10c3bac`) bodies and
  their `MaskPositioningTransform`/`VerticalMaskScaleTransform`/SVGSurface
  calls are audited but not implemented. No pixels are drawn by this slice.
- The apps-grid screen itself is not implemented. The stock home portal has no
  card-row scrollbar; there is no stock full-screen apps-grid capture in the
  repository, only the firmware resource geometry. A reconstruction would not
  be visible parity.
- Display/scanout binding and independent startup remain open; the live-DB
  visible run and real downloaded-PNG decoding remain acceptance gates.
