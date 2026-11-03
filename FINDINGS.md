# Current findings and correction map

Initial offline audit:2026-10-31. Subsequent dated reports add implementation
and TV evidence, including UI-SCENE-REVIEW on2026-11-01. This guides conclusions;
topic reports retain detailed provenance. See [report index](artifacts/reports/README.md).

## Evidence rules

SETTINGS-ROW-TEXTENTRY closes the text-entry family: one parameterised body
serves 0xd/0xe/0xf/0x10 with names 1/0/3/2 from 0x2ebc/0x2ec0/0x2ec4/0x2ec8
and an empty-label create; trailing setters differ (mode/max 0/4, 0xa/5,
0xa/5, 1/10) and the created source is setNumericContents(row[0x2e00]) for
0xd/0xe/0xf versus setText(row[0x2e04]) for 0x10. 33608 native/C cases pass.
The differential caught an un-stubbed SVGTextEntryBox::setText (0x10b8160)
that let the guest slide through zero halfwords into a neighbouring stub;
a callee audit now covers all twelve call sites. Cases 5..12, 0x11..0x15 and
0x17..0x19, construction, child-visibility internals, live resources,
scrollbar/skin/visible gates remain open.

SETTINGS-ROW-13 implements renderItems type 0xd (text-entry box): looks up
"selectionbox", drives row+0x18d8, writes row[0x2ebc]/[0x2ec0]/[0x2ec4]/[0x2ec8]
into slots 1/0/3/2, creates from an empty label with (0,0xc,0xd), then sets
entry mode0, max chars4 and automatic cursor; created adds
setNumericContents(row[0x2e00]). 24392 native/C cases pass. Two harness gaps
were the per-case key literal (0xd55140) and the empty-label literal
(0xd55144), both needing owned redirects. Cases 5..12, 0xe..0x15 and
0x17..0x19, construction, child-visibility internals, live resources,
scrollbar/skin/visible gates remain open.

SETTINGS-ROW-SELECTION implements renderItems types 0x2/0x3: both look up
"selectionbox" and drive row+0x1014; case 0x3 additionally calls the row's
setSelectionBoxImageId with row[0x2e54], and the differential proved that call
comes AFTER the lookup and BEFORE the null check. Uncreated names use slot
order 1/3/0/2 (0x26b+font when case2 has key1, else
0x2e9c/0x2ea0/0x2ea4/0x2ea8) — distinct from the shared tail's 1/0/3/2 —
and create from an empty label; created adds setSelectedItemWithId(row[0x2de8]).
21320 native/C cases pass. setSelectionBoxImageId internals (0xd54374) remain
a controlled stub. Cases 5..21 and 23..25, construction, child-visibility
internals, live resources, scrollbar/skin/visible gates remain open.

SETTINGS-ROW-16 implements renderItems type 0x16: getRectangle("checkbox"),
isCreated(row+0x27a8), then names 9/10/11/12 into slots 1/0/3/2, alignment 5,
a seven-argument empty-label create and setChecked(row byte 0x2e44), or
EASIRect math + setWindowRect/setText(empty) when created. 7592 native/C
cases pass. Two harness fidelity fixes were required: row label slots are
UString objects (first word is the char*) and must be dereferenced
everywhere, and the checkbox create is a seven-argument call needing the
wide preserving trampoline. The five-key presence grid replaces the four-key
one. Cases 2..25 except 0x16, construction, child-visibility internals, live
resources, scrollbar/skin/visible gates remain open.

SETTINGS-ROW-1A implements renderItems type 0x1a: the shared 0x104 tail's
0x1a sub-branch writes 0x2e8c into all four name slots and skips setFont,
then the case body looks up "button" and drives the 0x684 button with
0x2ed0/0x2ecc/0x2ed8/0x2ed4 (create) or EASIRect math + setWindowRect/
setText (created). 3032 native/C cases pass. The case body resolves "button"
through its own literal (0xd54e24, second level 0x178d5e8) which must be
redirected to the owned string or the body faults on vendor rodata. A null
key2rect sends the switch straight to the case body with no tail. Switch
cases 2..25, construction, child-visibility internals, live resources,
scrollbar/skin/visible gates remain open.

SETTINGS-ROW-RENDER implements renderItems for types 0, 1 and 4 plus the
0x1b/default exits: prologue base render with three rectangle lookups, the
shared 0x104 tail for every non-4 type with a key2 rect (type 1 uses
row+0x3c4), type-4 button block with key4 lookup, and a switch whose table
entry for type 4 was read from the ELF. 2264 native/C cases pass. The tail's
create rejoins the switch; the button create passes key4rect directly (the
fold temp is dead); the rebuild flag_store slot is key2rect, replaced by
key3rect on the reconstruct path, and null with the flag set faults
natively (excluded). Saved Ghidra ASM had phantom instructions past the
switch head; objdump is the source of truth. Switch cases 2..25, row
construction, child visibility internals, live resources, scrollbar/skin/
visible gates remain open.

SETTINGS-ROW-SETTERS implements the saved label/progress/range bodies: label
selects +0x3c4 for type 1 else +0x104; progress drives +0x1570 for 0x17 else
+0x1724 with wrapped ((value-lo)*100)/(hi-lo) and always formats the stored
value into +0x24e8; range drives +0x13c0 for type 9 else +0x1498. Widget calls
run only when created; stores always run. 324 native/C cases pass. Fragments
must include the final pop; the progress format literal points at the shipped
empty string. __aeabi_idiv edges (INT_MAX/MIN on nonzero/zero, SIGFPE on 0/0)
are replicated without UB. Case bodies 2..26, construction/visibility-
internals/live resources/scrollbar/skin/visible gates remain open.

SETTINGS-ROW-TYPE implements setType: changed cache/rebuild stores precede
renderItems, unchanged clears rebuild but still queries visibility, and the
query follows rendering.12480 native cases pass. Exact qualified child calls
matter: type24 omits1ca8, unknown types only update two label children. Do not
substitute hide-all or a generic visibility mask. Saved actual renderItems reads
the flag, so no invented post-render dirty clear. Host/sanitizer/ARM-qemu/TV CPU
join to owned model/cache passes; actual widgets/rendering/live resources open.

SETTINGS-MODEL replaces placeholder text/vector providers for an explicit owned
decoded snapshot: all item strings/value texts/image bytes/default localization
are copied, temporary capacities avoid hidden assignment/append allocation
failure, and copied values have distinct lifetimes.2048 real-provider calls plus
fault/reentry checks pass host/sanitizer/ARM-qemu/real-TV CPU. Row sink remains a
recorder, not a widget renderer. Firmware operator<< uses %d; empty/equality
results must be read from ARM clz/shift, not void-looking metadata. Full UString,
live DB/resource/module/widget/rendering/startup/visible gates remain open.

SETTINGS-DRAW implements the parent dispatcher previously saved by SVG-LIST-
REFRESH:18441 native/C cases and prior groups pass. Progress/mixed modes mutate
backing item types, not only local row type; image/button/selection/text/slider/
checkbox paths and temporary string/vector order are explicit. Empty value-list
buttons use resource key OK unless no-default; numeric endpoint fallback builds
and copies separate min/max strings. Actual provider/formatting/widget/rendering
behavior remains unproven; controlled dispatcher traces do not close those gates.

SVG-LIST-REFRESH implements single-row base/manager refresh and row setChecked;
2880+24 native/C comparisons pass. Refresh uses total+8 capped by allocated
slots, not visible size+0x10; unsigned first+count can wrap, unlike a relative
bounds test. Parent+0x60 is SettingItemsListBox::drawRow, now SETTINGS-DRAW.
Uncreated checkboxes do not update the row cache; created ones set then store.
An additional row-view test bug formed out-of-bounds rv_slots[i*8] pointers;
corrected indexing and all prior reference groups pass. This is not a proven
historical crash cause. Concrete parent/provider/rendering gates remain open.

