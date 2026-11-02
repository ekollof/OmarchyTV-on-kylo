# Read-only UI object inventory — 2026-11-02

tools/re/ui-object-scan.c provides a bounded read-only inventory of known
Aurora vtable candidates in readable private heap mappings. It was run on the
verified stock process. It found no settings-instance candidate in that scope,
so it does not resolve the actual text-placement gap or justify changing UI fits.

## Firmware fields and safe read scope

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh ARM/decompile in
analysis/ui-live-objects-2026-11-02/window-fields.txt verifies
UIWindow::isCreated1152a40 reads byte+64; getClientRect1152974 reads window
corners+28..34 and applies the virtual margin rectangle. Existing row/textbox
field and concrete vtable evidence is in UI-ROW-WINDOWS/UI-TEXTBOX-COMPOSE/
UI-TEXT-VIEWPORT. Primary vptrs include the8-byte C++ vtable header:

| class | vptr |
|---|---|
| SettingItemsListBoxRow |14dbe2c|
| SettingItemsListBox |14dbd74|
| SVGTextBox |153b904|
| PictureSettingsDialog |14e87a4|
| SettingsDialog |14dbeac|

The native tool accepts an explicit PID, verifies its executable path equals
/vendor/com.nvt.idtv/aurora.elf and opens only that process's maps/mem read-only.
The wrapper separately verifies firmware MD5, boot and live PID. It selects
exact [heap] maps with rw-p permissions, offset/inode0 and device00:00; it
excludes named files, devices, stacks and other anonymous arenas. Maximum
scan256MiB. Blocks include enough lookahead for a full row candidate across a
chunk boundary. It never dereferences a discovered target pointer, calls target
functions, uses ptrace/signals, opens input/devices or writes target memory.

It prints vtable candidates, raw created/state/window fields and selected row/
textbox/list metadata, not strings or arbitrary buffers. These are non-atomic
candidates: a matching word can be data/stale allocation, and live object
lifetime/active-screen ownership is not established. Short reads/map changes/
budget failures return incomplete/error; absence in this heap subset says
nothing about other arenas, subclasses or future UI creation. --snapshot reads
only a bounded regular file for offline testing; it does not accept device/FIFO
input as a snapshot.

## Native observation

The verified live process was1785, stock boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197;1967 remains the zombie and was not inspected. Complete scan of
four readable heap ranges consumed176934912 bytes and found six SVGTextBox
vtable candidates, all with created0, zero window/text/font fields and default
alignment4. It found no configured settings-list, row or picture-dialog vptr
candidate in the selected heap. The six records are in tv-scan.txt.

Do not rescan this unchanged inactive-settings sample expecting it to prove
coordinates. Obtain an actual settings instance during a suitable UI window or
continue the firmware/text/resource investigation. The full process map is a
dated sample, not a future address list to reuse blindly.

## Screen/state observation and authorization context

Before navigation, fb1 both backing frames were blank, pan0,1080. One firmware-
mapped DIAL Menu1048 request received HTTP201; subsequent framebuffer reads
still showed blank OSD. fb0 frame0 and fb2 both frames were also blank. There
was no observed settings/menu effect, so no post-key scan was performed.
The state WebSocket answered `<tv_state value='OTHER'/>` to a read-only tvstate
query. HTTP201 is dispatch/transport evidence, not proof of key handling.

The owner then explicitly reported the set was awake and showing HDMI1 video
from the external source. Blank OSD/OTHER therefore must not be diagnosed as
standby, a dead screen or a crashed UI. No power key, service restart, device
probe, settings write or further navigation followed that report. This turn
returned to view-only work; AGENTS remains the standing authorization source.
The physical video/source was not changed. Menu delivery/parser behavior is
unresolved: do not invent a quote-parser or input-routing defect from this one
unobserved result.

The saved screenshot PNGs under ignored workbench/live-ui-20261102 are durable
local artifacts, not redistributed assets. tvshot used its established BGRA
conversion for32bpp and RGB565 for fb2. Blank captures are not visible scanout
proof. Existing tools/tvshot.py was used for capture, no new Python code.

The old tools/wspoll.c was NOT executed: inspection found ext[4] with an8-byte
read for127-length frames, unhandled short/header-coalesced reads and stale/
uninitialized state paths. The query instead used the host Node WebSocket API
with a5-second deadline. Repair/test the old tool before using it, especially
on a TV whose crash handler can reset the set. The DIAL report/tool index now
flags this concrete stale helper; its historical state claims remain dated.

## Verification and cleanup

tests/ui_object_scan.c verifies a row spanning the64KiB boundary, its embedded
textbox, signed metadata, truncated/unknown candidates, numeric bounds and
refusal of a different executable. Full strict host suite, focused ASan/UBSan
and ARM/qemu tests pass; the actual native scan completed successfully.
Native build-tv-v3/ui-scan/ui-object-scan:32736 bytes,
MD5 c7ac91ee12465bad32819cebfbe13c57, maximum GLIBC2.7. Separate successful
size/hash/firmware/executable verification preceded execution.

/tmp/oa-ui-scan-20261102 was removed and absence/processes independently
checked. Same stock boot/live1785, hook/hosts hashes, SSH22/telnet2323,
Samba REJECT and safety markers. No persistent change or helper/job pending.
The goal remains incomplete; actual parent/text/resource/ratio state and visible
FA/UFA acceptance remain required, alongside the other replacement gates.
