# Setting-list parent draw dispatcher — 2026-11-03

`common/settings_draw.c` implements the complete
SettingItemsListBox::drawRow control flow through typed row events and explicit
owned text/value-copy providers. It is the parent virtual+0x60 reached by the
previous single-row refresh milestone. No new module, service or display
ownership is introduced. Lower widgets and actual resource providers remain
pending, so this is not visible rendering acceptance.

## Firmware and contract

Source Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only decompile and
ARM instructions: `analysis/svg-list-refresh-2026-11-03/parent-source.txt`.
The offline reference copies2448 bytes from d50f60, CRC32 `c91978ad`, preserving
the embedded branch table and inverse-three literal `aaaaaaab`. Only the
guard pointer, empty-string pointer and fixed resource name pointer are
relocated. The source literals are independently checked: empty at12f5cda,
resource name **OK** at151c06c. It is a lookup key, not an invented label.

The signed item count/index guard precedes any row action. Valid entries set
the row ID, then choose progress/mixed mode. Progress mode converts backing
types9/10/11 to23/24/25; mixed mode converts only9 to23. The conversion
**writes the backing item type**, not just a local variable. Existing progress
types receive the ball flag. Buttons4/8/26 compute long-button state from the
first value text, or the empty/default OK lookup, with an explicit temporary.
Types11/25 set endpoint image IDs before the row type and raw active byte.

Types5/6 set copied button text, hide the image and mark the row selectable.
They choose additional glyph, service logo, full image details, glyph path,
image URL or numeric glyph in the original priority order. Type7 has its
separate image-only path and does not set the ordinary label. Other types
copy/set/free the label before their value-specific path:

| type | lower actions |
|---|---|
| 2,3 | clear items, copy values, append each copied ID/text, select ID;3 also sets selection image |
| 4,8,26 | first value/default text, copy and set button text |
| 9,11 | range then slide value |
| 10,12 | range/slide, copied endpoint texts or formatted numeric limits |
| 13 | numeric text value |
| 16..21 | copied first value to text/date/time/duration/password/IP widget |
| 22 | checked byte only when the original value vector is nonempty |
| 23,25 | range then progress value |
| 24 | range/progress, copied endpoint texts or formatted numeric limits |
| 27 | clear selectable flag |
| other | no additional value action |

The endpoint fallback uses four owned strings: construct/append minimum,
construct/append maximum, copy both for the by-value setter, then destroy in
reverse order before the value snapshot. The two-value path uses two copies.
Selection and text-entry paths also release string temporaries before dropping
the vector snapshot. Borrowed item/value/image handles are never destroyed.

The public API uses pinned decoded model objects and opaque text/image handles;
it does not reproduce vendor object layout or retain events. All callbacks
receive context. Row actions and infallible text operations must be synchronous;
store_type updates both the decoded item and its backing provider. The provider
owns a distinct immutable values_copy snapshot through values_drop, even when
empty. Missing callbacks/row/style return EINVAL before callbacks; missing
valid-index items return ENODATA. Constructor NULL or vector-copy errno unwinds
owned temporaries while retaining earlier actions/type writeback. These guards
and allocation-failure returns are outside native valid-tree success parity.

## Evidence and limits

**18,441** shipped-ARM/C comparisons pass:36 full-width types,16 style flag
combinations,32 value/image/string variants, plus9 empty/signed-index exits.
The comparison checks complete setter/string/vector traces, copied IDs/text
tokens, image handle identity, final backing type and untouched item bytes.
The C fixture poisons freed objects, rejects borrowed destruction and leaked or
misordered ownership, and deep-copies its bounded value snapshot. The native
primitive fixtures track stack-object ownership separately from content.

String/vector/resource/lower-widget implementations are controlled on both
sides. This proves the original dispatcher ordering and operands, not full
UString behavior, integer formatting, native vector allocation, resource
lookup, widget pixels or hardware effects. No constructor/platform/HAL init
is run. Cases use bounded synthetic valid trees, not the TV's live settings.

Host tests add2048 type/style/value cases and staged constructor/vector-copy
failure checks, including progress type writeback retained after allocation
failure. Strict full host suite and actual Unix IPC pass; focused ASan/UBSan
and fresh ARM/qemu units pass. Isolated build `build-tv-v3/settings-draw`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_draw | 69108 | de6663fcf9ab09417d3da564a7540840 |
| tests/tab_policy_firmware | 865972 | 3475e6122271bb8f53986e0c078d7b22 |

No TV access/wake/staging/deployment, keys, settings/network/boot mutation or
hardware ownership change. No pending helper/job. Next compose owned settings
items, values/text providers and real row widgets with this dispatcher and the
existing refresh/input chain, then display/scanout/startup and independent native
visible acceptance. Scrollbar construction/rendering internals remain open.
