# Settings-row render coverage correction — 2026-11-03

This turn fixes a **silent coverage hole** in the renderItems differential and
closes type 0x9's created slider path. No new case body was added.

## The hole

The sweep used `for(m=0;m<128;++m)` for an 11-key presence mask, so bits 7..10
— `image2`, `banner`, `slidebar`, `text2` — were **always zero**. Type 0x9
therefore never looked up its `slidebar`/`text2` rectangles and its whole case
body was skipped; only the shared 0x104 tail ran. The reported 95624
"comparisons" did not cover type 0x9's body at all.

The sweep is now split into two passes:

- **Pass A** — full 11-key presence coverage (`m` 0..2047) with the
  point/word/label/flag dimensions fixed.
- **Pass B** — point/word/label/flag coverage with every key present
  (`m=2047`).

Both call a single `render_case()` helper so the fixture setup and comparison
logic cannot drift apart. Total **66312** comparisons (down from the inflated
95624, but now actually exercising every key).

## Type 0x9 created slider path

`0xd559ce`/`0xd559d0`: EASIRect copy, point query, offset,
`SVGSlideBar::setWindowRect`, then `setSliderRangeValues(row[0x2df0],
row[0x2df4])` and `setSliderValue(row[0x2df8])`. The C body previously
treated this branch as a no-op; it now emits set_rect, set_slider_range and
set_slider_value. A new `set_slider_range` callback was added.

## Second NUL-sled caught

The native side had no trampoline for `SVGSlideBar::setWindowRect`
(0x10c4790), so the guest executed zero halfwords and slid into the
`setSliderRangeValues` stub at 0x10c4974 — producing a plausible-looking
range event with a stack pointer as its operand. Installing the missing stub
made the real divergence visible. This is the third instance of that failure
mode in the harness (previously `setText` and the page arithmetic), so any new
case must enumerate every callee before sweeping.

## Verification

66312 native/C comparisons pass; host, focused ASan/UBSan, ARM/qemu, full
`verify.cjs` (33 ok lines) and full `make check` pass. Isolated
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 137816 | 9ee7b135e0425d619379400d798ec87c |
| tests/tab_policy_firmware | 1114416 | 9a734643a7b592128c464d3f77529ab4 |

Focused real-TV CPU run: exit 0, staging hash-verified and removed, no helper
left, no-reset flag retained. No device/screen/input/settings/network/boot
change.

## Still open

Case bodies 0xa..0xc, 0x11..0x15 and 0x17..0x19 remain unimplemented; type
0xa's partial mapping is in ../settings-row-render-a-2026-11-03/NOTES.md.
`getImageId`'s real 62-way mapping and the image/selection internals are still
controlled stubs, and there is no row construction, child visibility
internals, live resources, scrollbar/skin, display/scanout binding,
independent startup or visible parity.