ARM-HARNESS-REVIEW supersedes the14-case animation acceptance below. Owned opaque
strings/context, character-bounded slicing (NULL names are empty), parsing before
destruction, comparison operand order and signed container counts now pass153
native cases. Helpers are controlled valid-UTF8 fixtures, not full UString proof.
Scrollbar stub10c3f50 previously wrote through the wrong mapping; installers now
check allocation bounds. OAL1 payload checks previously hid behind publication;
unpublished restore now rejects invalid geometry/handled values atomically. Text
temporary is max-aligned. Old SIGSEGV has no retained core and didn't reproduce
in the reviewed starting binary; don't claim a historical cause or TV fault.
Use working host ARM GDB; bundled GDB's ncurses5 dependency is absent.

SVG list animation: SVG-LIST-ANIM implements startSelectAnimation gates,
rect snapshot and first-match animation start; 14 native/C comparisons
pass. Disabled/missing-row/unchanged-rect exits, the +0x24c/+0x25c
archive/store, the +12 name/+28 duration fields and atof*1000.0f float bits
are asserted. UString-deref, atof-PLT and ARM-trampoline slips were caught
and fixed. Parent refresh, scrollbar construction/rendering,
backend/visible gates remain open.

SVG list slide: SVG-LIST-SLIDE implements setSlideValue progress/slider/text
routing; 72 native/C comparisons pass. Two flag bytes and the type word
select progress vs slider paths; +0x1498/+0x13c0 bar, +0x1ca8 box, empty
UString format and +0x2df8 store are asserted; progress paths store nothing.
Page-size, stub-convention and stale-store slips were caught and fixed.
Parent refresh, scrollbar construction/rendering,
backend/visible gates remain open.

SVG list widgets: SVG-LIST-WIDGETS implements the numeric forward and the
selected-id gates/walk; 21 native/C comparisons pass. The +0x28c cell holds
the array address (two loads, not one); the +0x18d8 row offset is pure
arithmetic. Row editing (slide/progress/text paths), parent refresh,
scrollbar construction/rendering, backend/visible
gates remain open (animation closed by SVG-LIST-ANIM).

SVG list module: SVG-LIST-MODULE publishes ui.list v1 as the first consumer;
host/sanitizer/ARM-qemu/actual-Unix-IPC pass. Owned 9/5/16 geometry with
page aliases; verified chain with inert render counters; list-state/list-key
over socket/CLI; OAL1 reload persistence; suspend/shutdown gates; clean exit.
Widget getters/editing, parent refresh, scrollbar construction/rendering,
display/scanout/startup and visible parity remain open (animation closed by SVG-LIST-ANIM).

SVG list update: SVG-LIST-UPDATE implements the row-view getters
(total/last/item) and the full refresh; 100+20 native/C comparisons pass.
Total is slots, last wraps, item bounds-checks; update stores the virtual
total then refreshes window/scrollbar/animation in order. Entry-point calls
in this harness go through a volatile pointer (direct memcpy-init calls
miscompile at -O2). Widget getters/editing/parent/construction/animation/
backend/visible gates remain open.

SVG list scrollbar: SVG-LIST-SCROLLBAR implements create (thin forward) and
setScrollPosition (gate/store/show); 294 native/C comparisons pass. Create
forwards unchanged; setScrollPosition stores only when created and signed
first stays within signed vis, draws the background only with a clear +0xc8
byte, then draws the bar. A page-mapping slip (stub outside the mapped page)
and a fragment-length slip (line count reused as byte count) were caught and
fixed. Construction/rendering/backend/visible gates remain open.

SVG list impl: SVG-LIST-IMPL implements updateImpl and rotateRows; 6952
native/C comparisons pass. Rows +0x1C/+0x28/+0x2C/+0x34 are setFocus/(sret)
getWindowRect/setWindowPosition/setVisible; owner +0x58/+0x5C/+0x60 are
displayMessageIfListEmpty/drawRowWithListEmptyMessage/drawRow. A prior
+0x28/+0x1C slot reading is corrected here: decimal-28 is 0x1C setFocus,
decimal-40 is 0x28 getWindowRect. Getters/editing/parent/construction/
animation/backend/visible gates remain open.

SVG list scrollbar: SVG-LIST-SCROLL implements updateScrollBar; 4320
native/C comparisons pass. Create iff nonzero size with signed size below
the signed +0x6c total or the +0x22c flag is set; rect from literal offsets
with wrapping arithmetic, shown iff the list is visible, then positioned;
otherwise a created bar is hidden. A harness stub defect that clobbered r3
is fixed; preserving installers verified. Construction/position/animation/
backend/visible gates remain open.

SVG list rows: SVG-LIST-ROWS implements the manager field layout, focusItem
and updatePartial; 324 native/C comparisons pass. +12 first, +16 count,
+20 selected-rel, +28 row array; rel=index-first wraps, returns on zero
count/rel-reach/rel-selected, otherwise row +28 (0 then 1) and rel store.
selectedItem is first+selected. Partial forwards (index,false). updateImpl,
row widgets, getters/editing, scrollbar/animation, backend/visible gates
remain open.

SVG list moves: SVG-LIST-MOVE implements the eight primitive move/selection/
window bodies; 5040 native/C comparisons pass with exact call order/args.
Window to-top posts partial 0, to-bottom posts aux-size unchecked; up/down
shift first-visible by n with no animation, selection up/down focus
selected-item by n, of-window variants focus first-visible edges. Move/window/
animation target the list box; only manager queries and focusItem target
this+0xf8. Void bodies: errno only. Move targets/RowManager/getters/editing/
backend/visible gates remain open.

SVG list bodies: SVG-LIST-BODIES implements the seven lower key bodies;
26048 native/C comparisons pass with exact call order/args and handled truth.
Unhandled on zero +0x1ec count or zero size with no callbacks. Pure size reads
repeat as shipped. Page-down wraps past the signed last-page start and scrolls
otherwise (the native run corrected an inverted first implementation). Move/
window/animation run on the list box; only manager queries and focusItem run on
this+0xf8. Select consumes with or without a parent; first/last synthesize
group indices 0/count-1. Primitive implementations/getters/editing/backend/
visible gates remain open.

SVG list input: SVG-LIST-INPUT implements key/message/typed-value routing;
999216 shipped key cases,49344 message joins and68640 row-value cases pass.
Select/layout movement precedes page-down, page-up, then first/last; full-width
aliases can collide. Only first/last/window take the original message. A false
key result doesn't retry Window. Row type chooses selection ID/numeric/check
getter or raw+2df8/+2dfc cache; unknown full-width types return0 without a getter.
SVG-LIST-BODIES implements the seven lower bodies; primitive implementations/
getters/editing/backend/visible gates remain open.

Settings list: SETTINGS-LIST implements map and Right/Left/Digit bodies;65600
native map/matcher and138240 body cases pass. Select20e routes Right/Left. Digit
returns row presence independently of child result. Vertical writeback captures
row, dispatches key, resolves the record selected afterward, reads original-row
value and stores it in that record. Pin both lifetimes. Horizontal mirror swaps
fresh Left/Right events; don't reuse the parent's Up/Down conversion. Missing-
record guard isn't native bad-tree parity. SVG-LIST-INPUT later adds the saved
lower routing/value dispatch; actual lower actions/getters/editing/backend/
visible gates remain open.

Settings navigation: SETTINGS-NAVIGATION implements d56a24;101384 native ARM
logical-write/action/state matches plus strict host/sanitizer/ARM and prior
groups. Active vector holds IDs passed to getItemIndex(int), not SettingItem
pointers or widget indices. Cursor wrap/first-last jumps precede lookup; viewport
comparisons are signed32 with wrapping arithmetic. Captured rows survive setter
changes. Invalid-count/cursor errors are explicit non-parity bounds. SETTINGS-LIST
later implements the widget map/two bodies; typed rows/underlying SVG and real
backend/visible integration remain open.

