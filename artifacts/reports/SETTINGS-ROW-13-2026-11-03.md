# Settings-row renderItems type 0xd — 2026-11-03

`common/settings_row_render.c` now covers type 0xd (13), the text-entry box.
It looks up the "selectionbox" rectangle and drives row+0x18d8. Uncreated
writes row word 0x2ec0/0x2ebc/0x2ec8/0x2ec4 into name slots 1/0/3/2,
creates from an empty label with the (0, 0xc, 0xd) triplet, then sets entry
mode 0, max chars 4 and automatic cursor movement. Created applies EASIRect
math, setWindowRect and setNumericContents from row[0x2e00] before the same
three trailing setters. **24392** native/C comparisons pass with exact call
order/args, rect words, text bytes and row state, plus strict host, focused
ASan/UBSan, ARM/qemu, all prior groups and a focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Case
0xd enters at jump-table target `0xd54e34` and uses SVGTextEntryBox
throughout:

1. `getRectangle("selectionbox")` (literal 0xd55140 -> 0x178d5b8); null →
   return. The child is `row + 0x18c0 + 0x18 = row+0x18d8`, matching the
   setType table's text-entry binding.
2. `isCreated`.
3. Uncreated: names row[0x2ec0] / row[0x2ebc] / row[0x2ec8] / row[0x2ec4]
   into slots **1/0/3/2**, then a UString from the shipped empty literal at
   0xd55144 (`0x12f5cda`), `SVGTextEntryBox::create(sl, 0xc, 0xd, ustr,
   rect, parent)` (seven args), temporary destructor.
4. Created: EASIRect copy/point/offset, `setWindowRect`, then
   `setNumericContents(row[0x2e00])`.
5. Both paths finish with `setEntryMode(0)`, `setMaxNoOfChars(4)` and
   `setCursorMoveAutomatic(1)`.

The differential caught two harness gaps, both the same class as earlier
cases: the case carries its **own** "selectionbox" literal (0xd55140) that
had to be redirected to the owned key, and it builds a UString from the
vendor empty literal at 0xd55144 which had to point at the owned empty
string or the constructor's `strlen` faulted on unmapped rodata.

## Verification and limits

**24392** native/C comparisons: types {0,1,2,3,4,0xd,0x1a,0x16} × 64 key
presence masks × created × 3 point variants × row-word set × label pair ×
rebuild flag, minus the 192 firmware-crashing type-4 inputs, plus eight
default-type cases. Host units add the 1/0/3/2 name order with the
0x2ebc/0x2ec0/0x2ec4/0x2ec8 mapping, the empty-label seven-arg create, the
entry-mode/max-chars/cursor-auto tail and the created numeric-contents
source. Strict full host/actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu
units, every prior group and the focused TV CPU test pass. Isolated build
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 80200 | cc442f3a259d0edfbf8ada9ce22e2fa9 |
| tests/tab_policy_firmware | 1048972 | 0d006d8c71faac8c0f9a0ac7467b20d1 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

Cases 0xe/0xf/0x10 share the same key and child and differ in entry mode,
max-chars and the created text source; they are the next slice. Still open:
switch case bodies 5..12 and 0xe..0x15 plus 0x17..0x19, row construction,
child visibility internals, live settings/resource acquisition, scrollbar
internals, skin/module/display/scanout/startup and independent visible parity.
