# Settings-row renderItems type 0xb (slider bar + images) — 2026-11-03

`common/settings_row_render.c` now covers type 0xb, the slider bar with two
launcher images. It looks up **"slidebar"**, **"image1"** then **"image2"**
(the first key shares type 0xa's literal at `0xd55a0c`; the image keys are its
own at `0xd55a18`/`0xd55a1c`). When both image rectangles exist it either
refreshes both images (both `row+0x2b28` and `row+0x2c0c` created: plain
setWindowRect with the rectangle words, **no** point offset) or creates each
from its `row[0x2e4c]`/`row[0x2e50]` scaling word. Then the `row+0x1498`
slide bar is refreshed (rect math + `setSliderRangeValues`) or created with the
`0x214/0x216/0x215/0x217` names, and both paths finish with
`setSliderValue(row[0x2df8])`. **74604** native/C comparisons pass with exact
call order/args, rect words, text bytes and row state, plus strict host,
focused ASan/UBSan, ARM/qemu, all prior groups and a focused real-TV CPU run
(exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Jump
table entry type 0xb (11) -> `0xd558e2`.

Flow, all objdump-verified:

1. `getRectangle("slidebar")` -> r9, `getRectangle("image1")` -> sl,
   `getRectangle("image2")` -> r6.
2. `if (sl == 0 || r6 == 0) goto 0xd55988` — the image block needs **both**
   rectangles, not either one.
3. `isCreated(row+0x2b28)` (0x2b00+0x28).
   - Not created -> create block.
   - Created -> `isCreated(row+0x2c0c)` (0x2c00+0xc); if that is also created
     -> refresh block, else create block.
4. Create block (`0xd5595c`): `create(r8, row[0x2e4c], image1rect+8, row)` when
   the scale word is non-zero, then
   `create(row+0x2c0c, row[0x2e50], image2rect+8, row)` likewise.
   **Quirk reproduced:** entering with image1 already created but image2 not
   re-creates image1, because the create call is unconditional on the 0x2e4c
   word rather than gated on `isCreated`.
5. Refresh block (`0xd56554`): EASIRect(image1rect+8) -> `setWindowRect(im1)`,
   `adds r6,#8` -> copy image2 rect words to the temp -> `setWindowRect(im2)`.
   No point query or offset here, unlike the slider path.
6. `if (slidebar rect == 0) return` (`0xd55988`).
7. `isCreated(row+0x1498)` (0x1480+0x18).
   - Not created: names **1=0x214, 0=0x216, 3=0x215, 2=0x217**,
     `setFocus(0)`, `create(0, row[0x2df0], row[0x2df4], rect+8, row)`.
   - Created: EASIRect copy, point, offset, `setWindowRect`, then
     `setSliderRangeValues(row[0x2df0], row[0x2df4])`.
8. Both paths jump to `0xd556ea`: `setSliderValue(row[0x2df8])`, return.

The whole reachable range was callee-audited **before** sweeping (the lesson
from the three NUL-sleds): every direct call site and both shared tails were
already covered by installed trampolines; only the case's own literal
redirects were new.

`is_created` in the harness now distinguishes the two image children from the
row-level child, so a third sweep pass covers the mixed state and asserts the
re-create quirk.

## Verification and limits

**74604** native/C comparisons: **Pass A** 18 types × 2048 eleven-key presence
masks × created; **Pass B** point/word/label/flag coverage with every key
present; **Pass C** type 0xb over both image-created states (mixed included);
plus the eight default-type cases. Host units add the image1/image2 create and
refresh orders, the no-point-offset refresh, the mixed re-create, the slider
name order, range and value. Strict full host/actual Unix IPC, focused
ASan/UBSan, fresh ARM/qemu units, every prior group and the focused TV CPU
test pass. Isolated build `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 159736 | 0aa47f2ca5618763f1350abe5b52b9a0 |
| tests/tab_policy_firmware | 1127136 | 6904c4fa5cfb7a46a542a6e470d810e7 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified the no-reset flag/free
space, staged only the focused binary and checked size/hash in a separate
connection before execution. Test exit 0 passed, staging was removed and the
`/proc/*/exe` scan found no helper; `/conf/coredumpnoreset` remained present.
No vendor fragment, device API, screen/input/audio control, service restart,
network/settings/boot mutation or persistent deployment was run on the TV.

Still open: case bodies 0xc, 0x11..0x15 and 0x17..0x19, `getImageId`'s real
62-way mapping, the `setSelectionBoxImageId`/`SVGImage` internals (the create
and refresh here are stubs), row construction, child visibility internals,
live resources, scrollbar internals, skin/module/display/scanout/startup and
independent visible parity.
