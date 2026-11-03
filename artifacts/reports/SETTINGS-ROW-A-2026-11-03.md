# Settings-row renderItems type 0xa (second slider row) — 2026-11-03

`common/settings_row_render.c` now covers type 0xa, the second slider row.
It looks up **"slidebar"** then **"text2"** (the same keys as type 0x9, but
through its own literals at `0xd55a0c`/`0xd55a10`), drives the `row+0x1ca8`
text box and the `row+0x1498` slide bar. The text box always uses the
`0x1e8/0x1e9/0x1ea/0x1eb` names in slots **1/0/3/2** with no font and no key1
branch, and always receives the `%d`-formatted `row[0x2df8]` value. The slide
bar uses `0x214/0x216/0x215/0x217` in slots 1/0/3/2; uncreated is created
from `row[0x2df0]`/`row[0x2df4]`, created gets `setSliderRangeValues`, and
both finish with `setSliderValue(row[0x2df8])`. **70456** native/C
comparisons pass with exact call order/args, rect words, text bytes and row
state, plus strict host, focused ASan/UBSan, ARM/qemu, all prior groups and a
focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Jump
table entry type 0xa (10) -> `0xd5575c`.

| aspect | 0x9 | 0xa |
|---|---|---|
| text-box names | 0x26d + font 0x1e when key1, else 0x1e8..0x1eb | always 0x1e8..0x1eb |
| text box child | row+0x1ca8 | row+0x1ca8 |
| slider child | row+0x13c0 | row+0x1498 |
| slider names | 0x26c | 0x214/0x216/0x215/0x217 |

A **callee audit** of the whole reachable range was done before sweeping
(the lesson from the last three NUL-sleds). Every one of the fourteen direct
call sites plus the two shared created blocks
(`0xd55892` text box, `0xd559d0` slider) was already covered by an existing
trampoline: `UString` ctor/dtor and `operator<<`, `getRectangle`, `isCreated`,
`setSVGName`, `SVGTextBox` setFocus/setElipses/create/setText/setWindowRect,
`SVGSlideBar` setFocus/create/setWindowRect/setSliderRangeValues/
setSliderValue, and the two EASIRect helpers.

The one new harness requirement was redirecting case 0xa's own key literals
and empty-label literal (`0xd55a14`), which had not been needed before.

## Verification and limits

**70456** native/C comparisons: 17 types over the full 11-key presence pass
plus the point/word/label/flag pass with every key present, minus the
type-4/5-6/7 firmware-crashing inputs. Host units add the 0x1e8.. and 0x214..
name orders, the empty-label text box create, the formatted value, the
created `setSliderRangeValues` pair and the final `setSliderValue`. Strict
full host/actual Unix IPC, focused ASan/UBSan, fresh ARM/qemu units, every
prior group and the focused TV CPU test pass. Isolated build
`build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 147256 | 682bb42a43bf68513cdbc58ac655dd0b |
| tests/tab_policy_firmware | 1118192 | f372ead9eaeb5d488a469284488b0570 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake and verified the no-reset flag;
staging was hash-checked before execution, the test exited 0 and staging was
removed with no helper left. No vendor fragment, device API, screen/input/
audio control, service restart, network/settings/boot mutation or persistent
deployment was run on the TV.

Still open: case bodies 0xb/0xc, 0x11..0x15 and 0x17..0x19, getImageId's real
62-way mapping, the image/selection internals, row construction, child
visibility internals, live resources, scrollbar internals, skin/module/
display/scanout/startup and independent visible parity.
