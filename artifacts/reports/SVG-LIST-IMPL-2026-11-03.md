# SVG-list update implementation — 2026-11-03

`common/svg_list_impl.c` implements SVGListBoxRowManager::updateImpl and
rotateRows, the state core behind every list key/move/scroll body. 6952
native/C comparisons pass with exact call order/args and resulting manager,
row-array and selection state, plus strict host, focused sanitizers and
ARM/qemu. The owner resumed the goal after the SVG-LIST-SCROLL stop; no TV
access, wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 10be504 | 372 | 6ec6aabd | RowManager::updateImpl |
| 10be42c | 216 | 5808927d | RowManager::rotateRows |

Only guard-pointer literals are redirected; row +0x1C setFocus, +0x28
getWindowRect, +0x2C setWindowPosition, +0x34 setVisible, owner +0x58
displayMessageIfListEmpty, +0x5C drawRowWithListEmptyMessage, +0x60 drawRow
and UI isVisible/gotFocus are controlled stubs. Real manager field reads and
row-array indexing execute verbatim. No vendor object initialization involved.

updateImpl clamps start to 0 when capacity<=slots else to index or
capacity-slots, unfocuses a selected row in range, rotates by start-first
unless full, then per row shows newly added rows, draws out-of-old-window
rows and draws in-window rows only when full. Trailing writeback stores
count/first, clamps a selected row past the end, refocuses on gotFocus, and
on empty count stores selected 0 and draws the empty message/row when
enabled. rotateRows swaps row pairs (i,i+delta) from the front (positive
delta) or the back (negative delta), exchanging full positions through
getWindowRect temporaries and setWindowPosition. All index arithmetic wraps
uint32; window comparisons are unsigned; the rotate direction test and loop
bound are signed. The differential caught a real page-size accounting slip
in the harness (executable region shorter than the mapped stub) and a
stub-convention defect (r3-clobbering branch to 4-arg stubs); installing a
register-preserving branch verified by objdump decode. A prior +0x28/+0x18
slot reading is corrected at source: the row focus virtual is +0x1C
(setFocus, decimal 28), while +0x28 is getWindowRect with a sret out-param.
NULL ops or any NULL callback returns EINVAL.

## Still open

updateImpl's construction/position bodies are this item's drawRow/drawEmpty
consumers; row getters and editing, parent refresh, startSelectAnimation and
the owned backend/context/consumer with permission/modal/focus/lifetime/
visible gates remain open. Internals are not actual widget/rendering
acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-impl-2026-11-03/.
