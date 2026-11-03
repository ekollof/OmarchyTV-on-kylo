# SVG-list row views and full refresh — 2026-11-03

`common/svg_list_rows.c` gains the RowManager row-view getters
(getTotalNumberOfRows, getLastVisibleItem, getItemRow) and
`common/svg_list_update.c` implements SVGListBoxBase::update, the full list
refresh. 100 + 20 native/C comparisons pass with exact call order/args and
resulting manager/list state, plus strict host, focused sanitizers and
ARM/qemu. The owner resumed the goal after the SVG-LIST-SCROLLBAR stop; no TV
access, wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 10be714 | 56 | 80e1c9cf | RowManager::getTotalNumberOfRows |
| 10be7ac | 56 | ec0fa064 | RowManager::getLastVisibleItem |
| 10be848 | 72 | cc441597 | RowManager::getItemRow |
| 10bc050 | 92 | 22020b30 | SVGListBoxBase::update |

Only guard-pointer literals are redirected; the +0x6c total virtual,
updateFull, updateScrollBar and startSelectAnimation are controlled stubs.
Real getFirstVisible bytes execute verbatim. No vendor object initialization
is involved.

getTotalNumberOfRows is slots (end-begin)/4 with an arithmetic shift;
getLastVisibleItem is first+size-1 wrapping; getItemRow returns null below
first or at/above first+size, else the row at the relative position. update
reads the total through the +0x6c virtual
(SettingItemsListBox::totalNumberOfItems; pure virtual on the base), stores
it to the +0x1ec count word, then runs the full update of the RowManager at
this+0xf8 with (first-visible,total), refreshes the scrollbar and restarts
the selection animation. The store and first-visible reads are C-side only
(no native call); their effects are asserted through the stored count word
and the (first,total) operands captured at updateFull. The row-callback
convention is standardized to context-free (matching native setFocus,
getWindowRect and setVisible receivers); owner callbacks keep context.

The differential caught a harness defect: direct calls through
memcpy-initialized function-pointer locals are miscompiled at -O2 by the
ARM cross compiler (wild call, crashing downstream checks); routing the
entry-point call through a volatile pointer is now required in this
harness. A fragment-length slip (56 used for the 48-byte getFirstVisible)
and a wrong-fragment CRC were also caught and corrected to next-symbol
extents. NULL ops or any NULL callback returns EINVAL.

## Still open

Row getters and editing at the widget level (getSelectedItemId, numeric/
checked/slide values), parent refresh, scrollbar construction/rendering
internals, startSelectAnimation, and the owned backend/context/consumer with
permission/modal/focus/lifetime/visible gates. Bodies are not actual
widget/rendering acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-update-2026-11-03/.