Base settings input: SETTINGS-INPUT implements qualified map and five body
callbacks;65600 actual ARM map routes and86400 native method traces pass. Don't
reuse the Tab map:255 versus2bb, Select/paging/parent and menu truth differ.
Mute returns captured match truth, not DemoApp result. Description sentinel is
the pointer to14d4d52 BLANK, not null/text equality. Preserve prior nonempty text
when sentinel skips assignment; test empty after possible assignment. Horizontal
mirror translates fresh navigation events, ordinary Up/Down retains the original.
Primary+84 is concrete updateSettings (base pure virtual), not updateDisplay.
SETTINGS-NAVIGATION later implements navigation; real list/parent refresh and
owned backend/consumer remain open.

Tab/settings leaves: TAB-LEAF implements volume/digits/removal and qualified
base menu/quit/guide.238860 actual ARM cases include shipped signed division.
Volume tests content+18 isShowing, not a capability or focus flag. Digit raw32
wrap precedes signed remainder10000; store precedes direct SVG end at1104/4725.
That qualified end differs from the others' virtual end. Exit210 ends29a,
Menu22d/25b/251 resets picture selection then ends0. Guide pushes navigation0xb,
not a key; Quit/removal synthesize owned keys. No callback is a permission or
hardware-success proof. SETTINGS-INPUT subsequently adds the base map and five
input bodies; real list/navigation/backend/UI remain open.

Tab fallback dispatch: TAB-DISPATCH implements d6fe14's type1004 map and the
pre-first caller join.65600 shipped map/matcher routes and16384 caller joins
pass; the latter uses the separately verified C pre-handler as a virtual method.
Full-width types/IDs matter; no wildcard entries occur in this concrete map.
Menu22d/251/2bb/25b/210 calls qualified SettingsDialog::onMenuPress and consumes
regardless of its result. Extra2c9/2c8/2da route unchanged to Right/Left.
All callbacks receive the original borrowed event; pre errors/consumption skip
fallback. Remaining base/leaf methods and backend/consumer are not attached.

Outer pre-handler: TAB-PREHANDLE implements d700a0 via owned backend callbacks;
65664 valid-tree shipped ARM traces/state/handled truth match. Marker is
vertical_tabs. Vertical Right rechecks activation after focus and may delegate
to base (d703bc ->d70526); don't flatten it to unconditional consumption.
Focused Left/Right consumes independently of child result; Menu/25b can delegate
to base after side effects. Original event identity differs from synthesized/
posted owned keys. Native caller tests zero/nonzero, so API normalizes handled.
Missing required child returns ENODATA instead of a native invalid dereference;
that safety path is not native parity proof. TAB-DISPATCH subsequently implements
the fallback map; real backend/UI integration remains open.

Tab events: TAB-EVENTS implements native onUpDownPress/onRightLeftPress behind
ordered backend callbacks;1008 actual shipped method-body traces/state matches
pass under qemu. Up/Down updates the list before clearing open, then reads the
possibly changed activation/content state. Left/Right gates once then hide/group
key/title/show even if hide changes the gate. These methods are not the outer
pre-handler: do not route every raw key to them or infer successful focus from
an action. No UI consumer/native backend/rendering is silently attached.

Hotel context: HOTEL-CONTEXT maps the separate HMCSettings table and raw
mode byte+1d/OSD word+28/picture byte+2d. db_tables is BSS initialized by
NvramStorage; its +60 binding is verified from the constructor. settings.hotel
and hotel-state SELECT only, unlike the vendor initializer's schema sync.
Saved DB row0/1/0 is not a live cache poll. SQLite int conversion precedes byte/
word stores;345 actual shipped row-copy cases and real vendor-SQLite ARM tests
pass. Publication failure/stop no longer removes peers or leaves cached DB reads
available; closed writable connections report unwritable. Full defaults/cache/
writer/controller/hardware acceptance remains open; no TV access.

Content header: CONTENT-HEADER-OWNERSHIP resolves Picture show's primary+50:
destroyComponents clears only that content object's own+140..150 pointer fields.
It is not paint and does not target TabDialog's separate header. Source ownership
does not establish matched-frame execution or raster parity.

Retained mutation: TAB-WRITEBACK adds ui.navigation v2 independent setters,
preserving the v1 prefix; OAN2 keeps all stored tab/item bits/submenu bytes and
reads legacy OAN1. Setters do not enqueue/draw/update/focus or infer a close.
1792 shipped setter cases/1792 DSO snapshot round-trips pass alongside full host,
sanitizer/ARM/actual Unix IPC. Picture content vptr14e8878 proves+8 show/+c hide/
+18 isShowing/+1c isFocused; old “validity/instant destruction on failure” claims
are corrected at source. hideSubMenu checks showing; TabDialog destroy handles
nonnull content and conditionally restores saved open for global search. These
are separate lifecycle paths, still awaiting the actual UI consumer. No TV access.

Profile context: TAB-CONTEXT resolves DemoApp+314/slot3c to getMenuCapability,
whose byte9 becomes TabDialog37a0 (record at least11 bytes). Normal source is
swprofile Config/Connectivity:Connectivity, default0, enabled exactly when1;
hotel support is Config/HotelTv:HotelMode/default1, also exactly1. Null capability
object's menu fallback sets connectivity1; Wi-Fi is a different gate. Bounded
scalar reader and reloadable ui.profile supply copied immutable facts only.
65281 actual shipped hashes/27 saved-profile lookups, host/sanitizer/ARM DSO
lifecycle and23-module host loader pass; no TV access or consumer/native context
acceptance. Saved profile header size exceeds file; do not copy beyond actual
bytes or assume a complete profile-integrity check from numeric lookups.

Initial selection: TAB-SELECTION resolves missing/unavailable requested tabs to
the first created tab (UIGroup selected index0); requested raw IDs map to
creation-order indices. Empty group has no focused tab while initial content
tab remains0. Common helper matches shipped group functions in9216 qemu cases,
with stubbed vector/focus callbacks, not full native UI execution. Later key/
modal/writeback/runtime context and consumer integration remain open.

TabDialog prerequisites: TAB-NAVIGATION-2026-11-03 recovers hotel/profile/app/
entry-dependent availability and signed item>0/open-submenu focus request.
Predicate matches the actual shipped ARM function in884736 qemu cases; native
context acquisition/content validity/focus permission/writeback/UI remain open.
QuickMenu(false) and launcher(true) entries are distinct. d6faec is
updateGlyphAndTitle; the real preHandleKeyPressMessage is d700a0. Older Select/
per-type key summaries conflated layers and cannot serve as a key controller.

Navigation provider: NAVIGATION-SERVICE-2026-11-03 implements reloadable state/
queue and actual nav-* CLI/Unix IPC. Firmware constructor defaults fields0;
message ctor stores route, not selection. Requests read current retained state
at take, so multiple pending requests can observe the later selection. Enqueue
is not construction/dispatch. OAN1 preserves queued IDs without native-struct
serialization. Capacity16 is a project limit; full pressure parity, actual
focus/modal/UI consumer, TabDialog writeback and4b/a6 context remain open.
Host/sanitizers/ARM-qemu/local Unix IPC passed; no TV access or drawing.

Launcher result contract: LAUNCHER-ROUTING-2026-11-03 follows the guarded main
Select/modal-return path to OperationFinder navigation13, distinct from QuickMenu
navigation12. Full62-entry signed conversion and28 unconditional ordered settings
plans are implemented/tested against firmware. Base network retains row; base
entries retain open-submenu flag. Enqueue-front is not immediate construction.
Do not equate this with every Select or wire the captured shortcut screen
straight into a full list.4b/a6 require extra portal/runtime context; no such
work, logging/telemetry or service/UI integration was silently included.

Subsequent-agent review: AGENT-WORK-REVIEW-2026-11-03 corrects scanner partial/
unterminated keys, unrestricted pointer reads and snapshot offsets. Fresh ARM
shows+ac lookup keys can survive UString assignment; raw+a4 metadata is always
retained but is not displayed character data. Menu inventory154 cases checks
out as syntactic case-local calls, not resolved goto/delegate outcomes.
SubLauncher setFocus/isShowing virtuals are verified. TabDialog was absent from
the identity table: no old scan proves it absent or identifies the visible tabs.
VK1043 differs from ICE593/0x251; matching XML names and DIAL app receipt do
not prove the internal key was handled. No-effect1043 does not identify a
profile/modal/translation failure. Apparent slider+30 remains conditional on
unmatched capture/candidate evidence, not a proved per-type renderer rule.

