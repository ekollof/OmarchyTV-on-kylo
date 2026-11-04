# SVGImage point-create overload and convertOperationIdToIDM — 2026-11-04

Two shipped bodies were recovered and verified: the remaining
`SVGImage::create(int, EASIPoint const&, UIWindow*)` overload and
`LauncherHelper::convertOperationIdToIDM`. The svg-image differential is now
**80** native/C cases (8 new point cases) and the launcher differential **536**
comparisons (67 new IDM lookups). Host units, focused ASan/UBSan, ARM/qemu,
full `verify.cjs` (35 ok lines) and a focused real-TV CPU run pass.

## SVGImage::create(int, EASIPoint const&, UIWindow*) — 0x10c9554

228 bytes, CRC `0x09f01190`. `common/svg_image.c` now implements it as
`oa_svg_image_create_point`:

1. `UIWindow::setId(this,id)`
2. `SVGContext::getImageSupplier` and the supplier's vtable slot 2 resource
   lookup; a missing resource returns immediately.
3. Default-construct the rectangle and copy the point into x/y.
4. **With a parent:** `getClientRect` (first call) adds the client origin to
   x/y; a second call computes `usable_w = client.w - client.x`; a third
   computes `usable_h = client.h - client.y`. The rectangle's width/height are
   the resource's size words at `+8`/`+12`, clamped to those usable extents.
5. **Without a parent:** width/height are the point plus the resource size
   words directly.
6. `imageCreate(this,&resource,&rect,parent)`.

The three `getClientRect` calls are reproduced because the shipped body
re-queries each coordinate; the differential compares their count and order.

## LauncherHelper::convertOperationIdToIDM — 0xe4ae0c

116 bytes, CRC `0xf2156f40`. A signed 16-bit table at `0x14efd84`, indexed by
operation id, with `op > 61` returning -1. The 62 values were extracted from
the ELF:

```
71 74 108 110 164 75 91 73 223 77 257 262 263 264 265 266
258 259 260 261 267 268 269 270 273 274 126 137 250 131 -1 -1
127 128 129 130 -1 -1 105 121 122 124 123 271 272 139 262 265
-1 125 127 287 250 251 252 253 243 245 134 135 136 295
```

Implemented as `oa_launcher_convert_operation_idm`; the differential maps the
vendor table address and calls the verbatim body over ops 0..63, 70, 100 and
`UINT32_MAX`.

## Verification

- svg-image: 8 new cases = supplier {0,1} × parent {none,present} × point
  {(10,20),(-5,7)}. The 32-byte resource, the client offset and the clamp are
  compared through the final `imageCreate` rectangle.
- launcher: 67 IDM lookups added to the 469 image-id comparisons.
- Host units add the point rectangle with/without a parent (including the
  three client queries and clamping to 6×6), the supplier gate, and spot
  checks of the IDM table plus its -1 defaults.

Isolated `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/svg_image | 49960 | 665dcdf16db3758f710b1f6503fe4569 |
| tests/tab_policy_firmware | 1238032 | 0f09604f887a618ed332fffefc161658 |

The focused real-TV CPU test ran the self-contained `svg_image` ARM unit:
exit 0, staging hash-verified and removed, no helper left, no-reset flag
retained. No device/screen/input/settings/network/service/boot change.

**Boundary:** the vendor calls (`getImageSupplier` vtable, `getClientRect`,
`EASIRect::EASIRect`, `imageCreate`) remain stubs in the differential. Still
open: row construction, child visibility internals, live resources,
scrollbar/skin, display/scanout binding, independent startup and visible
parity.
