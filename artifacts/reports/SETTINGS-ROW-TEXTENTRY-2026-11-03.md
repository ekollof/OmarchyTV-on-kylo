# Settings-row renderItems text-entry cases 0xe/0xf/0x10 — 2026-11-03

`common/settings_row_render.c` now covers the whole text-entry family
(0xd, 0xe, 0xf, 0x10) through one parameterised body. All four look up the
"selectionbox" rectangle and drive row+0x18d8: uncreated writes row word
0x2ec0/0x2ebc/0x2ec8/0x2ec4 into name slots 1/0/3/2 and creates from an empty
label with the (0, 0xc, 0xd) triplet; created applies EASIRect math and
setWindowRect. They differ only in the trailing setters and the created text
source:

| type | entry mode | max chars | created source |
|---|---:|---:|---|
| 0xd | 0 | 4 | setNumericContents(row[0x2e00]) |
| 0xe | 0xa | 5 | setNumericContents(row[0x2e00]) |
| 0xf | 0xa | 5 | setNumericContents(row[0x2e00]) |
| 0x10 | 1 | 10 | setText(row[0x2e04]) |

All finish with `setEntryMode`, `setMaxNoOfChars` and
`setCursorMoveAutomatic(1)`. **33608** native/C comparisons pass with exact
call order/args, rect words, text bytes and row state, plus strict host,
focused ASan/UBSan, ARM/qemu, all prior groups and a focused real-TV CPU run
(exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. The
jump table at 0xd5454c was re-read directly and confirms 0xe -> 0xd54fdc,
0xf -> 0xd5506e, 0x10 -> 0xd54f0a (type 0xd -> 0xd54e34 is already covered).
0xe and 0xf are behavioural clones sharing the created tail at 0xd5510a and
the mode/max tail at 0xd550f8; 0x10 has its own created tail at 0xd54fa6
(calling `SVGTextEntryBox::setText` with row+0x2e04) and rejoins at 0xd54f9a.

**The differential caught an un-stubbed callee**: `SVGTextEntryBox::setText`
at 0x10b8160 had no trampoline, so the guest executed zero halfwords
(`movs r0,r0` sleds) and slid into the neighbouring
setNumericContents stub at 0x10b81f4, producing a numeric event with a garbage
operand. The trace comparison exposed it immediately. A full callee audit of
the four bodies now lists twelve call sites and every one is installed.

## Verification and limits

**33608** native/C comparisons: types
{0,1,2,3,4,0xd,0xe,0xf,0x10,0x1a,0x16} × 64 key presence masks × created ×
3 point variants × row-word set × label pair × rebuild flag, minus the 192
firmware-crashing type-4 inputs, plus eight default-type cases. Host units add
the 1/0/3/2 name order, the empty-label seven-arg create, each case's
mode/max pair and both created sources (numeric for 0xe/0xf, value text for
0x10). Strict full host/actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu
units, every prior group and the focused TV CPU test pass. Isolated build
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 84360 | 5478829a37dba33078596133b049a5da |
| tests/tab_policy_firmware | 1049500 | 5c32008cb7758bcb3d3ddceb015533a5 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

The text-entry family is now closed. Still open: switch case bodies 5..12,
0x11..0x15 and 0x17..0x19, row construction, child visibility internals, live
settings/resource acquisition, scrollbar internals, skin/module/display/
scanout/startup and independent visible parity.
