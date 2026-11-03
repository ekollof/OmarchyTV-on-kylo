# SVG-list widget getters — 2026-11-03

`common/svg_list_widgets.c` implements two widget-level getters behind the
typed row-value router: SettingItemsListBoxRow::getNumericContents (forward
to the embedded SVGTextEntryBox at row+0x18d8) and
SettingItemsListBox::getSelectedItemId (0xffffffff gate on uncreated list or
selected item -1, else the first word of the entry at the selected index in
the array whose address is stored behind list+0x28c). 21 native/C comparisons
pass with exact call order/args and return values, plus strict host, focused
sanitizers and ARM/qemu. The owner resumed the goal after the
SVG-LIST-MODULE stop; no TV access, wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 0xd531f4 | 56 | 9b91e0ae | SettingItemsListBoxRow::getNumericContents |
| 0xd50ce0 | 84 | e58623fa | SettingItemsListBox::getSelectedItemId |

Only guard-pointer literals are redirected; the embedded text entry's
getNumericContents, isCreated, list getSelectedItem and the entry/row objects
are controlled fixtures, not a vendor UI. The +0x28c cell holds the array
address (two loads: cell then array, then entry, then its first word); the
+0x18d8 row offset is pure address arithmetic with no observable call. No
vendor object initialization is involved.

The differential caught a real layout misread during development (single
instead of double indirection at +0x28c, found via fault-PC capture); the
cell-then-array transport is now asserted by real native bytes. A harness
stub-convention slip (thumb/predicate double-install on one address) and a
cold-start page mapping fault were also caught and fixed. NULL ops or any
NULL callback returns EINVAL (zero / 0xffffffff result respectively).

## Still open

Row editing (setSlideValue with its progress/slider/text paths, setChecked),
parent refresh, scrollbar construction/rendering internals,
startSelectAnimation, and the owned backend/context/consumer beyond
words/counters with display/scanout binding, independent startup and visible
parity. Getters are not widget/rendering acceptance, and no visible parity is
claimed.

Evidence and reproduction: artifacts/analysis/svg-list-widgets-2026-11-03/.
