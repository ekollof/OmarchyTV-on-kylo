# SVGScrollBar render bodies — 2026-11-04

> Superseded in part by WORK-REVIEW-2026-11-04: +6c is a four-state widget
> index, not orientation. Fixed two-entry caches, signed arithmetic and
> first-window calculation/second-window placement. The expanded36-case
> render corpus uses the real divider and a changing-window fixture.
> The8-case results below are historical and did not cover those defects.

`common/svg_scrollbar.c` now implements the shipped
`SVGScrollBar::renderBackground` (`0x10c3658`) and `renderBar` (`0x10c3bac`)
as ordered ops-driven sequences. The differential has **8** native/C cases;
`verify.cjs` now has **42** ok lines. Host, ASan/UBSan, ARM/qemu, full
`make check` (265 ok) and a focused real-TV CPU run pass.

## Recovered sequence

The two bodies share one pattern with the recovered standard names:

| role | background | bar |
|---|---|---|
| top mask / image / row | `mTopM` (0x0f) / `gTopM` (0x03) / `mTopM` | `bar_mTopM` (0x22) / `bar_gTopM` (0x20) / `bar_mTopM` |
| bottom mask / image / row | `mBotM` (0x18) / `gBotM` (0x0c) / `mBotM` | `bar_mBotM` (0x23) / `bar_gBotM` (0x21) / `bar_mBotM` |
| middle mask / image / row | `mRowM` (0x12) / `gRowM` (0x06) / `mRowM` | `bar_mRowM` (0x1e) / `bar_gRowM` (0x1b) / `bar_mRowM` |

Both bodies:

1. Read the cached supplier image at `+0x74 + orientation*4` (id at
   `+0x84 + orientation*4`); return when absent.
2. Resize the background surface (`this+0x98`) to the whole window rect, or
   the bar surface (`this+0xa4`) to
   `{left, top+bar_pos, right, top+bar_pos+bar_size}`.
3. Build the surface-local rect `{0,0,right-left,height}`.
4. Mask top: alignment 3, set the surface rect, mask name, container, draw
   the `g*` image, look up the mask rectangle, reset the mask.
5. Mask bottom: set the bottom mask name, alignment 4, draw, look up the
   bottom mask rectangle.
6. Row rect: `{0, top_cap_height, width, height - bottom_cap_height}`; set the
   vertical transform's surface rect/name/container and draw the `gRowM`
   image. `renderBackground` sets the `+0xc8` background-rendered flag;
   `renderBar` with `bar_size==0` clears both surfaces and clears the flag.
   `renderBar` calls `calculateBarSizeAndPosition` first, so the window rect is
   queried twice (once inside the calculation and once for the bar surface).
7. `renderBar` draws the middle row only when `min_bar < bar_size`.

Callee audit: `UIWindow::getWindowRect` (vtable slot 0x28/`0x1152649`),
`SVGContext::getSVGSupplier/getSVGRenderer`, `SVGWindow::getWidgetStandardName`,
`EASIRect::EASIRect` (`0x9a7fcc`), `MaskPositioningTransform` (ctor/reset/
setMaskContainer/setAlignment/setMaskName/setSurfaceUIRect/getMaskRectangle),
`VerticalMaskScaleTransform` (ctor/setSurfaceUIRect/setMaskName/setMaskContainer),
`RectangleContainer` (ctor/getRectangle/dtor), `SVGSurface::resizeUISurface/
clearUISurface`, the renderer vtable slot 0xc draw and the signed divider at
`0x1285478`.

The `+0xc8` byte was previously named `background_hidden`; the render bodies
show it is a **background-rendered** flag (`renderBackground` sets it;
`renderBar` with an empty bar clears it; `setScrollPosition` calls
`renderBackground` only when it is clear). The API/test name is corrected to
`background_rendered`; the behavior is unchanged.

## Verification

`tests/svg_scrollbar_render.c` exercises the background (1750,180,8x720 track,
top 20/bottom 16, row 0,20,8,704), the bar (20/15 -> 540 px, row 0,20,8,524),
the empty bar (clear both surfaces, flag 0), the min-clamped bar (600 px, no
middle row) and the no-image early return. It asserts the full ordered trace
of window rects, surface resizes/clears, rect constructions, alignments, mask
names/containers, draw calls and cap heights.

`tests/svg_scrollbar_render_native.h` copies `0x10c3658..0x10c3f08` verbatim
(renderBackground + calculateBarSizeAndPosition + renderBar), redirects the
three stack-guard pools, supplies a fake window-rect vtable, fake
supplier/renderer tables, all transform/container/surface stubs, and the
signed divider. The 8 cases vary background/bar, cache/supplier, empty bar,
min clamp and first position; they compare the full trace, the cache and the
`+0xc8` flag.

| executable | bytes | MD5 |
|---|---:|---|
| tests/svg_scrollbar_render | 55752 | 5cc5a6a259bf1f69bb80c893da620742 |
| tests/tab_policy_firmware | — | see artifacts/analysis/svg-scrollbar-render-2026-11-04/native-final.txt |

The focused real-TV CPU test ran the self-contained render ARM unit: exit 0,
staging hash-verified and removed, no helper left, no-reset flag retained. No
device/screen/input/settings/network/service/boot change.

## Limits

- The ops are abstract: the vendor mask transforms, `SVGSurface` and renderer
  draw calls are stubbed. This verifies the shipped sequence, standard names,
  surface/mask setup and row geometry, not pixels.
- No apps-grid screen or scrollbar rasterizer is wired yet; the 1750,180,8x720
  scrollbar is not visible in `ui_vendor`. The stock home portal has no
  scrollbar and there is no stock apps-grid capture.
- Display/scanout binding, independent startup, the live-DB visible run and
  real downloaded-PNG decoding remain open. Against the objective, this is a
  verified render-path body slice, not bug-for-bug or visible parity.