Glyph routing/background (2026-11-03): UI-GLYPH-ROUTING proves the firmware
nonnull-context GPU branch and checks saved vtables/context bytes; it does not
trace the captured frame. Opaque-parent native comparison gives1318 white core/
1072 differences vs vendor1446; opacity alone does not close the mismatch.
Title alpha255 is an explicit test condition, not independently observed there.
Actual parent paint/active FBO/presentation is next; do not refit title geometry
or infer CPU rendering from the existence of CPU fallback methods.

GPU title limit (2026-11-03): UI-FONT-GPU implements coverage textures/font
shader plus the shipped RGBA FBO/composition pass in the lower GLES owner.
Native private-pbuffer checks pass; they do not establish title parity. One-pass
core1971/vendor1446 differs525, two-pass core1228 differs1150. Native varying
declarations give the same full output as the old composition shader here;
neither GPU usage nor precision changes close the saved pixel discrepancy.
Configured context association is not execution tracing of the title command.
The earlier Surface address is now freed/rejected: do not follow its stale fields.
No new geometry correction follows from these raster results. Service text
integration, actual rendering route/state and independent acceptance remain open.

Font X correction (2026-11-03): FONT-X-OFFSET checks the actual ARM b4247c
ldrsb from object+5 and b42482 rsbs. This is -signed global minimum-left,
NOT a zero getter. Earlier claims inferred a return value from vtable identity
and misread saved instructions. Font18 minimum-23/live header e9 and font7
minimum-18 account for title23 and normal MODE-row18 without fitted geometry.
Boxed renderer now reserves that offset; raw glyph origins remain explicit.
Title matches retained position and capture bounds, not yet all raster pixels.

Retained title origin (2026-11-03): UI-TITLE-LIVE-GRAPH follows read-only native
widget->Surface->TextCommand->LineObj fields while owner shows stock settings.
Surface108,5,691,95 and LineObj23,5 account for current ink136..388/31..85;
exact text and zero margins/unit glyph ratio verified. Correction: the
BitmapFont getter is not zero; FONT-X-OFFSET proves its metric-derived23.
Do not shift widget/resource geometry to implement that offset.
Flag+1b1 flows to CharClipping, not bold/font type. Early framebuffer samples
captured launcher frames before the second owner cue; no MAU bug follows.

Title layout correction (2026-11-03): UI-TITLE-BOX integrates the existing
boxed renderer into ui.title. The previous diagnostic bypassed vertical layout;
font18 height80 in a90px viewport accounts for5px Y via center positioning.
Glyph bearing invariance did not force shifted widget geometry. Full viewport/
clipping now used;23px X delta remains open, title default-off, no fitted inset.
Native CPU corpus passes without drawing or relinquishing stock ownership.

Subsequent UI review (2026-11-02): UI-WORK-REVIEW fixes scanner read span after
extended draw-control fields and gates presets on ui.edit=1. Later saved live
scan has a created font18 title candidate and list/row geometry, superseding
the earlier no-created-instance sample. Matching geometry narrows the remaining
title delta; non-atomic candidates without draw-control/frame association are
not definitive proof that every other transform/input is excluded. Title stays
default-off. Focused ink and lab slider edits are not full focused-font/widget
or settings parity. Research preserved under workbench/agent-ui-review-20261102.

Label content (2026-11-02): UI-ROW-LABELS traces ordinary SettingItem key
localization and drawRow without a prefix. Profile section Languages/value
Dictionary resolves MODE to exact Dutch Modus, with no hidden spaces/control
markup. Custom hashed IDs and locale slots are now established for this record;
the flattened string inventory alone did not establish them. The translation-
fit branch can choose a maximum-width translation; its live enablement is
unread. Stored Dutch MODE is ruled out as an18px-padding source, while actual
parent/window/resource/ratio and visible parity remain open. Character-conversion
slim_i18n symbols are not the UI dictionary-key lookup entry point.

Live UI scope (2026-11-02): UI-LIVE-OBJECTS scans only readable private [heap]
for known firmware vptrs; matches are non-atomic candidates, not active-window
proof, and absent matches do not cover other arenas/subclasses. Current scan
finds only6 uncreated textbox candidates. Owner confirms HDMI1 video playing:
blank OSD and WS OTHER must not be called standby or a failed display. Menu
HTTP201 did not prove handling. Old wspoll.c has an8-byte read into ext[4] and
other framing/state defects; it was not run. Host WS queried state instead.

Local text rendering (2026-11-02): UI-TEXT-VIEWPORT verifies widget alignment4
maps to HAL start/center; all9 mappings/default implemented in text_origin and
the new unit-ratio single-line font_box renderer. Local Y2/1 for actual tested
regular/bold surfaces is not absolute live placement proof. Dialog margins,
initial list insets and setLabel itself add no fixed padding. Correction:
font7's global minimum-left-18 DOES explain conditional X31 -> fitted49.
Use the recovered FONT-X-OFFSET rule, not a fitted18px constant; other labels'
indentation and live style/interaction integration remain open.

Textbox composition (2026-11-02): UI-TEXTBOX-COMPOSE confirms lines==1 uses
full-height horizontal row slices, without subtracting top/bottom caps. Actual
489/488 give corrected text heights66/70 and regular/bold font7/20 for the
tested explicit parent, not a measured live instance. Their text colors are
raw stroke strings; fill is none. A new normal fresh-box composer covers
geometry/style only; actual parent/glyph/ratio/clip and visual acceptance open.
Pre-mask textPos returns ENOTSUP pending full second-pass/cache behavior.

Ordered masks (2026-11-02): UI-ROW-WINDOWS proves renderGroup clears/builds
the same container consulted by its transform. First mask is stored raw;
pre-mask rectangles are identity, later rectangles use the cached first mask.
Prelooking up all raw rectangles would change behavior. New ordered geometry
helper composes actual460 child windows for a supplied parent; it does not
establish live parent/font/raster parity. Animation-group seek/depth quirks are
explicitly outside this helper's supported path, not silently normalized.

First theme consumer (2026-11-02): UI-THEME-SKIN replaces ui_vendor's hardcoded
marker-presence list with actual held provider lookup, testing all8 numeric
priorities and removal/rebind. No view persists beyond selection; active mode
is a scalar. This is the existing ui.launch_anim=1 stand-in diagnostic, not
evidence of a vendor launcher transition or complete theme/widget integration.
Real pack still selects DialogLauncher mode60/type4; settings origins stay fitted.

Theme ownership (2026-11-02): UI-THEME-PROVIDER adds owned index/binary snapshots
and ui.theme v1. Case-sensitive CRC32 name lookup is the first matching hash,
ID lookup is index position. All record boundaries validate before publication.
Returned views/strings borrow provider memory: keep the registry hold until
finished, not just through lookup. File edits cannot change published bytes;
reload rereads paths but not host configuration. Host old-binary rollback and
actual UI consumer/style/rendering integration remain open.

Runtime resource view (2026-11-02): UI-BINSVG-VIEW adds bounded single-record
parsing and raw named rectangle lookup. All702 shipped records validate;
resource477 geometry feeds maintained layout helpers without hardcoded slice
coordinates. RectangleContainer returns the first duplicate; group selection
includes descendants of the first matching group and stops at its closing tag.
Raw resource geometry is not transformed widget/window or framebuffer geometry.
Indexed loading/provider and UI integration remain open.

Horizontal text correction (2026-11-02): UI-HORIZONTAL-ORIGIN checks the ARM
glyph-data offset: end alignment subtracts final right ink boundary byte3,
not advance byte4. Physical width excludes final spacing and adjusts first/last
bearings; pen advance is a separate API. Center overhang trims both ends and
successful slices reset X0. End traversal paints in reverse. New plain aligned
line renderer is tested offscreen; full widget/style/shaping integration is open.

