# Settings-row renderItems type 0x7 (banner button) — 2026-11-03

`common/settings_row_render.c` now covers type 0x7, the banner-button
variant. It looks up the **direct** "banner" string (literal 0xd54b30 holds
0x14fb6e0, not a pointer-to-pointer), drives the row+0x684 button, and either
refreshes it (created: EASIRect math plus setWindowRect only — no setText) or
writes the type-5 names into slots 1/3/0/2, four glyph ids from
`getImageId(row[0x2dd4], state)`, elipses 1 and shadow 0x94, then creates with
icon row[0x2dd4] and finishes with `setDrawOnTop(1)`. **40904** native/C
comparisons pass with exact call order/args, rect words, text bytes and row
state, plus strict host, focused ASan/UBSan, ARM/qemu, all prior groups and a
focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Jump
table entry type 7 -> 0xd54918.

Case 0x7 differs from 0x5/0x6 in four ways: it has no `destroy` /
`resetCachedXMLEv`, no image1/image2 handling and no `0x92/0x93` stores; its
created path does **not** call setText; and its rebuild path folds the
**text1** rectangle (prologue slot [r7,#12], bit 1 of the presence mask)
into the button temp — a null text1 with the 0x2e45 flag faults at
`ldr r0,[r0,#8]`, so that input is excluded from the sweep.

No new callbacks were needed: `get_rect`, `is_created`, `set_name`,
`glyph_id`, `set_glyph`, `set_elipses`, `set_shadow`, `set_text_alignment`,
`create_button` (with the icon argument added last turn), `set_rect` and
`set_draw_on_top` cover it.

## Verification and limits

**40904** native/C comparisons: 14 types × 64 eight-key presence masks ×
created × 3 point variants × row-word set × label pair × rebuild flag, minus
the type-4 / type-5-6 / type-7 firmware-crashing inputs, plus eight
default-type cases. Host units add the banner lookup, the 1/3/0/2 name order,
four glyph ids, elipses/shadow, the icon argument and the created refresh
without setText. Strict full host/actual Unix IPC, focused ASan/UBSan, fresh
ARM/qemu units, every prior group and the focused TV CPU test pass. Isolated
build `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 117948 | 556e9adbfe0f37fcb4e245c59d767fb1 |
| tests/tab_policy_firmware | 1089004 | e8c29ea89e6b4c4d818a3eba678f87af |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked its hash before execution. Test
exit 0 passed, staging was removed and the /proc/*/exe scan found no helper;
/conf/coredumpnoreset remained present. No vendor fragment, device API,
screen/input/audio control, service restart, network/settings/boot mutation
or persistent deployment was run on the TV.

Still open: case bodies 0x8..0xc, 0x11..0x15 and 0x17..0x19, `getImageId`'s
real 62-way mapping, the image/selection internals, row construction, child
visibility internals, live resources, scrollbar internals, skin/module/
display/scanout/startup and independent visible parity.
