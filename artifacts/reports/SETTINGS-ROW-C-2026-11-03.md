# Settings-row renderItems type 0xc (text2/text3/text4 + slider) — 2026-11-03

`common/settings_row_render.c` now covers type 0xc, the three-text-box row
sharing the slide bar. It looks up **"slidebar"**, **"text2"**, **"text3"**
then **"text4"** through its own literals at `0xd55cd4`/`0xd55cd8`/`0xd55cdc`/
`0xd55ce0` (plus the shared empty label at `0xd55ce4`). `text2` drives
`row+0x1ca8` with the formatted `row[0x2df8]` value; `text3`+`text4` (both
rectangles required) drive `row+0x1f68` and `row+0x2228` from the row UStrings
at `0x2e2c`/`0x2e34`, with text alignment 6 applied only to `0x1f68` and
`setElipses` applied only to `0x1f68` (not `0x2228`). The `row+0x1498` slide
bar then refreshes or, uncreated, does `setFocus(0)` + create + value with
**no name writes at all**. **312224** native/C comparisons pass with exact
call order/args, rect words, text bytes and row state, plus strict host,
focused ASan/UBSan, ARM/qemu, all prior groups and a focused real-TV CPU run
(exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Jump
table entry type 0xc (12) -> `0xd55a20`.

1. `getRectangle("slidebar")` -> [r7,#12]; then `"text2"` -> r9, `"text3"` ->
   r8, `"text4"` -> fp.
2. If `text2` is present: `isCreated(row+0x1ca8)`.
   - Uncreated: names 0x1e8/0x1e9/0x1ea/0x1eb in slots 1/0/3/2,
     `setFocus(0)`, `setElipses(0)`, `create(0,"",rect+8,1,row)`.
   - Created: EASIRect copy, point, offset, `setWindowRect`.
   - Both then build `UString("") << row[0x2df8]` and `setText`.
3. If **both** `text3` and `text4` are present: `isCreated(row+0x1f68)`
   (the `0x2228` child's state is never queried).
   - Uncreated: names on `0x1f68`, `setFocus`, `setElipses`,
     `setTextAlignment(6)`, create, `setText(row UString 0x2e2c)`; then names
     on `0x2228`, `setFocus` only, create,
     `setText(row UString 0x2e34)`.
   - Created: one point query, rect math for `0x1f68`, `setWindowRect`; rect
     math for `0x2228` reusing the same point, `setWindowRect`; then both
     setText calls.
4. If the slidebar rect is present: `isCreated(row+0x1498)`.
   - Created: rect math, `setWindowRect`, `setSliderRangeValues(0x2df0,
     0x2df4)`, `setSliderValue(0x2df8)`.
   - Uncreated: **no `setSVGName` calls** — `setFocus(0)`, create from
     `0x2df0`/`0x2df4`, `setSliderValue(0x2df8)` (the `0xd55872` tail, which
     0xa reaches only after writing the 0x214.. names).

The host trace caught one ordering error before the differential: the second
box's `setText` comes **immediately after its create**, before the third box's
name writes, not after both creates.

## Verification and limits

**312224** native/C comparisons: **Pass A** 19 types × 8192 thirteen-key
presence masks × created; **Pass B** point/word/label/flag coverage with every
key present; **Pass C** type 0xb over both image-created states; **Pass D**
type 0xc over both textbox-created states; plus eight default-type cases.
Host units add the four-key lookup order, the three name sets, alignment 6 on
the second box only, both row UStrings, the value formatting, the created
refresh order and the nameless slider create. Strict full host/actual Unix
IPC, focused ASan/UBSan, fresh ARM/qemu units, every prior group and the
focused TV CPU test pass. Isolated build `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 173688 | 581135cfd4492a0dacf8aac5bd2ec1bd |
| tests/tab_policy_firmware | 1133784 | 40ba362932bf993d0d87e06c1d0440fc |

The whole reachable range was callee-audited before sweeping; the audit found
`SVGTextBox::setTextAlignment` (0x10c72a8) had **no trampoline**, which would
have been a fourth NUL-sled. It is now installed.

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified the no-reset flag/free
space, staged only the focused binary and checked size/hash in a separate
connection before execution. Test exit 0 passed, staging was removed and the
`/proc/*/exe` scan found no helper; `/conf/coredumpnoreset` remained present.
No vendor fragment, device API, screen/input/audio control, service restart,
network/settings/boot mutation or persistent deployment was run on the TV.

Still open: case bodies 0x11..0x15 and 0x17..0x19, `getImageId`'s real 62-way
mapping, the `setSelectionBoxImageId`/`SVGImage` internals, row construction,
child visibility internals, live resources, scrollbar internals,
skin/module/display/scanout/startup and independent visible parity.
