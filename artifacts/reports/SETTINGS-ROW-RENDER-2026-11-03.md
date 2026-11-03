# Settings-row renderItems (0/1/4/shared tail) — 2026-11-03

`common/settings_row_render.c` implements SettingItemsListBoxRow::renderItems
for row types 0, 1 and 4, plus the immediate 0x1b default and the switch
default for any other type outside 2..26. The prologue always runs the base
render and three rectangle lookups ("alexa" at ELF 0x14be1b, "text1" at
0x1543bbf, "labelLong" at 0x14df0f4); type 4 additionally looks up "button"
at 0x151cfff. With a key2 rectangle every non-4 type runs the shared
row+0x104 tail (type 1 uses row+0x3c4 instead); only 0x1b exits before it,
and a missing key2 skips straight to the switch head. The tail's create
rejoins the switch, which dispatches case 4 back into the button body for
type 4 and defaults otherwise. Uncreated children get setSVGName rows,
created ones get EASIRect math (verbatim-copied copy/offset helpers) plus
setWindowRect, and every path ends in a create call. **2264** native/C
comparisons pass with exact call order/args, rect words, text bytes and row
state, plus strict host, focused ASan/UBSan, ARM/qemu, all prior groups and
a focused real-TV CPU run (exit 0). No screen/input/device/settings/network/
service/boot/persistent change; staging removed, no helper remains.

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump and ELF
(the saved Ghidra `--asm` dump contains phantom instructions past the
switch head and was abandoned as a source). The offline reference copies
0xd54400/8746 bytes at CRC32 `5b7e119b`, preserving the jump table at
0xd5454c and the literal pool; only the guard-pointer literal is redirected.
The EASIRect copy (0xd51d00/58, CRC `119bb116`), offset (0xd2af88/74, CRC
`bc62c27a`) and default constructor (0x9a7f94/52, CRC `2a2b3d80`) run verbatim
from copied bytes with their own guard literals redirected. Entry stubs
cover the base render, UString ctor/dtor, getRectangle, isCreated, the 3-arg
setSVGName, focus/elipses/font, both setWindowRects, both setTexts, both
creates (plus a Thumb-2 `ldr.w r12`/`bx r12` preserving trampoline for the
5/6-argument forms, whose encoding was corrected after a `bx sp` mis-encode
crashed). The vtable+0x28 slot carries the owned point callback:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 0xd54400 | 8746 | 5b7e119b | renderItems (0/1/4/default paths) |
| 0xd51d00 | 58 | 119bb116 | EASIRect copy ctor |
| 0xd2af88 | 74 | bc62c27a | EASIRect::offset |
| 0x9a7f94 | 52 | 2a2b3d80 | EASIRect default ctor |

Three consecutive fragment truncations (each cutting the final return word)
were caught by execution sliding into NOP sleds; every copied span now ends
past its return. The type-1 label block, the 0x26d/0x2e90 shared-tail rows,
the reconstruct store, the fallthrough from the shared tail back into case
4 and the dead temp math before the button create (mov r3,r9 passes key4rect
directly, objdump-verified) are asserted. The native 0x2dec-valued third
create argument turned out to be the row+0x2dd8 label reloaded into sl
(0xd565ce, objdump-verified), not a stale word. The type-4 not-created copy
fold reads the shared slot, which is key2rect on entry or key3rect after the
reconstruct store; with the flag set and a null slot it faults natively
(`ldr r4,[r0,#0x8]` with r0==0), so that input stays excluded from the sweep.

## Verification and limits

**2264** native/C comparisons: types {0,1,4} crossed with key-rect
presence, created flags, point/row-word/label variants and rebuild flags
(minus 48 firmware-crashing inputs), plus four default types in both created
states. They compare complete ordered traces, rect words, text bytes and
full row state (no row stores exist; the zero-fill default ctor is
unobservable and runs verbatim). Host units additionally cover open-type
gates (0x1b immediate, 2..26 ENOTSUP incl. 0x1a-before-tail), invalid ops and
the fallthrough/reconstruct joins. Strict full host/actual Unix IPC, focused
ASan/UBSan, fresh ARM/qemu units, every prior group and the focused TV CPU
test pass. Isolated build `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 49420 | c2bf2da32e109d5914526814c13b98d0 |
| tests/tab_policy_firmware | 1004372 | f60da58c6f420ab9a6bffe4090e0d6b7 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

This closes the pre-switch render paths and the type-4 body. Still open:
switch cases 2..26 (incl. the 0x1a tail branch), row construction, child
visibility internals, live settings/resource acquisition, scrollbar internals,
skin/module/display/scanout/startup and independent visible parity. Use the
saved render source for the next case-group slice.
