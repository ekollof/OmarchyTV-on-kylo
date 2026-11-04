# SVGScrollBar geometry/state — 2026-11-04

`common/svg_scrollbar.c` now implements the shipped `SVGScrollBar` state and
bar geometry: `setScrollPosition` (`0x10c4138`, 0x70 bytes, CRC
`0x71cda0c0`) and `calculateBarSizeAndPosition` (`0x10c3af0`, 0xbc bytes, CRC
`0x37806af8`), both Thumb. The differential has **3120** native/C cases;
`verify.cjs` now has **40** ok lines. Host, ASan/UBSan, ARM/qemu, full
`make check` (265 ok) and a focused real-TV CPU run pass.

## Where this fits

The stock home portal capture has no card-row scrollbar; the scrollbar is the
full-screen apps portal's element `scrollbar` at `1750,180,8x720`
(APPS-GRID). The recovered geometry is independent of the 8 px width and uses
the 720 px track height. The UI module does not yet implement the apps-grid
screen or its skin rendering; this turn recovers the reusable scrollbar state
and pixel geometry that the skin needs.

## Callee audit

- `calculateBarSizeAndPosition` calls `UIWindow::getWindowRect` through vtable
  slot `0x28` (`0x1152649`); the returned `EASIRect` is
  `{left,top,right,bottom}` and the track height is `bottom-top`. The shipped
  body was verified against a fake vtable whose slot 10 writes that rect.
- The two proportional calculations call the signed divider at `0x1285478`
  (`__aeabi_idiv`-class). The body's guards keep both divisors nonzero; the
  differential uses the same signed C division and asserts the divisor.
- `setScrollPosition` calls `UIWindow::isCreated` (`0x1152a40`),
  `SVGScrollBar::renderBackground` (`0x10c3658`) and
  `SVGScrollBar::renderBar` (`0x10c3bac`). The latter two remain stubs/ops.
- Not implemented: `calculateMinBarSize` (`0x10c397c`), which resolves the
  skin's `getWidgetStandardName` IDs 0x20/0x22/0x21/0x23 through the SVG
  supplier/renderer and `RectangleContainer`; the background/bar drawing bodies
  and their SVG resources are also still open.

## Recovered behavior

Object fields: `+0xb0` bar pixel position, `+0xb4` bar pixel size, `+0xb8`
minimum bar size, `+0xbc` total content units, `+0xc0` visible/page units,
`+0xc4` first visible unit, `+0xc8` hide-background byte. The branch order is
significant:

1. `total == 0`: position 0, size = track height, then the final rule below
   zeroes it for a nonzero track.
2. `total <= visible`: position 0, size 0 (no bar).
3. `total <= 0` (negative values not caught above): leave the previous bar
   fields untouched; only the final rule runs.
4. Otherwise: `size = height*visible/total`, clamped up to `min_bar` and down
   to `height`; `position = (height-size)*first/(total-visible)` when
   `total > visible+first`, else `height-size`.
5. Final rule: if `size == height`, store 0 (a full-height bar is hidden).

`setScrollPosition(total,visible,first)` rejects when the window is not created
or `first > total`, storing nothing and returning 0. On acceptance it stores
the three fields, calls `renderBackground` only when `+0xc8` is clear, then
`renderBar`, and returns 1.

## Verification

`tests/svg_scrollbar.c` uses the apps-grid track height 720 and asserts the
principal cases: zero/equal totals, first at 0/5/10/15, the min-size clamp, the
height clamp, the full-height hide, the negative-total preserve, a zero-height
track, and the created/busy/background-hidden gates. `tests/svg_scrollbar_native.h`
copies both bodies at their original addresses, redirects only the stack
guards, installs the signed-divider and `isCreated`/render stubs, and uses a
fake object vtable. It runs 2880 geometry and 240 state cases (3120 total),
comparing every field and requiring no writes outside `+0xb0..+0xc8`.

| executable | bytes | MD5 |
|---|---:|---|
| tests/svg_scrollbar | 27208 | e8714135611df38b9fa7fdcd08e37a57 |
| tests/tab_policy_firmware | — | d8f340696db82ce7fb6df6efdad203a0 |

The focused real-TV CPU test ran the self-contained `svg_scrollbar` ARM unit:
exit 0, staging hash-verified and removed, no helper left, no-reset flag
retained. No device/screen/input/settings/network/service/boot change.

## Limits and next gates

- The skin minimum-bar size, background/bar rendering and the apps-grid screen
  are not integrated; the home portal has no scrollbar (verified from the stock
  capture), so there is no visible scrollbar parity claim yet.
- Display/scanout binding for an independent startup candidate is still open;
  the visible probe demonstrated the existing vendor-initialized fb1 path, not
  cold startup.
- The live-DB visible run and real downloaded-image decoding remain explicit
  acceptance gates from the previous report.
