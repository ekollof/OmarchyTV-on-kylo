# SVGImage::imageCreate — 2026-11-04

`common/svg_image.c` now implements `SVGImage::imageCreate` (the 288-byte body
at `0x10c9430`), one level below the create/destroy orchestration already
covered. **72** native/C cases pass for the svg-image group (56 prior + **16**
new `imageCreate` grid cases): the shipped body is copied verbatim to its own
address and called with every vendor call stubbed, and the ordered traces match
the owned C exactly. Host unit, focused ASan/UBSan, ARM/qemu, full `verify.cjs`
(35 ok lines) and a focused real-TV CPU run all pass.

## Recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF.

`SVGImage::imageCreate(UIImageResource const& res, EASIRect const& rect,
UIWindow *parent)`:

1. `UIWindow::setParentWindow(this, parent)`
2. `SVGWindow::setWindowRect(this, rect)`
3. `isCreated(this)`.
4. **Created:** `SVGSurface::resizeUISurface(this+0x9c, rect)`, then
   `targetHandle(this)` (shared_ptr), then
   `drawImageResource(surface, res, 0, 0, row byte 0xa8, target,
   row word 0xac, float 0xb4, float 0xb8, row word 0xbc)`, then the
   shared_ptr destructor.
5. **Uncreated:** `surface = createSurface(this, rect, 1, 0)`,
   `setUISurface(this+0x9c, surface)`, then
   `targetHandle(this)` and either
   `drawImageResourceScaled(surface, res, 0, 0, target)` when row byte
   `0xb0` is non-zero, or the same full `drawImageResource` call as above;
   destructor, then `UIWindow::create(this)`.

New API: `oa_svg_image_image_create(img,resource,rect,parent,ops)` with the
vendor calls behind `struct oa_svg_image_surface_ops` (floats are raw IEEE
words at the module boundary).

## Verification method

`tests/svg_image_native.h` copies the shipped body at `0x10c9430` (its original
address), backs the stack-guard literal at `0x10c9550`, and installs
register-preserving stubs for setParentWindow, setWindowRect, isCreated,
resizeUISurface, targetHandle, the two draw calls, createSurface,
setUISurface, UIWindow::create and the shared_count destructor. Two details:

- `0x1149000` is held by an earlier test group, so the three `targetHandle`
  `bl` instructions are retargeted to a stub in the body's own page instead of
  mapping the vendor address.
- The shipped draw call passes a pointer to the `shared_ptr` object; the stub
  dereferences it and traces the token, so the native and owned sides compare
  values rather than stack addresses.

Grid: created {0,1} × `0xb0` {0,1} × field set {A,B} × parent {none,present}
= 16 cases. Each compares the full ordered trace including the final
`imageCreate` field words and the release call.

## Verification and limits

Host units cover created ordering, the `0xb0`-set scaled path, the `0xb0`-clear
full-draw path, surface create/set, release and `UIWindow::create`. Isolated
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/svg_image | 47624 | 393ae80d6fe9578884d193ec56b3cf47 |
| tests/tab_policy_firmware | 1232196 | 3c1081844a190c3e2ef1d63a0756aa41 |

The focused real-TV CPU test ran the self-contained `svg_image` ARM unit:
exit 0, staging hash-verified and removed, no helper left, no-reset flag
retained. No device/screen/input/settings/network/service/boot change.

**Boundary:** the vendor calls remain stubs in the differential —
`SVGSurface::resizeUISurface/drawImageResource/drawImageResourceScaled`,
`createSurface`, `setUISurface`, `targetHandle` and `UIWindow::create`. What
is now verified is the branching, argument order and field sourcing of
`imageCreate` itself.

Also found: there is a **second overload**
`SVGImage::create(int, EASIPoint const&, UIWindow*)` at `0x10c9554`
(228 bytes, CRC `0x09f01190`) that builds a rectangle from a point, applies
client-rect clamping, and calls `imageCreate`. It is not implemented yet.

Still open: the EASIPoint create overload, `convertOperationIdToIDM`, row
construction, child visibility internals/live resources, scrollbar/skin,
display/scanout binding and independent startup.
