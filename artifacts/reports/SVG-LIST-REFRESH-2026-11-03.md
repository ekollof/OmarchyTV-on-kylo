# SVG list row refresh and checked values — 2026-11-03

`common/svg_list_update.c` now implements the joined base/manager single-row
refresh, and `common/svg_list_widgets.c` implements checked-value routing.
These are synchronous context-bound callbacks; no new service, hardware
ownership, drawing or settings mutation is introduced.

## Firmware evidence

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`.
Read-only Ghidra decompilation and ARM instructions are saved in
`analysis/svg-list-refresh-2026-11-03/source.txt`.

| address | bytes | CRC32 | method |
|---|---:|---|---|
| 10bd0b4 | 52 | d995d3f6 | SVGListBoxBase::updateRow |
| 10be368 | 100 | 4bbe4149 | SVGListBoxRowManager::updateRow |
| d51cac | 84 | d5ec3f94 | SettingItemsListBoxRow::setChecked |

The base adds0xf8 to its receiver and forwards the index unchanged. The
manager reads first-visible at+0xc and returns if index is unsigned-below
first. It caps array slots (end-begin arithmetic-shifted by2) by the **total
item field+8**, not visible size+0x10. It compares index against the wrapping
sum first+min(total,slots), with unsigned operands. It then invokes the
parent at manager+4, virtual+0x60, with absolute index and row-array handle
at index-first. This is not equivalent to a relative-index bounds test when
the upper bound wraps. It performs no subsequent focus, scrollbar or
animation update.

`parent-source.txt` also records the actual SettingItemsListBox vtable:
its address point is14dbd74; +0x60 at14dbdd4 holds Thumb d50f61,
**SettingItemsListBox::drawRow**. That larger type/style/value dispatcher is
saved with instructions for continuation, but is not implemented by this
milestone. Completing this forwarding body does not close the parent renderer.

setChecked queries isCreated on the embedded checkbox row+0x27a8. Only a
nonzero result invokes SVGCheckBox::setChecked, followed by the cached byte
store at row+0x2e44. Uncreated boxes leave the cache unchanged. The public
API uses C bool, so arbitrary noncanonical register values are not claimed
as bool-ABI parity. All callbacks receive context explicitly, are pinned
for the call and retain no handle. Invalid providers return EINVAL before
running callbacks.

## Verification and limits

The optional offline reference executes both original base and manager bodies
joined, as well as the manager entry alone: **2,880** comparisons over total,
array size, index, first-visible and overflow boundaries. Visible size is
deliberately unrelated; callback count, absolute index and row identity match.
The **24** checked cases cover four raw created results, both bool inputs and
three old cache bytes; the setter observes the old cache, and full-row byte
comparison verifies the final store and untouched neighbors. Guard literals
are redirected; framework/getter/setter implementations are controlled.

Also corrected old `rv_slots[i*8]` indexing of a16x8 array in the row-view
reference to `rv_slots[i]`: previous tests formed out-of-bounds pointers even
though they did not dereference them. All100 row-view cases and all previous
reference groups still pass. This is an additional harness defect, not a
proven explanation of the historical SIGSEGV.

Strict full host suite and actual Unix IPC pass. Focused ASan/UBSan and fresh
ARM/qemu update/widgets units pass. Isolated build `build-tv-v3/list-refresh`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/svg_list_update | 40576 | 74a98ddc619d6efc9864a8044e67ff41 |
| tests/svg_list_widgets | 50976 | 920cbb14407eecd17e09f42ab261ac69 |
| tests/tab_policy_firmware | 747480 | 7183f3a632fec0682e9ddf83fb016ede |

No TV access/wake/staging/deployment or hardware/input/network/boot changes.
No native startup, widget rendering or visible acceptance is claimed. Next
recover/implement the saved parent drawRow type/style/value dispatcher and
the owned resource/widget backend, scrollbar construction/rendering, then
bind display/scanout and independently accept real startup and visible parity.
