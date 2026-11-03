# SVG-list row editing — 2026-11-03

`common/svg_list_widgets.c` gains SettingItemsListBoxRow::setSlideValue: the
+0x2e47 progress shortcut and +0x2e48/type-23 progress shortcut, otherwise the
slider at row+0x1498 (row+0x13c0 when +0x2e48 is clear and the type is 9) set
when created, the value stored at row+0x2df8, and the text box at row+0x1ca8
updated through an empty-format UString when created. 72 native/C comparisons
pass with exact call order/args, written values and stored words, plus strict
host, focused sanitizers and ARM/qemu. The owner resumed the goal after the
SVG-LIST-WIDGETS stop; no TV access, wake, staging, deployment or pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragment:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 0xd52efc | 272 | 621d5980 | SettingItemsListBoxRow::setSlideValue |

Only guard-pointer literals are redirected; setProgressValue, isCreated,
setSliderValue, the UString constructor/append/destructor and textbox setText
are controlled stubs, not a vendor UI. The empty text format literal was read
verbatim from the dump (an empty string at file address 0x12f5cda); both
sides use an empty format. No vendor object initialization is involved.

Flag reads (+0x2e47, +0x2e48, type word +0x2dec), the slider/textbox address
arithmetic (+0x1498/+0x13c0/+0x1ca8) and the store word (+0x2df8) are asserted
exactly; the native UString stack buffer is a transport temporary and is not
compared. Progress paths store nothing, which the differential checks by
comparing the stored word only on slider paths.

The differential caught a real page-size accounting slip (272-byte fragment
overflowing its 4K page), a harness stub-convention gap (void 2-arg callees
need the preserving installer), and a comparison bug (progress paths leave a
stale store word). NULL ops, NULL row or any NULL callback returns EINVAL.

## Still open

Parent refresh, scrollbar construction/rendering internals,
startSelectAnimation, and the owned backend/context/consumer with
display/scanout binding, independent startup and visible parity. Editing is
not actual widget/rendering acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-slide-2026-11-03/.
