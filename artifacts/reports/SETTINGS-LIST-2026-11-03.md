# Settings list dispatch, row delivery and writeback — 2026-11-03

Subsequent [SVG-LIST-INPUT](SVG-LIST-INPUT-2026-11-03.md) implements the three
lower routing/value-dispatch methods called unimplemented in this report's
dated next steps. Actual lower actions/getters/editing/backend/UI remain open.

`common/settings_list_input.c` implements SettingItemsListBox's concrete map
and Right/Left and Digit bodies. Lower SVG-list navigation, typed row editing/
values, parent refresh and the owned display/runtime consumer remain open.

## Firmware contract

Source was already saved in analysis/settings-navigation-2026-11-03/list-keys.txt.
Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | method |
|---|---:|---|---|
| d508b8 | 232 | de92e067 | SettingItemsListBox::onRightLeftPress |
| d509a0 | 80 | 03344216 | SettingItemsListBox::onDigitPress |
| d509f0 | 328 | 472aef94 | SettingItemsListBox::handleMessage |

Only guard-pointer literals are redirected. The map executes the real shipped
matcher. Full type1004 is required:20d/20c/**20e**/2da route Right/Left,200..209
route Digit. Other IDs/types delegate to qualified SVGListBoxBase::handleMessage.
All map callbacks forward the original event and propagate handled truth. Select
20e does not get a separate row-dispatch shortcut; unknown events aren't consumed.

Digit obtains the selected row and forwards its original event if present. Its
return is **row presence**, ignoring the child's handled result. Right/Left
always consumes, even with no row or a zero lower return.

Nonhorizontal Right/Left captures the selected row, delivers the original key,
then re-reads selection and resolves a backing SettingItem record through the
vector at+28c. It captures that record, reads getSelectedItemValue from the
**original row**, and writes raw32 bits to the captured record+20. A changed
selection need not write to the initially selected record. Preserve this source
ordering rather than assuming a fixed row-to-record association.

Horizontal Right/Left forwards the original event to qualified
SVGListBoxBase::onKeyPress when mirror is false. When true,20c/20d swap through
fresh events; other keys retain the original. This is different from the parent
SettingsDialog's Left/Right-to-Up/Down conversion. NULL marks a synthesized
event in our API; the backend must construct it from the supplied full key.

ROW/RECORD queries use opaque32-bit handles,0 meaning missing, not vendor
pointers. ROW_VALUE takes the captured row; STORE_VALUE takes the already
captured record and raw value. Queries are pure current-state reads. Actions
may change selection/record mappings before later queries. Pin both row and
record lifetimes plus code/context/message during this synchronous call, gate
permission/lifecycle and use nonblocking actions. Nothing is retained afterward;
callback invocation proves no actual UI/hardware effect.

EINVAL for invalid callbacks/output/kind runs no callback or output mutation.
A missing backing record after row delivery returns ENODATA, retaining prior
actions and leaving handled unchanged. This guards a source-invalid dereference,
not native bad-tree parity or evidence of a reachable vendor/hardware fault.

## Verification and bounds

**65,600** shipped map/matcher comparisons match exact route, original identity
and result truth across full-width types/IDs and lower results0/1/255/ffffffff.
**138,240** actual body comparisons vary layout/mirror0/1/255, selected/missing
row, selection/record-map changes during delivery, child returns, raw row values
and ten key classes. Original-row value changes during delivery; post-key record
mapping can reverse. These distinguish target and timing errors in writeback.

Direct native record stores are observed after return as logical STORE_VALUE
actions. Corpus values differ from all initial record sentinels, making writes
observable. Message constructors/destructor and row/SVG helpers are controlled
stubs; no vendor UI/framework initialization runs. Real event construction,
row editing/typed extraction and SVG navigation/rendering remain unverified by
this corpus. Missing-record safety is tested only against our C contract.

Full strict host suite, focused ASan/UBSan, ARM/qemu unit and all prior reference
groups pass. Isolated build-tv-v3/tab-policy unit30512 bytes, MD5
175c832887165a55aa210d4e0dc33742; reference358336 bytes, MD5
3ac22cad43a7e8901ad147801d5e296b. No new module/service/ABI, TV access/wake,
staging/deployment, actual keys/drawing/settings write or hardware acceptance.

Next source is already saved in analysis/settings-list-2026-11-03/lower-list.txt:
row getSelectedItemValue d53494, SVGListBoxBase::onKeyPress10bcf4c and
handleMessage10bd060. These three remain **unimplemented**. Row values depend on
type (selection/numeric/check widgets or cached fields), not a universal selected
index. Base key routing uses page aliases at+224/+228 and layout; lower movement/
select/page methods still need investigation. Do not guess fixed page keys or
conflate this router with the active-ID navigation algorithm.

Then compose owned content/renderer/runtime context and the queue consumer.
Parent refresh and native visible/modal/focus/permission/lifetime gates remain
open. The diagnostic UI is unchanged.
