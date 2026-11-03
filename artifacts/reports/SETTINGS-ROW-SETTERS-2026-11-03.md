# Settings-row label/progress/range setters — 2026-11-03

`common/settings_row_setters.c` implements SettingItemsListBoxRow::setLabel,
setProgressValue and setBarRangeValues, the remaining row setter bodies saved
in analysis/settings-row-2026-11-03/source.txt. Label writes row+0x3c4 for
type 1 else row+0x104; progress drives row+0x1570 for type 0x17 else
row+0x1724 with percent ((value-lo)*100)/(hi-lo) and always formats the
stored value into row+0x24e8; range drives row+0x13c0 for type 9 else
row+0x1498. Widget updates run only when the child reports created, but the
row stores (+0x2dd8 label, +0x2dfc value, +0x2df0/+0x2df4 bounds) always run.
324 native/C comparisons pass with exact call order/args, stored bytes and
formatted text, plus strict host, focused ASan/UBSan, ARM/qemu, full prior
groups and a focused real-TV CPU run (exit 0). No screen/input/device/settings/
network/service/boot/persistent change; staging removed, no helper remains.

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only decompile and ARM
come from the saved source file; the offline reference copies three fragments
with only guard-pointer literals redirected:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 0xd52498 | 96 | 1d395f7f | setLabel |
| 0xd52e40 | 180 | 9b0f3b13 | setProgressValue |
| 0xd53330 | 110 | 56d1fb77 | setBarRangeValues |

The first harness cut each fragment 4 bytes short of its final `pop {pc}`,
so label's epilogue slid through `movs r0,r0` NOPs into the progress bytes;
lengths above include the return. The progress format literal at 0xd52ef8
points at the shipped empty string (also 0x12f5cda, as in the slide check)
and is redirected to an owned empty string instead of mapping vendor rodata.

The division at 0x1285478 is the shipped __aeabi_idiv (verified by reading:
truncation toward zero, INT32_MAX/INT32_MIN for nonzero/zero, SIGFPE via
raise(8) for 0/0). The owned `oa_setting_row_percent` replicates wraparound
(verified: subs/muls pairs) and those edges without UB; the 0/0 input is
excluded from every sweep and maps to 0 with a note. UString temporaries are
pointer-slot stubs in the established owned-string convention; the format is
always the empty literal, so text is plain decimal. Empty labels, swapped and
extreme bounds, division-by-zero (nonzero numerator), INT32_MIN/-1 and full
32-bit minima/maxima are swept; created gates are crossed on both children.

## Verification and limits

**324** native/C comparisons (64 label, 224 progress, 36 range)
cover type-selected children, created gating, store-always order, formatted
text bytes and full row memcmp. Host units additionally cover percent edges
and invalid ops/args gates. Strict full host, focused ASan/UBSan, fresh
ARM/qemu units, every prior group and the focused TV CPU test pass. Isolated
build `build-tv-v3/settings-row-setters`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_setters | 33304 | 91fdb5c24bb31ce2344a7796e6ed6cb8 |
| tests/tab_policy_firmware | 933404 | d295d9d4fb62e388a55e4cc0aa6eda46 |

The standalone TV binary requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

This closes the saved setter bodies. Still open: actual renderItems, row
construction, child visibility internals, live settings/resource acquisition,
scrollbar internals, skin/module/display/scanout/startup and independent
visible parity. Use the saved render source for the next provider/widget
integration work.
