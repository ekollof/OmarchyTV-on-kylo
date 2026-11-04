# Handoff — 2026-11-04

Read AGENTS.md, MEMORY.md, FINDINGS.md and TODO.md. Documentation was consolidated
to remove competing status snapshots. Raw evidence and pre-cleanup docs remain
local; use artifacts/reports/README.md to distinguish current conclusions from
historical experiments. The original documentation cleanup was offline; later
authorized live checks are recorded below and in MEMORY.md.

## Current objective and safety

Full vendor parity except telemetry/advertising, with reloadable modules and
eventually native OmarchyTV. Replacement is incomplete. November 1 owner
authorization permits temporary idle hardware/CEC/audio tests; see AGENTS.md.
View-only interrogation remains permanently authorized. Persistent replacement
and firmware/boot-hook changes are outside this test window.
Installed/recovery artifacts remain ABI 1; current development is ABI 3 in
openaurora/build-tv-v3. Preserve build-tv and historical build-tv-v2; never mix ABIs.
QUIESCENCE-2026-11-01 records the busy-module gate. SHUTDOWN-2026-11-01 records
the later host loop: it dispatches completion work and retries stop/unload, and
it does not dlclose a module that still has no completion path. Nine groups pass
strict host, ASan/UBSan and ARM/qemu. That core is not deployed.
OWNER-WORKER-2026-11-01 adds offline CEC and audio workers. They accept injected
I/O only. mod_cec does not open hardware.

## Previous stop and current review

The previous agent stopped after15e8de8 and recorded the pause in b50e3f0.
The owner then requested a review and fixes on2026-11-04. The review below
supersedes the old clean/verified snapshot where it identifies defects; it
does not start the pending imageCreate subsystem or persistent replacement.

## Immediate continuation point

Latest: [UI-FB-SIM](artifacts/reports/UI-FB-SIM-2026-11-04.md) answers whether
the TV framebuffer can be simulated offline. The real fbdev display module
runs against a fake framebuffer on the host and as an ARM binary under
qemu-arm-static (tests/display_present, eight groups, exit0). The same GLES
scene code runs on host Mesa llvmpipe through the existing
tests/egl_default_display.c adapter (`make scene-gles-mesa`): nine transition
types are pixel-exact and reopen/cleanup pass. User-mode qemu cannot emulate
Mali or the ntk /dev/mali ioctls, and the sysroot has no ARM EGL besides the
vendor blob, so vendor rasterization and stock-vs-ours scanout comparison still
need the set. Recommended use: scene-gles-mesa plus fake-fb transport tests
per slice; batch TV runs for final visual acceptance. Next offline step is a
1920x1080 fake-fb capture harness and a Mesa-wrapped renderer DSO for the full
settings preview. The pending firmware feature remains SVGImage::imageCreate;
no persistent/device change, TV untouched beyond prior authorized work.

Previous: [UI-DISPLAY-REVIEW](artifacts/reports/UI-DISPLAY-REVIEW-2026-11-04.md)
repairs the owner's missing focused sliders and Modus chevrons in the existing
captured-panel/live-text preview. Firmware-derived FA/UFA track/PNG composition,
shared shadow digit/knob values, separate selection images; no live setting writes.
Export the four local PNGs with tools/export-slider-assets.cjs into ui.dir before
using ui.text. Seven real GPU states/screenshots pass; Contrast slider and Modus
chevron crops differ from independently captured stock by at most2/255 per channel.
Full16588800-byte framebuffer, region/MAU/pan restored and separately hash-checked;
staging/backup removed, no helper. See MEMORY for identities. Captured tabs/values,
normal glyph shapes recolored for focus, fitted text origins, native widget/live
provider/imageCreate integration and independent startup remain open. No persistent
deployment. Pending feature is still imageCreate; do not equate this preview fix
with general SVG rendering or settings parity.

Previous: [UI-REVIEW](artifacts/reports/UI-REVIEW-2026-11-04.md) reviews the work
since28ef959. Fixed16-byte image resource overflow (actual supplier writes32),
aligned its buffer, validated callbacks and clarified borrowed lifetime. Fixed
renderer signed overflow/division by sharing checked percentage math. Every
zero divisor raises in firmware: old saturated-return notes/stubs were wrong;
current API returns EDOM explicitly, preserving prior effects where applicable.
Checked bytes now forward raw; paired image/text created states vary independently.
Actual shipped divider replaces both stubs.1771396 renderer/276 setter/56 image
comparisons and prior groups pass; full strict host/Unix IPC, ASan/UBSan, ARM/qemu
and all three focused TV CPU units pass. Staging hash checked separately,
removed/no helper, no-reset retained. Build-tv-v3/ui-review identities in report/
MEMORY. No device/screen/settings/network/boot/persistent change, no pending job.
Next feature remains imageCreate; remaining backend/startup/visible gates stay
open. Zero-range crash/error policy must stay explicit in parity claims.

Previous: [SVG-IMAGE](artifacts/reports/SVG-IMAGE-2026-11-03.md) implements
SVGImage::create (0x10c9638) and SVGImage::destroy (0x10c8ef4) orchestration in
common/svg_image.c. create does UIWindow::setId, the SVGContext image-supplier
lookup, the supplier vtable slot2 resource gate, a rectangle copy, an optional
parent getClientRect + EASIRect::offset (x into words 0/2, y into 1/3), then
imageCreate. destroy does SVGWindow::destroy then
SVGSurface::setCleanUISurface(this+0x9c). 18 native/C cases compare against
the verbatim bytes copied at their original addresses, with a fake supplier
object/vtable and the shipped offset fragment. Host, focused ASan/UBSan,
ARM/qemu, full verify.cjs (35 ok lines) and full make check pass, plus a
focused real-TV CPU run of the svg_image ARM unit (exit 0, staging
hash-verified and removed). Boundary: imageCreate (0x10c9430, the
SVGSurface/UISurface/drawImageResource raster and actor subsystem) is still an
ops callback, as are the vendor calls behind the ops. No screen/input/device/
settings/network/service/boot/persistent change. Next: imageCreate internals,
then convertOperationIdToIDM, row construction, child visibility internals/
live resources, then scrollbar/skin/module/display/startup and visible parity.

Previous: [SETTINGS-ROW-SELIMAGE](artifacts/reports/SETTINGS-ROW-SELIMAGE-2026-11-03.md)
implements the row-level setSelectionBoxImageId: store the id at row+0x2e54,
look up the "image" rectangle, destroy row+0x2cf0 unconditionally, then
re-read the id and create from rect+8 when non-zero. The shipped 140-byte body
at 0xd54374 (CRC 0xcf7f04a2) is copied verbatim and called from the copied
renderItems case-0x3 path, so the internals are differentially verified rather
than stubbed. A 15th presence key ("image") widens the sweep to 32768 masks and
1770796 comparisons; verify.cjs's qemu timeout was raised to 180 s because the
run now takes ~33 s. Host, focused ASan/UBSan, ARM/qemu, full verify.cjs
(34 ok lines) and full make check pass, plus a focused real-TV CPU run (exit 0,
staging hash-verified and removed). SVGImage::destroy/create internals remain
stubs. No screen/input/device/settings/network/service/boot/persistent change.
Next: the SVGImage destroy/create internals themselves, then
convertOperationIdToIDM, row construction, child visibility internals/live
resources, then scrollbar/skin/module/display/startup and visible parity.

Previous: [LAUNCHER-IMAGEID](artifacts/reports/LAUNCHER-IMAGEID-2026-11-03.md)
implements the real LauncherHelper::getImageId in
common/launcher_image_id.c. It is a 62-entry tbh switch: op>61 returns -1,
state==1 selects the alternate id and every other state takes the default;
ops 30/31/36/37 share the -1 block, 42/56/57 ignore state, op52 is asymmetric
(825/790). 469 native/C comparisons call the verbatim shipped body at 0xe4aaf4
(copied to 0xe4a000, guard literal 0x01791d68 backed by page 0x1791000) and
match exactly. Host structural checks, focused ASan/UBSan, ARM/qemu, full
verify.cjs (34 ok lines) and full make check pass, plus a focused real-TV CPU
run (exit 0, staging hash-verified and removed). The render ops glyph_id hook
is proven to accept the real function (op2→733/734/734/734). No screen/input/
device/settings/network/service/boot/persistent change. Next: the adjacent
convertOperationIdToIDM (0xe4ae0c), then SVGImage/setSelectionBoxImageId
internals, then row construction, child visibility internals/live resources,
then scrollbar/skin/module/display/startup and independent visible parity.

Previous: [SETTINGS-ROW-17-19](artifacts/reports/SETTINGS-ROW-17-19-2026-11-03.md)
closes the renderItems switch: 0x17/0x18 are progressbar+text2 and 0x19 adds
image1/image2 first. Percentage is
100*(row[0x2dfc]-row[0x2df0])/(row[0x2df4]-row[0x2df0]) with 32-bit wrap and
truncating signed division (silent __aeabi_idiv trampoline); bars are
row+0x1570 (0x17) or row+0x1724 (0x18/0x19). 0x17 alone consults the alexa
rect and row byte 0x2e46 for its name/ball branches; 0x18/0x19 always write
0x1bd/0x1be/0x1c0/0x1bf and apply setDoubleDirectionalBar(1) when 0x2e46 is
set; 0x19's image refresh sets setVisible(1), unlike 0xb. 886060 shipped ARM/C
cases and prior groups pass, plus full host/Unix IPC, focused ASan/UBSan,
ARM/qemu and focused real TV CPU. Division was only exercised with a fixed
10..90 range. No screen/input/device/settings/network/service/boot/persistent
change; staging removed, no helper. Every renderItems case 0..0x1a is
implemented; next is the real LauncherHelper::getImageId mapping and
SVGImage/setSelectionBoxImageId internals, then row construction, child
visibility internals/live resources, then scrollbar/skin/module/display/
startup and independent visible parity.

Previous: [SETTINGS-ROW-11-15](artifacts/reports/SETTINGS-ROW-11-15-2026-11-03.md)
closes the text-entry variants 0x11..0x15 through the shared body: selectionbox
via their own literals, child row+0x18d8, names 0x2ec0/0x2ebc/0x2ec8/0x2ec4 in
slots 1/0/3/2, create(0,0xc,0xd,empty), mode/max 3/10, 4/5, 5/none, 2/4,
9/15, and created sources setText(0x2e0c/0x2e14/0x2e1c/0x2e24) except 0x15's
setNumericContents(0x2e00). 0x13 returns after setEntryMode with no max or
cursor call. 394384 shipped ARM/C cases and prior groups pass, plus full
host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real TV CPU. All
row-side UString reads now use the generic row_ustring. No screen/input/device/
settings/network/service/boot/persistent change; staging removed, no helper.
Next implement cases 0x17..0x19, then getImageId's real mapping and image
internals, then row construction/visibility/live resources, then scrollbar/
skin/module/display/startup/independent visible parity.

Previous: [SETTINGS-ROW-C](artifacts/reports/SETTINGS-ROW-C-2026-11-03.md)
implements renderItems type 0xc, the text2/text3/text4 row with the shared
slide bar: four own key literals, the formatted value for text2, row UStrings
0x2e2c/0x2e34 for text3/text4 with alignment 6 and setElipses only on the
0x1f68 box, and a nameless uncreated slider tail at 0xd55872. 312224 shipped
ARM/C cases and prior groups pass under the full 13-key presence mask plus
separate image/textbox state passes, plus full host/Unix IPC, focused
ASan/UBSan, ARM/qemu and focused real TV CPU. The callee audit caught the
missing SVGTextBox::setTextAlignment trampoline. No screen/input/device/
settings/network/service/boot/persistent change; staging removed, no helper.
Next implement cases 0x11..0x15 and 0x17..0x19, then getImageId's real mapping
and image internals, then row construction/visibility/live resources, then
scrollbar/skin/module/display/startup/independent visible parity.

Previous: [SETTINGS-ROW-B](artifacts/reports/SETTINGS-ROW-B-2026-11-03.md)
implements renderItems type 0xb, the slider bar with image1/image2: three
lookups, both-image-rect gating, a fork on each image's own created state
(plain no-offset setWindowRect refresh, or create from
row[0x2e4c]/row[0x2e50]), the mixed-state quirk that re-creates image1, then
the row+0x1498 slider with 0x214/0x216/0x215/0x217 names, range and
setSliderValue(row[0x2df8]). 74604 shipped ARM/C cases and prior groups pass,
plus full host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real TV CPU.
A full callee audit preceded the sweep; the harness now separates the image
children's created state with a dedicated pass. No screen/input/device/
settings/network/service/boot/persistent change; staging removed, no helper.
Next implement case 0xc, then 0x11..0x15 and 0x17..0x19, then getImageId's
real mapping and image internals, then row construction/visibility/live
resources, then scrollbar/skin/module/display/startup/independent visible
parity.

Previous: [SETTINGS-ROW-A](artifacts/reports/SETTINGS-ROW-A-2026-11-03.md)
implements renderItems type 0xa, the second slider row: its own
"slidebar"/"text2" literals, the fixed 0x1e8/0x1e9/0x1ea/0x1eb text-box names
in slots 1/0/3/2 with no font, the formatted row[0x2df8] value, and the
row+0x1498 slider with 0x214/0x216/0x215/0x217 names plus
setSliderRangeValues on the created path. 70456 shipped ARM/C cases and prior
groups pass, plus full host/Unix IPC, focused ASan/UBSan, ARM/qemu and
focused real TV CPU. A full callee audit preceded the sweep and only the
case's own literal redirects were new. No screen/input/device/settings/
network/service/boot/persistent change; staging removed, no helper. Next
implement cases 0xb/0xc, then 0x11..0x15 and 0x17..0x19, then getImageId's
real mapping and image internals, then row construction/visibility/live
resources, then scrollbar/skin/module/display/startup/independent visible
parity.

Previous: [SETTINGS-ROW-COVERAGE](artifacts/reports/SETTINGS-ROW-COVERAGE-2026-11-03.md)
corrects a silent harness hole: the presence mask iterated only 7 bits, so
image2/banner/slidebar/text2 were never present and type 0x9's case body
never executed. The sweep now runs a full 11-key pass plus a separate
point/word/label/flag pass through one shared `render_case()` helper, and
type 0x9's created slider path is implemented (setWindowRect +
setSliderRangeValues + setSliderValue). 66312 genuine comparisons pass with
host, focused ASan/UBSan, ARM/qemu, full verify.cjs, full make check and a
focused real-TV CPU run (exit 0, staging hash-verified and removed). A third
NUL-sled was caught (SVGSlideBar::setWindowRect 0x10c4790 had no trampoline
and slid into the range stub), so every callee of a new case must be
enumerated before sweeping.

