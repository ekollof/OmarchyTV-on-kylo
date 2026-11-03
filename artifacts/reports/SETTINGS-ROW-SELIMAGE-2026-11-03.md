# SettingItemsListBoxRow::setSelectionBoxImageId internals — 2026-11-03

`common/settings_row_render.c` now implements the row-level
`setSelectionBoxImageId(int id)` (shipped at `0xd54374`) instead of calling a
controlled stub. Case 0x3 invokes the implementation, and the differential
copies the **verbatim shipped body** into the row mapping, so the real
lookup/destroy/create sequence is exercised on both sides. **1770796**
native/C comparisons pass with exact call order/args, rect words, text bytes
and row state, plus strict host, focused ASan/UBSan, ARM/qemu, all prior
groups and a focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. The
function is 140 bytes (`0xd54374..0xd54400`, CRC `0xcf7f04a2`):

1. Store the argument at `row+0x2e54` (inline `str`).
2. `getRectangle(row+0xa8, "image")` (literal `0xd543fc` -> `0x178d5d0`).
   Return if absent.
3. `SVGImage::destroy(row+0x2cf0)` — **unconditional**, even when the image
   was never created.
4. Re-read `row[0x2e54]`; if non-zero,
   `SVGImage::create(row+0x2cf0, id, rect+8, row)`.

The only callees are the UString constructor/destructor, `getRectangle`,
`SVGImage::destroy` and `SVGImage::create`, all of which are controlled stubs
in the differential. The store is inline in the shipped code, so the C side
uses the silent `store_row_word` side effect; the row memcmp verifies it, and
`row_word(0x2e54)` now reads the live row so the post-destroy re-read is
faithful.

## Harness changes

- The `set_selection_image` ops hook is gone; `destroy_image` was added and
  the C body performs the lookup/destroy/create itself.
- The shipped `setSelectionBoxImageId` body is copied into the row page and
  called by the copied `renderItems` case-0x3 path. Its stack-guard literal
  (`0xd543f8`) and its `"image"` key slot (`0xd543fc`) are the only
  redirections.
- A 15th presence key, `"image"`, was added, so the sweep is 32768 masks.
  Total: 27 types × 32768 × 2 + 1296 + 4 + 4 + 12 + 8 = **1770796**.
- Two harness bugs were found while wiring it: the key literal is a
  **pointer-to-pointer**, so the redirect must target a `const char *` slot
  (pointing at the string array itself made the UString constructor read
  `"imag"` as a pointer and crash); and `verify.cjs`'s 30 s qemu timeout was
  too small for the doubled sweep (now 180 s; the run takes ~33 s).

## Verification and limits

Host units add the image lookup/destroy/create order, the zero-id path
(destroy but no create) and the missing-rectangle path (no destroy). Strict
full host/actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu units, every
prior group and the focused TV CPU test pass. Isolated build
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 230100 | b906eb04493376658c6d82588e4089cc |
| tests/tab_policy_firmware | 1173116 | 2db63cd8508c518d76ef9b08332df52d |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified the no-reset flag, staged
only the focused binary and checked size/hash in a separate connection before
execution. Test exit 0 passed, staging was removed and the `/proc/*/exe` scan
found no helper; `/conf/coredumpnoreset` remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

**Boundary:** what is now verified is the row-level orchestration of
`setSelectionBoxImageId`. `SVGImage::destroy` and `SVGImage::create`
themselves (the vendor XML/SVG resource machinery behind them) remain
controlled stubs; that is the next, much larger slice. Still open: row
construction, child visibility internals, live resources, scrollbar internals,
skin/module/display/scanout/startup and independent visible parity.
