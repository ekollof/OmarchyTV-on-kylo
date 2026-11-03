# Base settings dispatch and input bodies — 2026-11-03

Subsequent [SETTINGS-NAVIGATION](SETTINGS-NAVIGATION-2026-11-03.md) implements
navigateInList and clarifies that its active vector contains item IDs, not
pointers. The real list widget, parent refresh and owned backend/UI remain open.

`common/settings_input.c` implements the concrete qualified SettingsDialog
message map plus its Up/Down, Select, Digit, Left/Right and Mute bodies. The
list widget, navigateInList algorithm, actual UString/layout, mute/audio effects,
parent refresh handling and owned UI adapter remain separate work. These are
method-body callbacks, not a wired or visibly accepted settings controller.

## Source and map

Shipped Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. The base map is already
in analysis/tab-leaf-2026-11-03/base.txt. New read-only handlers.txt in
analysis/settings-input-2026-11-03 supplies the five bodies. The native reference
executes those fragments, redirecting only guard-pointer literals:

| address | bytes | CRC32 | source |
|---|---:|---|---|
| d57238 | 644 | 8c5f1f20 | SettingsDialog::handleMessage |
| d56b5c | 84 | a0f13041 | onUpDownPress |
| d56bb0 | 68 | 586acb19 | onSelectPress |
| d56bf4 | 160 | f8fa89c9 | onDigitPress |
| d570bc | 380 | 244af783 | onRightLeftPress |
| d56d00 | 56 | 6b63d6ad | onMuteKey |

The map executes the same real96-byte matcher as TAB-DISPATCH; no wildcard
entries occur. All key routes require the full type1004. It routes:

| hexadecimal IDs | qualified target |
|---|---|
| 22d,251,25b,210,255 | SettingsDialog::onMenuPress |
| 20d,20c,2da | onRightLeftPress |
| 20a,20b,2db,2dc | onUpDownPress |
| 20e | onSelectPress |
| 21f,28b | onQuitAndIgnoreMessage |
| 222 | direct DemoApp::toggleAndDisplayMuteState |
| 200 through209 | onDigitPress |
| 230,24c | onGuidePress |
| other IDs/types | AnyUIRefreshMessageFullScreenDialog::handleMessage |

Do not reuse TabDialog's map:255 versus2bb, Select,2db/2dc and the base target
differ. This base map propagates handler-result truth, including Menu. The
outer TabDialog map instead forces Menu consumption. Mute captures the successful
matcher result in r5, calls DemoApp, then returns that captured value regardless
of the mute call's return. Thus Mute always consumes and carries no original
message operand. Other callbacks forward the original borrowed event. The C
API separates errno from normalized handled0/1, preserving those distinctions.

## Ordered input bodies

Up/Down uses navigateInList when the active item-ID vector is nonempty; otherwise
it delivers the original event to the list component at this+17c. It then calls
updateDisplay regardless of the lower result. Select delivers the original
event to that list and then invokes primary virtual+84. Mute's auxiliary method
calls DemoApp directly and consumes; the map itself bypasses this wrapper.

Digit first delivers the original list event, then obtains the **current** selected
ID and maps it to a signed item index. A nonnegative index with a nonnull item
may update the description at this+120. Item description+0c is compared by
pointer identity to the literal token at14d4d52, whose bytes are `BLANK\0`.
This is neither a null check nor text equality. If it is a different pointer,
the method assigns its C string. It then tests whether the resulting/current
description is empty; only then does it assign the item's UString label at+4.
Sentinel plus nonempty prior description therefore preserves the old text.
GeneralDialog::addMenuNavigationInfo and virtual+84 follow even if no item was
found. The captured item index stays fixed across assignment callbacks.

Left/Right in a nonhorizontal list follows that same description-update path.
Horizontal mode with an empty active vector sends the original list event and
updates display. With a nonempty vector it reads OSD horizontal mirror state:
unmirrored Right becomes Down and Left becomes Up; mirrored reverses that
mapping.2db/2dc are forwarded as fresh events unchanged, other keys navigate
nowhere. updateDisplay follows in every horizontal case. Translated navigation
events carry NULL in our API and require fresh backend events; ordinary Up/Down
navigateInList calls retain the original borrowed event instead.

verify-metadata.cjs checks the source hash, both BLANK-token literals and
primary+84 slots. SettingsDialog points to6b14ac (`__cxa_pure_virtual@plt` in
the objdump cache), while TabDialog points to d6eb09, updateSettings. An adapter
must implement the correct concrete virtual callback, not instantiate an
abstract base or replace it with updateDisplay. Backend metadata must represent
the known BLANK token explicitly, rather than carrying vendor pointers across
reloads or treating every string with equal text as that token.

## Verification and limits

**65,600** shipped map/matcher cases compare exact route, event identity and
handled truth across full-width types/IDs and lower returns0/1/255/ffffffff.
**86,400** actual five-body comparisons vary active count, horizontal/mirror
0/1/255, negative/nonnegative indices, missing/present items, token identity,
text emptiness, state changes during callbacks and ten key classes. The alternate
description pointer contains the same `BLANK` text, deliberately verifying that
text equality does not select the sentinel path. Selected ID changes after list
dispatch; description emptiness changes after assignment. Logical action order,
arguments, original versus synthetic message identity and final fixture state
match. Native methods execute with controlled list/string/navigation/display/
mute helpers; those helpers' implementations are not proven by this corpus.

Full strict host suite, focused ASan/UBSan, ARM/qemu unit test, metadata verifier
and every earlier firmware group pass. Isolated build-tv-v3/tab-policy unit33244
bytes, MD5 ad1ad9e636556ead378bc63c3afbcf02; reference293592 bytes, MD5
4f6b30beff52c1ef1c70f23a79160beb. No new module/service/ABI, native deployment,
TV access/wake, key/draw, mute/setting change or hardware acceptance occurred.

Next recover/implement navigateInList at d56a24 and the list component's key
behavior, then compose owned content/renderer/runtime context and the queue
consumer. Parent refresh handling remains a real fallback, not unconditional
consumption. Keep permission/modal/focus/lifetime and independent visible gates.
