# SettingItemsListBoxRow construction — 2026-11-04

`common/settings_row_construct.c` now implements the shipped constructor
`SettingItemsListBoxRow::SettingItemsListBoxRow(UIFramework&, SVGContext&)`
(`0xd53d0c`, Thumb, 0x4cd bytes, CRC `0xe68cf4e1`). **96** native/C checks
pass: the ordered base/child/UString/interface calls and all 47 default word
fields plus one byte, for mirror-off and mirror-on. Host unit, focused
ASan/UBSan, ARM/qemu, full `verify.cjs` (36 ok lines) and a focused real-TV CPU
run all pass.

## Recovered behavior

The constructor:
1. Calls the `SVGListBoxRow` base constructor with `(row, framework, context)`.
2. Constructs **17 children** in order, at fixed offsets:

| offset | child |
|---|---|
| 0x104, 0x3c4 | SVGTextBox, SVGTextBox |
| 0x684 | SVGButton |
| 0x1014 | SVGSelectionBox |
| 0x13c0, 0x1498 | SVGSlideBar, SVGSlideBar |
| 0x1570, 0x1724 | SVGProgressBar, SVGProgressBar |
| 0x18d8 | SVGTextEntryBox |
| 0x1ca8, 0x1f68, 0x2228, 0x24e8 | SVGTextBox ×4 |
| 0x27a8 | SVGCheckBox |
| 0x2b28, 0x2c0c, 0x2cf0 | SVGImage ×3 |

3. Constructs **10 UStrings** at `0x2dd8, 0x2de0, 0x2e04, 0x2e0c, 0x2e14,
   0x2e1c, 0x2e24, 0x2e2c, 0x2e34, 0x2e3c`; all but `0x2e24` are built from
   the shared empty literal, `0x2e24` uses the default constructor.
4. Writes the default fields: `0x2dec = 27`, `0x2df0 = -1`, `0x2df4 = 1`,
   `0x2e49 = 1`, `0x2e68 = 0x234`, `0x2dd4/0x2de8/0x2df8/0x2dfc = 0`, bytes
   `0x2e45..0x2e48 = 0`, and the per-child name-id defaults:
   - `0x2e6c..0x2e88 = 0x1f1,0x1f0,0x1f3,0x1f2` twice
   - `0x2e8c..0x2e98 = 0x1e9,0x1e8,0x1eb,0x1ea`
   - `0x2e9c..0x2ea8 = 0x1fa,0x1f8,0x1fb,0x1f9`
   - `0x2eac..0x2eb8 = 0x1f1,0x1f0,0x1f3,0x1f2`
   - `0x2ebc..0x2ec8 = 0x1fe,0x1fc,0x1ff,0x1fd`
   - `0x2ecc..0x2ed8 = 0x1f5,0x1f4,0x1f7,0x1f6`
5. Probes `UIFramework::uiControlInterface()` and
   `EASIControlInterface::getOSDHorizontalMirror()`; when mirrored it rewrites
   `0x2e4c = 120` and `0x2e50 = 119` (the 0x92/0x93 image scaling words).

The per-child name defaults are the same IDs `renderItems` later writes, so
this closes the "where do the initial SVG names come from" gap.

## Verification method

`tests/settings_row_construct_native.h` copies the constructor at its original
address (an 8 KiB mapping — the body crosses a page boundary), redirects its
stack-guard and empty-string literals, and installs stubs for the base,
8 child-constructor kinds (17 calls), both UString constructors, the framework
interface getter and the OSD mirror getter. Two cases (mirror off/on) compare:

- the full ordered call trace (kind + child offset + literal/default flag),
- the 47 default word fields and the `0x2e49` byte, after filling both row
  buffers with `0xA5` so a missing store is visible.

`row+0` (the vtable pointer) is excluded because it holds a vendor address.

The ctor is **Thumb**, which initially produced garbage under plain objdump;
the report records `--disassembler-options=force-thumb` as required for this
function.

## Verification and limits

Host unit covers the order, the 10 string slots, mirror off/on and every
default. Isolated `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_construct | 41296 | fe301c0f647461920972441c40236c63 |
| tests/tab_policy_firmware | 1284356 | 9ec46aa90b38c839baa037ab648d5d71 |

The focused real-TV CPU test ran the self-contained `settings_row_construct`
ARM unit: exit 0, staging hash-verified and removed, no helper left, no-reset
flag retained. No device/screen/input/settings/network/service/boot change.

**Boundary:** the child constructors themselves remain vendor stubs. The
inline default stores are read from the body and compared, but base-class
internals and vendor vtables are outside this differential. Still open: child
visibility internals, live settings resources, scrollbar/skin, display/scanout
binding, independent startup and visible parity.