Latest: [SETTINGS-ROW-SLIDER](artifacts/reports/SETTINGS-ROW-SLIDER-2026-11-03.md)
implements renderItems type 0x9, the slider row: "slidebar" then "text2"
lookups, a row+0x1ca8 text box created with the empty char* label and extra 1
that always receives the %d-formatted row[0x2df8] value, and a row+0x13c0
slide bar created from row[0x2df0]/[0x2df4] then setSliderValue from 0x2df8.
95624 shipped ARM/C cases and prior groups pass, plus full host/Unix IPC,
focused ASan/UBSan, ARM/qemu and focused real TV CPU. Two earlier readings
were corrected: types 5/6's rebuild fold reads text1 (slot 12), not image1,
and setScalingType receives the stale isCreated result. No screen/input/
device/settings/network/service/boot/persistent change; staging removed, no
helper. Next implement cases 0xa..0xc, then 0x11..0x15 and 0x17..0x19, then
(getImageId's real mapping and image internals after that). Type 0xa is
partially mapped in artifacts/analysis/settings-row-render-a-2026-11-03/
NOTES.md: the uncreated text-box and slider paths are recovered, but the
created slider branch at 0xd559d0 is not, so nothing was committed.
getImageId's real mapping and image internals, then row construction/
visibility/live resources, then scrollbar/skin/module/display/startup/
independent visible parity.

Previous: [SETTINGS-ROW-BUTTON8](artifacts/reports/SETTINGS-ROW-BUTTON8-2026-11-03.md)
implements renderItems type 0x8, the plain button: "button" lookup,
row+0x684 child, 0x2eac/0x2eb0/0x2eb4/0x2eb8 names in slots 1/0/3/2, icon-0
create and a created setWindowRect+setText refresh, with no images, glyphs,
stores or draw-on-top. 43976 shipped ARM/C cases and prior groups pass, plus
full host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real TV CPU. No
new callbacks were needed and the case has no rebuild branch, so no crash
input. No screen/input/device/settings/network/service/boot/persistent change;
staging removed, no helper. Next implement cases 0x9..0xc, then 0x11..0x15 and
0x17..0x19, then the real getImageId mapping and image internals, then row
construction/visibility/live resources, then scrollbar/skin/module/display/
startup/independent visible parity.

Previous: [SETTINGS-ROW-BANNER](artifacts/reports/SETTINGS-ROW-BANNER-2026-11-03.md)
implements renderItems type 0x7, the banner button: a direct "banner" string
lookup, row+0x684 button, no destroy/images/stores, type-5 names, four glyph
ids from `getImageId(row[0x2dd4], state)`, elipses 1, shadow 0x94, an
icon-carrying create, a created refresh with setWindowRect only (no setText)
and setDrawOnTop(1). 40904 shipped ARM/C cases and prior groups pass, plus
full host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real TV CPU. No
new callbacks were needed. No screen/input/device/settings/network/service/
boot/persistent change; staging removed, no helper. Next implement cases
0x8..0xc, then 0x11..0x15 and 0x17..0x19, then the real getImageId mapping
and image internals, then row construction/visibility/live resources, then
scrollbar/skin/module/display/startup/independent visible parity.

Previous: [SETTINGS-ROW-BUTTON](artifacts/reports/SETTINGS-ROW-BUTTON-2026-11-03.md)
implements renderItems types 0x5/0x6, the launcher button plus image1/image2:
three rectangle lookups, refresh-or-destroy+reinit, type-5/type-6 name sets,
four glyph ids from `getImageId(row[0x2dd4], state)`, highlight/sublauncher/
elipses/shadow, an icon-carrying create, inline 0x92/0x93 scaling stores and
setDrawOnTop(1). 38216 shipped ARM/C cases and prior groups pass, plus full
host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real TV CPU. Two ABI
corrections landed: getImageId is two-argument (op,state), and create_button
carries the icon id. No screen/input/device/settings/network/service/boot/
persistent change; staging removed, no helper. Next implement cases 0x7..0xc
and 0x11..0x15 plus 0x17..0x19, then the real getImageId mapping and image
internals, then row construction/visibility/live resources, then scrollbar/
skin/module/display/startup/independent visible parity. Bodies are not actual
widget/rendering acceptance.

Previous: [SETTINGS-ROW-TEXTENTRY](artifacts/reports/SETTINGS-ROW-TEXTENTRY-2026-11-03.md)
closes the text-entry family 0xd/0xe/0xf/0x10 with one parameterised body:
"selectionbox" lookup, row+0x18d8 child, names 1/0/3/2 from
0x2ebc/0x2ec0/0x2ec4/0x2ec8, empty-label seven-arg create, then per-case
mode/max (0/4, 0xa/5, 0xa/5, 1/10) and automatic cursor; the created source
is setNumericContents(row[0x2e00]) for 0xd/0xe/0xf and setText(row[0x2e04])
for 0x10. 33608 shipped ARM/C cases and prior groups pass, plus full
host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real TV CPU. The
differential caught an un-stubbed setText that slid through zero halfwords;
all twelve family callees are now installed. No screen/input/device/settings/
network/service/boot/persistent change; staging removed, no helper. Next
implement cases 5..12, 0x11..0x15 and 0x17..0x19, then row construction/
visibility internals/live resources, then scrollbar/skin/module/display/
startup/independent visible parity. Bodies are not actual widget/rendering
acceptance.

Previous: [SETTINGS-ROW-13](artifacts/reports/SETTINGS-ROW-13-2026-11-03.md)
implements renderItems type 0xd, the text-entry box: "selectionbox" lookup,
row+0x18d8 child, names 1/0/3/2 from 0x2ebc/0x2ec0/0x2ec4/0x2ec8, empty-label
seven-arg create, then entry-mode 0 / max-chars 4 / cursor-auto; created adds
setNumericContents(row[0x2e00]). 24392 shipped ARM/C cases and prior groups
pass, plus full host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real
TV CPU. Two per-case literal redirects were needed (0xd55140 key, 0xd55144
empty label). No screen/input/device/settings/network/service/boot/persistent
change; staging removed, no helper. Next implement cases 0xe/0xf/0x10 (same
widget, differing mode/max/text), then the remaining bodies 5..12 and
0x17..0x19, then row construction/visibility internals/live resources, then
scrollbar/skin/module/display/startup/independent visible parity. Bodies are
not actual widget/rendering acceptance.

Previous: [SETTINGS-ROW-SELECTION](artifacts/reports/SETTINGS-ROW-SELECTION-2026-11-03.md)
implements renderItems types 0x2/0x3: both look up "selectionbox" and drive
row+0x1014; case 0x3 also calls setSelectionBoxImageId(row[0x2e54]) between
the lookup and the null check. Uncreated names use slot order 1/3/0/2
(0x26b+font when case2 has key1, else 0x2e9c/0x2ea0/0x2ea4/0x2ea8) and an
empty-label create; created adds setSelectedItemWithId(row[0x2de8]). 21320
shipped ARM/C cases and prior groups pass, plus full host/Unix IPC, focused
ASan/UBSan, ARM/qemu and focused real TV CPU. No screen/input/device/settings/
network/service/boot/persistent change; staging removed, no helper. Next
implement switch cases 5..21 and 23..25, then row construction/child
visibility internals and live resources, then scrollbar/skin/module/display/
startup/independent visible parity. Bodies are not actual widget/rendering
acceptance.

Previous: [SETTINGS-ROW-16](artifacts/reports/SETTINGS-ROW-16-2026-11-03.md)
implements renderItems type 0x16: the checkbox case body (names 9/10/11/12,
alignment 5, empty-label seven-argument create, row byte 0x2e44 checked; or
EASIRect math + setWindowRect/setText when created). 7592 shipped ARM/C cases
and prior groups pass, plus full host/Unix IPC, focused ASan/UBSan, ARM/qemu
and focused real TV CPU. Two harness fidelity fixes landed: row label slots
are UString objects and are dereferenced everywhere, and the seven-argument
create uses a wide preserving trampoline. No screen/input/device/settings/
network/service/boot/persistent change; staging removed, no helper. Next
implement switch cases 2..25 except 0x16, then row construction/child
visibility internals and live resources, then scrollbar/skin/module/display/
startup/independent visible parity. Bodies are not actual widget/rendering
acceptance.

Previous: [SETTINGS-ROW-1A](artifacts/reports/SETTINGS-ROW-1A-2026-11-03.md)
implements renderItems type 0x1a: the shared 0x104 tail writes 0x2e8c into
all four name slots and skips setFont, then the case body drives the 0x684
button from the "button" rectangle (0x2edx names + create, or EASIRect math +
setWindowRect/setText). 3032 shipped ARM/C cases and prior groups pass, plus
full host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real TV CPU. No
screen/input/device/settings/network/service/boot/persistent change; staging
removed, no helper. Next implement switch cases 2..25, then row construction/
child visibility internals and live resources, then scrollbar/skin/module/
display/startup/independent visible parity. Bodies are not actual widget/
rendering acceptance.

Previous: [SETTINGS-ROW-RENDER](artifacts/reports/SETTINGS-ROW-RENDER-2026-11-03.md)(artifacts/reports/SETTINGS-ROW-RENDER-2026-11-03.md)
implements renderItems prologue, shared 0x104 tail, type 0/1 label paths and
the type-4 button body with fallthrough back into case 4. 2264 shipped ARM/C
cases and prior groups pass, plus full host/Unix IPC, focused ASan/UBSan,
ARM/qemu and focused real TV CPU. All call shapes and the jump table verified
instruction-by-instruction against objdump/ELF (the Ghidra ASM dump lied).
No screen/input/device/settings/network/service/boot/persistent change;
staging removed, no helper. Next implement switch cases 2..26 (starting with
the 0x1a tail branch), row construction/child visibility internals and live
resources, then scrollbar/skin/module/display/startup/independent visible
parity. Bodies are not actual widget/rendering acceptance.

Previous: [SETTINGS-ROW-SETTERS](artifacts/reports/SETTINGS-ROW-SETTERS-2026-11-03.md)
implements the saved setLabel/setProgressValue/setBarRangeValues bodies.
324 shipped ARM/C cases and prior groups pass, plus full host/Unix IPC,
focused ASan/UBSan, ARM/qemu and focused real TV CPU. Type-selected children,
created gates and always-stores are exact; percent keeps shipped wraparound
and divide edges. No screen/input/device/settings/network/service/boot/
persistent change; staging removed, no helper. Next integrate actual
renderItems/row construction and remaining live resources with these
providers, then scrollbar/skin/module/display/startup/independent visible
parity. Setters alone are not actual widget/rendering acceptance.

Previous: [SETTINGS-ROW-TYPE](artifacts/reports/SETTINGS-ROW-TYPE-2026-11-03.md)
implements setType cache/rebuild-before-render and exact qualified child
visibility ordering.12480 shipped ARM/C cases and prior groups pass, plus full
host/Unix IPC, focused ASan/UBSan, ARM/qemu and focused real-TV CPU. Owned-model
type event joins to actual cached row/child fields; other row setters and widget
rendering remain open. Type24 omits child+1ca8; unknown types affect only the two
labels. No generic hide-all or unconditional post-render flag clear. Saved full
renderItems source in analysis/settings-row-2026-11-03/render-source.txt; label/
progress/range source also saved, not implemented here. Next integrate actual row
construction/rendering and remaining setters/live resources with these providers.
Build-tv-v3/settings-row identities in MEMORY/report. TV temporary CPU staging
size/hash separately verified, test exit0, removed/no helper, no-reset flag kept.
No device/screen/input/settings/network/services/boot/persistent change. No pending
local job/helper. Skin/module/display/startup/independent visible parity stay open.

Previous: [SETTINGS-MODEL](artifacts/reports/SETTINGS-MODEL-2026-11-03.md) adds
owned item/text/value/image providers feeding the existing drawRow dispatcher.
2048 real-provider dispatches, allocation-failure cleanup/retry, source-buffer
overwrite, signed numeric limits, localized default and per-model reentry gates
pass strict full host/Unix IPC, focused ASan/UBSan, ARM/qemu and real TV CPU.
The row sink is a recorder; there was no drawing or device/control operation.
Temporary TV staging size/hash verified separately, removed, no helper remains;
/conf/coredumpnoreset retained. Build-tv-v3/settings-model binary identities in
MEMORY/report. New common/settings_model.c exposes deep-owned decoded snapshots;
no module/service or live DB/resource acquisition yet. Next connect these real
providers to actual item/resource consumers, ui.list/skin and row widgets; finish
scrollbar internals, display/scanout/startup and independent visible acceptance.
No pending local test/Ghidra/helper job.

Previous: [SETTINGS-DRAW](artifacts/reports/SETTINGS-DRAW-2026-11-03.md) implements
the complete SettingItemsListBox::drawRow dispatcher behind parent virtual+0x60.
18441 shipped ARM/C comparisons pass for36 types,16 styles,32 value/image/text
variants and9 bounds exits, plus every prior group. Strict full host/Unix IPC,
focused ASan/UBSan and fresh ARM/qemu units pass. Type writeback9..11->23..25,
mixed9->23, full setter order/image priority and temporary string/vector lifetimes
are explicit. Providers are controlled, not actual UString/vector/widgets or
visible rendering. New common/settings_draw.c, typed context-bound API; no new
module/service. build-tv-v3/settings-draw; identities in MEMORY/report. No TV
access/deployment or pending job/helper. Next compose owned item/value/text/
widget providers with this dispatcher and existing refresh/input chain, then
display/scanout/startup/independent visible acceptance; scrollbar internals open.

Previous: [SVG-LIST-REFRESH](artifacts/reports/SVG-LIST-REFRESH-2026-11-03.md)
implements base/manager single-row refresh and row setChecked. 2880+24 native/C
comparisons plus all prior groups, strict full host/Unix IPC, focused ASan/UBSan
and fresh ARM/qemu units pass. Refresh uses total+8 capped by array slots,
unsigned wrapping upper bound, exact parent/index/row operands. Checkbox cache
stores only after the created-gated setter. Also fixed row-view fixture's
out-of-bounds rv_slots[i*8] pointer arithmetic. Fresh build-tv-v3/list-refresh;
identities in MEMORY/report. No TV access/deployment or pending job/helper.
Next implement the actual parent dispatcher: vtable+0x60 resolves to
SettingItemsListBox::drawRow at d50f60, saved with ARM instructions in
analysis/svg-list-refresh-2026-11-03/parent-source.txt. This callback body is
still pending, alongside scrollbar construction/rendering and owned resource/
widget backend/display binding/startup/independent visible acceptance.

Previous: [ARM-HARNESS-REVIEW](artifacts/reports/ARM-HARNESS-REVIEW-2026-11-03.md)
reviews0493f91 and preceding list work. Fresh ARM corpus153 + all prior groups,
full strict host/Unix IPC, focused ASan/UBSan and ARM animation/widgets/ui.list
tests pass. Corrected owned/context-bound UString handles, UTF8/empty/NULL name
semantics, parse-before-free/equality order and signed counts. Every trampoline
installer checks its recorded mapping; scrollbar10c3f50 now uses10c3000, not
adjacent10c4000. OAL1 restore validates unpublished payloads atomically; temporary
text storage is max-aligned. Module routing is vertical (old prose corrected).
AGENTS has bounded first-fault/ABI/GDB guidance. Host /usr/bin/gdb reads ARM ELF;
bundled GDB lacks ncurses5. Historical19:12 SIGSEGV has no retained core and did
not reproduce in the starting14-case binary: no exact old cause is claimed.
Do not resume crash loops or infer a TV fault. Evidence: arm-harness-review-
2026-11-03; isolated binaries build-tv-v3/arm-review, hashes in MEMORY/report.
Remaining work is actual providers/rendering/parent refresh/display binding/
startup/native acceptance. No TV access/deployment or pending job/helper.

Previous: [SVG-LIST-ANIM](artifacts/reports/SVG-LIST-ANIM-2026-11-03.md)
implements startSelectAnimation gates, rect snapshot and first-match
animation start. 14 native/C comparisons pass with exact call order/args,
snapshot words and started float bits, plus full strict host/focused
sanitizer/ARM and every prior group. No new service/module, TV access/wake/
staging/deployment or pending job. Hashes in MEMORY/report. Next implement
parent refresh, scrollbar construction/rendering internals, and the owned
backend/context/consumer with display/scanout binding, then independent
startup and native visible parity. Animation start is not actual
motion/rendering acceptance.

Previous: [SVG-LIST-SLIDE](artifacts/reports/SVG-LIST-SLIDE-2026-11-03.md)
implements setSlideValue progress/slider/text routing. 72 native/C
comparisons pass with exact call order/args, written values and stored
words, plus full strict host/focused sanitizer/ARM and every prior group.
No new service/module, TV access/wake/staging/deployment or pending job.
Hashes in MEMORY/report. Next implement parent refresh, scrollbar
construction/rendering internals, startSelectAnimation, and the owned
backend/context/consumer with display/scanout binding, then independent
startup and native visible parity. Bodies are not actual widget/rendering
acceptance.

Previous: [SVG-LIST-WIDGETS](artifacts/reports/SVG-LIST-WIDGETS-2026-11-03.md)
implements the widget-level numeric forward and selected-id gates/walk. 21
native/C comparisons pass with exact call order/args and return values, plus
full strict host/focused sanitizer/ARM and every prior group. No new service/
module, TV access/wake/staging/deployment or pending job. Hashes in
MEMORY/report. Next implement row editing (setSlideValue progress/slider/text
paths, setChecked), parent refresh, scrollbar construction/rendering
internals, startSelectAnimation, and the owned backend/context/consumer with
display/scanout binding, then independent startup and native visible parity.
Bodies are not actual widget/rendering acceptance.

Previous: [SVG-LIST-MODULE](artifacts/reports/SVG-LIST-MODULE-2026-11-03.md)
publishes ui.list v1, the first service consumer of the verified SVG-list
layers. Host, focused ASan/UBSan, ARM/qemu and actual Unix-socket IPC pass:
owned 9-item/5-row/16-slot geometry, verified key chain with inert render
counters, list-state/list-key over socket/CLI, OAL1 reload persistence,
suspend/shutdown gates, clean exit with socket removal. No new service/
module beyond ui.list, no TV access/wake/staging/deployment or pending job.
Hashes in MEMORY/report. Next implement widget-level row getters and editing,
parent refresh, scrollbar construction/rendering internals,
startSelectAnimation, and the owned backend/context/consumer beyond
words/counters with display/scanout binding, then independent startup and
native visible parity. Bodies are not actual widget/rendering acceptance.

Previous: [SVG-LIST-UPDATE](artifacts/reports/SVG-LIST-UPDATE-2026-11-03.md)
implements the RowManager row-view getters and the SVGListBoxBase full
refresh. 100+20 native/C comparisons pass with exact call order/args and
resulting list state, plus full strict host/focused sanitizer/ARM and every
prior group. Total is slot arithmetic, last-visible wraps, item-row
bounds-checks; update stores the virtual total then refreshes window,
scrollbar and animation in order. No new service/module/UI consumer, TV
access/wake/staging/deployment or pending job. Hashes in MEMORY/report. Next
implement widget-level row getters and editing, parent refresh, scrollbar
construction/rendering internals, startSelectAnimation, and the owned
backend/context/consumer with permission/modal/focus/lifetime/visible
gates, then demonstrate reloadable modules via IPC/socket/CLI. Bodies are
not actual widget/rendering acceptance.

Previous: [SVG-LIST-SCROLLBAR](artifacts/reports/SVG-LIST-SCROLLBAR-2026-11-03.md)
implements scrollbar create-forward and setScrollPosition gating/store/show.
294 native/C comparisons pass with exact call order/args and resulting bar
state, plus full strict host/focused sanitizer/ARM and every prior group.
Create forwards unchanged; setScrollPosition stores only when created and
signed first stays within signed vis, draws the background only with a clear
+0xc8 byte, then always draws the bar. No new service/module/UI consumer, TV
access/wake/staging/deployment or pending job. Hashes in MEMORY/report.
SVG-LIST-UPDATE implements the row-view getters and full refresh;
widget-level getters/editing, parent refresh, scrollbar construction/
rendering internals, startSelectAnimation and the owned backend/consumer
remain open, then the reloadable-module demonstration. Bodies are not actual
widget/rendering acceptance.

Previous: [SVG-LIST-IMPL](artifacts/reports/SVG-LIST-IMPL-2026-11-03.md)
implements updateImpl and rotateRows, the state core behind every list
key/move/scroll body. 6952 native/C comparisons pass with exact call order/
args and resulting manager/array/selection state, plus full strict host/
focused sanitizer/ARM and every prior group. Start clamping, unfocus gating,
full/partial distinction, per-row show/update/draw, hide sweeps, writeback/
clamp/refocus and the empty-message path are asserted, as is the rotate
pair-swap with full position exchange. Row +0x1C is setFocus and +0x28 is
sret getWindowRect (decimal-28 is 0x1C, decimal-40 is 0x28). No new service/
module/UI consumer, TV access/wake/staging/deployment or pending job.
Hashes in MEMORY/report. SVG-LIST-SCROLLBAR implements scrollbar create
forward and setScrollPosition gating; row getters/editing, parent refresh,
scrollbar construction/rendering internals, startSelectAnimation and the
owned backend/consumer remain open, then the reloadable-module demonstration.
Bodies are not actual widget/rendering acceptance.

Previous: [SVG-LIST-SCROLL](artifacts/reports/SVG-LIST-SCROLL-2026-11-03.md)
implements SVGListBoxBase::updateScrollBar called by every move primitive.
4320 native/C comparisons pass with exact gating, rect math, show and position
sequencing, plus full strict host/focused sanitizer/ARM and every prior
group. Create iff nonzero size with signed size below the signed +0x6c total
or the +0x22c flag; rect from literal list offsets with wrapping arithmetic;
shown iff the list is visible; hide path clears a created bar. The
differential caught a stub-convention defect (r3-clobbering branch to 4-arg
stubs); multi-arg installers now preserve every argument register. No new
service/module/UI consumer, TV access/wake/staging/deployment or pending job.
Hashes in MEMORY/report. SVG-LIST-IMPL implements updateImpl and rotateRows;
row getters and editing, parent refresh, scrollbar construction/position
bodies, startSelectAnimation and the owned backend/consumer remain open, then
the reloadable-module demonstration. Bodies are not actual widget/rendering
acceptance.

Previous: [SVG-LIST-ROWS](artifacts/reports/SVG-LIST-ROWS-2026-11-03.md)
implements the RowManager internals behind every list key/move body: the
+12/+16/+20/+28 field layout, the focusItem switch/store body and the
updatePartial forward. 324 native/C comparisons pass with exact field/ordering/
store semantics, plus full strict host/focused sanitizer/ARM and every prior
group. Getters are raw field reads; selectedItem adds first and selected row;
focus computes wrapping rel, returns on zero count/rel-reach/rel-selected,
otherwise row +28 (0 then 1) and rel store; partial forwards (index,false).
No new service/module/UI consumer, TV access/wake/staging/deployment or
pending job. Hashes in MEMORY/report. SVG-LIST-SCROLL implements
updateScrollBar (gating, literal rect, show, position); updateImpl, row
widget internals, row getters/editing, parent refresh, scrollbar
construction/position bodies, startSelectAnimation and the owned
backend/consumer remain open. Internals are not
actual widget/rendering acceptance.

Previous: [SVG-LIST-MOVE](artifacts/reports/SVG-LIST-MOVE-2026-11-03.md)
implements the eight primitive list move/selection/window bodies called by
the SVG-LIST-BODIES key bodies. 5040 native/C comparisons pass with exact
call order and args, plus full strict host/focused sanitizer/ARM and every
prior group. Window to-top posts partial 0, to-bottom posts unchecked
aux-size; up/down shift first-visible by n with no animation while only the
to-top/to-bottom moves animate; selection bodies focus by item/first-visible
offsets. The list-box versus this+0xf8 receiver split is asserted. No new
service/module/UI consumer, TV access/wake/staging/deployment or pending job.
Hashes in MEMORY/report. SVG-LIST-ROWS implements the RowManager internals
(field layout, focusItem body, updatePartial forward); updateImpl, row widget
internals, row getters/editing, parent refresh, scrollbar/animation bodies
and the owned backend/consumer remain open.
Bodies are not actual widget/rendering acceptance.

Previous: [SVG-LIST-BODIES](artifacts/reports/SVG-LIST-BODIES-2026-11-03.md)
implements the seven lower SVGListBoxBase key bodies called by the
SVG-LIST-INPUT router. 26048 native/C comparisons pass with exact call
order/args and handled truth, plus full strict host/focused sanitizer/ARM and
every prior group. Unhandled on zero +0x1ec count or zero size with no
callbacks; pure size reads repeat as shipped. The differential corrected a
page-down scroll/wrap inversion and proved move/window/animation primitives
run on the list box while only manager queries and focusItem run on this+0xf8.
Select consumes with or without a parent; first/last synthesize group indices
0/count-1. No new service/module/UI consumer, TV access/wake/deployment or
pending job. Hashes in MEMORY/report. SVG-LIST-MOVE implements the eight
primitive move/selection/window bodies; RowManager internals, row getters and
editing, parent refresh and the owned backend/context/consumer remain open.
Bodies are not actual widget/rendering acceptance.

Previous: [SVG-LIST-INPUT](artifacts/reports/SVG-LIST-INPUT-2026-11-03.md) finishes
SVGListBoxBase key/message routing and typed row-value dispatch.999216 native
key cases,49344 message joins and68640 row-value cases pass, with full strict
host/focused sanitizer/ARM and every prior group. Preserve Select/layout/page-
down/page-up/first-last priority, full-width widget aliases and original-message
operands only for first/last/window. Unhandled key never retries Window. Typed
row values distinguish selection IDs/numeric/check getters from two raw caches.
No new service/module/UI consumer, TV access/wake/deployment or pending job.
Hashes in MEMORY/report; source reused from settings-list-2026-11-03/lower-list.txt.
Owner requested finish, handoff, commit/push and stop; await owner resume before
further implementation. SVG-LIST-BODIES implements the seven lower
move/select/page/first-last bodies; primitive row getters/editing/parent
refresh and owned backend/context/consumer remain open. Callback routing is
not actual widget/rendering acceptance.

Previous: [SETTINGS-LIST](artifacts/reports/SETTINGS-LIST-2026-11-03.md) adds the
SettingItemsListBox map and Right/Left/Digit bodies.65600 shipped map/matcher
and138240 native body cases pass, plus full host/focused sanitizer/ARM and prior
groups. Digit returns row presence, not child result; Select routes Right/Left.
Vertical writeback reads original-row value into the record selected AFTER key
delivery; both handles need lifetime pinning. Horizontal mirror swaps fresh
Left/Right events, unlike the parent's Up/Down translation. Missing-record
safety is explicit, outside native valid-tree acceptance. SVG-LIST-INPUT later implements three
lower methods saved in analysis/settings-list-2026-11-03/lower-list.txt: typed
row getSelectedItemValue, SVGListBoxBase key routing and its message map. Lower
move/select/page methods still need investigation. Then owned backend/context/
consumer, parent refresh and native permission/modal/focus/lifetime/visible
gates. No new service/module/UI consumer. Hashes in MEMORY/report. No TV access/
wake/deployment or pending Ghidra/build/helper job.

Previous: [SETTINGS-NAVIGATION](artifacts/reports/SETTINGS-NAVIGATION-2026-11-03.md)
implements navigateInList via callbacks.101384 shipped ARM cases match logical
cursor/item/row actions and final state, with full host/focused sanitizer/ARM
and prior groups passing. Active vector holds item IDs, cursor is not a widget
index; preserve signed viewport comparisons, uint32 wrap and captured geometry.
Explicit invalid-count/cursor bounds are not native bad-tree parity acceptance.
SETTINGS-LIST subsequently implements the three widget bodies saved in
analysis/settings-navigation-2026-11-03/list-keys.txt. Digit returns row presence;
vertical Right/Left reads original-row value into the record selected after
key dispatch. Horizontal mirror uses fresh events. Then resolve lower SVG/row
and parent-refresh behavior and wire the owned backend/context/consumer with
permission/modal/focus/lifetime/native-visible gates. No new module/service or
UI consumer. Native hashes in MEMORY/report. No TV access/wake/deployment or
pending Ghidra/build/helper job.

Previous: [SETTINGS-INPUT](artifacts/reports/SETTINGS-INPUT-2026-11-03.md) adds the
concrete base Settings map and five input bodies.65600 native map/matcher cases
and86400 five-body traces/state/handled cases pass, plus full host, focused
sanitizer, ARM unit, metadata verifier and prior groups. Map differences from
TabDialog include255/2bb, Select, paging, qualified parent and menu consumption;
Mute consumes regardless of lower return. Description sentinel is pointer
identity to BLANK, not equality/null. Horizontal mirror translates fresh events;
ordinary navigation forwards the original. Metadata confirms virtual+84 is
TabDialog::updateSettings; base slot is pure virtual. SETTINGS-NAVIGATION later
adds navigateInList; remaining work includes list widget behavior and owned backend/
runtime context/consumer, retaining parent refresh and native permission/focus/
modal/lifetime/visible gates. No new service/module or wired UI consumer.
Hashes in MEMORY/report; evidence: analysis/settings-input-2026-11-03.
No TV access/wake/deployment or pending Ghidra/build/helper job.

Previous: [TAB-LEAF](artifacts/reports/TAB-LEAF-2026-11-03.md) implements six
dispatched bodies: Tab volume/digits/removal and qualified Settings menu/quit/
guide.238860 shipped ARM comparisons include real signed division, raw overflow,
logical field-write/action order, event identity and handled truth. Full host,
focused ASan/UBSan, ARM unit and prior groups pass. Volume tests isShowing;
digit triggers1104/4725 store first then call qualified SVG end, distinct from
the others' virtual end. Guide posts navigation0xb, not a key; reposts are owned.
SETTINGS-INPUT subsequently implements the base map and five input bodies.
The map's decompile/ARM is already in tab-leaf-2026-11-03/base.txt.
Remaining integration needs the owned backend/
runtime context and queue consumer with modal/focus/permission/lifetime gates.
No new module/service or wired UI consumer. Native hashes in MEMORY/report.
No TV access/wake/deployment or pending Ghidra/build/helper job.

Previous: [TAB-DISPATCH](artifacts/reports/TAB-DISPATCH-2026-11-03.md) adds the
full TabDialog fallback map and pre-handler-first key join.65600 shipped ARM
map/matcher cases and16384 shipped caller joins match route/original identity/
handled truth and pre-action state. Full host/focused sanitizer/ARM and prior
corpora pass. Wrong types/high-bit IDs go to qualified SettingsDialog fallback;
the menu route directly invokes its qualified base method and always consumes.
Both stages pin the same borrowed message; errors do not execute fallback.
No new module/service or wired UI consumer. Next recover remaining base/leaf
handlers, then compose the owned content/renderer/runtime-context adapter and
queue consumer, retaining permission/modal/focus/lifetime gates. Native hashes
are in MEMORY/report. No TV access/wake/deployment or job/helper pending.

Previous: [TAB-PREHANDLE](artifacts/reports/TAB-PREHANDLE-2026-11-03.md) implements
the full outer pre-handler method via callbacks.65664 valid-tree action/message/
state/handled-truth cases match actual shipped ARM; full host/focused sanitizer/
ARM and prior corpora pass. Marker is vertical_tabs; vertical Right can delegate
to base after focus, and native caller consumes zero/nonzero truth. Source-invalid
missing-child trees are not executed; C reports ENODATA, possibly after prior
actions, without changing handled. No new service/module or UI wiring. Next
implement the owned content/renderer/context
adapter, then attach a queue consumer. Keep source/permission/lifetime/visible
acceptance gates. Native identities in MEMORY/report. No TV access/wake/deployment
or jobs/helpers pending. Analysis/reproduction: tab-prehandle-2026-11-03.

Previous: [TAB-EVENTS](artifacts/reports/TAB-EVENTS-2026-11-03.md) implements
the two ordered Up/Down and Left/Right controller bodies using backend callbacks.
1008 actual shipped method traces/final states match under qemu, plus strict
full host/focused sanitizer/ARM tests and previous reference groups. Build
identities in MEMORY/report. Original borrowed message identity is preserved
through GROUP_KEY; adapters must not retain it after return. No callback is
retained; queries reflect state after earlier actions, not a static plan. Codes
are full ICE values, not RC bytes. Source bodies always consume; C API returns0
success/positive errno. This remains unwired to ui_vendor and is not the full
pre-handler/router. Next complete outer focus/permission/modal handling and
connect owned backend content/context. Do not replace the diagnostic shortcut
with unconditional dispatch. No TV access/wake/deployment/Ghidra/helper pending.
Evidence/reproduction: analysis/tab-events-2026-11-03; source bodies reused from
tab-writeback/content-lifecycle, not re-decompiled or probed on hardware.

Previous: [HOTEL-CONTEXT](artifacts/reports/HOTEL-CONTEXT-2026-11-03.md) adds
settings.hotel/readonly hotel-state via real Unix IPC, reading the separate
HMCSettings table. Field mapping+1d/+28/+2d and SQLite narrowing are verified;
saved DB0/1/0 is not a live vendor cache sample. oa_tab_apply_settings combines
held ui.profile facts without overwriting app/media/entry. Settings publication
rollback/removal/closed-writable bugs are fixed; either service hold pins module.
345 shipped row-copy cases, prior corpora, real vendor-SQLite ARM DSO/corpus,
full host/focused sanitizers/IPC and23-source-module host loader pass. Native
identities in MEMORY/report. No TV access/wake/deployment, no jobs/helpers.
[CONTENT-HEADER-OWNERSHIP](artifacts/reports/CONTENT-HEADER-OWNERSHIP-2026-11-03.md)
resolves show+50: remove content's own five full-screen components; parent header
is separate. No pixel/render acceptance follows. Next integrate persisted-row/
profile services into the controller, complete native hotel defaults/writer/cache
behavior and actual app/media/entry/content/key/permission/writeback handling.
No automatic queue consumer or complete visible parity yet. Evidence/reproduction:
analysis/content-lifecycle-2026-11-03; do not repeat raw BSS-pointer reads.

Previous: [TAB-WRITEBACK](artifacts/reports/TAB-WRITEBACK-2026-11-03.md) adds
ui.navigation v2 independent setters + nav-set-tab/item/submenu through actual
Unix IPC. No enqueue/draw/focus/update is implied. v1 prefix remains; OAN2 raw
state saves read legacy OAN1.1792 shipped setter cases/1792 DSO round-trips,
prior corpora, full host, focused sanitizer/ARM and23-module host loader pass.
Native identities in MEMORY/report, no TV access/wake/deployment/jobs/helpers.
Critical correction: Picture content+18 isShowing, +8/+c show/hide. hideSubMenu
checks showing again; old immediate-destruction/resource-validity claim is wrong.
Content interface slots/extrema names and destroy/up-down ordering are now
documented; do not implement a generic save-selection-on-close action. Next
integrate the verified per-event state changes with mutable context/content/
permission/key handling and held ui.profile; no automatic queue consumer or
visible parity is claimed. Analysis/reproduction: tab-writeback-2026-11-03.

Previous: [TAB-CONTEXT](artifacts/reports/TAB-CONTEXT-2026-11-03.md) resolves
network tab37a0 to menu-capability byte9, sourced from software-profile
connectivity==1. common/profile and reloadable ui_profile/ui.profile v1 now
provide copied immutable connectivity/hotel-support facts with firmware defaults.
This is not mutable hotel activation. Saved profile both1; no TV polling/wake.
65281 actual shipped hashes/27 saved-profile lookups plus prior884736/9216
corpora pass. Full host suite, focused sanitizer/ARM-qemu DSO lifecycle and
23-module host loader pass. Build identities in MEMORY/report; no jobs/helpers.
Next hold/release the profile provider in the consumer and recover mutable
hotel/app/media/entry state, descriptions and content permission/writeback;
the queue/UI consumer and visible parity still remain open. Source23 modules
does not mean installed or tested native23-module startup. Example config:
openaurora/examples/ui-profile.conf; durable evidence/reproduction under analysis/
tab-context-2026-11-03. No persistent/recovery/installed ABI changes.

Previous: [TAB-SELECTION](artifacts/reports/TAB-SELECTION-2026-11-03.md) closes
initial unavailable-tab fallback: first created tab, at group index0. Requested
raw tab IDs are translated to creation-order indices; empty group has no focus
but retains initial content0. oa_tab_initial_selection and9216 actual shipped
group-code qemu cases pass, with vector/focus callbacks stubbed. Availability
corpus884736, full host suite and focused sanitizer/ARM tests pass. No TV
access/wake, helper or job pending. Native build identities are in MEMORY/report.
Next acquire actual context/description inputs and recover content permission/
per-tab state/writeback before attaching the lifecycle-safe queue/UI consumer.
Initial fallback is resolved; later key/modal/dynamic availability is not.

Previous: [TAB-NAVIGATION](artifacts/reports/TAB-NAVIGATION-2026-11-03.md) reviews
the pending consumer prerequisites. Availability now matches the actual shipped
ARM function in884736 offline qemu cases; common/tab_policy also implements the
signed item>0/open-submenu initial focus request. QuickMenu/launcher entry,
hotel/profile/app state gate tabs; content/password/network gates can still
refuse focus. Do not unconditionally create five tabs or focus picture content.
Correct key-handler address d700a0; d6faec is updateGlyphAndTitle, and old
per-type Select summary belonged to other layers. Full host suite and focused
sanitizer/ARM tests pass;
no TV connection/wake/deployment. Evidence and reproduction live under analysis/
tab-navigation-2026-11-03. TAB-SELECTION subsequently resolves initial unavailable
fallback; real context/content validity/per-tab state and a safe consumer remain.
The helper alone does not dispatch queued navigation or establish visible parity.

Previous: [NAVIGATION-SERVICE](artifacts/reports/NAVIGATION-SERVICE-2026-11-03.md)
adds reloadable ui_navigation/ui.navigation v1 plus nav-state/nav-operation/
nav-result/nav-take via actual CLI/Unix IPC. Firmware defaults tab/row/open0;
independent retained fields and route-only messages are verified. Queue holds
IDs, take returns current selection, not submit-time state. OAN1 explicit reload
encoding preserves pending requests; normal holds, standby/deferred activation,
publication failure retry/removal callbacks and strict parser checks pass.
Capacity16 is a project bound, not vendor queue parity. Full strict host,
focused sanitizers, ARM/qemu and actual local Unix socket tests pass; host
--check includes new source module22. Native DSO26488/da07746c756b32322156219eceb36142/
GLIBC2.4; test482168/feeab5416f0c2be1e926db176ece119c/GLIBC2.17, isolated
build-tv-v3/navigation-service. No TV connection/wake/staging/deployment or jobs.

Next attach the lifecycle-safe focus/modal/drawing consumer. The service queues
but never draws or constructs TabDialog, and does not infer modal results from
Select. Actual TabDialog initialization/state writeback, production queue
pressure contract, guarded4b/a6/provider actions and real visible acceptance
remain open. Existing captured SCREEN_SETTINGS Select->full-list is still a
diagnostic shortcut. nav-take is destructive; nav-state is the read-only query.
Last imported TV session reports suspension, not a fresh state poll.

Previous: [LAUNCHER-ROUTING](artifacts/reports/LAUNCHER-ROUTING-2026-11-03.md)
adds common/launcher_route: all62 operation-ID -> signed dialog results and28
ordered unconditional settings plans. Fresh ARM/decompile closes the conditional
MainLauncher active-item Select -> converted endDialog result -> DemoApp modal
return -> OperationFinder -> front navigation13 -> TabDialog(true,false) path.
This is distinct from QuickMenu/navigation12; earlier focus/edit/sublauncher
branches can intercept Select, so no universal live key recipe is claimed.
Base network retains the row; base categories retain open-submenu state. Plans
preserve setter/queue order. Queue insertion is not immediate dialog dispatch.
Logging/telemetry are excluded. Guarded a6 and portal-prework4b remain unsupported.

Full strict host/focused sanitizers/ARM-qemu pass. Independent corpus matches
all62 signed ELF table values and28 ordered source-derived action sequences.
Native test build only26996/MD5 33b69aae50bed048b2c73a592d7c1acf/GLIBC2.4,
build-tv-v3/launcher-route. No TV connection/wake/staging/keys or pending helper.
NAVIGATION-SERVICE subsequently adds the queued state/IPC provider; the
navigation/focus/modal/drawing consumer remains to be integrated;
existing SCREEN_SETTINGS Select->full-list is a diagnostic shortcut, not this
vendor flow. Installation/runtime/provider gates and TabDialog construction/
retained state/visible acceptance remain open. Do not execute plans on every
Select, serialize raw native structs, or copy excluded vendor logging/ad work.

Previous: [AGENT-WORK-REVIEW](artifacts/reports/AGENT-WORK-REVIEW-2026-11-03.md)
reviews ff89e27..e7e7333. Fixed scanner key-pointer map bounds, incomplete/
unterminated key acceptance, snapshot virtual-offset translation and escaping.
Fresh firmware proves+ac lookup keys can survive later current-UString updates:
scanner always retains+a4 metadata, which is not the text buffer. Full strict
host, focused sanitizers and ARM/qemu pass; final native build only is37392
bytes/MD5 dc22cd80cc1d6b0f5e26ba730c3186e9/maxGLIBC2.7 in build-tv-v3/
review-latest. No TV connection/staging/deployment or helper/job pending.

Other-agent menu evidence is now indexed:154 case-local navigation call lists
match fresh decompile, and SubLauncher setFocus/isShowing slots are verified.
TabDialog was NOT inventoried by the scanner: old absence/ownership claims
were too strong and are corrected. VK/DIAL1043 and ICE593/0x251 are distinct
values; app-log receipt of1043 is not ICE-handling proof. Two recorded no-effect
tests do not establish which gate failed; do not repeat them unchanged. The
latest imported session reports TV suspended; this review did not poll/wake it.
LAUNCHER-ROUTING subsequently closes the conditional result conversion/dispatch;
key/profile/focus gates and the live route remain open. Then obtain a
matched full-list instance/current text/draw state before resolving the apparent
slider-label+30. No fixed indent or shortcuts-to-full-list conversion is proved.

Previous rendering checkpoint: [UI-GLYPH-ROUTING](artifacts/reports/UI-GLYPH-ROUTING-2026-11-03.md)
closes conditional dispatch: TextCommand passes its context to drawCharacter;
nonnull GPU getter selects OpenGL drawText, null selects CPU fallback. Saved
vtables/context verify nonnull configuration, not captured-frame execution.
Probe explicit opaque base178,29,43,255 gives1318 white core/1072 differences
vs vendor1446. Transparency alone is NOT a fix. RGB is a background sample;
title alpha255 is an explicit comparison condition, not independently proved.
Next establish actual parent paint/active FBO/presentation for a matched complete
scene comparison. Do not repeat bare shader/precision/opaque-base experiments
or refit the established23,5 origin. ui.title stays default-off.

At that checkpoint the owner requested handoff/stop. Its final native
build-tv-v3/glyph-background/egl-glyph-probe161920/MD5
e59d750f830df849618c4898281778bd/maxGLIBC2.7; host GLES/focused sanitizers and
real private-pbuffer synthetic/font corpus pass. Separate hash verification,
download hash and cleanup complete. Same boot/fb1 backing/pan/access/privacy;
no keys/window/swap/restart/settings/persistent mutation or helper/job pending.
Private output gpu-title-opaque.rgba stays in workbench/title-horizontal-20261103.
Offline verifier: analysis/ui-glyph-routing-2026-11-03/verify-route.cjs.

Foundation: [UI-FONT-GPU](artifacts/reports/UI-FONT-GPU-2026-11-03.md) adds a lower
coverage-glyph API with shipped shader/quad/UV/blend and RGBA FBO composition.
Full strict host, focused sanitizers, ARM/qemu geometry and owner-approved native
private-pbuffer synthetic/title checks pass. No glyph broker/wire/UI integration
or visible parity. One-pass opaque core1971/vendor1446 differs525; recovered
two-pass primitive1228 differs1150. Matching native varying declarations gives
the same full FBO output hash; this is NOT a pixel/precision fix. Keep ui.title
default-off. UI-GLYPH-ROUTING supplies the subsequent conditional route and
opaque-parent checks; complete scene/presentation parity is still open.
Configured framework->task->client->renderer->context association was read,
not execution-traced. Old Surface06b40840 was freed/rejected; do not dereference
saved heap pointers without fresh bounds/vptr/lifetime validation.

Native final build-tv-v3/glyph-gpu/egl-glyph-probe160268 bytes/MD5
772970e414644b47ec2c3b415a7a1cbd/maxGLIBC2.7; separate size/hash verification
before each run. Private corpus8294400/25e09914c06a252c31cf92de49d419ef.
Temporary files/helpers absent; same boot/fb1 backing41735daa69dc137743b59efe12db4a19/
pan0,1080/access/privacy, no keys/window/restart/settings/persistent mutation.
No helper/job pending. Real raster/font bytes remain ignored under workbench/
title-horizontal-20261103; evidence and shader extractor in analysis/ui-font-gpu-
2026-11-03. GPU calls stay on an owning worker/helper, never the host loop.

Established placement: [FONT-X-OFFSET](artifacts/reports/FONT-X-OFFSET-2026-11-03.md)
corrects the old zero-getter claim: ARM b4247c/b42482 returns negative signed
font+5/global minimum-left. Font18 e9 ->23, font7 ->18. Boxed layout applies
that metric; title origin23,5/ink136..388,31..85 matches retained graph and
capture bounds. MODE's31->49 is explained; indentation/focus/styles remain open.
[UI-TITLE-LIVE-GRAPH](artifacts/reports/UI-TITLE-LIVE-GRAPH-2026-11-03.md) owns
the saved Surface108,5,691,95/LineObj23,5/exact text/unit ratio observations;
[UI-TITLE-BOX](artifacts/reports/UI-TITLE-BOX-2026-11-03.md) establishes vertical
Y5. Their old X-assignment/zero-getter/shift hypotheses are superseded, not next
work. CharClipping+1b1 is not bold. Early launcher captures do not prove a MAU
defect. Do not re-investigate parent translation to implement the closed X offset.

Previous: [UI-WORK-REVIEW](artifacts/reports/UI-WORK-REVIEW-2026-11-02.md)
reviews the subsequent agent's work through6822fce. Fixed an ASan-reproduced
scanner overflow (new+2b0 fields require2b1 readable bytes, not224) and ungated
ui.edit_init; added exact/truncated-span, disabled/invalid preset, default-off
title and partial-subscription cleanup coverage. Full strict host/focused
ASan-UBSan/ARM-qemu checks pass. Isolated native-target build in build-tv-v3/
review-20261102; no TV operation/deployment in this review. Prior live scan does
contain a created font18 title candidate matching window57,5,691,95/local text
51,0,634,90 and actual list/row geometry. Its non-atomic nature and missing draw
controls do not definitively isolate the remaining ink delta. ui.title remains
default-off; no fitted offsets, complete FA styles or real slider parity claimed.
Next inspect title drawText runtime controls/ratios/spacing from firmware, then
style/interaction integration and visual acceptance. Research tree formerly in
/tmp/opencode is preserved at artifacts/workbench/agent-ui-review-20261102;
see report for tests/hashes and source corrections. No helper/job pending.

Previous: [UI-ROW-LABELS](artifacts/reports/UI-ROW-LABELS-2026-11-02.md)
finishes the offline ordinary-label source trace: key constructor -> stored
UString -> drawRow -> setLabel adds no prefix. Shipped langprofile section
Languages/value Dictionary resolves MODE hash5aa5907f to record987; dut slot10
is exactly Modus (4d6f64757300), eng slot0 Mode. Translation-fit mode instead
selects maximum-width translation; its live flag remains unread. Durable
analysis/ui-row-labels-2026-11-02 includes ARM/decompiles and verify-label.cjs;
run node on that script from root. Hash-checked bounded2435x41 traversal passes.
This rules out hidden padding in stored Dutch MODE, not actual instance/text/
parent/resource/ratio placement. No guessed18px fix, renderer change, production
localization loader or visual acceptance. Next establish actual transform/
instance inputs before replacing fitted origins. Owner confirms awake HDMI1
video; this investigation was entirely offline, with no TV connection/staging.
Last verified TV/access/privacy state remains the sample below. Handoff updated;
stop here as requested. No helper/build/Ghidra job pending.

Previous: [UI-LIVE-OBJECTS](artifacts/reports/UI-LIVE-OBJECTS-2026-11-02.md)
adds read-only tools/re/ui-object-scan.c. Firmware vptr/field inventory checks
exact readable private [heap] maps with256MiB cap, no ptrace/input/functions/
memory writes. Full strict host/focused sanitizers/ARM-qemu tests and native
scan pass:4 heap ranges/176934912 bytes,6 uncreated textbox candidates, no
settings/list/row/dialog match IN THAT SUBSET. Not proof of process-wide absence;
do not repeat unchanged scan to establish placement. Stock firmware hash verified.
Owner reported awake HDMI1 external video; blank OSD/WS OTHER are not standby
or crash evidence. One Menu1048 POST got201 without observed menu effect; no
further navigation/power/device operation followed the owner report. This turn
returned to view-only work; AGENTS still owns standing authorization. Old
tools/wspoll.c has concrete frame-buffer/short-read/state bugs and was not run;
host Node WS query used instead. Fix it before reuse. Native build-tv-v3/ui-scan,
analysis/ui-live-objects-2026-11-02, ignored screenshots workbench/live-ui-20261102.
Same boot/live1785/access/privacy; staging removed, no helper/job pending.
Actual settings parent/text/resource/ratio and skin/visible parity remain open.

Previous: [UI-TEXT-VIEWPORT](artifacts/reports/UI-TEXT-VIEWPORT-2026-11-02.md)
adds common/font_box.c: widget1..9 alignment bridge -> vertical/line render,
local text-surface clipping, center trim/end reverse. Actual460->489/488
surfaces +font7/20 render54 offscreen lines; local Y2/1. Full strict host,
focused sanitizers/ARM-qemu/native TV CPU corpus passes. Native build-tv-v3/
font-box, analysis/ui-text-viewport-2026-11-02. Fresh source rules out fixed
padding from Settings/Picture/SVGDialog margin slots (the earlier BitmapFont
zero-X claim is corrected by FONT-X-OFFSET),
base-list ctor insets or the setLabel setter. Actual text may still carry
spacing/markup; original parent/window/resource/ratio state remains unverified.
For the explicit test parent the regular origin is31,184, not the old fitted
49,184: do not add a guessed18px adjustment. Next establish ACTUAL instance/
text/ratio state, then skin integration and visible FA/UFA parity. Existing
UI fits unchanged. Same stock boot/live1785/access/privacy; staging removed,
no helper/job pending. This renderer supports plain single-line unit ratios,
not complete shaping/localization/retained styles.

Previous: [UI-TEXTBOX-COMPOSE](artifacts/reports/UI-TEXTBOX-COMPOSE-2026-11-02.md)
adds oa_widget_single_line_textbox: normal fresh/unlocked single-line geometry,
alignment/font marker policy, full-height horizontal slices (no cap subtraction)
and textPos float32 height correction. Actual460->489/488 +20 live startup
font metrics yield text heights66/70 and fonts7/20 for an explicit parent.
Raw text colors are stroke #FFFFFF/#7E000A; fill is none. Pre-mask textPos
explicitly ENOTSUP: complete second-pass/cache semantics remain open for that
layout. Full strict host/focused sanitizers/ARM-qemu/native TV CPU tests pass.
Native build-tv-v3/textbox; analysis/ui-textbox-compose-2026-11-02. Same stock
boot/live1785/access/privacy; staging removed, no helper/job pending. Fresh
constructors confirm default row-items460 and PictureSettingsDialog resource179;
MB667 comparison does not establish active selection. Next ACTUAL parent/window
state and drawText/TextRenderer glyph origin/ratio/alignment/clip, then visible
FA/UFA acceptance. Existing captured panels/fitted text origins remain unchanged;
do not add a fit-derived offset to incomplete coordinates.

Previous: [UI-ROW-WINDOWS](artifacts/reports/UI-ROW-WINDOWS-2026-11-02.md)
adds common/widget_layout.c for ordered rowItems/mRowM geometry and absolute
child windows. renderGroup clears/builds the transform's SAME container: first
mask is raw, earlier items identity, later children use cached first mask.
Do not prelookup all raw masks then transform everything. Actual460 derives
text1/labelLong/text2 windows for an explicit749x70 parent; not a live object
measurement. Full strict host/focused sanitizers/ARM-qemu/native TV CPU checks
pass; build-tv-v3/row-windows, analysis/ui-row-windows-2026-11-02. Fresh durable
renderItems d54400/base10bf06c/renderGroup ef2330 replaces missing /tmp logs.
ID48 pointer chain confirms rowItems. Animation-group path explicitly ENOTSUP
due to renderer seek/depth quirks; no such tag in460. Same stock boot/live1785/
access/privacy; staging removed, no helper/job pending. Next actual row window
and textbox slice/textPos/foreground/font/clip composition, then visible FA/UFA
acceptance. Raw488/489 slices saved; no new fitted offsets. Existing UI origins
and captured panels unchanged.

Previous: [UI-THEME-SKIN](artifacts/reports/UI-THEME-SKIN-2026-11-02.md)
wires ui_vendor to held ui.theme views for DialogLauncher's first-present
animation marker policy (57..64 -> types1..8). No borrowed view survives
selection; active animation keeps a copied mode through theme removal. Late
bind, forced removal, suspend/resume/replacement and failed-start release
pass full strict host/focused sanitizer/ARM-qemu/native TV policy tests.
Live pack selects actual mode60/type4. Recorder display/injected progress;
no new visible acceptance or device/display operation. ui.launch_anim remains
default0: vendor main launcher does not itself start that stand-in transition.
No-provider fallback4 and ui.fs_type lab override retained. Native build-tv-v3/
theme; analysis/ui-theme-skin-2026-11-02, same stock boot/live1785/access/privacy,
all staging/fixtures removed, no helper/job pending. Next actual settings
rowItems/group transforms, widget windows and textbox/font/clip origins. Row
renderBackground10bec08 is durable in UI-MASK-LAYOUT's list-row-layout.txt;
renderItems d54400 literals are in UI-TEXT-SETUP README, older raw /tmp extracts
are gone. Captured panels/fitted text positions remain unchanged.

Previous: [UI-THEME-PROVIDER](artifacts/reports/UI-THEME-PROVIDER-2026-11-02.md)
adds common/svg_pack.c and mod_ui_theme / ui.theme v1. Explicit index/binary
paths load a fully validated owned immutable snapshot; record(index)/named(CRC)
views require a service hold through all borrowed data use. Normal holds pin
provider; standby retains data; suspended replacement defers first I/O until
resume. Failed replacement has no old-binary rollback, no stale service;
explicit load recovers. Full strict host/focused sanitizers/ARM-qemu/native TV
real-DSO lifecycle+702-record corpus checks pass. Source now21 modules; native
isolated build-tv-v3/theme, analysis/ui-theme-provider-2026-11-02. Preserved
16-module stage/installed ABI1 unchanged. Same stock boot/live1785/access/privacy;
all staging/fixtures removed, no helper/job pending. Next actual UI consumer
lookup/release/rebind and state/group/window/slice/textbox/font composition,
then FA/UFA visible acceptance. This provider has no renderer; current captured
panels/fitted text origins unchanged. examples/ui-theme.conf documents paths.

Previous: [UI-BINSVG-VIEW](artifacts/reports/UI-BINSVG-VIEW-2026-11-02.md)
adds common/binsvg.c: bounded immutable single-record view, element iteration
and whole-record/first-group named rectangle lookup. Firmware confirms first
duplicate wins; nested groups included, later same-name groups not searched.
All702 records/11888 elements/6733 rectangles/39 animations pass host,
ASan-UBSan, ARM-qemu and native TV checks; full strict host suite passes.
Actual477 named slices feed existing measurement/stretch/row helpers; explicit
750x826 window remains a test input, not a live widget measurement. Native
build-tv-v3/binsvg, analysis/ui-binsvg-view-2026-11-02. Stock boot/live1785/
access/privacy unchanged; staging removed, no helper/job pending. Next indexed
resource loader/ownership and reloadable immutable theme provider, then actual
widget/textbox styles, transforms, viewport/clip and origin integration. Current
UI fitted origins/captured panels unchanged; reader does not draw or load files.

Previous: [UI-HORIZONTAL-ORIGIN](artifacts/reports/UI-HORIZONTAL-ORIGIN-2026-11-02.md)
adds oa_font_text_layout_line/draw_line: plain EF41 unit-ratio start/end/center,
physical width distinct from pen advance, symmetric overhang trim/reset-X,
end right-bearing and reverse paint order. Correct earlier final-advance claim:
ARM reads byte3=right, not byte4=advance. Whole line/range/origins validated
before paint; word justification4 explicitly ENOTSUP. Synthetic overlap test
checks actual paint order;30 real font7/16 lines pass host/sanitizer/ARM-qemu/
native TV offscreen checks, full host suite passes. Isolated build-tv-v3/font-line
and analysis/ui-horizontal-origin-2026-11-02 identify evidence. Same stock boot/
live1785/access/privacy; staging removed, no helper/job pending. Next concrete
row/textbox style, viewport/clip and vertical-origin integration from runtime
resources; baseline/shaping/localization/word justification/full truncation and
focused text acceptance remain open. Existing UI fitted origins unchanged.

Previous: [UI-TEXT-ORIGIN](artifacts/reports/UI-TEXT-ORIGIN-2026-11-02.md)
confirms client-margin slot+38 binds the zero-margin base getter for actual
list/setting-list/row/setting-row/textbox classes. common/text_origin.c now
implements vertical start/end/center/spread with center clipping-capacity
adjustment, plus per-line floor((ref+y+fontOffset)*ratio)-floor(ref*ratio).
Center divides toward0; ratio rounding floors. Bitmap logical height is signed
extremes height; its Y offset0. Strict full host/focused ASan-UBSan/ARM-qemu/
native TV contracts pass. Build-tv-v3/text-origin; analysis/ui-text-origin-
2026-11-02. Same stock boot/live1785/access/privacy; staging removed, no helper/
job pending. Next horizontal placement/truncation/direction (Line1134600,
TextRenderer1133050), actual widget style/ratios and coordinate composition,
then theme/UI integration. Do not guess right-minus-width: firmware end path
uses final-glyph right ink boundary/style reversal (the earlier advance claim
was corrected above). Optional live labels remain fitted.

Previous: [UI-SLICE-STRETCH](artifacts/reports/UI-SLICE-STRETCH-2026-11-02.md)
implements retained double-ratio measurement, horizontal rectangle stretch and
middle-surface construction in common/text_layout.c. Relative X and width
truncate independently; height unchanged; nonpositive mask width retains ratio.
Caller owns mask-cache validity (surface setter does not reset it). Test composes
raw477 slices through measurement/stretch/painted fields/row placement, deriving
height70,width749,insets0/1 for explicit750px window, without capture fitting.
49,538 independent VFP rectangle cases pass ARM/qemu and native TV; strict
full host/focused sanitizers pass. Native build-tv-v3/slice-stretch; analysis/
ui-slice-stretch-2026-11-02. Same stock boot/live1785/access/privacy; staging
removed. Row create adds parent client origin; client rect applies virtual
margins, base getter zeroes. Next confirm subclass margin binding and actual
child windows, then TextFactory/LineObj origin/baseline/alignment and runtime
theme/UI integration. Optional live text origins remain fitted. No helper/job pending.

Previous: [UI-LIST-METRICS](artifacts/reports/UI-LIST-METRICS-2026-11-02.md)
adds raw-slice measurement, visible-count update and painted-row cache updates
to common/text_layout.c. calculateHeight resets top/bottom/steps but preserves
row size/side insets; count runs before painted insets, last rowPos wins, missing
rowPos preserves state. Raw475/477 slices give step70/top16/bottom10. Older
widget name expansion is withdrawn: literal→char*→string, not four-entry variants;
normal IDs14/17/18/22/23 are mTopL/mRowL/mRowM/mRowR/mBotL. Fresh pointer
chains/raw ARM retained. Full strict host/focused sanitizers/ARM-qemu and native
list_metrics TV test pass; same boot/live1785/access/privacy, staging removed.
Native build-tv-v3/list-metrics, analysis/ui-list-metrics-2026-11-02. Next row
manager/window-coordinate step, complete slice/stretch transforms, then text
origin/alignment; no fitted UI origins changed. No helper/job pending.

Previous: [UI-MASK-LAYOUT](artifacts/reports/UI-MASK-LAYOUT-2026-11-02.md)
adds oa_text_rect_position_mask and oa_list_row_rect to common/text_layout.c.
Mask numeric cases0/1/3/5 left-top,2/6 right-top,4/7 left-bottom,8 right-bottom;
unsupported values only subtract mask origin, absent mask leaves input alone.
No resizing. Row formulas cover vertical/horizontal/mirror (mirror uses actual
window.right). Inputs are explicit runtime fields; no guessed theme constants.
Strict full host/focused ASan-UBSan/ARM-qemu/TV pass, including previous49,728
VFP comparisons. No display/device operation; same stock boot/PID/access/privacy,
staging removed. Native build-tv-v3/mask-layout and analysis/ui-mask-layout-
2026-11-02 identify evidence. Next trace population of list/window/slice metrics
and text origin/baseline, then integrate complete layout before replacing fits.
Fresh list-row-layout.txt includes getRowPosition10bbaa8 and row background
10bec08. No helper or Ghidra job pending. Goal remains active and incomplete.

Previous: [UI-TEXT-GEOMETRY](artifacts/reports/UI-TEXT-GEOMETRY-2026-11-02.md)
reviews443ebd0 and closes textPos consumption: single-line branch10c6396→
6b7a copies textPos into+1ec, then foreground10c7a18..26 consumes it;
+254 is separate addition. RectHandler/constructors prove corners x0,y0,x1,y1.
Manual rescale10c624e..6296 changes **height**, with float32/truncation toward
zero, not width/double/round-half-away. common/text_layout.c implements this
bounded arithmetic;49,728 ARM VFP comparisons pass qemu and real TV, strict
host/sanitizer/full suite pass. No device/display access. Same stock boot/PID/
access/privacy; staging removed. build-tv-v3/text-geometry and durable analysis
ui-text-geometry-2026-11-02 identify evidence. Next resolve slice/mask alignment,
actual row allocation and text origin/baseline; integrate complete geometry
before replacing fitted origins. Full goal resumed by owner; no helper/job pending.

Previous: text-setup firmware decode (analysis/ui-text-setup-2026-11-01, commits
61ab377..df42937 plus four follow-ups) mapped the settings text pipeline
without writing UI code, per the firmware-first rule: state-indexed list/row
resources, row widget slots + per-state FA/UFA/FI/UFI ids, renderItems
branches, PictureSettingsDialog row inventory (type-9 sliders ids 1-4,
type-2 selections, type-4 buttons) with onRightLeftPress/onSelectPress/tab
semantics, TabDialog tab composition (tab 0 = PictureSettingsDialog),
asm-verified TextBox branches (+0x1b4 ternary, alignment bitmask), the +0x1b4
writer (create's 4th arg; rows pass 1), the value display path (setSlideValue
→ +0x1ca8 value textbox), the full 53-site widget-name table (textPos twice,
font-marker scan, alignment interleave), and the text-rect rescale formula
x1 = x0 + (A/B)*span with position preserved, and the container record layout
{UString name; coords} with +0x254/+0x1ec both receiving looked-up rect names.
The textPos/addition question and incorrect axis/rounding/name claims are
corrected by UI-TEXT-GEOMETRY above and at source. Title/tab/slider/focused text
stay captured; optional live labels still use fitted geometry. Old pause was
revoked by owner resume. Do not look for the vanished /tmp/opencode logs.

Previous: [UI-GLYPH-DRAW](artifacts/reports/UI-GLYPH-DRAW-2026-11-01.md) implements
common/font_draw.c, an allocation-free clipped single-glyph RGBA compositor from
firmware bearings/coverage/source-over rules. All52022 shipped glyphs render to
offscreen scratch canvases with exact coverage/border checks. Host full249 plus
three font groups, focused ASan/UBSan, ARM/qemu and TV pass. No screen/device
operation; same stock boot/PID1785/access/privacy, staging removed. Native
build-tv-v3/font-draw and analysis/ui-text-layout-2026-11-01 identify evidence.
Font markers now resolve regular/bold/light/italic/boldItalic/lightItalic types
0..5 with first-present priority. Focused label488 has fontBold/textPos height42;
unfocused489 has no font marker/height40. Recolored captured ink is not focused
text parity; correction added at its source. Next trace actual row line-count
setup and transformed textPos, origin/baseline/alignment/spacing/substitution,
then connect text through the UI engine. Report lists exact fresh entry points.
No helper/build/Ghidra job is pending; goal remains incomplete and paused at
the owner's request. Do not resume hardware experiments from older paragraphs.

Previous: [UI-FONT-SELECTION](artifacts/reports/UI-FONT-SELECTION-2026-11-01.md)
implements common/font_finder.c: startup20 IDs and maximum heights recovered,
stable typed floor selection after signed height/line-count division, global
smallest fallback (font8 even for too-small type0). Font IDs directly index
decoded records. Strict full host249 plus two font groups, focused sanitizers,
ARM/qemu and both tests on TV pass. Same stock boot/PID/access/privacy, staging
removed. build-tv-v3/font-selection and corresponding analysis identify tests.
Continue SVGTextBox text-style type/rectangle/line count, baseline/alignment,
substitution/clipping/blending and UI engine font/text integration. Widgets
still render captured/reconstructed text. No helper or Ghidra job pending.

Previous: [UI-FONTS](artifacts/reports/UI-FONTS-2026-11-01.md) recovers the
theme's38 EF41 grayscale bitmap fonts, signed metrics and packed UTF8 lookup.
common/font_bitmap.c is a bounded borrowed-data reader; all records pass host,
ASan/UBSan, ARM-qemu and TV checks. Full host suite passes. No UI text rendering
is wired yet. Font-finder population/selection is recovered above; continue SVG text
style/font selection, baseline/alignment/fallback; do not substitute guessed
FreeType/browser Fontconfig. Old native-row FreeType claim corrected at source.
Native build-tv-v3/font-bitmap and analysis/ui-fonts-2026-11-01 hold evidence.
Same stock boot/PID1785/access/privacy; all staging removed, no helper/job pending.

Previous: [SKIN-SELECTION](artifacts/reports/SKIN-SELECTION-2026-11-01.md) adds
`skin NAME`/`skin` to socket/stdin control and configured selection after autoload.
Missing/start/enter/first-frame failures load vendor only after safe candidate
unload, including retained failed-init DSOs. Busy cleanup is never forced.
ui.skin v1 is a copied status companion; ui.engine stays v2. Runtime override is
memory only. Host249/focused sanitizers/Unix socket/native contracts and actual
GPU Omarchy/vendor/missing-skin fallback pass; full backing/pan/MAU/region
restored. Same stock boot/PID1785/access/privacy; all staging removed. Native
build-tv-v3/skin-selection and analysis manifests identify the tested bundle.
Source20/preserved production16/installed ABI1 remain separate; no persistent
deployment. Next return to vendor text/focus/content parity; Omarchy is still
a skeleton. No exact cadence, cold/TV standby or process-crash recovery claim.
No probe is pending.

Latest review through670ce3c: [UI-ENGINE-REVIEW](artifacts/reports/UI-ENGINE-REVIEW-2026-11-01.md)
fixes queued foreign engine code/payload leaks, companion rebinds, both skins'
re-registration, timer failures, malformed assets, failed GPU receipts and
inherited transforms. Current service is ui.engine v2; matching skins register
their own dispatch trampoline. Old register_skin fails ENOTSUP. Host242 checks,
focused sanitizers/native contracts and actual GPU vendor-instant Select,
11 rail revisions/reload/module cycle/BGRA alpha/full restore pass. Same stock
boot/PID1785, backing41735daa69dc137743b59efe12db4a19/pan0,1080/access/privacy;
all staging removed. No persistent deployment. Native artifacts are in
build-tv-v3/ui-engine-review; final-tested.md5/final-stage.md5 in the analysis
directory identify them. Initial transform timeout was restored and fixed;
tv-ui-final.txt owns accepted hardware evidence. The lab transition now needs
an explicit diagnostic flag. Content/fallback/visual gates below remain open.
No probe or job remains pending.

Current UI seam and screens (2026-11-01): `ui.engine` (modules/ui_engine) owns
display/alpha discovery and rebinding, OAI1 assets, draw/present/async receipts
and readiness, the key topic and the `ui.fps` repaint timer; it selects the
active skin by `ui.skin` and posts skin enter/paint/key to the skin's owner
token. `ui_vendor` (renamed from ui_fb) is the vendor skin and still publishes
OA_SVC_PORTAL; `ui_omarchy` is a skeleton skin (dark shell/rail/tiles, own key
handler). tests/ui_skin switches vendor<->omarchy by unload/load with no
rebuild. Full make check 236 ok.

Implemented vendor screens, all entered and left from the portal:

- source selection (DialogSourceSelection 185), fb1-verified against the
  R/B-corrected p0/06 capture (RMSE 0.28%, AE 0.0073%).
- captured Beeld quick tiles (p0/03) via rail 4; p0/03 is
  SubLauncherDialog::initialisePictureSettingsShortcuts.
- captured full picture panel (shot-settings.png; DialogSettings_tab 179) via
  Select on the quick tiles; panel area opaque, rest of the OSD transparent
  (fb1 alpha 0 outside, 255 inside), RGB identical to the corrected capture.
- Up/Down row selection over the panel using a reconstructed FA row: 479
  stretched #FE9419 bar, 488/496/504 #7E000A text, 70 px pitch, disabled
  "Beeld zoomen" row skipped, `ui.sel=N` lab preselect. fb1 matches the
  reconstructed row-4 reference exactly; this is not a vendor focused-row
  capture.

Canonical state is MEMORY (UI-ENGINE-SPLIT, UI-SKIN-SEAM, SOURCE-SELECT-TV,
SETTINGS-SCREEN/PANEL/ROWS) and TODO. Evidence:
artifacts/analysis/source-selection-2026-11-01/,
settings-2026-11-01/, settings-panel-2026-11-01/,
settings-focus-2026-11-01/. Module identities are in MEMORY.

Next UI work: exact focused-row/other-tab evidence (a vendor capture of a
focused row) or the vendor bitmap-font text/metrics path before claiming focus
pixels; then value changes, Beeld/Geluid tabs, scrolling and localization.
Control-plane fallback is implemented by SKIN-SELECTION above. Real OmarchyTV
screens remain pending (ui_omarchy is a skeleton); vendor remains the known
working fallback. Channel list, EPG/PVR, broadcast/DVB, native
audio/CEC ownership, independent startup, remote behavior and visible scanout
remain outside this UI slice. Keep the firmware-first and fb1-transparency
semantics: named SVG backgrounds are descriptions, and non-panel OSD areas use
clear_rgba.

Real-display scene warp: tools/display/scene-probe.c was cross-built (ARM), staged
in /tmp with SHA-256 checked, and run against real fb1 with vendor Aurora still
running. All eight types presented at p=0..1; the identity endpoint readback
matched BGRA (80,0,0,255) for every type; all 16,588,800 bytes were restored with
snapshot_match=1 and the original yoffset 0. Per-present timings: types
0/2/3/4/5/6 87-117 ms; type 1 keystone 483 ms; type 7 427 ms; type 8 493 ms.
Vendor PIDs 1967/1785 unchanged, staging removed. This is readback, not scanout
proof, and the software timings are far below smooth 30/60 fps.

Real GPU UI cadence: completion pacing is implemented and verified. The rail
tick now waits for the async present serial before advancing, so every decoded
frame is presented in order instead of being coalesced. The preserved rail stage
(refreshed to the current modules) plus our 10 ms-poll ui_vendor ran on the set with
OA_SCENE_PROBE_UI=1 and OA_SCENE_PROBE_RAIL=1: 11 image revisions over the
2000 ms window, backdrop BGRA24,23,22,153, dialog type-4 completion, reload and
host suspend/resume passed, and the full snapshot restored (match=1). Vendor PIDs
1967/1785 unchanged. The earlier 660 ms window under-counted because the
full-frame async path completes roughly 100 ms/frame; the first run with the
stage's older modules showed an opaque 13,14,14,255 backdrop because that
scene_display predated the alpha service. Refresh every module when reusing a
preserved stage. Frame order and scanout still need panel observation.

Panel observation (owner, two real-GPU runs): the rail highlight plays but appears
to restart before completing because the probe's 2000 ms window ends with a
focus-changing Down key; extend the hold before that key. The dialog transition
visibly moves DOWN: we apply only the full-screen type-4 shift to the whole
scene, while SVGDialog::drawAnimatedRectangles (0x10c5161) draws the dialog's own
named animation rectangle from the dialog description. Recover that named
rect/motion and move the dialog upward over the background shift, then re-observe
on the panel. The earlier backdrop/readback proofs say nothing about visible
frame order.

**That named-rectangle/motion explanation and suggested fix are superseded.**
Commits2fb2b1f/ef2dc4c and MEMORY's SELECT-PATH establish no independent named
rect motion here and no launcher Select transition at all. Current defaults
switch instantly; type4 is a lab path. Do not resume the obsolete offset/inversion
experiment from this historical observation paragraph.

Owner explicitly requested checking **exact** vendor animation fidelity, then
requested this handoff and stop. The audit remains incomplete; **do not claim the
animations are exact**. The rail-loop question it raised was resolved and fixed
in commit 4ebb900 (one-shot per focus, last frame held). Worktree evidence is
preserved in artifacts/analysis/animation-audit-2026-11-01, from fresh read-only
decompile/ARM inspection of Aurora MD5 64c252340d3af6c5d66072611b7bad7a:

- **Rail loop assumption is contradicted by the decoder path.** APNG file
  num_plays0 is metadata, not proof Aurora loops it. PngImageDecoderImpl's
  decodeFirstFrame b2f9a8 sets frame index+30=0 and frame count+38 from
  png_get_num_frames. decodeNextFrame b30654 increments index and only decodes
  when index<count (b3067c..b30686); it records timestamp+60 before decoding.
  The inspected trailing/finish paths do not rearm that sequence at its end.
  FindCallers finds no internal reference to png_get_num_plays, only its export.
  This supports a finite decoded sequence per decoder instance, whereas our
  rail_hl_tick_cb incremented modulo11 forever. The higher-level check is now
  done: onAnimImgDecoded only draws the decoded frame, decodeNextFrame has no
  rearm, and the only index reset is decodeFirstFrame when a new image starts
  (focus/state change). Fixed in mod_ui_vendor: a 30 ms one-shot per focus that
  holds the last frame; tests/ui_rail_hl asserts the stop. Vendor focus/hold
  behavior on the panel is still unobserved, so do not claim exact rail
  playback. Older reports/source comments/tests calling our looping policy
  “vendor” are superseded; their frame-count/asset/hash evidence remains valid.
- Vendor PNG scheduling uses a separate worker: threadMain b2eee0 polls the
  queue at10ms (r2=10 at b2ef00), checks unsigned now-last against delay+34 in
  isReadyToDecode b2fc84, advances one frame and posts decoded-image work.
  updateFrameProperties b2f918 copies the fcTL delay numerator directly; it
  does not convert the denominator there. SVGImage::onAnimImgDecoded10c9320
  checks created/visible/surface, then UISurfaceImpl::drawAnimatedImage1161dd0
  updates the image and requests OSD. Our UI-loop30ms full-canvas repaint is
  not that worker architecture. Five actual GPU revisions in nominal660ms do
  not prove the11-frame30ms sequence reached scanout. Avoid guessing frame
  skipping/catch-up to mask the current repaint/upload bottleneck.
- **Animation start timer differs:** vendor UIAnimationController::startAnimation
  1139898 kills any existing timer and requests a fresh immediate timer on
  every accepted start. mod_ui_anim currently posts a kick only if no timer
  exists, preserving an existing timer's phase. Any fix must keep timer
  registration under ui_anim's owner token and test overlapping/restarted
  animations and allocation/failure/reload behavior. Not fixed yet.
- Vendor controller uses a dynamically allocated std::list; our common/ui_anim.c
  has four slots and uint32 durations versus vendor float durations. These are
  further scope/contract differences, not evidence all vendor effects fit.
  Cubic min(1,1+(p-1)^3),1000ms dialog duration, linear/truncating list geometry,
  cancel versus finalise and the eight full-screen vertex formulas retain the
  existing instruction-derived evidence. That does not establish every caller,
  bootstrap enablement, callback ordering, widget effect or pixel sequence.

Raw entry points: image-player.txt, png-scheduling.txt, png-loop-filter.txt,
png-finish-filter.txt, png-end-state.txt, png-lifetime-references.txt. The last
file also contains the PngImageDecoderImpl vtable so virtual calls can be checked
against actual targets. All headless jobs finished; no TV probe is running.
Next continue this audit/fixes, then exact panel sequences and independent
backdrop appearance over real video. Keep source API/firmware-first requirements.
The already-tested transparency/asset milestones need no repeat extraction.

Latest follow-up: [RAIL-ASSETS](artifacts/reports/RAIL-ASSETS-2026-11-01.md)
exports the verified APNG to durable ignored workbench/rail-ui-20261101.
Full normal assets exposed a24-icon cache that silently truncated the eleven
highlight frames to nine; capacity now29 and regression checks every frame.
Focused strict/sanitizer + full host226 checks pass. Corrected UI on real GPU
passes navigation/animated+final receipts, reload and module cycle, full restore
and independent hash check. Same boot/PID/access/privacy; all staging removed.
Installed assets and matching production16-module stage unchanged.

Next: measure genuine rail repaint/upload/completion cadence and obtain panel
frame/appearance acceptance. Seven completed revisions in nominal660ms do not
prove the11-frame30ms loop reached scanout. Preserve this performance/visual
gap; do not repeat asset extraction or assume the earlier sparse unit covered
a full bundle. Native artifacts are in build-tv-v3/rail-assets; see report for
manifest distinctions before reusing the preserved local test bundle.

Latest milestone: [UI-ASYNC](artifacts/reports/UI-ASYNC-2026-11-01.md) wires ui_vendor
to display.async with owned completion sampler, both holds/aliases, fresh serial
gating and cached dialog progress. Four strict host/sanitizer/TV groups pass;
full host226 checks includes CPU regressions. Actual portal/assets, Down/Select/
Exit, completed dialog identity, UI reload/fresh readiness and host-only module
suspend/resume/new receipt pass on real TV. Full framebuffer/region/MAU/pan
restored and independently hashed. Same boot/PID1785/access/privacy; staging,
backups/readiness/helpers removed. Native identities in MEMORY/report. Source18
modules, matching production16 unchanged; no persistent deployment.

Remaining vendor visual/interaction/content parity includes rail scanout
acceptance after the RAIL-ASSETS follow-up above. The installed asset bundle
still lacks rail_hl01..11; do not claim the old UI-ASYNC run tested that
animation. Full vendor screens/actions, actual input, cold/TV standby,
native audio/CEC and normal replacement remain open. Firmware/decompile first.
The UI test used project-local key events and application-module suspend/resume,
not stock keys, UART capture, TV power management or native cold takeover.
No display.renderer option/automatic provider selection exists; explicitly choose
scene_display instead of CPU display. Keep build-tv-v3/ui-async separate from
the old matching stage. The UI test's zero generic grid counter was unused;
successful UI completion serials are logged separately.

Previous milestone: [SCENE-DISPLAY](artifacts/reports/SCENE-DISPLAY-2026-11-01.md)
adds optional scene_display, RGBA canvas and separate display.async request/latest
receipts. One inflight and one latest desired frame, sealed snapshots, cached
revisions and final-frame retry independent of animation timers. Legacy present
returns ENOTSUP; subsequent UI-ASYNC supplies its consumer. Four strict host/sanitizer/TV groups
pass, full host222 checks. Real101-request burst's final identity receipt passes;
the backed-up TV run observes412 GPU completions, host drain/unload and complete
restoration/independent hash match. Same boot/PID1785/access/privacy; staging,
backup/helpers removed. Source18 modules, matching production16 unchanged.
Native identities in MEMORY/report; no persistent deployment.

UI-ASYNC above supplies receipt/readiness/lifetime integration and cached progress.
Genuine rail/title animation still repaints. Remaining visual/input/standby/cold
acceptance is separate from that integration milestone.
Do not load both display and scene_display providers. No display.renderer option
or automatic selection exists. Separate diagnostic flags are not product config.

Previous milestone: [GRAPHICS-MODULE](artifacts/reports/GRAPHICS-MODULE-2026-11-01.md)
adds mod_graphics/display.gpu v1 (explicit graphics.broker/renderer paths), owned
watch/timer, nonblocking ABI3 quiescence and host-id public tickets. Four strict
host/sanitizer/TV lifecycle groups pass; full host218 checks. Actual service
completes410 GPU frames and the real host reactor drains/unloads it before full
snapshot/region/MAU/pan restoration and independent hash match. Live Mali hash
matches offline; decompiled/ARM native-window/swap/MAU extracts were rechecked.
No new hardware calls. Same stock boot/PID1785/access/privacy; staging/backup/
FIFOs/helpers removed. Isolated native identities in MEMORY/report. Source17
modules, preserved production stage16; no persistent deployment.

SCENE-DISPLAY above supplies the canvas/coalescer; ui_vendor integration must keep immutable
revisions, final-frame retry, completion-driven first-frame readiness and service
holds through reload/suspend/resume. CPU display v2 remains default; no
display.renderer option. The service's is_open is not visible-frame readiness.
Do not reuse a DSO static ticket counter as a public reload token. Abnormal GPU
child death latches cleanup_unproven; automatic recovery needs native release
evidence, not a guessed reset. No actual GPU child was killed in fault tests.
The new scene-module-client is a blocking diagnostic adapter, not UI code.

Previous milestone: [SCENE-CLIENT](artifacts/reports/SCENE-CLIENT-2026-11-01.md)
adds common/scene_client.c: one pending/result, instance tickets, nonblocking
send/receive, copied completions, EOF shutdown and WNOHANG reap/destroy gate.
Five strict host/sanitizer/TV groups pass, including FIFO-blocked finish/close,
foreign-fork rejection, bad replies, send failures and target exec failure.
Full host214 checks pass. The maintained client also completes410 actual GPU
frames in the snapshot diagnostic, child exit0 and complete restoration.
Same stock boot/PID1785/full framebuffer hash/pan/access/privacy; staging,
backup/FIFOs/helpers removed. Isolated build-tv-v3/scene-client identities in
MEMORY/report; no matching production stage rebuild.

GRAPHICS-MODULE above supplies the provider/watch/timer/quiescence. Consumer
coalescing, immutable revisions/final-frame retry and first-frame readiness remain
pending. No display.renderer option yet.
tools/display/scene-probe-client.c now wraps the maintained client but is a
BLOCKING diagnostic adapter; never move its polling/wait loops into the host.

Previous milestone: [SCENE-HELPER](artifacts/reports/SCENE-HELPER-2026-11-01.md)
adds an EGL-free executable that sanitizes inherited descriptors before loading
the renderer DSO. The DSO remains loaded until child exit. Host/sanitizer/TV
process fixtures and410 real TV GPU frame requests pass. Child21079 drained/
reaped, full16588800-byte backing/pan/MAU/region restored and independently
hashed. Staging/backup/helper removed, same boot/PID1785/access/privacy.
Full host209 checks pass. Isolated build-tv-v3/scene-helper identities are in
MEMORY/report; matching production stage and historical diagnostics unchanged.

The lower nonblocking parent now exists as SCENE-CLIENT above; host provider and
ui_vendor reload/suspend/resume/failed-cleanup integration remain open. CPU display
is still production.

Earlier lower-layer milestone: [SCENE-BROKER](artifacts/reports/SCENE-BROKER-2026-11-01.md)
adds OGS1 local IPC, sealed immutable8294400-byte RGBA images and a blocking
owner server with upload/cache revisions, finish-before-completion and retained
failed-close images/owners. Four groups pass strict host, ASan/UBSan, ARM/qemu
and actual TV using injected rendering callbacks only. Native unit87760bytes/
ba89e0afce422f03e8254ae1b0c307cc in build-tv-v3/scene-broker. Target kernel
rejects shared read-only mappings of sealed files; verified instructions justify
private read-only mapping with all seals intact. Staging removed, same boot/
PID1785/full framebuffer hash/pan/access/privacy. Production stage unchanged.

The bootstrap/load boundary and actual GL finish are now implemented by
SCENE-HELPER above; no async module service yet. Reuse common/scene_broker*.c
and its tests for parent completion/coalescing/quiescence. Do not repeat the solved sealed
MAP_SHARED experiment or mislabel qemu's small-control ancillary leak as a TV bug.

Owner resumed the goal and reaffirmed real display/hardware testing. Latest
visible milestone: [EGL-WINDOW](artifacts/reports/EGL-WINDOW-2026-11-01.md) proves direct
native GPU presentation on the actual display. The owner watched all eight
effects and confirmed upright, full-screen and smooth output. Each effect ran
51 presents in about1000.4ms at our50Hz pacing. Full16588800-byte backing,
region, primary MAU0 and original pan0 were restored; independent full MD5
before/after41735daa69dc137743b59efe12db4a19. Same stock boot/PID/access/privacy.
All temporary probe/backup files removed. No production stage rebuild/deployment.

common/scene_gles.c now supports explicit pbuffer or native-window ownership,
retained failed-close handles and cached textures. Native dimensions are four
bytes (uint16 width/height). The diagnostic sets process-local FRAMEBUFFER_2
to/dev/fb1 and MALI_MAU_SWITCH=0,0,0,0,0, using the existing primary two buffers.
Input top-down RGBA; pbuffer readback bottom-up premultiplied RGBA; native raw
BGRA corner checks passed. The production display module is still CPU-based;
no display.renderer switch or async GPU service exists yet.

Current tested binaries under build-tv-v3/egl-window: egl-window-probe88780bytes/
5a66a935317e0623660151ffac96a926, egl-scene-probe74504bytes/
aed778d42cc3aa4e3ff8ddc995b9767c. Both maxGLIBC2.17. The updated pbuffer probe
also passes cached-texture/empty-cache, bounds, foreign-thread render/close
rejection and prior pixel/reopen checks on actual GPU. The old70384byte pbuffer
identity is historical, source commit8a535b6; its initial build path was rebuilt.
New Ghidra project
ghidra_mali/Mali/libmali.so is only partially analyzed (180s timeout). Use exact
Thumb extraction and inspect errors; DumpThumbFirmware.java makes temporary
metadata under readOnly and logs discarded changes. Do not repeat full analysis
or mistake missing-function metadata for a missing API.

Prior review:
[UI-SCENE-REVIEW](artifacts/reports/UI-SCENE-REVIEW-2026-11-01.md) fixes the
scene rasterizer to use fan-triangle interpolation, the firmware's GL_LINEAR
filter and progress-alpha fade into premultiplied BGRA. ui_vendor now presents on
each progress callback, clears both display aliases before provider unload,
clears a frozen transform on animation-provider removal, cancels interrupted
dialogs and keeps failed resume suspended. DialogLauncher still selects type4
by the recovered first-present mode rule; ui.fs_type is an explicit override.

Full strict host suite203 ok, focused ASan/UBSan and ARM/qemu display tests pass.
Two real-fb1 runs (no fake screen, no HAL init) each restored16588800 bytes and
the original yoffset0, snapshot_match1. Optimized slide-up batch fell from
4499.56 to399.68ms for five presents; general keystone/scale remain slow.
This is existing-layer draw/commit/readback evidence, not smooth playback,
independent startup or vendor frame-sequence acceptance. Probe101324bytes,
MD547dcd8ca304e67f42eed48c3a3172b33, maxGLIBC2.17, preserved with ARM units
under build-tv-v3/review-scene. Production stage.md5 below is unchanged.
Staging removed, no helper left; same stock boot/PID/access/privacy protections.

Next concrete step: integrate an asynchronous GPU display provider. Prefer a
separate owning helper process, following audio_mixer/broker's nonblocking IPC
and WNOHANG lifecycle: it isolates Mali's process-global environment/cache and
driver threads from reloadable DSOs. The lower renderer is synchronous; never
call blocking swap/destroy from the host loop or its ABI3 quiesce gate. Define
bounded immutable frame transfer/cached revisions, coalescing and completion/
first-frame readiness before wiring ui_vendor. Existing present() means commit,
so enqueue success cannot masquerade as that receipt. Preserve source holds,
pending final frames and failed-close ownership through stop/reload/suspend.
Then verify the actual module lifecycle on the real display, vendor frame
sequences/cadence and remaining screens/
broadcast/settings, native audio/CEC ownership and independent startup. Do not
repeat the general CPU warp benchmark as a way to establish smooth animations.
No TV probe or upload remains pending. Native-window entrypoint research is in
artifacts/analysis/egl-scene-2026-11-01/mali-window-entry.txt.

Earlier audio review: [SPEAKER-GAIN](artifacts/reports/SPEAKER-GAIN-2026-11-01.md)
confirms direct SetOPUGain registration and raw channel bounds0..0x8000.
It bypasses UI AQ/balance/ratio processing. GetOPUGain remaps cached100 to0x8000;
never use it as an exact rollback snapshot. Lower codec/client dispatch now has
three wire and six session groups passing host/sanitizers/ARM-qemu/TV with
injected I/O only. No live write or broker/service setter was added. Verified
TV units:37668bytes/a90d0a2c4635af1210a9ecb1467d9bf2 and
67756bytes/bcd8ad1eff1f78aaa0dbb97543e7dc2c, preserved in
analysis/cec-ownership-2026-11-01/speaker-wire-tested-tv.tar under artifacts.
Staging was removed; stock boot/PID/access/privacy unchanged. The production
16-module build below was not rebuilt with this unused lower primitive;
build-tv-v3/tests contains the newer unit binaries.

MIXER-MODULE-2026-11-01 adds mod_audio_mixer/audio.mixer v1
and mixer-query/result/cancel. All query/reply I/O is nonblocking; one request or
uncollected result, generation-scoped tickets, cancel/drain, owned watch+reap
timer and WNOHANG teardown. Service holds survive suspend/resume; stale tickets
reject after reload. No HAL dependency or setter. Five groups pass host,
sanitizer, ARM/qemu and real TV. Real TV socket query/reload/query returned
L/R32768 twice, and the standalone core's config/start/status/quit passed.
Both native stages were removed; stock boot/PID/access/privacy unchanged.

Current matching build-tv-v3 is core08f549d4026cfb626c1ffc93b2e02fc6 plus16
modules, helper and supervisor. All16 modules passed real-TV --check. Exact
production identities: build-tv-v3/stage.md5 and mixer-matching-tested-tv.md5
under analysis. Previous corefc9.../13-module stage was archived, including
its two test fixtures, in artifacts/archive/build-tv-v3-before-mixer-20261101.tar
with SHA256 and every old stage.md5 entry verified. ABI1 recovery/installed and
ABI2 build remain unchanged. Read MIXER-MODULE before reusing old snapshot claims.

Firmware finding: target libc's posix_spawn can return0 before exec failure
and child exit127. Verified directly in target libc instructions and minimal
qemu reproduction; this is NOT a qemu defect. Obvious missing paths reject via
X_OK check, but asynchronous exec failure/EOF is still handled/reaped. Failed
resume cleans partially registered sockets; it is not automatically followed
by host stop. The initial failed test assumed newer host-libc semantics.

Next integrate the firmware-understood ALSA speaker out gain control through
Mod_Cell_SetVol, preserving AQ table selection, balance and ratio semantics.
Keep any writable acceptance separate from the query-only broker/service.
Do not substitute raw SetOPUGain or decoder opcode33, or treat query/transport
success as audible acceptance. Full native
amp/routing/standby, CEC ownership and UI parity remain open.

AUDIO-BROKER-2026-11-01 adds a separate executable with
lazy PID-bound netlink and fixed32-byte AF_UNIX/SOCK_SEQPACKET IPC. It supports
GetOPUGain only, no HAL init/setters, and drops unrelated inherited fds on exec.
Fresh read-only Ghidra instructions connect mod_plat's Mixer_Ctrl_Init to
Hal_IfCmd_Init's PID binding: a thread alone does not solve that conflict.
Host/sanitizer/ARM-qemu fixtures pass. A real posix_spawn child on TV returned
device0 L/R32768, then exited/reaped cleanly on IPC EOF. Parent85640bytes MD5
bf227ea86b93b3a8c0c0055250fe0348; helper62176bytes MD5
2e2856c667df2a40d2cbb38589a6753d, both preserved under analysis. Helper max
GLIBC2.4; parent max2.15. The initial fixture's fcntl@2.28 dependency was caught
under qemu before any TV upload and replaced by the integer-only SYS_fcntl
F_DUPFD operation. Do not confuse that loader failure with a missing driver.
Staging /tmp/oa-audio-broker-20261101 removed; stock boot/PID/access/privacy
unchanged. MIXER-MODULE supplies the subsequent nonblocking parent service;
do not run the blocking child serve loop in the host. Full writable audio
parity remains open.

CEC-ASYNC-2026-11-01 completes the corresponding lower
CEC request/result API: acquire/send/receive/release_async and take(). Acquire
copies path/tables, send copies frame, and receive reports1 frame/0 empty/-1
error. Failed close retains the copied backend/fd across result collection and
shutdown retries. Four groups pass host/sanitizer/ARM-qemu/TV, injected I/O only.
Native test86172bytes/d2ca064a491065637b78f9349c56e2b0 is preserved as
artifacts/analysis/cec-ownership-2026-11-01/cec-async-tested-tv.elf. Temporary
/tmp/oa-cec-async-20261101 was removed; stock boot/PID/access/privacy unchanged.
Full host suite passes. Legacy synchronous waiters in both owners also stop
waiting if an async request takes the slot. Next connect module services with
explicit backend/consumer lifetime and completion polling/notification; neither
async API adds cancellation of an executing ioctl or native device ownership.

AUDIO-ASYNC-2026-11-01 adds nonblocking lower audio-owner
open/gain/close submissions and take() results. One request or uncollected result
occupies the bounded slot; tickets are instance-scoped. I/O tables and gain
results are copied; callback context/code remain borrowed through close/join.
Quiesce drains accepted work before closing and preserves an untaken value result.
Three groups pass host/sanitizer/ARM-qemu/TV using injected I/O only. Verified
TV test was75264bytes/71a53754d8c48fe97cfb65580c0f8d2c, preserved under
artifacts/analysis/cec-ownership-2026-11-01/audio-async-tested-tv.elf. Staging
/tmp/oa-audio-async-20261101 removed; stock boot/PID/access/privacy unchanged.
No real mixer binding or production deployment. bind_session remains ENOTSUP.
Connect module services with explicit consumer/provider lifetime and completion
polling/notification. Lower
CEC receive success1 no longer propagates stale worker errno; regression added.

No TV helper or headless-analysis job was left running. The read-only CEC cache
probe was removed from /tmp after its hash-checked run. Stock boot
ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 was not rechecked on the later read.
That read found SSH up and no aurora process; PID 1737 is no longer the live UI.
See MEMORY. Nothing was restarted. Historical ABI 3 core MD5 was
fc9eb71f924dd850c7ba4f1c9b8a02cd, max GLIBC2.17. It is not installed.

The subsequent review supersedes that absence observation: view-only read found
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live Aurora PID1785, zombie1967.
No restart/write was performed. See MEMORY and WORK-REVIEW-2026-11-01.
Review fixed owner state races, removed the idle 500ms quiesce wait, retained
joinability on join error, and allows reload to retry an outstanding release gate.
Busy lifecycle commands now need retry; --force still cannot bypass release.
The loop-thread audio bind_session service now always returns ENOTSUP because
the underlying owner RPC blocks its caller. Keep it disabled until an async
service contract exists. The lower injected owner API still works off-loop.
Animation timers now register through a ui_anim-owned post, resume after suspend,
and preserve new animations started by completion callbacks. Float-to-int bounds
and a missing fresh-build test dependency were fixed too. Full host checks and
focused sanitizer/ARM-qemu runs pass; review builds are build-review-san/arm.
That previous stage is now archived; current stage.md5 identifies the refreshed
matching16-module build described above.
No review binary was uploaded or deployed. Existing hardware-acceptance gates
and failure-notification work for the queued animation timer remain in TODO.

Same-boot live reads, with no node creation, CEC transmit, receive, STBC write
or amplifier write:

- ntkcec remains major 229 with no /dev/ntkcec node. /dev/video28 is still the
  shared HDMI-switch node. Stock still owns the exclusive raw device.
- One CEC controller remains at 0x40b9000. LA8 is seen, PA1000, power On, name
  Steam Deck. LA4 still mirrors that metadata with seen=0.
- Amp descriptor count is still 1, object 0x033341c0, name ESMT AD82587_D.
  The 612-byte AOUT cache at 0x0197a138 matches the saved snapshot byte for byte.
  Output 0 still owns the present speakers. Register 03 was not reread.
- VPL_CEC_Open (0x616ce4) reads physical 0xfc040200 and, when bit 1 is clear,
  writes STBC 0xf011 with that low byte plus bit 1 before DRV_CEC_Open. The raw
  device backend does not perform that step itself. Do not map that address or
  open ntkcec while stock holds the exclusive semaphore.

The offline workers now sit on that sequence. common/cec_authority.c writes
0xf011 only through injected I/O when bit 1 is clear. common/cec_owner.c and
common/audio_owner.c keep blocking calls off the host loop. mod_cec publishes
cec.owner with hardware_open_supported() == 0. Suspend and resume join and
recreate that idle worker. A failed CEC close stays on the worker and does not
post, so host shutdown waits. NULL audio I/O is ENOTSUP. mod_audio creates the
audio owner and does not bind a socket; enabled HAL volume still refuses
suspend. Do not bind a second netlink socket in the vendor process, map
0xfc040200, or open ntkcec while stock holds the device. No gain, table,
profile or CEC takeover has been accepted. Read OWNER-WORKER-2026-11-01.

New code: include/openaurora/module.h adds optional int quiesce(void *ctx),
core/module.c retains busy instances through failed init/start and stop/unload/
reload, core/control.c exposes cleanup_pending/activation_rejected, and
tests/quiesce{,_fixture}.{c,h} exercise the lifetime contract. Gates must be
nonblocking, tolerate NULL/partial contexts and repeated calls, and finish
fallible release before returning zero. --force never bypasses them. Failed
init/rejected replacement instances remain discoverable but cannot activate.
Read [QUIESCENCE](artifacts/reports/QUIESCENCE-2026-11-01.md) before continuing;
do not mistake the device-free fixture for independent audio/CEC acceptance.

UI parity explicitly includes vendor animations/transitions, focus feedback,
scrolling and other UI/UX effects. Recover their timing/behavior from firmware;
static screenshots alone are insufficient. Raw 32bpp OSD captures are BGRA on all
measured layers, including fb1. OAI1/blit inputs are RGBA and converted by display;
saved screenshot PNGs are RGB already. Never apply a second swap to corrected
PNGs or assume a different vendor/API buffer shares the framebuffer format.

## Display firmware evidence and deferred hardware acceptance

Latest research is in
[OSD-BUFFER-CONTRACT](artifacts/reports/OSD-BUFFER-CONTRACT-2026-10-31.md),
[OSD-KERNEL-INIT](artifacts/reports/OSD-KERNEL-INIT-2026-10-31.md) and
[OSD-MAU-SELECTION](artifacts/reports/OSD-MAU-SELECTION-2026-10-31.md).

Established: private GEM create sets framebuffer-mode, page-aligns its requested
size and allocates runs from reserved OSD pools. Initial front requests one
surface S; back shares it (count1), requests S (count2) or 2*S (count3).
Sync_FrontSurface2HW advertises 2*align4096(S), but the OSD commit gets address/
size separately from its GEM. Size helper reads GEM +0x70; its aligned-size
initialization is now verified in both target NAND kernel slots. Fresh kernel
decompression matches the reconstructed ELF load segments, avoiding sibling
firmware/cache ambiguity. No large contiguous mapping follows from doubled metadata.

Alpha/PM follow-up is now in OSD-KERNEL-INIT: blend-enable writes cursor planes
only; normal-plane global alpha is separate. Driver PM restores cached registers
and palettes. PanDisplay handles first-pan plane/region show, so the existing
fbdev backend is the first independent-validation candidate, not a reason to
invent new DRM/alpha/PM setup calls. Existing present omission is fixed offline,
not accepted on panel.
Next: complete cold/resume integration reasoning and verify initialization order,
geometry/backing retention and first post-wake frame at a future authorized test.
Native module power orchestration still needs implementation independently of
kernel device restore. Continue safe offline implementation while TV is in use.
Later surface switching/resizing is also unverified. Do not repeat palette,
dumb-buffer, watchdog-library or dp_order experiments.

| entry point | location |
|---|---|
| Aurora front/back requests | CreateWindow 0x942690 |
| advertised extent | Sync_FrontSurface2HW 0x940388 |
| private GEM init | ntkndpdrm _drm_gem_init ELF .text+0x14620 |
| backing-size helper | ndp_osddp_fb_buffer_size .text+0xd944 |
| OSD commit callback | osdp_plane_mode_set .text+0x4534c |
| OSD driver sequence | ndp_osd_plane_mode_set .text+0x47a40 |

Ghidra project paths/script conventions are in AGENTS.md. New raw logs are in
artifacts/analysis/osd-2026-10-31, especially oa-osd-gem-pool-sizing.txt,
oa-osd-window-extent.txt, oa-osd-sync-extent.txt, oa-osd-plane-backing-commit.txt,
oa-osd-fb-backing-helpers.txt and oa-osd-gem-object-init.txt. New instruction logs:
oa-osd-kernel-gem-size.txt, oa-osd-kernel1-gem-size.txt and oa-osd-update-controls.txt.
Update-control byte4 commits; byte5 requests gated vsync. Command0x63 enables
the region, not alpha; plane-show also respects force-disable state. No headless/tool
session was left running. Latest raw follow-up: oa-osd-alpha-resume.txt,
oa-osd-drm-alpha-restore.txt, oa-osd-pm-registration.txt,
oa-osd-pm-relocations.txt and oa-osd-window-alpha.txt. The narrow
ndp_osd_restore_plane_mode_set helper restores logical plane0 only; it is not
an all-plane resume API.

## Other outstanding implementation work

Owner's current priority: CEC/audio. Start with
[CEC-AUDIO-CONTRACT](artifacts/reports/CEC-AUDIO-CONTRACT-2026-10-31.md), which
corrects ARC direction/old conformance claims and records tested audio ABI,
prelude ownership and cache/handoff changes. Framing helpers remain in
common/cec.c. mod_cec now publishes cec.audio for injected System Audio and ARC
routing; that is not a live transport. Follow-up
[CEC-TRANSPORT-CONTRACT](artifacts/reports/CEC-TRANSPORT-CONTRACT-2026-10-31.md)
recovers the private V4L2/kernel path and tests both packet layouts. It finds
lost status, a mandatory 16-byte operand buffer, driver-filter/spec mismatches
and broken VPL close/reopen. Do not repeat the old UART transport or Init/Close
reload plan; the current backend and next integration work are below.
Latest: [CEC-OWNERSHIP](artifacts/reports/CEC-OWNERSHIP-2026-11-01.md) recovers
PreInit's emulated calls and real Linux registration. common/cec_device.c now
implements the blocking raw backend, tested with injected I/O on host/sanitizer/
ARM-qemu. OWNER-WORKER adds the authority helper, the owning worker and an idle
cec module. Failed hardware close stays owned across quiesce generations.
Suspend and resume now join the idle worker. There is still no live open. The backend stage
made no TV query. Board amp/output
routing remains open; pcbconfig -a writes tag files and is not a view-only query.
Latest [CEC-LIVE](artifacts/reports/CEC-LIVE-2026-11-01.md) records shared-owner
queries on /dev/video28 and Steam Deck LA8/PA1000. No independent raw initialization
or receive was attempted. Temporary staging removed, stock boot/UI/access intact.
Spec compliance overrides Aurora defects: identity validators and regression tests
reject broadcast OSD-name traffic and accept valid broadcast System Audio Mode.
OWNER-WORKER's route follow-up is the injected System Audio/ARC policy on
mod_cec. Native exclusive open and electrical ARC are still open.
Audio follow-up: [AUDIO-ROUTING](artifacts/reports/AUDIO-ROUTING-2026-11-01.md)
identifies the live amplifier as AD82587_D, not AD82586C. Read it before native
amp work. Offline tools/audio/audio-state.c decodes the verified AOUT cache;
speaker gain, decoder main volume and amp register volume are separate controls.
Board resources and UI-volume conversion must precede audio writes/bring-up.
Follow-up [AUDIO-BOARD](artifacts/reports/AUDIO-BOARD-2026-11-01.md) now recovers
those board resources and records a successful native register read on the TV.
Continue with UI-volume conversion and explicit amp lifecycle/ownership. Mute uses
MCU GPIO208/STBC8; reset uses GPIO114. Do not cold-call NDAL STBC or treat the
successful bus3/address34 read as audible/initialization acceptance.
Latest [AUDIO-VOLUME](artifacts/reports/AUDIO-VOLUME-2026-11-01.md) recovers UI
speaker-gain routing and normal/aggressive AQ curves. Pure common/audio_curve.c
matches all101 live normal-table entries and passes real-TV runtime tests.
Next integrate gain/table/profile/default handling and explicit audio lifecycle;
the helper is not yet a production audio path. Ignore SetMainVolume as a setter
(range-check only), and omit the Samba telemetry calls in the traced ICE path.
Follow-up [AUDIO-CURVE-MODULE](artifacts/reports/AUDIO-CURVE-MODULE-2026-11-01.md)
adds the reloadable audio.curve v1 data provider and local example configuration.
Real DSO lifecycle/holds/failure/suspend tests pass including on TV; no hardware
gain or native audio lifecycle integration yet. Core config is not reread by
module reload; AQ contents are. Do not promise rollback after failed replacement.
Read [AUDIO-GAIN-TRANSPORT](artifacts/reports/AUDIO-GAIN-TRANSPORT-2026-11-01.md)
before gain integration: scalar returns are real, but message-backed HAL0 hides
send failure and the paired ALSA getter only sees a request cache. The receiver
copies the table; dispatcher timing is now resolved by AUDIO-NETLINK below. New
read-only mixer query passed with the stock ALSA_CONFIG_PATH; do not replay
platform/audio init while stock owns it. Alsa / ntkndpalsa.ko project is durable.
Latest [AUDIO-NETLINK](artifacts/reports/AUDIO-NETLINK-2026-11-01.md) proves
synchronous callback/table-copy lifetime from target kernel instructions. No
operation ACK exists. Bounded message/reply codec passes host/sanitizer/ARM-qemu;
native GetOPUGain query passed on TV. Next coordinate process/socket ownership
and blocking execution; do not reuse a HAL-owned socket, replay EINTR or treat
dispatch as successful DSP application. Codec is not wired into native audio yet.
The same report's session follow-up implements common/audio_client.c: process/fd
ownership, bounded query validation and checked dispatch. Five host/sanitizer/
ARM-qemu groups and full host suite pass; refactored query passed on TV. Reuse
this library. OWNER-WORKER connects an owning worker that rejects a real socket
unless an I/O table is injected. mod_audio now creates that owner. Real
table/selection/gain writes and audio acceptance remain untested. Enabled HAL
suspend is refused rather than reported as standby.
All required /tmp/opencode assets now live in artifacts/workbench; use those
paths, not ephemeral aliases. Firmware, Ghidra databases, toolchains, Python
tools and original research evidence were retained; see its README/SHA256SUMS.

Latest UI work: [UI-ANIMATIONS](artifacts/reports/UI-ANIMATIONS-2026-10-31.md).
Offline tools/re/svg-animations.c consumes all 702 pack records and extracts 39
descriptors with exact boundaries; host/sanitizer/ARM TSVs match. Controller uses
an immediate timer then 10 ms cadence, message id 0x96; cancel and finalise differ.
Full-screen dialog path has 1000 ms duration/cubic ease-out; list selection is
linear; text scrolling has 800 ms edge pauses, quantized duration and a separate
2000 ms repeat path. Do not reuse one easing/duration everywhere or mistake the
message id for 150 ms. New svg-names tool resolves all 702 resource names/IDs from
ELF svg_name_list and verifies CRC32 against the pack index; all 39 descriptors
are named. Widget states are UFA/FA/UFI/FI at 0/1/2/3, category arrays use a
{1,0,3,2} remap; category styles differ from generic launcher tiles. Ad-banner
effects are research only and excluded from porting. Later follow-ups recover
full-screen types and rail timing and implement consumers; UI-SCENE-REVIEW above
records actual framebuffer tests and remaining performance/visual gates.
Raw evidence: artifacts/analysis/ui-animation-2026-10-31.
Name/caller follow-up: artifacts/analysis/ui-resource-bindings-2026-10-31.

Clock prerequisite now implemented: mod_ui_clock requires ticker's raw clock,
publishes ui.clock v1 and saves numeric FROST_Time state across reload. Preserve
the recovered 90,000,000-ms period, 12,343 correction quantum, ARM32 truncation
and strict > boundaries. Seven DSO groups pass host/sanitizer/ARM-qemu, including
failed-resume retry and the real ticker/ui_clock dependency chain. The one-second
sampling timer is our choice; absolute vendor phase/history remains unverified.
Constructor animation enablement is false; setAnimationEnable merely writes the
flag. mod_ui_anim now publishes ui.anim v1 on ui.clock: four slots, duration
below 1 rejected, same id replaced, cancel without a final callback, finalise
at progress 1. List rectangles truncate; dialog progress is min(1, 1+(p-1)^3)
over the recovered 1000 ms select path. The full-screen progress mapping is
recovered: SVGDialog::startAnimation maps dialog modes 57..64 to HAL types 1..8
for a 1000 ms controller, onAnimationProgress feeds the cubic, destroy forces
1.0, and `oa_fs_apply` reproduces the per-type vertex rules. The display-v2
consumer and its subsequent review are described above. Text-slide duration/margin/repeat are wired in mod_ui_vendor with
enablement gating, box clipping and the 0.99 repeat; suspend/stop cleanup is
tested. Rail mapping now uses the recovered category highlight APNG at30ms;
the generic300ms tile records do not define rail animation. Visible rail/frame
parity remains open.
Raw clock evidence/tests: artifacts/analysis/ui-clock-2026-10-31.

Latest power evidence: [POWER-STANDBY-CONTRACT](artifacts/reports/POWER-STANDBY-CONTRACT-2026-10-31.md),
raw artifacts/analysis/power-2026-10-31. Outer standby/idtvSuspend/idtvResume and
wake-network policy were freshly decompiled with instructions. They own substantial
device/network prep/restore omitted by mod_power. The target ntkethmac private-ioctl
handler is now decoded: opcode 1 stores next-suspend policy, 0 keeps PHY powered,
1 requests power-down during driver PM. Imported module matches dump; driver
suspend enables magic packets and resume restores MAC/DMA/PHY. Native wake flags
and MCU ownership still need handling. MonitorTask_Reset is vendor task-state masking,
not watchdog disable; UART SuspendSend/ResumeSend synchronize a semaphore.
Internal oa_module_sleep_cycle now implements final entry/restoration ledger,
phase-specific results, mutation/nesting guards and shutdown cancellation. Nine
additional host/sanitizer/ARM-qemu groups use fake entry. It preserves modules
already suspended and refuses active modules missing paired hooks. mod_power now
uses the host.sleep service and final RAM-entry callback; ordinary hooks do not
sleep. CLI standby, notification order, failed-write counting and cleanup pass
seven wrapped-power groups on host/sanitizer/ARM-qemu. The public module ABI stays
unchanged; power now requires the host-owned coordinator service. Next assign
remaining hooks/device/MCU wake ownership and recovery-state signaling before
hardware acceptance. No guessed no-op hooks or physical tests while TV is in use.
No TV state or recovery artifact changed; hardware acceptance remains open.

Reload restore-error/stopped/suspended semantics are now implemented and covered
by nine host/sanitizer/ARM-qemu regression groups. Suspended replacement activation
is deferred until resume; there is no old-binary rollback. Dependency ordering
now snapshots required services/lookup holds, so providers resume before consumers
even after suspended reload changes slots. CLI reports callback failures; cycles
reject before callbacks. Nine additional host/sanitizer/ARM-qemu groups pass.
Control reply formatting now bounds every append; an ASan-reproduced full-service
status overflow is fixed. Full-table Unix socket replies and tiny buffers pass
host/sanitizer/ARM-qemu checks. Unix sockets now use bounded nonblocking output
queues, pause command reads under backpressure, drain half-close replies and
handle short writes/EINTR/EAGAIN/broken readers. Five further real-socket groups
cover those paths plus whole-line input rejection and cleanup. Console stdout
remains synchronous; finish its blocked/broken-reader lifecycle separately.
Historical ABI 2 offline core MD5 54a626f4c3a00b2e90490bc2626fe055 (13 modules, including
ui_clock and audio_curve) differs from the historical 11-module TV-preflight build;
installed/recovery files remain untouched. MODULE-SAFETY contains details.
TODO still tracks native power/device ownership, callback rollback limits, IR code-space and
repeat/release integration, native power lifecycle, HAL init ownership, audio/
source/settings parity and remaining TV features. Injected portal hit testing,
open/home/back/source actions and list/dialog animation are in UI-PARITY;
cold scanout, labels and other screens are not.
Avoid treating old tier/symbol-count plans as acceptance criteria.
