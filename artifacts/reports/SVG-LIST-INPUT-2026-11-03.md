# SVG-list routing and typed row values — 2026-11-03

Subsequent [SVG-LIST-BODIES](SVG-LIST-BODIES-2026-11-03.md) implements the seven
lower move/select/page/first-last bodies called by this router. Primitive
move/getter/editing implementations, parent refresh and the owned backend/
consumer remain open.

`common/svg_list_input.c` implements SVGListBoxBase::onKeyPress/handleMessage
and SettingItemsListBoxRow::getSelectedItemValue dispatch through owned callbacks.
Actual movement/select/page methods, primitive widget getters/editing, parent
refresh and the owned renderer/runtime consumer remain open. The owner requested
finishing this item, updating the handoff, committing/pushing and stopping.

## Firmware contract

Source already exists in analysis/settings-list-2026-11-03/lower-list.txt.
Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | method |
|---|---:|---|---|
| 10bcf4c | 276 | 5d13e755 | SVGListBoxBase::onKeyPress |
| 10bd060 | 84 | b2f5e4c8 | SVGListBoxBase::handleMessage |
| d53494 | 144 | e82eb084 | SettingItemsListBoxRow::getSelectedItemValue |

Only guard-pointer literals are redirected; the row's embedded branch table is
preserved. Framework/message and lower widget methods are controlled stubs.
No vendor object initialization or TV/device access is involved.

Select20e precedes every layout/page alias. Horizontal Right20d/Left20c select
Down/Up; vertical Down20b/Up20a select those operations instead. Then the widget's
page-down key at+228 has priority over page-up at+224. Only afterward do2db/2dc
select first/last group; unknown keys return unhandled without invoking a helper.
Aliases are full-width per-widget data: collisions, zero and ffffffff must not
be replaced with assumed global page keys. This layer does not perform mirror
translation; SETTINGS-LIST already owns that preceding conversion.

First/last receive the exact original event (ARM explicitly supplies r1), as
does qualified UIWindow::handleMessage fallback. Select/up/down/page helpers
have no message operand. Their result truth propagates. The duplicate2db key
read in the native router is equivalent to one comparison under the C API's
immutable decoded-key/pure-field contract; repeated virtual getter calls are
not emulated as observable side effects.

handleMessage uses full type1004 to call the key body once. A false key result
does **not** retry UIWindow fallback. Other message types directly invoke that
qualified fallback with the original event. errno and normalized handled0/1
are separate; invalid callbacks/output return EINVAL without callback/output
mutation. Callers pin immutable context/code/event, supply permission/lifetime
gates and nonblocking helpers. No pointer is retained or hardware effect inferred.

The row type is the full word at+2dec. getSelectedItemValue chooses:

| hexadecimal type | value source |
|---|---|
| 2,3 | selection widget+1014: getSelectedItemId, not selected index |
| 9..c | cached raw word+2df8 |
| d..f | text-entry widget+18d8: getNumericContents |
| 16 | check widget+27a8: getChecked |
| 17..19 | cached raw word+2dfc |
| other | zero, no getter |

The API exposes these five sources explicitly and returns their raw32 bits.
It does not normalize a noncanonical synthetic check value, cast an ID to an
index, or reinterpret a numeric/cached value as text. Primitive widget getter
implementation, row construction and real editing behavior are separate gates.

## Verification and remaining work

**999,216** actual native key cases cover keys0..3ff plus full-width outliers,
three layout bytes, nine-by-nine page aliases including collisions with reserved
keys, and lower results0/1/255/ffffffff. **49,344** native message joins execute
both shipped routing bodies across four types/layouts/results and the key
corpus, checking helper choice, original/no-message operands and handled truth.
**68,640** row comparisons cover types0..ff plus four full-width outliers and
264 raw-value patterns, including signed/high-bit edges. Native primitive getter
choice and exact returned bits/cache offsets match. Source cache fields are
direct reads; the C backend exposes them as pure query operands, not extra native
widget getter calls. All prior reference groups remain required and pass.

Full strict host suite, focused ASan/UBSan and ARM/qemu unit checks pass.
Isolated build-tv-v3/tab-policy unit26824 bytes, MD5
9c01c0509bbedcf847a4ab550d2f3fc5; reference387540 bytes, MD5
3277a450de22e3949b2f19e1df75eee5. No new module/service/ABI, TV access/wake,
staging/deployment, keys/drawing/settings mutation or native visible acceptance.

After owner resumes, recover the lower movement/select/page/first/last bodies
called by the saved router, then primitive row getters/editing and parent
refresh. Compose the owned content/renderer/runtime context and queue consumer
with modal/focus/permission/lifetime and independent native visible acceptance.
Current callback routing is not proof of actual widget effects; the diagnostic
UI remains unchanged. No additional hardware or implementation work is pending
in the background at this handoff.
