# SVG-list scrollbar — 2026-11-03

`common/svg_list_scroll.c` implements SVGListBoxBase::updateScrollBar, called
by every move primitive. 4320 native/C comparisons pass with exact gating,
rect math, show and position sequencing, plus strict host, focused sanitizers
and ARM/qemu. The owner resumed the goal after the SVG-LIST-ROWS stop; no TV
access, wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 10bbe04 | 216 | fbc862b8 | SVGListBoxBase::updateScrollBar |
| 10be6e4 | 48 | 50e85ab7 | RowManager::size (real) |
| 10be77c | 48 | 687fe952 | RowManager::getFirstVisibleItem (real) |
| 9a7fcc | 56 | e9107f7d | EASIRect ctor (pinned, stubbed at runtime) |

Only guard-pointer literals are redirected; the +0x6c total virtual,
isCreated/isVisible/setVisible, create/setScrollPosition are controlled
stubs, not a vendor UI. Real RowManager bytes execute against fixture
fields. No vendor object initialization is involved.

Create path iff size is nonzero with signed size below the signed +0x6c
total (SettingItemsListBox::totalNumberOfItems; pure virtual on the base),
or the +0x22c flag word is nonzero; otherwise an existing scrollbar is
hidden. On the create path an uncreated scrollbar is built from the literal
list fields x=[0x214] y=[0x1fc] w=[0x21c] h=([0x1f0]*[0x1f4]+[0x1fc])-
([0x1f4]-[0x1f8]) with wrapping uint32 arithmetic, shown iff the list is
visible, then positioned with (total,size,first-visible). On the hide path a
created scrollbar is set to the flag value (zero there). The differential
caught a stub-convention defect during development (r3-clobbering branch to
4-arg stubs); all multi-arg installers now preserve every argument register,
verified by objdump decode. NULL ops or any NULL callback returns EINVAL.

## Still open

updateImpl and row widget internals, row getters and editing, parent refresh,
updateScrollBar's construction/position targets (create/setScrollPosition
bodies), startSelectAnimation, and the owned backend/context/consumer with
permission/modal/focus/lifetime/visible gates. Bodies are not actual
widget/rendering acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-scroll-2026-11-03/.