Text origins (2026-11-02): UI-TEXT-ORIGIN confirms concrete list/row/textbox
zero-margin bindings and recovers vertical policies. Center's half-division
truncates toward0; per-line ratio conversion floors two reference-dependent
positions separately. Existing font_text_draw still takes a caller-supplied
glyph origin, not a recovered complete baseline. Horizontal style/direction/
truncation and actual widget inputs remain open.

Horizontal stretch (2026-11-02): UI-SLICE-STRETCH preserves double ratio and
separate truncation of relative X and width; independently scaling corners can
misplace an edge. Nonpositive mask width retains prior ratio; changing surface
does not reset mask validity. The477 composed arithmetic path passes, but actual
runtime parent/window/text origin and UI integration remain open.

List measurement (2026-11-02): UI-LIST-METRICS recovers two phases: raw slices
reset top/bottom/steps, then painted rowPos updates cached row size/side insets.
Visible count precedes that loop, so horizontal count sees previous insets.
Missing rowPos retains state. The old text-setup widget-name expansion and
four-variant-table theory are withdrawn: pointer chains prove normal IDs
14/17/18/22/23=mTopL/mRowL/mRowM/mRowR/mBotL. Runtime UI integration is open.

Mask/row layout (2026-11-02): UI-MASK-LAYOUT records exact numeric anchor
cases rather than an assumed centered grid. Mask positioning preserves spans;
unsupported alignment still subtracts mask origin. Mirrored horizontal rows
use window.right, not width. Tested pure helpers exist, but runtime field
population, complete slice composition and text baseline remain open.

Text setup correction (2026-11-02): UI-TEXT-GEOMETRY closes the single-line
consumer: textPos is copied to+1ec then used by foreground; +254 is addition.
The manual correction scales height with float32/truncation toward zero, not
width or round-half-away. Font-marker65 is fontRegular. Newer text-setup prose
did not override the already preserved instruction evidence; corrected there.
49,728 VFP comparisons pass qemu/TV, but full mask/row/baseline layout is open.

Native font correction (2026-11-01): the theme's38 EF41 font records contain
signed metrics, packed UTF8 keys and raw8-bit coverage, decoded by UIBitmapFont /
hal::BitmapFont. Its file-font pack is empty. FreeType/browser references alone
did not prove the settings-row raster path. UI-FONTS records the bounded reader
and all-record host/sanitizer/ARM-qemu/TV checks. UI-FONT-SELECTION later recovers
startup20 IDs/heights and generic typed selection with global-smallest fallback;
per-widget font choice and text layout/render integration remain open.
Some oversized signed metrics wrap;
vendor draw loops leave those glyphs blank, rather than reading a huge bitmap.
UI-GLYPH-DRAW subsequently supplies a tested single-glyph compositor and resolves
SVG font-marker priority. Focused label488 changes to fontBold and textPos height42,
while unfocused489 has no font marker and height40. Recoloring captured white
ink alone therefore does not establish focused text parity. Whole-string layout
and actual widget setup remain open; glyph tests draw only offscreen memory.

UI engine review correction (2026-11-01): posting engine code under the skin
owner did not protect engine lifetime and leaked heap userdata when skin posts
were purged. ui.engine v2 now posts only skin-owned trampolines, uses a bounded
value queue and re-registers both skins after engine replacement. See UI-ENGINE-
REVIEW for242 host checks/sanitizers/native contracts and real GPU restoration.
Earlier v1 seam evidence does not establish those failure paths.

“Implemented,” “loads,” “read back,” “historically visible,” and “works independently
at cold boot” are separate claims. Tests only support their exercised scope.
A failed probe may show a bad request or missing init, not an unusable API.

Use the TV's own firmware dump. Medion/sibling-kernel findings in old chronology
are not target evidence. Ghidra signatures and decompilation can be wrong:
check instruction arguments/return paths and relocations. Aurora's regular ELF
symbol table is stripped but rich dynamic symbols remain; nm -D works.
Symbol/function counts depend on the counting method and do not establish an ABI.

## Known red herrings and replacements

