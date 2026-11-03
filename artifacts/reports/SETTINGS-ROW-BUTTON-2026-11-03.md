# Settings-row renderItems types 0x5/0x6 (launcher button) — 2026-11-03

`common/settings_row_render.c` now covers the launcher-button cases. Both do
three rectangle lookups ("button", "image1", "image2") and drive the
row+0x684 button plus the row+0x2b28 / row+0x2c0c images. Created with no
rebuild flag refreshes the button (EASIRect math + setText); otherwise the
body destroys, optionally resets cached XML, applies the four names (type 5:
0x2e70/0x2e78/0x2e6c/0x2e74; type 6: 0x2e80/0x2e88/0x2e7c/0x2e84, slots
1/3/0/2), four glyph ids from `glyph_id(row[0x2dd4], state)`, highlight
row[0x2e68], sublauncher highlight 0x237, elipses 1 and shadow 0x94, then
creates with icon id row[0x2dd4]. The common tail stores 0x92/0x93 into
row[0x2e4c]/[0x2e50] and drives both images before `setDrawOnTop(1)`.
**38216** native/C comparisons pass with exact call order/args, rect words,
text bytes and row state, plus strict host, focused ASan/UBSan, ARM/qemu, all
prior groups and a focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Jump
table at 0xd5454c maps types 5 and 6 to the same body at 0xd547ca; the only
behavioural difference is the name set selected by `cmp r3,#6` at 0xd563b6.

Recovered details that mattered:

- `SVGButton::create` takes an **icon id** in r1 (here row[0x2dd4]), so the
  `create_button` callback gained that argument; earlier cases pass 0.
- `LauncherHelper::getImageId` is a 780-byte pure switch over 62 operation
  ids. It takes only **(op, state)** — my first callback shape carried an
  extra row argument, which shifted the register mapping and produced a
  wrong glyph id. The corrected callback is two-argument and the harness
  returns `(op<<8)|state` so the wiring is observable.
- The row stores at 0x2e4c/0x2e50 are **inline `str`**, not calls, so the
  native trace has no store event; the C-side `store_row_word` callback is a
  silent side effect and the row memcmp verifies the words.
- The rebuild path reloads the image1 rectangle and dereferences it
  (`ldr r0,[r0,#8]` at 0xd564a0). With a null image1 and the 0x2e45 flag the
  firmware faults, so that input is excluded from the sweep.

## Verification and limits

**38216** native/C comparisons: 13 types (0,1,2,3,4,5,6,0xd,0xe,0xf,0x10,
0x1a,0x16) × 64 eight-key presence masks × created × 3 point variants ×
row-word set × label pair × rebuild flag, minus the type-4 and type-5/6
firmware-crashing inputs, plus eight default-type cases. Host units add both
name sets, the four glyph ids, highlight/sublauncher/elipses/shadow, the icon
argument, both image scaling types and `setDrawOnTop`. Strict full host/
actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu units, every prior group
and the focused TV CPU test pass. Isolated build
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 113460 | accfc355c08beec4f27529c75d2f3e28 |
| tests/tab_policy_firmware | 1086648 | 0ecf0c2d784ea39266e0d7db6229d3ed |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

The launcher button is closed. Still open: `LauncherHelper::getImageId`'s
real 62-way mapping, `setSelectionBoxImageId`/`SVGImage` internals, switch
case bodies 0x7..0xc and 0x11..0x15 plus 0x17..0x19, row construction, child
visibility internals, live resources, scrollbar internals, skin/module/
display/scanout/startup and independent visible parity.
