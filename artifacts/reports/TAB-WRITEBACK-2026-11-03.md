# Retained settings setters, reload migration and content lifecycle correction

ui.navigation v2 adds independent retained tab/item/submenu setters, including
CLI/Unix IPC access. Existing v1 methods and their prefix remain available.
OAN2 snapshots preserve raw state; prior OAN1 snapshots still load. This closes
the missing mutation mechanism, not the whole TabDialog lifecycle or UI consumer.

## Shipped source contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Sequential read-only Ghidra/ARM
evidence is retained in `../analysis/tab-writeback-2026-11-03/`:

| DemoApp setter | ARM entry | actual store |
|---|---|---|
| setTabDialogLastSelectedIndex | cccaac | raw32 bits at+fd8 |
| setSettingsListMenuLastSelectedItem | ccca44 | raw32 bits at+fd0 |
| setOpenSubMenuOfTabDialog | ccca0c | low byte at+1259 |

These setters neither validate widget IDs, enqueue navigation nor focus/update
an existing list. Normal boolean callers use0/1; the byte store itself preserves
other byte values. Consumers must distinguish stored tab bits from valid IDs0..4
and item/result IDs from visible row indices. TabDialog::setSelectedItem d6ebe8
is **another** setter: it writes that dialog's local requested tab+4a8, not
DemoApp's retained item or UIGroup's creation-order selected index.

Fresh setter-callers.txt contains33/70/29 call references respectively for
open/item/tab. It is not proof of all possible indirect calls or field stores.
The inspected TabDialog teardown has no unconditional retained-tab writeback.
The lifecycle is layered, as lifecycle.txt/content-writeback.txt show:

- onUpDownPress d6f874 checks the retained open flag. If nonzero, it sets item0,
  calls SettingsDialog::updateActiveList, then clears the open flag, in that order.
  Active tab-buttons plus Down can show/focus content. That side-effect sequence
  cannot be replaced by a single generic reset before the list update.
- hideSubMenu d6f09c requires nonnull **showing** content, calls its hide method,
  then its deleting destructor, clears the pointer and restores repeat/tab state.
- destroy d6f8f4 handles any nonnull content. It saves the pre-hide open flag,
  hides/deletes content, clears the pointer, then restores the saved open flag
  only when getReturnToGlobalSearchDialog is nonzero. Base destruction and mouse
  activation follow. This differs from hideSubMenu's showing guard.
- Picture/Sound content destruction clears the open flag. Their excluded
  setting-change logging/posting code is not ported. A generic “save current
  tab/item/open on every close” would conflate these paths.

These findings constrain the pending consumer; the new service does not silently
perform any of those modal/show/hide/update operations when a setter is called.

## Corrected content-interface interpretation

content-interface.txt proves PictureSettingsDialog's ctor literal14e87a4 plus
d4 gives secondary vptr14e8878 at object+4a0. The actual slots are:

| offset | method (through adjustment thunk) |
|---|---|
| 00/04 | destructor / deleting destructor |
| 08/0c | show / hide |
| 10/14 | setFocus / changeVisibility |
| 18/1c | isShowing / isFocused |
| 20/24 | handleKeyPressMessage / updateTexts |
| 28/2c/30 | getFocusedItemIndex / getBiggestItemIndex / getSmallestActiveItemIndex |
| 34/38 | shouldHandleBackKey / setSelectedItem |

isShowing e07754 returns byte4c8; isFocused e07790 returns byte4c9. show e07a1c
invokes primary virtuals+4c/+50 and sets showing1/focus0. The saved primary table
names+4c as init and+50 as SVGFullScreenDialog::destroyComponents; do not label
the latter paint without inspecting its implementation. hide e07a80 calls
primary destroy when the window is created, then clears both flags. The primary
vtable confirms slot24 is PictureSettingsDialog::destroy e0a738.

**Correction to TAB-NAVIGATION and the older menu notes:** content+18 was wrongly
called a resource “validity” check; +8/+c are show/hide, not create/destroy.
showSubMenu calls show, then isShowing, and on false calls hideSubMenu. Since
hideSubMenu checks isShowing again, immediate deletion cannot be inferred from
that first false result. If it remains false, hideSubMenu does nothing; full
TabDialog destruction later handles the nonnull pointer. This is source behavior,
not a newly observed live failure, global leak diagnosis or hardware API rule.
Installation password and network permission gates remain separate checks.

getFocusedItemIndex returns-1 when content is unfocused; otherwise it converts
the selected item ID into an index. The extrema methods inspect actual list
items/active state. These are now named inputs for the later key controller,
not invented constants or proof of full native list behavior.

## Service, control and snapshot behavior

oa_ui_navigation_iface_v2 has the unchanged v1 interface as its first member,
followed by set_tab(int32), set_item(int32), set_open_submenu(uint8). Version2 is
published at the v1 prefix address. lookup(min_version1) still supports all four
existing calls; new setters require version2. Setters change one retained field
only, even when the queue is full. Paired standby returns EBUSY; unavailable
providers return ENODEV before removal fan-out. Normal holds still pin the DSO.

`nav-set-tab <signed32>`, `nav-set-item <signed32>` and
`nav-set-submenu <0..255>` return ok and do not enqueue/draw. Existing state/take
text keeps unsigned raw tab bits and signed item output. Missing/extra/force,
overflow, signed-byte and nondecimal arguments reject before mutation. A v1-only
provider reports ENOTSUP for new commands without dereferencing a v2 tail.
Host shutdown rejects control mutations with ESHUTDOWN.

OAN2 retains the16+4*N/max80-byte explicit little-endian layout, but permits
all32 stored tab/item bits and all256 submenu bytes. Count/reserved/route IDs,
exact length and version still validate atomically. OAN1 migration retains its
original tab0..4/open0..1 restrictions. New writes use OAN2; older module binaries
cannot restore this newer schema. No native structs are serialized, no queue
requests are silently discarded, and setters do not consume queue capacity.

## Verification and remaining work

The optional offline ARM reference now executes the three shipped setter
fragments unchanged except stack-guard pointer literals:56/52/52 bytes, CRC32
6fea0252/5e86d1a0/6b0e9511. All1,792 boundary signed32 ×raw-byte cases preserve
exact fields and surrounding bytes. That proves native store semantics; separate
real-DSO tests validate the service and1,792 OAN2 raw-state round-trips.
Prior884736 availability/9216 selection/65281 hash/27 lookup corpora still pass.

Full strict host suite, focused ASan/UBSan, ARM/qemu DSO tests, actual Unix-socket
commands and the23-source-module host loader preflight pass. Invalid/legacy
snapshot, v1-only peer, full queue, shutdown/suspend, consumer holds/removal,
pending-state reload and deferred publication/retry are covered. No TV access,
wake, staging, deployment, keys, drawing or persistent/recovery mutation occurred.

Isolated build-tv-v3/tab-policy native module28936 bytes/MD5
4e0364e286f500720dad7d917422190e/maxGLIBC2.4; DSO test492940/
d686e6827c8b088e289c56ae9dce3929; optional reference75988/
23314a4b7f20806daebfcacecd32424c. These are development builds, not target
loader/startup/visible acceptance. No helper/build/Ghidra job remains pending.

Next integrate actual per-event state changes with content/menu handling and
held profile facts. Mutable hotel/media/app/entry state, per-tab selection,
permission checks, full focus/key/modal controller and visible UI acceptance
remain open. There is still no automatic queue consumer or generic close action.
