# Settings-row renderItems type 0x8 (plain button) — 2026-11-03

`common/settings_row_render.c` now covers type 0x8, the plain button. It
looks up "button", drives the row+0x684 child; created applies EASIRect math,
setWindowRect and setText, otherwise it writes
row[0x2eb0]/[0x2eac]/[0x2eb8]/[0x2eb4] into name slots 1/0/3/2 and creates
with icon 0. No images, glyphs, stores or draw-on-top. **43976** native/C
comparisons pass with exact call order/args, rect words, text bytes and row
state, plus strict host, focused ASan/UBSan, ARM/qemu, all prior groups and a
focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Jump
table entry type 8 -> 0xd54a66.

Case 0x8 is the type-4 button body's key1-missing branch extracted into its
own case: the same 0x2eb0/0x2eac/0x2eb8/0x2eb4 name set in slots 1/0/3/2,
the same icon-0 create, and the same created refresh, but with no key1
branch, no images, no `0x92/0x93` stores and no `setDrawOnTop`. It also has no
`0x2e45` rebuild branch, so there is no firmware-crashing input for this
case.

No new callbacks were needed — the ops surface assembled for types 4/5/6/7
already covers it.

## Verification and limits

**43976** native/C comparisons: 15 types × 64 eight-key presence masks ×
created × 3 point variants × row-word set × label pair × rebuild flag, minus
the type-4 / type-5-6 / type-7 firmware-crashing inputs, plus eight
default-type cases. Host units add the 1/0/3/2 name order with the
0x2eac/0x2eb0/0x2eb4/0x2eb8 mapping and the created setWindowRect+setText
refresh. Strict full host/actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu
units, every prior group and the focused TV CPU test pass. Isolated build
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 122876 | a9960598965254a2454a35d1677ae2b0 |
| tests/tab_policy_firmware | 1090492 | 321845c147d592de59a292c8ad296523 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag and staged
only the focused binary after checking its hash. Test exit 0 passed, staging
was removed and the /proc/*/exe scan found no helper. No vendor fragment,
device API, screen/input/audio control, service restart, network/settings/
boot mutation or persistent deployment was run on the TV.

Still open: case bodies 0x9..0xc, 0x11..0x15 and 0x17..0x19, getImageId's real
62-way mapping, the image/selection internals, row construction, child
visibility internals, live resources, scrollbar internals, skin/module/
display/scanout/startup and independent visible parity.
