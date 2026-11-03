# SVG-list move primitives — 2026-11-03

`common/svg_list_move.c` implements the eight primitive SVGListBoxBase
move/selection/window bodies called by the key bodies in SVG-LIST-BODIES
(moveWindowToTop/Bottom, moveSelectionToTop/BottomOfWindow, moveWindowUp/Down,
moveSelectionUp/Down). 5040 native/C comparisons pass with exact call order
and args, plus strict host, focused sanitizers and ARM/qemu. The owner resumed
the goal after the SVG-LIST-BODIES stop; no TV access, wake, staging,
deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 10bc2e0 | 68 | 188ffe10 | moveWindowToTop |
| 10bc324 | 88 | 7e9785c6 | moveWindowToBottom |
| 10bc37c | 64 | 818828e6 | moveSelectionToTopOfWindow |
| 10bc3bc | 76 | 2c731f65 | moveSelectionToBottomOfWindow |
| 10bc408 | 80 | bae06b9a | moveWindowUp |
| 10bc458 | 80 | 915e414d | moveWindowDown |
| 10bc4a8 | 68 | d2fd2b9d | moveSelectionUp |
| 10bc5bc | 68 | 2e6b9b7f | moveSelectionDown |

Only guard-pointer literals are redirected; RowManager
(size/selectedItem/getFirstVisibleItem/focusItem), updatePartial,
updateScrollBar and startSelectAnimation callees are controlled stubs, not a
vendor UI instance.

All arithmetic wraps uint32_t with no signed interpretation.
moveWindowToTop posts partial index 0; moveWindowToBottom posts aux-size with
no null check (callers establish the word). Window up/down shift
first-visible by minus/plus n and do NOT animate; only the to-top/to-bottom
window moves end with startSelectAnimation. Selection up/down focus
selected-item minus/plus n; the of-window variants focus first-visible and
first-visible+size-1. The differential asserts the receiver split proved by
SVG-LIST-BODIES: move/window/animation primitives run on the list box, while
RowManager queries and focusItem run on this+0xf8; getSelectedItem runs on the
list. These bodies are void in firmware: errno only, no handled. NULL ops or
any NULL callback returns EINVAL.

## Still open

Subsequent [SVG-LIST-ROWS](SVG-LIST-ROWS-2026-11-03.md) implements the
RowManager internals (field layout, focusItem body, updatePartial forward).
updateImpl, row widget internals, row getters and editing, parent refresh,
scrollbar/animation bodies and the owned backend/consumer remain open. Bodies
are not actual widget/rendering acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-move-2026-11-03/.