| superseded claim or tempting detour | current conclusion and evidence |
|---|---|
| A failed module load returning NULL means fallback can start immediately | Init can retain a busy DSO despite NULL. SKIN-SELECTION tracks the retained candidate and waits for normal unload/quiescence before loading vendor; native injected-gate tests verify no overlap. |
| `gridFA.svg` `rowPos` (208,160,81,69) is the portal's item geometry | gridFA (18, 1280x720) is a scrollbar template used by later screens. The apps portal uses `FullScreenGridFA.svg` (375): rowPos/mRowM (225,121,320,230), mRowL (156,122,0,230). Its mTopL/mBotL are zero-size paths/polylines, not rectangles. APPS-GRID. |
| Readiness must track the newest queued UI serial, or a retained old receipt is enough after reload | Track the first accepted request of that UI/provider generation. Any later actual successful receipt in that namespace can acknowledge it; old serials cannot. Chasing each new request can starve readiness under repaint/coalescing. UI-ASYNC. |
| The TV's installed OAI1 bundle includes the recovered rail-highlight animation | Its /buffer/openaurora/ui inventory still lacks rail_hl01..11. RAIL-ASSETS exports verified vendor frames to durable ignored workbench and tests a private temporary complete bundle on the real GPU; installed assets are unchanged. Visible frame order/30ms cadence remain open. |
| The original rail test proves a full asset bundle loops all eleven frames | It loaded only highlights. MAX_ICONS24 silently admitted just nine frames alongside title/five glyphs/nine cards. RAIL-ASSETS derives capacity29 from maximum content, reproduces the old failure and checks every frame/wrap with a full normal bundle. GPU image receipts do not establish scanout frame order/cadence. |
| A unique completion ticket also makes image cache revisions safe after consumer reload | GPU upload revisions must increase independently. scene_display uses host-owner-id revision namespaces and increments activation revisions when reusing an open provider; an old cached revision cannot identify a newly allocated canvas. SCENE-DISPLAY. |
| GPU enqueue success can replace synchronous display.present success or acknowledge UI readiness | scene_display publishes separate display.async request/latest receipts; its legacy present rejects ENOTSUP. Coalesced serials do not get success receipts. ui_fb must consume actual completions before writing readiness; that integration is still pending. SCENE-DISPLAY. |
| A client DSO's static ticket counter guarantees public ticket uniqueness across reload | Unloading can reset that counter. mod_graphics maps internal tickets to host owner-id/sequence tokens; actual DSO reload tests reject old tickets. GRAPHICS-MODULE. |
| Reaping a signaled GPU child certifies EGL cleanup or permits automatic recovery | Raw wait status proves process death. Graphics quiescence latches unproven cleanup and retains the faulted provider; native abnormal release/recovery remains a separate firmware investigation. Device-free fixture only; no actual GPU was killed. GRAPHICS-MODULE. |
| A sealed image cannot be mapped on the TV, or qemu's ancillary leak proves a kernel leak | Actual target mmap_region rejects VM_SHARED when sealing made the writable count negative, even for PROT_READ. MAP_PRIVATE passes with all seals retained. Excess-descriptor cleanup passes on TV; the small-control-buffer leak occurred only under ARM/qemu. SCENE-BROKER records both distinct findings. |
| Closing inherited fd>=3 in a directly Mali-linked helper is necessarily safe | Library constructors run before main and can own descriptors by then. SCENE-HELPER now tests an EGL-free bootstrap that closes inherited fds before loading the renderer. The renderer remains loaded until child exit; no async host service exists yet. |
| A10ms controller tick requires100Hz scanout; pbuffer readback timings describe direct GPU presentation | Controller sampling, display refresh and renderer pacing are distinct. Native EGL window/cache diagnostic achieved its50Hz target and owner-observed smooth full-screen effects, with complete restoration. Production async service/worker remains open. EGL-WINDOW. |
| Successful GPU rendering/readback means smooth native display, or the driver says a shared segment was created so remove it | Offscreen Mali tests pass, but1080p round trips cost about60ms/frame before display commit. Native-window/swap ownership is still open. Existing stock-owned key33787/shmid32769 merely had attachment timestamps updated; never remove it. EGL-SCENE. |
| A quad with vertical sides implies bilinear UV interpolation; geometry alone reproduces the animation | Vendor draws GL_TRIANGLE_FAN with w=1, uses GL_LINEAR texel sampling and an alpha shader/blend. Correct fan interpolation, filtering and fade are implemented; real fb1 commits/restoration pass, but general CPU transforms are slow. UI-SCENE-REVIEW. |
| Sending dialog progress to display guarantees frames appear; a v1 pointer clear also clears v2 | ui_fb now paints on every progress event without unrelated timers and clears both aliases before provider dlclose. Interrupt/failed-resume/provider removal regressions pass. UI-SCENE-REVIEW. |
| GetOPUGain can back up and exactly restore raw gain | Kernel getter reads cached fields, then reply handler remaps exactly100 to0x8000. Those raw values cannot be distinguished by this query. No DSP/audio ACK exists. SPEAKER-GAIN. |
| SetOPUGain accepts the UI's0..100 speaker-volume scale | Direct channel operands are bounded0..0x8000 and bypass Mod_Cell_SetVol's AQ/balance/ratio path. UI speaker out gain is a separate ALSA control; lower codec/client tests do not establish audible application. SPEAKER-GAIN. |
| posix_spawn return0 proves helper exec/startup succeeded; qemu return0 for missing exec is a bug | Target libc forks, returns PID/0 and can later exit127 after exec failure. Direct ARM instructions and minimal reproduction confirm it. Module handles delayed EOF/reaping; X_OK only catches obvious missing paths. MIXER-MODULE. |
| Moving I/O to a worker makes its service RPC nonblocking | Owner open/send/gain/receive RPCs still wait for the worker. Loop-thread bind_session now refuses all arguments until async completion exists. Quiesce no longer waits 500ms. WORK-REVIEW. |
| Animation service timer_add automatically belongs to the provider | Service calls inherit the consumer's current owner. ui_anim now registers its timer through its own owned post; resume restores the timer. WORK-REVIEW. |
| An empty post queue proves a busy shutdown gate cannot finish | A worker can post later. Busy gates may wait indefinitely; no-work errors concern non-quiescence holds. WORK-REVIEW/SHUTDOWN. |
| SSH closed, no root/RCE, TV offline | Historical pre-root/outage states. Current last verified rooted stock UI is in MEMORY and STABILITY. |
| Web OmarchyTV currently boots by default | Historically proven; now web/native boot disabled, oa_safe present, oa_boot absent. MEMORY owns state. |
| Every aurora.elf path names vendor code | artifacts/payload/aurora.elf is a shell boot hook. vendor:/com.nvt.idtv/aurora.elf is the ARM application. Check file type/provenance. |
| Any recovery image/stick builder restores current protections | Only the hashed recovery-stability pair is the tested current pair. Raw mtd13/pre-stability files are older. Artifact map distinguishes them. |
| Failed USB attempts proved malformed .scr syntax | Cause unestablished. Exact older 584-byte script succeeded. LED blinking is not execution proof. STABILITY. |
| Native replacement can be reduced to an HDMI launcher | No. Tiers order full parity work; only telemetry/ads are excluded by owner. ARCHITECTURE/TODO. |
| Revisit script.txt, IRsim or broken Robotest to obtain root | Root already exists; AURORA-INFLUENCE records those old dead ends. IR driver injection paths also do not prove active app startup paths. |
| fb0 BGRA but fb1 RGBA | All measured 32bpp OSD layers are BGRA. Old tvshot red/blue decoding was wrong; pre-fix captures need provenance. UI-PARITY. |
| Correcting BGRA means swapping every RGBA buffer or saved PNG | OAI1/blit inputs intentionally use RGBA; display converts to BGRA. Screenshot PNGs are already RGB. tvshot's separate RGB565 decoder and legacy splash writer were corrected offline. UI-PARITY. |
| 1.62/255 image error proves UI parity | One selected framebuffer comparison, not interactive behavior, all screens or visible independent startup. UI-PARITY/TODO-AUDIT. |
| Captured background RGB(14,14,13) proves the vendor portal is opaque | It does not establish alpha or composition over video. UI-TRANSPARENCY recovers the unnamed#242628/raw opacity6 rectangle and ARM conversion to alpha153. display.alpha and corrected source-over/CPU premultiplication now produce actual GPU BGRA24,23,22,153. Independent real-video appearance remains open. |
| Static screen parity permits omitting animations or UI flair | Vendor transitions, focus/selection feedback, scrolling and other UI/UX effects are required. List/dialog controller behavior is offline in mod_ui_anim; the text-slide caller is wired in mod_ui_fb with the vendor repeat. The full-screen progress mapping is recovered and encoded (`oa_fs_apply`), but no renderer consumes it; rail button mapping and frame sequences remain open. |
| `setFullScreenAnimationProgress` has no recovered mapping | `SVGDialog::startAnimation` @0x10c5009 maps dialog modes 0x39..0x40 through the table at 0x153b80c to HAL types 1..8 and starts a 1000 ms controller; `onAnimationProgress` feeds `min(1,1+(p-1)^3)`; `destroy` forces 1.0. The HAL transforms one 4-vertex clip-space quad: type 1 scales the right vertices (keystone), 2/3 shift x, 4/5 shift y, 7/8 scale. `SVGDialog::create` tries the static mode table 57..64 in order and starts the first whose animation widget exists. Display v2 now warps an offscreen scene and ui_fb consumes it; the widget binding for our stand-in dialog is still a config selection. UI-ANIMATIONS. |
| The category rail animates its item positions | `MainLauncherMenuDialog` moves items synchronously; the five category buttons get `setMainlauncherHighlightId` in `init`, and `renderForeground` creates the highlight image at the dialog's `mainlauncher_highlight` rect. `DialogMainMenu.svg` button windows are `(6,y,160,160)`; the capture places the 210x210 highlight at `(focused_x-5, focused_y+10)`. Implemented: 11 frames at 30 ms played once per focus (the last frame is held; decodeNextFrame does not rearm), playhead reset on focus. The 0.3 s scale records on 536/537 are launcher tiles, not the rail. UI-ANIMATIONS. |
| 0x96 in the animation timer is a 150 ms frame delay | It is the message id; the recovered controller starts immediately and schedules later ticks after 10 ms. UI-ANIMATIONS. |
| Animation time is ordinary monotonic milliseconds with uint32 rollover | FROST_Time uses a 90,000,000-ms period and a 12,343 correction quantum with strict comparisons. ui.clock and ui.anim are tested offline. Absolute live vendor phase remains unverified. UI-ANIMATIONS. |
| Theme linear mode means every vendor transition is linear | Dialog progress is min(1, 1+(p-1)^3) over1000ms; list selection truncates a linear blend. Text slide has its own pause/rate (800ms,50px per1000ms); the category rail uses the recovered11-frame30ms APNG loop. UI-ANIMATIONS/RAIL-ASSETS; visible cadence remains unaccepted. |
| The text-slide repeat is a repeating timer | `SVGTextBox::onAnimationProgress` @0x10c8e48 arms one 2000 ms timer when progress > 0.99 (0x10c8ee8) in state 3 (`setTimer(target,2000,200)` → `CabotTimerStart(handle,2000)`); the next pass restarts at margin 0. mod_ui_fb re-arms only after that pass completes. Only the 10 ms framework sample repeats. UI-ANIMATIONS. |
| svg.bin has length-prefixed names and no vector/animation data | It is a typed binary BinSVG stream with NUL-terminated ids, vector primitives and animation records. Outer record lengths come from the index. The bounded reader walks the entire target pack. UI-ANIMATIONS/UI-ASSETS. |
| BinSVG hashes require guessed widget names | Exported svg_name_list resolves all 702 names/IDs; CRC32(name) matches every index entry. All 39 animation descriptors are named; ad-banner effects remain excluded. UI-ANIMATIONS. |
| WIDGET_STATE uses FA/UFA/FI/UFI array order | Internal order is UFA/FA/UFI/FI (0/1/2/3); category code remaps {1,0,3,2} into its FA/UFA/FI/UFI resource arrays. Active and focus are independent. UI-ANIMATIONS. |
| Kernel fbs are created only by Aurora | KER_OSD_DrvInit registers five framebuffers. Registration, node publication and presentation differ. OSD-KERNEL-INIT. |
| 131/132 acquire/commit a framebuffer | Palette set/get, 12-byte {start,count,32-bit pointer}. Do not repeat descriptor permutations. OSD-BUFFER-CONTRACT. |
| Dumb-buffer mmap failures or mode_valid=0 rule out DRM | Driver CREATE_DUMB allocates only 4096 bytes; actual Aurora OSD uses private GEM create and ADDFB2/SETPLANE. OSD-BUFFER-CONTRACT. |
| Hal_Video_DrmInit/_kms_init or FlushPlaneInfo commits the OSD | ID discovery/cache path, not explicit SETPLANE. Separate OSD Update_Enable commits. DRM-PLANE-CONTRACT. |
| display.pan=0 means omit FBIOPAN_DISPLAY | It disables rotation only; every present must commit. Fixed/tested offline; not deployed/accepted on panel. OSD-BUFFER-CONTRACT. |
| Pixel readback/present success/ready receipt proves scanout | None proves hardware is displaying that buffer. Cold ownership, enable/crop/alpha/resume acceptance stays open. RECOVERY/TODO. |
| Doubled framebuffer metadata guarantees doubled mapped storage | Initial front requests S; Sync advertises 2*align4096(S). OSD commit retrieves GEM-derived address/size. Keep metadata, crop and backing extent distinct. OSD-BUFFER-CONTRACT. |
| Successful MAU0 query explains old invisible output | It only shows primary memory selected at query time. Earlier failure cause remains unproved. OSD-MAU-SELECTION. |
| Missing alpha/show/resume calls must be added to mod_display | Normal-plane blend-enable does no register write; SetPar sets global alpha. PanDisplay handles first show/region enable, and registered kernel PM restores cached state. Validate the existing fbdev path first. OSD-KERNEL-INIT. |
| ndp_osd_restore_plane_mode_set restores all OSD planes | It conditionally replays logical plane0 only. It is not an all-plane resume API. OSD-KERNEL-INIT. |
| dp_order/stacking is the established visibility fix | Previous invocation reset the set; no cause established. Do not retry as a guess. DRM-PLANE-CONTRACT. |
| Watchdog requires cold NDAL STBC calls | That path reset the set. pcbconfig/wdtctl packet path was recovered and tested; no further takeover currently authorized. PANEL-TAKEOVER historical evidence plus RECOVERY. |
| Watchdog disabled means recovery guaranteed | Separate supervisor has fake-hardware tests; it cannot recover every kernel/hardware/supervisor failure. Real cold takeover unverified. RECOVERY. |
| PWM Open five words / Set three words / all return void | Open six words, Set four; Init/Open/Set/Terminate scalar returns verified in ARM instructions. Close no channel. PWM-CONTRACT-RECHECK/header. |
| PWM nonzero means brightness changed; sweep channels until it works | A nonzero shortcut may skip ioctl. Board constants exist; trace profile/driver and ownership first. Panel independent acceptance open. |
| Reusable HAL counts 121/643/160 imply missing APIs | Defined unique dynamic function names recount: 321 Hal_* in NDAL3, 80 NDAL_* in NDAL_EX. Counts are navigation only. PLAT/DEV/ICE implementations stay inside Aurora. |
| TV uses musl / compiler glibc <2.34 guarantees loadability | TV is glibc2.23. Check actual required versions/closure, then preflight. Musl target is stub/sanity only. ARCHITECTURE/RUNTIME-CHECK. |
| RTLD_LAZY / loader exit0 means all imports work | Missing V4L2 imports were reported despite exit0. --check parses diagnostics; rebuilt closure passes. RUNTIME-CHECK. |
| IR key map recovered means UART protocol solved | NEC/RC/ICE/DFB/DIAL differ. RC field, repeats/releases and integration remain unverified; reading ttyS2 consumes stock input. IR-KEYMAP/TODO-AUDIT. |
| Standalone dialserver means no Aurora needed for TV services | Transport runs independently; execution/state still use vendor engine. DIAL/WS/DLNA replacement remains work. |
| mem/WoL test proves native standby lifecycle | Raw suspend/wake only. Host service/CLI coordination is now tested offline; device/wake/hardware acceptance remains open. POWER-STANDBY-CONTRACT. |
| idtvSuspend/Resume are optional empty middleware hooks | They own wake-network/PHY policy, audio/tuner/VBI, UART synchronization and device restoration. POWER-STANDBY-CONTRACT. |
| MCPU_MonitorTask_Reset disables the hardware watchdog; Uart_SuspendSend sends an MCU packet | The first changes mutex-protected vendor monitor state; the UART send pair waits/signals a sender semaphore. Neither is a guessed MCU packet API. POWER-STANDBY-CONTRACT. |
| Ethernet 0x89f0 opcode 1 immediately powers down the link | It stores next-suspend policy. Value 0 keeps PHY powered; value 1 requests PHY shutdown during driver PM. Other opcodes can write hardware directly. POWER-STANDBY-CONTRACT. |
| HAL call did not crash, therefore audio/source/platform works | Audible output/routing/ownership and omitted routing operations remain open. TODO-AUDIT and current modules. |
| AD82586C symbols/config references identify this TV's main amp | Verified live descriptor/object/vtable selects one ESMT AD82587_D (id1). AD82586C is another supported driver; subwoofer helper names are not main amp selection. AUDIO-ROUTING. |
| Decoder main volume, speaker gain and amp volume are interchangeable | Separate controls/scales: decoder opcode33, per-speaker EasySet items42/45/4f/4a, amp I2C register3. Registered speaker0 does not prove native audible/mute parity. AUDIO-ROUTING. |
| AD82587 mute is another normal SoC GPIO; vendor address68 is seven-bit | Mute resolves to physical208/STBC8, reset to normal GPIO114. I2C bus3 uses seven-bit34 (vendor eight-bit68). Fixed register read passed; no MCU/reset/write acceptance. AUDIO-BOARD. |
| Both READ flags in vendor I2C messages prove a malformed transaction | Target PreInit adapter takes the first buffer as subaddress and second flag as direction. Both messages completed on real hardware. Generic adapter assumptions do not describe this custom path. AUDIO-BOARD. |
| Hal_Iic_Read success proves all data transferred | NDAL only checks negative return; disabled bus can return0. New fixed probe requires count2 and returned1b/05/05 on the actual amp. AUDIO-BOARD. |
| UI volume is a percentage to write directly to amp register03 | The traced path selects speaker gain through101-entry AQ DSP tables. New decoder matches all101 live entries; amplifier and decoder-main-volume controls are separate. Complete policy/integration remains open. AUDIO-VOLUME. |
| PLAT_AOUT_SetMainVolume success means volume was applied | ARM instructions only range-check r1<=100; no device write. Use the recovered speaker-gain path, not this misleadingly named wrapper. AUDIO-VOLUME. |
| Correct scalar audio ABI means return0 proves a gain/table command succeeded | Message-backed Hal_Audio_Send returns0 even after sendmsg failure. Kernel receiver failure does not reach that wrapper. Preserve explicit transport/completion/lifetime checks. AUDIO-GAIN-TRANSPORT. |
| ALSA speaker gain readback certifies DSP application | Setter caches the request before lower calls and returns0 regardless; getter reads that cache. Real-TV metadata/value query passed, but audible/DSP acceptance remains open. AUDIO-GAIN-TRANSPORT. |
| Fresh-process ALSA hw:0 lookup failure means no speaker control exists | Vendor ALSA requires ALSA_CONFIG_PATH=/vendor/etc/alsa.conf, set by run.sh. The unchanged diagnostic then returned one integer control, range0..100/value0. --check does not validate resource configuration. AUDIO-GAIN-TRANSPORT. |
| Mixer table pointer must remain valid indefinitely, or full send means operation success | Target netlink invokes the callback synchronously; the receiver copies the table before return. Callback results are discarded. Retain through the syscall; dispatch is not an operation/DSP/audio ACK. AUDIO-NETLINK. |
| Any fresh netlink port can receive a mixer reply; HAL socket can be duplicated freely | Reply helper requires a real process id matching the bound port; HAL same-pid binding can conflict. Native integration needs coordinated ownership. Fresh-process GetOPUGain passed. AUDIO-NETLINK. |
| _cmd_capability_p or GermBeacon is a harmless query | The first sets bypass capability; the second loads/reloads command text. Neither was executed as a query. AUDIO-NETLINK. |
| Hal_Audio_Ctrl/Protect/Mixer/Proxy have void returns; audio should repeat init | Instructions show scalar returns. Shared vendor_audio.h and single plat prelude ownership are implemented/tested offline; routing/partial-init restart remain open. CEC-AUDIO-CONTRACT. |
| TV sends Initiate ARC C0 to the amplifier | Audio System is ARC receiver and sends C0/C5; TV is ARC transmitter and sends C1/C2/C3/C4. Old CEC-SPEC direction/conformance claims corrected against 1.4. CEC-AUDIO-CONTRACT. |
| CEC audio volume 127 is maximum volume | 127 means unknown, 101..126 reserved, 0..100 percent. cec.audio stores Report Audio Status as external UI state and mod_audio refuses local volume and mute while System Audio Mode is on. CEC-AUDIO-CONTRACT, OWNER-WORKER. |
| Normal CEC frames use MCU UART / ttyS2 | Private V4L2 controls feed ntkdriver_nondovi CEC. MCU enable/wake/authority is separate; UART reads would steal input. CEC-TRANSPORT-CONTRACT. |
| NDAL Process_Command success proves a message was accepted | It returns ioctl success but drops a separate driver-result byte. Driver filtering also rejects some spec-valid frames while upper layers process them anyway. CEC-TRANSPORT-CONTRACT. |
| Bug-for-bug parity requires copying CEC spec violations | Owner explicitly requires spec compliance first. Directed OSD-name and broadcast System Audio Mode regressions override the vendor filter. The injected cec.audio route follows 1.3a System Audio and 1.4 ARC; native bus acceptance remains open. CEC-LIVE, OWNER-WORKER. |
| Missing /dev/ntkcec means no native character device is registered | Live /proc/devices lists major 229; temporary node open failed with EPERM under the stock exclusive owner. No independent raw initialization has been accepted. CEC-LIVE. |
| Steam Deck must use LA4; cache proves fresh replies | Live known device is LA8/PA1000; unseen LA4 later mirrors metadata. Three query transmissions accepted, but reply payloads/freshness were not captured. CEC-LIVE. |
| CEC operands only need 14 bytes of receive storage | VPL receive copies 16 bytes even on failure paths. Tested raw/VPL marshalling requires 16-byte storage and explicit status/metadata validation. CEC-TRANSPORT-CONTRACT. |
| NDAL CEC Init / Init(NULL) implements safe module reload | Kernel close stores fd 0 and passes a null close argument; open treats fd != -1 as already open. Native hardware ownership/lifetime must be recovered separately. CEC-TRANSPORT-CONTRACT. |
| vu_ioctl/vu_open are ordinary Linux system calls | They are PreInit's kernel emulation API. The same driver also registers a real Linux cdev; the new raw backend uses independent native fds. Offline authority and workers are in OWNER-WORKER. Hardware open remains unaccepted. CEC-OWNERSHIP. |
| Host exit can dlclose after one ignored quiescence failure | oa_host_run retries stop/unload after completion posts, module timers and module watches. A remaining module with no such path returns an error and stays loaded. SHUTDOWN. A failed CEC close does not post, so that wait is intentional. OWNER-WORKER. |
| Failed CEC hardware close can be followed by discarding the fd/session | The raw backend and owning worker retain fd and state. Each quiesce generation retries close once. The worker does not exit or call on_settle while close fails. Only a successful hardware close, or the client's final OS close for audio, consumes the fd. CEC-OWNERSHIP, OWNER-WORKER. |
| One yielding quiesce is enough for an idle owner reload | A yield loop can return before the worker is scheduled, leaving cleanup_pending and blocking later suspend. Quiesce no longer waits 500 ms (see the corrected row above); an open CEC session still returns immediately, so a failed close does not stall shutdown's poll. OWNER-WORKER lifecycle follow-up. |
| Flattened language inventory contains proven IDs/Dutch slot | 56,715 text lines are an inventory; original IDs/grouping/alignment not recovered. UI content work remains. |
| Text-slide margin returns to zero at/after the duration | Forward direction ends at **distance**; `margin = (int)(distance×frac)` with `frac` clamped only above. Only the reverse flag (`distance − margin`) ends at zero. Re-read of instructions at 0xef0264, 2026-11-01; helper constants pause 800 ms, divisor 50, ×1000 confirmed from 0xeefc54/0xeefc8c/0xef0260. Implemented as `oa_anim_slide_duration`/`oa_anim_slide_margin` (`ui.anim` v2), host/ASan-UBSan/ARM-qemu tested. UI-ANIMATIONS. |
| Load average ~15 means the TV is struggling or something is leaking | Measured 2026-11-01: `runnable 1/496`, `CPU 97.6% idle`, and `D total = 15` — all of them vendor **kernel threads** (`ppid=2`): `_NerveMTask_`, `TCON_ARM_TASK`, `PLAT_WDT`, `DRV_DBG_UART`, `DRV_TVADC`, `MHL_MID_EvtThre`, `DRV_VD`, `_VPL_SRC_KADC`, `SRC_ModeTask`, `SRC_MonitorTask`, `VPL_SCLR_CTRL_M`, `DRV_VBI`, `PQ_MPFW_TASK`, `PQ_SHELL_DBG_TA`, `KER_CI_Timer_Ta`, all in `msleep`. This 4.9 vendor kernel maps `msleep()` to `schedule_timeout_uninterruptible`, so every sleeping driver thread counts as load permanently. Aurora's 239 threads were 0 in D; none were ours. Flat 13-15 all session incl. fresh boots. Not a health signal on this set — judge by runnable count, CPU idle and UI/ssh response. |

