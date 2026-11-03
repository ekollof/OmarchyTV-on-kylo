# Active-ID settings navigation — 2026-11-03

`common/settings_navigation.c` now implements the complete
SettingsDialog::navigateInList method through owned state/widget callbacks.
It is the algorithm behind SETTINGS-INPUT's NAVIGATE action, not a complete
list widget, renderer adapter or attached navigation queue consumer.

## Firmware contract

Source: analysis/settings-navigation-2026-11-03/navigation.txt, read-only Ghidra
decompile and ARM instructions for d56a24. Shipped Aurora MD5
`64c252340d3af6c5d66072611b7bad7a`. The312-byte native reference fragment has
CRC32 **59f359fd**; only its guard-pointer literal at d56b58 is redirected.

The active vector at+48c/+490 contains **item IDs**. The method loads its current
entry and passes that value to SettingItemsListBox::getItemIndex(int). It is not
an array of SettingItem pointers or an array of widget indices. Earlier
SETTINGS-INPUT wording “active pointer vector” described its word width too
loosely; that wording is corrected there. Cursor+498 indexes these active IDs;
the resolved signed widget index is a separate value.

An empty vector does nothing. With more than one entry, Down20b increments the
cursor and wraps at the final entry; Up20a decrements and wraps at zero.2db jumps
to the first entry if not already there;2dc jumps to the last if not already
there. One entry leaves the cursor untouched. Unknown keys also leave it alone,
but still perform the normal visibility/selection calculation when nonempty.
The source writes the cursor before loading the selected active ID.

After mapping that ID to a widget index, it obtains biggest-visible index and
row count. The first visible index is wrapped32-bit `biggest - rows + 1`.
Comparisons are **signed32**, while subtractions/additions follow ARM wrap;
portable C signed overflow or unsigned comparisons would differ at boundaries.
An index within the inclusive visible range sets selected row to `index-top`.
An index outside it normally sets selected item directly. Specifically for Down
and `index >= rows` (signed), it instead sets selected item to `index-rows+1`,
then selected row to `rows-1`. The geometry is captured before those callbacks;
do not re-read rows after setSelectedItem changes backend state.

The C API exposes those three distinct state/widget operations. SET_CURSOR
only stores its operand; it is not a navigation callback with arbitrary effects.
The caller pins immutable active-ID storage and code/context for the synchronous
call, and supplies permission/lifetime gates and nonblocking widget actions.
No callback, context, event or pointer is retained. No key is forwarded or
allocated by this method. Source callers ignore its return; our API reports
errno separately from widget operations rather than treating native r0 residue
as a handled result.

COUNT must describe a valid ARM vector length. Our0x1fffffff bound represents
that supported valid-vector domain, not a probed hardware limit. Excess count
returns EOVERFLOW before actions. A resulting cursor outside the vector returns
ERANGE before active-ID lookup, possibly after the source-ordered cursor write.
These bound source-invalid memory reads, not native parity acceptance for bad
trees. A key that repairs an initially invalid cursor before dereference is
still implemented, including uint32 wrap from ffffffff to zero on Down.

## Verification

**101,384** actual shipped ARM comparisons pass.101376 cover counts0..6 and
every valid cursor, eight key classes, signed/unsigned edge bit patterns for
mapped index/biggest-visible/rows, and backend geometry mutation during item
selection. Eight additional cases cover cursor repairs before lookup. Native
cursor stores are observed at the subsequent getItemIndex call and recorded as
logical state-write actions; source instructions/stores are not intercepted.
Widget getters/setters and key access are controlled stubs. Exact state-write/
item/row action order, arguments and final fixture state match. All earlier
reference groups still pass.

Strict full host suite, focused ASan/UBSan and ARM/qemu unit checks pass.
Isolated build-tv-v3/tab-policy unit28652 bytes, MD5
329ea6b5c147420d8b66e2ea4bd247b1; reference320444 bytes, MD5
b26527ce44cd218d558d24f0ca7da126. No new module/service/ABI, TV access/wake,
staging, deployment, actual widget rendering, settings change or hardware test.
The reference does not validate the real widget's response to every synthetic
signed/geometry edge operand, nor actual source vector construction/lifecycle.

## Next list boundary, already extracted

list-keys.txt preserves onRightLeftPress d508b8, onDigitPress d509a0 and the
SettingItemsListBox map d509f0. They remain **unimplemented**; do not repeat the
extraction or mistake these source notes for completed parity:

- The map routes20d/20c/20e/2da to its Right/Left body,200..209 to Digit, and
  other messages to qualified SVGListBoxBase::handleMessage.
- Digit gets the selected row and forwards the original key when present. Its
  return is **row presence**, independently of the child's handled result.
- Right/Left in vertical mode captures the selected row, delivers its key,
  re-reads selection to choose the backing record, reads the original row's
  selected value and stores it at the newly selected record+20. Pin those
  separate row/record lifetimes rather than silently changing this ordering.
- Horizontal mode uses OSD mirror to swap Left/Right via fresh events; other
  cases forward the original to SVGListBoxBase::onKeyPress. It consumes.

Continue with those concrete bodies and their lower SVGListBox/row behavior,
then wire the owned content/renderer/runtime context and queue consumer.
Parent refresh and independent native visible/modal/focus/permission/lifetime
acceptance remain open. The existing diagnostic UI has not been replaced.