## What is established

- Root boot/access and privacy maintenance survived the tested recovery boot;
  internet consent must remain 1. Standby still closes services.
- ABI 2 worker ownership/descriptor checks, registry regressions and state-aware
  reload have host/sanitizer/ARM evidence. Suspended reload defers hardware start;
  restore failures reject/clean the candidate. Required-service/lookup dependencies
  now order lifecycle transitions and errors reach CLI. An internal sleep-cycle
  engine adds final entry/ledger restoration and nine fake-entry regression groups;
  seven wrapped-power groups also cover service/CLI and notification integration.
  Native device/wake/hardware acceptance and binary rollback remain open.
- Full-table status memory corruption was reproduced with ASan and fixed through
  bounded reply formatting. Full list/status socket replies pass host/sanitizer/
  ARM-qemu tests. Unix transport now has bounded nonblocking queues and five
  backpressure/partial-write/disconnect/input/cleanup regression groups on those
  targets; console stdout remains synchronous and is separate open work.
- Rebuilt core and eleven matching modules passed read-only preflight on real
  hardware from /tmp. The persistent installation remains ABI 1.
- Vendor OSD allocation and commit are recoverable from offline firmware; current
  evidence now replaces blind draw/allocation/stacking probes.

## What is still unknown

Cold scanout and resume ownership; complete IR behavior; independent panel/audio/
source/power/settings/services parity; remaining broadcast/CEC/apps/PQ/network
features; complete interactive UI and native OmarchyTV. See TODO for individual
acceptance tasks, not the “next experiment” paragraphs in historical reports.

## Useful provenance

- Raw OSD evidence: artifacts/analysis/osd-2026-10-31; supplied function names,
  hashes and instruction addresses are in the corresponding current reports.
- Runtime evidence: artifacts/analysis/runtime-check-2026-10-31.
- Firmware/assets/recovery identities: artifacts/README.md.
- The original chronology is preserved under artifacts/archive. Its confident
  prose and old checkmarks do not override this correction map.
