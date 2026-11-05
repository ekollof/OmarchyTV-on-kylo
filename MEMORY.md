# Current project state — last verified 2026-11-04

AMP-VIDEO-OWNER-2026-11-04 adds firmware/relocation evidence only: video24 is
extin, whose actual open/close callbacks have no input teardown; the similarly
named release_func belongs to input selection. Private controls forward to
EX_PLAT. Full libv4l2_fd_open behavior/coherent authority remain unresolved;
no native backend, MCU call or new acceptance claim. The metadata-only TV
query used tvssh, which sent WoL after its initial reachability failure.
Observed char81:1, ndp_comp_extin, ndp-extin/ntkndpextin. No device open, fb1
access, staging or boot/PID continuity sample. Existing deployment unchanged
by these commands; this is not a current health/ownership test.

AMP-INIT-2026-11-04 adds blocking injection-only AD82587 Init orchestration.
29952 cases match complete native Init/Reset/Mute control traces/image/cache/
return;45 total reference groups pass in build-tv-v3/amp-init. Strict host,
leak-enabled sanitizer, ARM units and full host366 checks pass. GPIO failures
are ignored by native Init, and1 only means the write loop completed. No native
backend/module hookup, paired shutdown or hardware acceptance. AMP-OWNER adds
an injected owning worker with copied inputs/completions, retained state,
draining quiesce and actual nonblocking join through TLS destructors. Four
groups pass strict host/leak-enabled sanitizer/fresh ARM-qemu; full host370
passes. Target glibc2.23 exports the required GLIBC_2.4 tryjoin symbol; this
does not connect a native backend or integrate module completion/holds.
After the owner reauthorized tests excluding active fb1, the exact60756-byte
ARM worker unit passed all four groups on the TV's actual glibc/pthread.
Hash checked before execution; staging removed/no helper; boot ID and vendor
PIDs1967/1785 unchanged, no-reset retained. No device or framebuffer was opened.
Evidence: amp-owner-tv-2026-11-04. Current permission allows non-display tests
and view-only interrogation; fb1 and stock operation must remain untouched.
This supersedes historical offline-only restrictions in older entries below.

AMP-I2C-2026-11-04 adds owned fixed-endpoint bus3/address34 transport, native
ARM32 syscalls and explicit write permission/Init boolean adapter. Fresh
Aurora/NDAL3/PreInit instructions compared before implementation;10ms lower
post-write delay is distinct from Init orchestration waits. Three host/leak-
enabled sanitizer/ARM-qemu groups and full host390 pass. Refactored native
amp-probe (28328 bytes, SHA256 d9870fe2ab7e6f24fa257c3c0f17d0e51a5439aa3cdb7baa5bcd993fcbf1e4fb)
read03..05=1b/05/05 with completed2 on TV. Staging removed/no helper; boot ID/
vendor PIDs1967/1785 unchanged, no-reset retained; no fb1/audio write. Native
write acceptance, GPIO/MCU/board authority, worker/module hookup and complete
amp lifecycle remain open. Strict completion counts intentionally differ from
the HAL's nonnegative false-success policy; do not claim failure parity.

AMP-GPIO-QUERY-2026-11-04 recovers unsafe HAL getter side effects before adding
fixed direct reset114 GET670d/670f. Hal_Gpio_GetDirection first marks a pin open
via670a; ordinary Hal_Gpio_Read writes sysfs export. The maintained direct
query has neither effect. Two host/leak-enabled sanitizer/ARM-qemu groups and
full host392 pass. Native probe21812 bytes, SHA256
19517981e612e04eef2c0fc2502cc7963ed7eab739ac1f3e9ac40405769f669d,
returned level1/cached direction0(output) after separate staging verification.
Staging removed/no helper; same boot/vendor PIDs1967/1785/no-reset retained;
no fb1, claim/config/level/reset/mute write or MCU call. This is cached state/
latch observation, not voltage/ownership/rollback acceptance. GPIO/MCU native
control, paired lifetime and consumer/module integration remain open.

AMP-STBC-2026-11-04 adds an injected fixed-MCU8 V4L2 codec after fresh NDAL_EX/
kernel instructions. Getter takes pin and TWO uint32 output pointers; driver
status byte is separate from ioctl return. Expose raw reply byte16/byte8, not a
guessed direction/level pair. Two host/leak-enabled sanitizer/ARM-qemu groups
and full host394 pass. No native fallback or TV/MCU/fb1 operation. OpenDevice
type10 maps video24 and installs process-global fd only in its fresh-open
branch; lower open/close/MCU authority and paired lifecycle remain unverified.
Native/physical/audible acceptance is open. New durable project: ghidra_ndal_ex.

AMP-PROFILE-MODULE-2026-11-04 adds audio_amp_profile/audio.amp_profile v1 and
amp-profile CLI/actual Unix IPC. Copied profile/name snapshots, holds/removal,
failure cleanup/fd counts, suspend/deferred reload/resume and replacement
recovery pass strict host, leak-enabled ASan/UBSan and fresh ARM/qemu. Actual
socket tests include the saved hwprofile; full host361 checks pass. Source now
builds26 modules; installed ABI1/recovery and preserved stages are unchanged.
No TV access/deployment/hardware calls; offline-only restriction remains.
Audio consumer, retained image ownership and native lifecycle remain open.

AUDIO-AMP-PROFILE-2026-11-04 adds bounded hwprofile-to-amp input snapshots and
borrowed profile payload reads. Saved profile selects12Watt,1b/05/05/fine80;
all four scalar defaults arec0, DRC defaults-1. Host/leak-enabled sanitizer/
fresh ARM units and full host356 checks pass;2312 native payload comparisons
bring the reference harness to44 groups, all passing. Provider/cache/retained
image ownership and hardware lifecycle/audible acceptance remain open. No TV
access; owner offline-only restriction remains in force.

AUDIO-AMP-NATIVE-2026-11-04:6000 register-image profiles and38 write failures
match the complete shipped Init body under ARM/qemu, with controlled hardware
callees/absent GPIO descriptions. All43 maintained reference groups pass in a
fresh build-tv-v3/audio-amp-native binary; no production-helper fix required.
This supersedes static-only comparison for the image calculation. Native
profile/image ownership, hardware lifecycle and audible acceptance remain open.
No TV access; the owner's offline-only restriction remains in force.

Current owner constraint,2026-11-04: "You can test on the TV, as long as you
don't use the active display (fb1)." Non-display tests/view-only queries are
reauthorized; fb1 and stock operation must stay untouched. This supersedes the
earlier "Don't touch the TV" restriction. No persistent replacement or boot
changes. The historical offline-only entries describe their original runs.

UI-STACK-IPC-2026-11-05 (host-only): a real `--sock`/stdin stack runs
settings/launcher.db + image.decode + recorder display + ui_engine + ui_vendor.
Launcher rows, decode hash, ui-state and dependency-safe reloads of
image_decode/settings/ui_engine/nav_display/ui_vendor pass. Host397 ok,
verify45. Host aa6ccb50…, settings f771e777…, image_decode b4a12158…,
engine3783189c…, vendor1419e99c…, recorder9daf8c5e…. Non-HAL only; physical
fb1, full hardware module set, all-consumer holds and cold startup remain.

UI-VENDOR-IPC-2026-11-05 (host-only): `ui-state` exposes `ui.portal screen`;
a real `--sock` host with mod_nav_display (recorder display), ui_engine and
ui_vendor is driven through start/use/stop/start/reload/unload/reload,
including a display-provider reload and consumer restart. `status ui_vendor`
via stdin reports started/ui.portal:v1. No TV/HAL/framebuffer. Full host396 ok,
verify45. Host aa6ccb50…, ui_vendor1419e99c…, ui_engine3783189c…,
recorder9daf8c5e…. Recorder-only; physical fb1, full module set/holds and cold
startup remain.

IMAGE-DECODE-IPC-2026-11-04 (host-only): a real `openaurora --sock` control
plane with an empty autoload config now drives image.decode through the
`image-decode <path>` command and stdin: load/start, PNG/JPEG FNV, invalid-file
errors, status, suspend/EAGAIN/resume, stop/ENODEV/start reuse, reload and
unload/reload all pass. No display/HAL/TV module is loaded. Host suite395 ok,
verify.cjs45. Host binary d65b35b2…, module eb102138…. Limits: ui_vendor/display
reload, held-provider rejection and cold startup remain separate.

VENDOR-DECODER-DIFF-2026-11-04 (offline/qemu): an ARM LD_PRELOAD constructor
executes the shipped PngImageDecoderImpl and JpegImageDecoder before vendor
main, using vendor MemoryBufferSource/MutableImage and requested RGBA
ImageProperties (model3; model2 is BGRA). Nine corpus PNGs plus gray1/2/4/8,
gray+alpha, palette, palette+tRNS, RGB16, RGBA16, interlaced and gAMA variants
are byte-exact RGBA. DMR-small/large JPEG are byte-exact against TurboJPEG
`TJFLAG_FASTDCT`, now used by image.decode; ImageMagick accurate DCT differed
by up to8. Probe2d5aff61…, aurora7d6e4ca5…. No TV/HAL/UI. Actual foxxum JPEGs,
progressive/CMYK, APNG, worker/frame lifetime and on-TV/stock parity remain
open.

STOCK-PARITY-2026-11-04 (offline analysis): the saved
artifacts/protocol/shot-launcher-right.png is a pre-correction R/B-swapped
capture. After RGB swap, Prime reads (25,148,249) and Canal (254,33,0),
matching the current source orientation. The corrected stock portal and the
offline portal share 171x95 card geometry at209+190i,478 and rail focus color,
but card artwork/crop differs (Netflix RMSE0.376, Prime0.216, others
0.066..0.104), background is13,14,14 vs36,38,40 and stock status/gear/refresh
chrome is absent. The old capture is not an apps-grid or pixel oracle; a fresh
same-session corrected stock apps-grid capture is still needed. No TV access.

FB-SNAPSHOT-COLD-BUNDLE-2026-11-04 (offline): common/fb_snapshot saves
pixels/var/fix,8 region words (RD0x8020c86c/WR0x4020c86b), read-only MAU and
pan; restore verifies MAU and restores pixels/region/pan. display_fb with
display.restore_snapshot=1 snapshots before region change, restores on stop/
suspend and re-snapshots on resume. fb-restore uses FBR2. Fake-fb library,
tool-file and actual display-module tests pass; host354 ok, focused
ASan/UBSan clean. `make cold-bundle` builds an ARM bundle with25 modules,
oa-supervise, fb-restore and cold config; manifest.sha256 verifies29 entries.
Not executed on TV; full live-driver equality and independent startup remain
open. FB-RESTORE-REVIEW now fixes stale backing/layout acceptance, skipped
region/pan cleanup after mmap failure and false file-save success on close
error. Focused strict host/leak-enabled sanitizer/fresh ARM-qemu and full
host372 checks pass. Var/fix
are saved metadata, not restored modes. DISPLAY-CLEANUP now retains restore
failures/fd, reports failed synchronous suspend and runs stop/unload cleanup
on a joined worker; blocked-driver/create/join/restore/unmap/close tests pass
both variants on host/leak-enabled sanitizers/fresh ARM-qemu, full host382.
Direct firmware teardown/PMI instructions compared; snapshot retry is project
policy, not vendor lifecycle parity. DISPLAY-DSO now passes actual display_fb
host/loader/control-dispatch hold, busy/force reload, deferred resume, failed
start and shutdown tests on host/leak-enabled sanitizer/fresh ARM-qemu; full
host387 passes. GDB confirmed a completed worker could leave shutdown in
poll(-1) after the last timer vanished; a20ms shutdown poll cap fixes gate
retry without later notifications. Exclusive ownership/native equality and
external socket-client/display integration remain open. No TV access; active fb1 is
excluded by current permission. The existing cold-bundle is historical and
must be rebuilt to include the fixes; it is not ready for authorized execution.

VISIBLE-IMAGES-2026-11-04: a supervised no-plat candidate (image_decode v2,
display_fb, settings, ui_engine, ui_vendor) painted the live apps grid on
vendor-initialized fb1 from an empty ui.dir and the real
/buffer/downloaded_images. Log:18 items, first0, scrollbar0/600,51 paints.
The visible capture has15 cards including two JPEG foxxum downloads
(3753/1685,384x216; DAZN/DW) and13 PNGs; the two JPEG card crops match
nearest-scaled originals within≈2.1%/1.1% AE. fb1 pixels/yoffset were saved
and restored; candidate bundle/ready files removed, /proc scan0, vendor PIDs
1967/1785 intact, coredumpnoreset present. This is supervised visible parity,
not independent startup or full region/MAU/pan equality. The test completed
before the current "Don't touch the TV" override; no TV command ran after it.
Independent startup is blocked until explicit reauthorization.

AUDIO-AMP-IMAGE-2026-11-04 recovers AD82587 lifecycle/return contracts and
implements pure38-register profile conversion with caller-supplied retained
image. Strict host, leak-enabled ASan/UBSan, fresh ARM/qemu and full host349
checks pass. Compared statically against decompiled ARM; native Init execution
comparison and hardware ownership/init/resume/audible acceptance remain open.
Open/Close only return object!=NULL; ResumeDevice returns task-create status,
and the task ignores init's result. No TV access or deployment in this work.

HOST-CONFIG-2026-11-04 fixes global configuration leakage: entries/count now
belong to each host's immutable init snapshot. Old implementation fails the
new cross-host regression; strict host, leak-enabled ASan/UBSan and fresh ARM/
qemu registry tests pass, plus54 core lifecycle/power/control/skin checks.
Module reload still does not reread config; reinit requires prior caller
teardown. No TV access/deployment. Separate PNG/JPEG edits remain outside this
milestone; the full Aurora replacement is still incomplete.

TODO-REAUDIT-2026-11-04 reviewed all183 checks at172cd57;182 were retained as
bounded milestones and PNG acceptance was reopened at that snapshot. The
reopened item is now narrowed by the classic-libpng/JPEG follow-up below: the
non-PNG/truncated/lifecycle tests and provider-copy ownership are fixed, but a
vendor-class differential, APNG/CMYK policy, worker scheduling and on-TV
decode remain open. Recorder alpha works; its scene transforms are metadata
only. Fresh full host and all42 maintained ARM reference groups pass. No TV
access or deployment. See the follow-up report and per-item ledger.

IMAGE-DECODE-JPEG-2026-11-04: modules/image_decode publishes image.decode v2.
PNG uses the shipped libpng1.6 classic API with Aurora b30918 setConversions
(expand/pack, strip16, gray/palette, tRNS/filler, no gamma); JPEG uses the
vendor libjpeg-turbo2.0.1 TurboJPEG API with TJFLAG_FASTDCT. Nine real
downloaded PNGs (RGB/RGBA/colormap,400x225) match ImageMagick; two DMR JPEGs
match the shipped JpegImageDecoder class exactly (ImageMagick accurate DCT
differs by up to8). Truncated/non-image inputs reject, suspend returns EAGAIN
and reload republishes. ui_engine keeps OAI1 first,
tries PNG then JPEG in ui.download_dir (default /buffer/downloaded_images),
copies decoded pixels then calls provider release under hold. check-ui-png/jpeg
cards verify 400x225 and48/120 sources plus first pixels. Full host347 ok,
focused decoder/UI ASan/UBSan clean, decoder and UI tests pass ARM/qemu with
vendor libpng/zlib and libjpeg-turbo. Host395 ok. tools/boot autoloads
image_decode. Limits: actual foxxum downloaded JPEGs were not copied offline;
APNG/CMYK/YCCK reject; vendor-class differential is separate and covers PNG
variants/JPEG fast-DCT; worker, on-TV decode and stock parity remain. No TV
access after the owner stop instruction; all decode/UI evidence is offline.

ALPHA-RECORDER-2026-11-04 closes the TODO-AUDIT reopened item:
tests/ui_nav_display.c composes cap_blend with oa_canvas_over instead of
forcing alpha255 and publishes display.alpha, so ui_engine's clear_rgba keeps
transparent regions and partial source-over. check-ui-sources verifies a
transparent panel surround, opaque panel, invalid clear alpha and half-alpha
over transparent/opaque destinations; full host check273 ok, focused
ASan/UBSan (ticker module leak suppressed, unrelated) and the same test under
ARM/qemu pass. Frame2 mean alpha0.40625 =780/1920 with corner alpha0
(artifacts/analysis/alpha-recorder-2026-11-04/). This is production-canvas
fidelity, not vendor-Mali pixel/scanout parity. AGENTS.md now requires a TODO
item to be implemented, tested and firmware-compared for parity work before
it is checked. No TV access or hardware change.

TODO-AUDIT-2026-11-04: all182 prior checked claims inventoried against source,
tests and dated evidence. One reopened (recorder transparent RGBA fidelity);
181 retained at bounded scope with66 descriptions corrected/narrowed.
Fresh full host,42 ARM firmware-reference groups, host vendor corpora, eight
new build-tv-v3/todo-audit ARM layout/font units and host Mesa checks pass.
Recovery script/current image/pristine revert hashes match local artifact map.
No TV access, hardware change or deployment. Historical root build-tv-v3 is
the preserved16-module production stage, not the latest whole source build.
The runtime/deployment state recorded by WORK-REVIEW below is unchanged.

WORK-REVIEW-2026-11-04: reviewed cf78486..4ba1599. Fixed scrollbar ARM wrap
and first-window geometry ordering, four SVGWindow-state caches (not two
orientations), advertising/tracking-field exclusion, image byte flag/callback
boundaries, apps reload/restart/sorted64-card cap/hidden track and shared
128-image cache. Full strict host, focused ASan/UBSan,42 isolated ARM/qemu
groups (14,576 scrollbar geometry/state,32 skin,36 render,80 image) pass.
Native launcher fixtures explicitly normalize only the3 excluded advertising
slots; ordinary fields compare unchanged. Build-tv-v3/work-review identities
and TV upload/hash checks are in analysis/work-review-2026-11-04. Focused
TV CPU units, read-only live DB54 rows and loader-only display_fb dependency
listing pass. No-plat binding rejects HAL init, cannot run mdev and no longer
links HAL libraries; no revised display draw claimed. tvssh used WoL for access
and final cleanup reconnect; a stalled pre-auth local SSH was stopped and a
bounded retry verified removal (tv-cleanup-final.txt).
stock live PID1785/zombie1967 observed. Staging removed/no helper and no-reset
retained. No settings/network/service/boot/persistent write. Full display
snapshot restoration, native watchdog/ownership/fallback and cold startup stay
open; earlier FreshWDT refusal/impossibility inference withdrawn at source.

OA-CANDIDATE-2026-11-04: mod_display_fb (modules/display/display.c with
-DOA_DISPLAY_NO_PLAT) binds a candidate openaurora host to the
vendor-initialized fb1 without plat. The host (settings,display_fb,ui_engine,
ui_vendor; ui.screen=apps; live /conf/sqlite_db/settingsDB.db) logged
"painted apps grid: 18 items, first 0, scrollbar 0/600"; candidate-apps_1.png
is a real 1920x1080 fb1 scanout capture of the grid/scrollbar. Vendor Aurora
stayed owner; candidate stopped, /buffer/oa_candidate and /tmp staging
removed, stock no-signal screen verified. This is the supervised display/
scanout binding, not a cold independent startup. WORK-REVIEW withdraws the
unsupported inference that a historical FreshWDT return1 proves kick refusal
or makes native watchdog maintenance impossible. Vendor stop/watchdog disable
remain unauthorized; the native ownership/watchdog/fallback contract is still
unverified. No cold takeover was executed. Stock screenshot acceptance did not
prove full display-state restoration. Real downloaded-PNG decoding, stock apps-grid pixel parity and
independent startup remain open. No persistent change.

UI-APPS-GRID-2026-11-04: ui_vendor SCREEN_APPS5 (ui.screen=apps) uses the
live parent-0 type2 LauncherItem rows, sorted by m_order_index/id; grid
183,180,1554x720,5x3 cells310x230; top strip0,0,1921,162 #481730; scrollbar
1750,180,8x720. paint_apps feeds the recovered
oa_svg_scrollbar_render_background/render_bar ops into a rasterizer (track
#811924, bar #FFFFFF, caps20/16, min36; geometry via
oa_svg_scrollbar_calculate). Synthetic20 rows->15 cells/bar540; dumped18
rows->15/bar600; offline-apps-grid.png and tv-apps-recorder.png (TV CPU
recorder, ARM) exist. The visible scene-module-probe apps mode confirmed
PORTAL hold screen=5 apps=1, but the layer-1 capture returned the vendor
"Geen signaal" frame and restore refused after the vendor region changed; the
stock screen was verified and the stale backup removed, so no visible apps
scanout is claimed. No stock apps-grid capture: chrome/cell fitting/colors are
a geometry/resource reconstruction. Display/scanout binding, independent
startup, live-DB visible and real PNG decoding remain open. Host check265 ok,
verify.cjs42 ok; /buffer and /tmp staging removed, no helper.

SVG-SCROLLBAR-RENDER-2026-11-04: renderBackground0x10c3658 and
renderBar0x10c3bac implemented as ops-driven sequences. Both cache the supplier
image per orientation, resize the background (this+0x98)/bar (this+0xa4)
surface, configure top/bottom masks with align3/4, draw gTopM/gBotM/gRowM (or
bar_gTopM/bar_gBotM/bar_gRowM) under mTopM/mBotM/mRowM (bar_m*), and stretch
the row rect {0,top_cap,width,height-bottom_cap}. renderBar calls
calculateBarSizeAndPosition first (two window-rect queries); an empty bar
clears both surfaces and clears +0xc8. The +0xc8 byte is background_rendered
(renamed from background_hidden): renderBackground sets it, setScrollPosition
calls background only when clear.8 native/C cases compare sequence/cache/flag;
host, ASan/UBSan, ARM/qemu, full verify.cjs (42 ok), full make check (265 ok)
and a focused TV CPU of the render unit pass. Unit55752/
5cc5a6a259bf1f69bb80c893da620742, reference tab-policy from native-final.txt.
Vendor mask/surface/renderer calls remain stubs; no pixels/apps-grid
screen/rasterizer, so the 1750,180,8x720 scrollbar is not visible. Display/
scanout binding, independent startup, live-DB visible and real PNG decoding
remain open. No device/screen/settings/network/boot change; TV staging removed.

SVG-SCROLLBAR-SKIN-2026-11-04: calculateMinBarSize0x10c397c (0x174 bytes,
CRC0xd9fdda32) implemented as oa_svg_scrollbar_min_bar. Widget standard names:
0x20 bar_gTopM,0x21 bar_gBotM,0x22 bar_mTopM,0x23 bar_mBotM; scrollbar
resources scrollbarFA/UFA/FI/UFI (IDs61-64); BinSVG cap heights20+16 ->
apps-grid minimum36px. The body caches the supplier image per orientation
(+0x74/+0x84), leaves +0xb8 unchanged without an image, then resets and adds
the two moving-cap heights via the renderer vtable slot+0x14 and
RectangleContainer.16 native/C cases (orientation x cache x supplier x
bottom-cap) compare min/cache/trace; host, ASan/UBSan, ARM/qemu, full
verify.cjs (41 ok), full make check (265 ok) and a focused TV CPU of the skin
unit pass. Unit26996/94e26f3959cb13ace4ef905a10d44af3. Geometry now uses the
36px min with the 20-item/15-visible apps-grid cases. renderBackground
0x10c3658/renderBar0x10c3bac bodies (MaskPositioningTransform/
VerticalMaskScaleTransform/SVGSurface), the apps-grid screen, display/scanout
binding and independent startup remain open. The home portal has no card-row
scrollbar and there is no stock apps-grid capture. Live-DB visible and real
PNG decoding remain acceptance gates. No device/screen/settings/network/boot
change; TV staging removed, no helper.

SVG-SCROLLBAR-2026-11-04: SVGScrollBar::setScrollPosition0x10c4138 (0x70
bytes, CRC0x71cda0c0) and calculateBarSizeAndPosition0x10c3af0 (0xbc bytes,
CRC0x37806af8) implemented in common/svg_scrollbar.c. Fields bar pos+0xb0, size
+0xb4, min+0xb8, total+0xbc, visible+0xc0, first+0xc4, background-hidden+0xc8.
Track height=UIWindow::getWindowRect bottom-top (vtable slot0x28/0x1152649).
Formulas: size=height*visible/total clamped to min/height; position by
first/(total-visible) or bottom; full-height bar hidden; negative total
preserves; setPosition rejects not-created/first>total and gates background.
3120 native/C cases (2880 geometry+240 state; no writes outside fields), host,
focused ASan/UBSan, ARM/qemu, full verify.cjs (40 ok), full make check (265 ok)
and a focused TV CPU of the svg_scrollbar unit pass. Unit27208/
e8714135611df38b9fa7fdcd08e37a57, reference tab_policy_firmware
d8f340696db82ce7fb6df6efdad203a0. Apps-grid scrollbar is1750,180,8x720; the
home portal has no scrollbar. calculateMinBarSize0x10c397c (skin IDs
0x20/0x22/0x21/0x23 via supplier/renderer/RectangleContainer), render
background/bar and the apps-grid screen remain open; display/scanout binding
and independent startup are not done. Live-DB visible run and real PNG
decoding remain acceptance gates. No device/screen/settings/network/boot
change; TV CPU staging removed, no helper.

UI-LIVE-CARDS-2026-11-04: ui_vendor reads the live parent-0 LauncherItem rows
through the reloadable launcher.db service. launcher.db adds a borrowed row
view/get; ui_vendor keeps item_type2 rows with a non-empty thumbnail name
(skips slash names), sorts by m_order_index then id, caps at MAX_CARDS and loads
each named OAI1 asset. ui.cards override wins; ui.live_cards=0 restores
DEFAULT_CARDS; missing service/DB/rows fall back. Recorder tests: synthetic3/3,
dumped and live12 selected/9 visible on the TV (the dump has20 parent0 type2
rows). Offline capture offline-portal.png and visible TV capture
tv-portal-live_1.png show the stock nine-card order. The visible dumped-DB
probe restored bytes=16588800 yoffset=1080 MAU=0 region_match=1
snapshot_match=1. The live-DB visible attempt reached the hold but captured the
stock frame and the probe refused restore after the vendor region changed; the
stock no-signal screen was verified intact and the stale backup removed, so no
visible live capture is claimed (live rows were rendered in the recorder test).
Staging used /buffer/oa_portal_20261104 and /buffer/oa_recorder (owner
suggestion), then /tmp; all removed, no helper process, coredumpnoreset present.
The TV was asleep; tvssh.sh woke it. Rail remains static; PNG decoding, live
refresh, scrollbar/skin, scanout binding and independent startup remain open.
Host check265 ok, verify.cjs39 ok.

LAUNCHER-DB-2026-11-04: settingsDB LauncherItem read half implemented.
Member map recovered from launcher_item_info_struct0xfabf50 (16 scalar
columns,10 text columns at+0x38..+0x93e sizes300/60/150; DB blobs match).
copyLauncherInfoToItem0xfac334 (0xf4 bytes, CRC0xa089d47e) implemented as
oa_launcher_info_to_item;8 native/C cases compare the0xa0 item and ten UString
assignments. common/launcher_db.c reads the table read-only with the
getAllLauncherItems (no filter) and getLauncherItemsByParentId (parent match,
skip m_is_deleted/m_is_out_of_sync unless flagged) semantics; the provider
0x107a0c4 is only a vtable forward. mod_settings publishes the reloadable
launcher.db service; launcher-items <parent|all> [deleted] [outofsync] prints
count+rows. Dumped DB=50 rows; live TV probe read-only all=54 parent0=27
parent1(incl-deleted)=24. Host, ASan/UBSan, ARM/qemu, verify.cjs (39 ok), full
make check (265 ok) and four TV runs pass (launcher_item 32-bit refcount,
launcher_db strict, live probe, module load/reload/stop); staging
hash-verified/removed. The shipped SequentialQuery/MemberAccess engine is not
ported and no UI consumes the rows yet. TV was asleep; tvssh.sh WoL woke it.
No screen/input/settings/network/service/boot/persistent change.

LAUNCHER-ITEM-2026-11-04: LauncherItem::LauncherItem(NovoLauncherItem const&)
0xe4794c (Thumb, 500 bytes, CRC0xd1bcb8c5) implemented in
common/launcher_item.c. This is the settingsDB row to UI object conversion used
by LauncherDialog::getLauncherItemsList; it copies17 word fields+4 bytes,
10 UString copies+1 default, the shared_ptr pair at+0x8c/+0x90 with a control+4
refcount increment, sets+0x94=-1 and+0x98..+0xa0=0, and when the thumbnail name
at source+0x3c is non-empty builds "/buffer/downloaded_images/"+name at+0x2c.
The cbz-on-isEmpty branch direction was inverted in the first reading and
caught by the differential. 8 native/C cases (pattern x empty x control) compare
trace,164-byte output and refcount; host, focused ASan/UBSan, ARM/qemu, full
verify.cjs (38 ok) and full make check pass; focused TV CPU of the launcher_item
ARM unit exit0 with staging hash-verified/removed and the 32-bit refcount path
asserted. Build-tv-v3/settings-row-render unit42596/
d5ec4b2a4bd6d58e7194787f2792191c; reference1331260/
c661b4c5d0167eec8658ce66bed80330. The settingsDB adapter/query and
NovoLauncherDataProvider are still unimplemented; UString ops/shared_count dtor
remain stubs; the C side always uses the atomic refcount branch. No
screen/input/device/settings/network/service/boot/persistent change. No pending
job/helper.

SVG-WIDGET-STATE-2026-11-04: widget-state qualification from renderItems
implemented in common/svg_widget_state.c. SVGWindow::setFocus0xeeebd4
(0->1/2->3 on,1->0/3->2 off at +0x6c); SVGWindow::setActive0xeee4ec (inverted
argument:0 activates0->2/1->3, non-zero deactivates2->0/3->1);
SVGTextBox::setFocus0x10c8a28 (redraw+text-slide only when the state changes and
UIWindow::isCreated); setElipses0x10c6f48 (mode5 at +0x220 ignores switching on,
off still stores at +0x1b1). 36 native/C cases (16 window, 16 textbox, 4
elipses) compare ordered traces and state after 0xAA poisoning; the bodies
(CRCs0x085d6c76/0x7b1ef395/0x117744b9/0x1606bd56) are copied verbatim. Host,
focused ASan/UBSan, ARM/qemu, full verify.cjs (37 ok) and full make check pass;
focused TV CPU of the svg_widget_state ARM unit exit0 with staging
hash-verified/removed. Build-tv-v3/settings-row-render unit23336/
417eef21aa10f02356edc929cddb7210; reference1300760/
21eb58423f8efa60054322648091d4b3. Vendor redraw/slide/isCreated calls remain
stubs. Live resources, scrollbar/skin, display/scanout and startup remain open.
No screen/input/device/settings/network/service/boot/persistent change. No
pending job/helper.

SETTINGS-ROW-CONSTRUCT-2026-11-04: SettingItemsListBoxRow constructor0xd53d0c
(Thumb, 0x4cd bytes, CRC0xe68cf4e1) implemented in
common/settings_row_construct.c: base +17 children at fixed offsets,10
UStrings,47 default fields/name-id defaults, mirror probe rewriting0x2e4c/
0x2e50 to120/119. 96 native/C checks (2 mirror states x(47 words+1 byte)) and
the ordered call trace pass; host, focused ASan/UBSan, ARM/qemu, full
verify.cjs (36 ok) and full make check pass; focused TV CPU of the
settings_row_construct ARM unit exit0 with staging hash-verified/removed.
Build-tv-v3/settings-row-render unit41296/
fe301c0f647461920972441c40236c63; reference1284356/
9ec46aa90b38c839baa037ab648d5d71. The ctor is Thumb (objdump needs
force-thumb). Child ctor internals/vtables remain stubs; visibility internals,
live resources, scrollbar/skin, display/scanout and startup remain open. No
screen/input/device/settings/network/service/boot/persistent change. No
pending job/helper.

SVG-IMAGE-POINT-IDM-2026-11-04: SVGImage::create(int,EASIPoint const&,
UIWindow*)0x10c9554 and LauncherHelper::convertOperationIdToIDM0xe4ae0c
implemented. Point overload: setId, supplier gate, rect x/y from the point,
three getClientRect calls for client origin and clamping of the resource size
words (+8/+12) to client.w-x/client.h-y, then imageCreate. IDM: 62-entry
signed16 table at0x14efd84, -1 default. svg-image differential now80 native/C
cases (8 point); launcher536 comparisons (67 IDM). Bodies CRC
0x09f01190/0xf2156f40 copied verbatim; the EASIRect ctor fragment is copied
too. Host, focused ASan/UBSan, ARM/qemu, full verify.cjs (35 ok) and full make
check pass; focused TV CPU of the svg_image ARM unit exit0 with staging
hash-verified/removed. Build-tv-v3/settings-row-render unit49960/
665dcdf16db3758f710b1f6503fe4569; reference1238032/
0f09604f887a618ed332fffefc161658. Vendor callees remain stubs. No
screen/input/device/settings/network/service/boot/persistent change. No
pending job/helper.

SVG-IMAGE-CREATE-2026-11-04: SVGImage::imageCreate0x10c9430 implemented in
common/svg_image.c; svg-image differential now72 native/C cases (16 new:
created/uncreated x0xb0 x field-set x parent). Created resizes+draws;
uncreated creates/sets the surface, chooses scaled draw by byte0xb0, then
UIWindow::create; field words0xa8/0xac/0xb4/0xb8/0xbc. Shipped body CRC
0x5c029f6e copied at its original address; guard literal0x10c9550; three
targetHandle BLs retargeted into the body page because0x1149000 is occupied.
Host, focused ASan/UBSan, ARM/qemu, full verify.cjs (35 ok) and full make check
pass; focused TV CPU of the svg_image ARM unit exit0 with staging
hash-verified/removed. Build-tv-v3/settings-row-render unit47624/
393ae80d6fe9578884d193ec56b3cf47; reference1232196/
3c1081844a190c3e2ef1d63a0756aa41. Vendor surface/draw calls remain stubs; the
EASIPoint create overload0x10c9554 (CRC0x09f01190) is unimplemented. No
screen/input/device/settings/network/service/boot/persistent change. No
pending job/helper.

This is the canonical state summary. Latest narrow verification is recorded
in the dated entries below; the earlier CEC session is historical. Notes are preserved in
[the archive](artifacts/archive/README.md); they contain superseded claims.

## TV and persistent recovery

UI-FB-SIM-2026-11-04 (layout capture addendum): the host test display recorder
rasterizes clear/fill/blit/blit_scaled into 1920x1080 RGBA frames when
OA_NAV_CAPTURE is set (numbered present frames plus a .panel snapshot in
check-ui-settings-panel). ui-layout-portal.png captures the real ui_vendor
portal draw calls without TV/GPU. Synthetic panel assets paint no background,
so a real-asset preview needs OAI1 export + real ui.dir; the Mesa-wrapped
renderer DSO is still needed for GPU composition. Layout evidence only, not
scanout/vendor parity. make check passes 265 ok. No device/screen change.

UI-FB-SIM-2026-11-04: offline display simulation feasibility. `tests/
display_present` runs the real fbdev display module with a fake framebuffer on
host and as an ARM binary under qemu-arm-static (8 groups pass). The same
GLES scene code runs on host Mesa llvmpipe via `make scene-gles-mesa` (adapter
tests/egl_default_display.c maps eglGetDisplay(1) to EGL_DEFAULT_DISPLAY; 9
transition types pixel-exact, reopen/cleanup pass). qemu-arm-static cannot
emulate Mali//dev/mali and the sysroot has no ARM EGL besides the vendor blob,
so vendor rasterization and stock-vs-ours scanout comparison still need the
TV. Evidence: artifacts/analysis/fb-sim-2026-11-04/ and
artifacts/reports/UI-FB-SIM-2026-11-04.md. No device/screen change; no
staging or helper. Pending offline work: 1920x1080 fake-fb capture harness and
a Mesa-wrapped renderer DSO for the full preview.

UI-DISPLAY-REVIEW-2026-11-04: owner observed missing focused sliders/Modus
chevrons during real display preview. Fixed the captured-panel/live-text skin
to redraw shipped FA/UFA slider and selection PNG children after background
erases, sharing shadow digit/knob values; final run captures normal/focus0/1/2/
3/4/7. Stock-reference widget crops within2/255 per channel; both buffers identical
for each held state. Strict host/ARM build and focused regressions pass. Build
build-tv-v3/display-review: scene-module-probe542560/dced32954abe4ca03087f5e93a3c0225;
mod_ui_vendor188488/3467dc0004bdf5b34360eb6f4c289c8e. Last TV sample: stock
framebuffer MD541735daa69dc137743b59efe12db4a19, pan0,0, full16588800-byte
backing/region/MAU0 restoration verified. No helper left; /tmp staging/backup
removed, coredumpnoreset retained. No services/boot/network/persistent change.
Diagnostic only; focused typography, dynamic tabs/values, native widget/provider/
imageCreate and independent startup remain open. Report and dated evidence own
the stock-buffer selection correction and earlier failed-preview records.

UI-REVIEW-2026-11-04: reviewed28ef959..b50e3f0. Fixed SVG image resource
capacity16->32/max alignment/callback validation; upper bytes now tested.
Renderer signed arithmetic UB fixed via shared checked percent helper; all
native zero-divisors raise, prior saturation claim withdrawn. EDOM is explicit
error-policy deviation; no native zero-divide executed. Raw checkbox byte and
independent paired-child coverage added. Real divide fragment replaces stubs.
1771396 renderer/276 setter/56 image and prior native groups pass, plus full
strict host/Unix IPC, focused ASan/UBSan, ARM/qemu and three real-TV CPU units.
Build-tv-v3/ui-review image29380/bfe1350de41b8a6b6a87609ca6c9d3e9;
render234244/c5c94cb8f6cfcd523538147589843772;
setters33896/46cd1dada531c715434e80101da0ecbd;
reference1198032/1f6f229b70eb6b2303df52c4bfd0992f. TV staging separately
hash/size verified, removed/no-helper scan passed, no-reset retained. No
screen/input/device/settings/network/service/boot/persistent change; no pending
job/helper. Review only; imageCreate/live backend/startup/visible gates remain open.

GOAL-PAUSED-2026-11-04: owner paused the goal after commit15e8de8
(SVGImage::create/destroy orchestration), pushed, working tree clean. At that
commit strict host/ASan/UBSan/ARM-qemu, verify.cjs (35 ok lines) and full make
check pass; focused TV run of the svg_image ARM unit exited0 with staging
hash-verified and removed, no helper, no-reset flag intact. Nothing is running,
no pending job/helper/staging. Resume at imageCreate0x10c9430, then
convertOperationIdToIDM, row construction, child visibility internals/live
resources, scrollbar/skin, display/scanout and independent startup. No
screen/input/device/settings/network/service/boot/persistent change.

SVG-IMAGE-2026-11-03: SVGImage::create/destroy orchestration implemented in
common/svg_image.c and verified by18 native/C cases. create0x10c9638:
UIWindow::setId, SVGContext::getImageSupplier, supplier vtable slot2 resource
gate, rect copy, optional parent getClientRect + EASIRect::offset (x to words
0/2, y to1/3), imageCreate. destroy0x10c8ef4: SVGWindow::destroy +
SVGSurface::setCleanUISurface(this+0x9c). The shipped bodies (CRCs
0xbdf31b8d/0xcb48ce8f) are copied at their original addresses with a fake
supplier object/vtable; the offset fragment is copied verbatim. imageCreate
0x10c9430 (SVGSurface/UISurface/drawImageResource raster+actor subsystem)
remains an ops callback, as do the vendor calls behind the ops. Host, focused
ASan/UBSan, ARM/qemu, full verify.cjs (35 ok) and full make check pass;
focused TV CPU of the svg_image ARM unit exit0 with staging hash-verified and
removed. Build-tv-v3/settings-row-render unit27492/
72cffb9a41550743f01580a9e804558e; reference1202736/
55ee2b711faf21b61183479d03ed86c4. No screen/input/device/settings/network/
service/boot/persistent change. No pending job/helper. imageCreate internals,
row construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-SELIMAGE-2026-11-03: setSelectionBoxImageId internals implemented
and verified by1770796 native/C comparisons. The shipped140-byte body at
0xd54374 is copied verbatim (CRC0xcf7f04a2) and called from the copied
renderItems case-0x3 path: store id at row+0x2e54, getRectangle("image"),
SVGImage::destroy(row+0x2cf0) unconditionally, then create from rect+8 when the
re-read id is nonzero. New ops destroy_image replaces set_selection_image; the
15th key "image" widens the mask to32768. verify.cjs's qemu timeout was raised
30s→180s (doubled sweep runs ~33s). Host, focused ASan/UBSan, ARM/qemu, full
verify.cjs (34 ok) and full make check pass; focused TV CPU exit0 with staging
hash-verified and removed. Build-tv-v3/settings-row-render unit230100/
b906eb04493376658c6d82588e4089cc; reference1173116/
2db63cd8508c518d76ef9b08332df52d. No screen/input/device/settings/network/
service/boot/persistent change. No pending job/helper. SVGImage::destroy/
create internals, convertOperationIdToIDM, row construction, child visibility
internals, live resources, scrollbar/skin/module/display/startup/visible
acceptance remain open.

LAUNCHER-IMAGEID-2026-11-03: real LauncherHelper::getImageId implemented in
common/launcher_image_id.c and verified by469 native/C comparisons against the
verbatim shipped body0xe4aaf4 (copied to0xe4a000; guard literal0x01791d68
backed by page0x1791000, canary written before mprotect). Mapping: op>61=-1,
state==1 alternate else default, ops30/31/36/37=-1, 42/56/57 state-ignoring,
op52=825/790 asymmetric. Host structural checks, ASan/UBSan, ARM/qemu, full
verify.cjs (now34 ok lines) and full make check pass; focused TV CPU exit0 with
staging hash-verified and removed. Render ops glyph_id can be wired straight to
the real function (op2→733/734/734/734 tested). Build-tv-v3/settings-row-render
unit224760/c389c67af6b879bcf931ef9269951408; reference1170852/
380939250f9bac3df2084c267908ab44. No screen/input/device/settings/network/
service/boot/persistent change. No pending job/helper.
convertOperationIdToIDM, setSelectionBoxImageId/SVGImage internals, row
construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-17-19-2026-11-03: renderItems types0x17..0x19 (progress-bar rows)
pass886060 native/C comparisons; every renderItems switch case0..0x1a is now
implemented. Percentage is100*(0x2dfc-0x2df0)/(0x2df4-0x2df0) with truncating
signed division matching the shipped __aeabi_idiv; bars are row+0x1570 (0x17)
or row+0x1724 (0x18/0x19). 0x17 alone uses the alexa rect and row byte0x2e46
for its name/ball branches; 0x18/0x19 use names0x1bd/0x1be/0x1c0/0x1bf and
setDoubleDirectionalBar(1); 0x19's image refresh sets setVisible(1), unlike
0xb. New callbacks: set_ball_image/create_progressbar/set_percentage/
set_double_directional plus a silent __aeabi_idiv trampoline. Division is only
tested with a fixed10..90 range (no div-by-zero characterisation). Build-tv-v3/
settings-row-render unit221916/6f887a7fe7ece4dd590c84e87b9e887e;
reference1168648/8c3f0e7a70091990cbc63513bd244367. TV already awake, no-reset
flag kept, staging size/hash separately verified, test exit0, staging removed,
no helper. No screen/input/device/settings/network/service/boot/persistent
change. No pending job/helper. getImageId's real mapping, image internals, row
construction, child-visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-11-15-2026-11-03: text-entry variants0x11..0x15 pass394384
native/C comparisons (mode/max3/10,4/5,5/none,2/4,9/15; created row
UStrings0x2e0c/2e14/2e1c/2e24 vs numeric0x2e00 for0x15; 0x13 omits max+cursor
and returns after setEntryMode) plus full strict host, focused ASan/UBSan,
ARM/qemu, all prior groups and focused real-TV CPU. All row-side UString reads
now use the generic row_ustring; no new trampoline was needed after the callee
audit. Build-tv-v3/settings-row-render unit184024/
da0010c9f48ec8f6b969fba1029aa0cb; reference1135628/
983fa244ea672a632a12710fdb7000f6. TV already awake, no-reset flag kept,
staging size/hash separately verified, test exit0, staging removed and no
helper in /proc/*/exe. No screen/input/device/settings/network/service/boot/
persistent change. No pending job/helper. Cases0x17..0x19, row construction,
child visibility internals, live resources, scrollbar/skin/module/display/
startup/visible acceptance remain open.

SETTINGS-ROW-C-2026-11-03: renderItems type0xc (text2/text3/text4+slider)
passes312224 native/C comparisons (four own literals, alignment6 on0x1f68
only, row UStrings0x2e2c/0x2e34, nameless uncreated slider) plus full strict
host, focused ASan/UBSan, ARM/qemu, all prior groups and focused real-TV CPU.
The sweep now covers the full 13-key presence mask plus separate image
(0xb) and textbox (0xc) state passes; the callee audit caught the missing
SVGTextBox::setTextAlignment trampoline. Build-tv-v3/settings-row-render
unit173688/581135cfd4492a0dacf8aac5bd2ec1bd; reference1133784/
40ba362932bf993d0d87e06c1d0440fc. TV already awake, no-reset flag kept,
staging size/hash separately verified, test exit0, staging removed and no
helper in /proc/*/exe. No screen/input/device/settings/network/service/boot/
persistent change. No pending job/helper. Cases0x11..0x15,0x17..0x19, row
construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-B-2026-11-03: renderItems type0xb (slider bar+image1/image2)
passes74604 native/C comparisons (both-rect gating, image-state fork with
plain setWindowRect refresh or0x2e4c/0x2e50 create, mixed-state image1
re-create quirk, row+0x1498 slider with0x214/0x216/0x215/0x217 and
setSliderValue) plus full strict host, focused ASan/UBSan, ARM/qemu, all
prior groups and focused real-TV CPU. Harness now separates the image
children's created state from the row child with a dedicated pass.
Build-tv-v3/settings-row-render unit159736/
0aa47f2ca5618763f1350abe5b52b9a0; reference1127136/
6904c4fa5cfb7a46a542a6e470d810e7. TV already awake, no-reset flag kept,
staging size/hash separately verified, test exit0, staging removed and no
helper in /proc/*/exe. No screen/input/device/settings/network/service/boot/
persistent change. No pending job/helper. Case0xc,0x11..0x15,0x17..0x19, row
construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-A-2026-11-03: renderItems type0xa (second slider row) passes70456
native/C comparisons (own slidebar/text2 literals, fixed0x1e8.. text-box
names with no font, row+0x1498 slider with0x214/0x216/0x215/0x217,
setSliderRangeValues on the created path) plus full strict host, focused
ASan/UBSan, ARM/qemu, all prior groups and focused real-TV CPU. A full callee
audit preceded the sweep; only the case's own key/label literal redirects
were new. Build-tv-v3/settings-row-render unit147256/
682bb42a43bf68513cdbc58ac655dd0b; reference1118192/
f372ead9eaeb5d488a469284488b0570. TV already awake, no-reset flag kept,
staging hash-verified, test exit0, staging removed, no helper. Cases0xb/0xc,
0x11..0x15,0x17..0x19, row construction, child visibility internals, live
resources, scrollbar/skin/module/display/startup/visible acceptance remain
open.

SETTINGS-ROW-COVERAGE-2026-11-03: corrected a silent renderItems coverage
hole — the presence mask iterated only7 bits so image2/banner/slidebar/text2
were never present and type0x9's body never ran. The sweep is now a full
11-key pass plus a separate point/word/label/flag pass, and type0x9's created
slider is implemented (setWindowRect+setSliderRangeValues+setSliderValue).
66312 genuine comparisons pass with host, focused ASan/UBSan, ARM/qemu, full
verify.cjs and full make check; TV CPU exit0 with staging hash-verified and
removed. A third NUL-sled (SVGSlideBar::setWindowRect 0x10c4790) was caught.
Build-tv-v3/settings-row-render unit137816/9ee7b135e0425d619379400d798ec87c;
reference1114416/9a734643a7b592128c464d3f77529ab4. Cases0xa..0xc,0x11..0x15,
0x17..0x19, row construction, visibility internals, live resources, scrollbar/
skin/module/display/startup/visible acceptance remain open.

SETTINGS-ROW-SLIDER-2026-11-03: renderItems type0x9 (slider row) passes95624
native/C comparisons ("slidebar"+"text2" lookups, empty-char* textbox create
with formatted0x2df8 text, slidebar create from0x2df0/0x2df4 plus
setSliderValue) plus full strict host, focused ASan/UBSan, ARM/qemu, all
prior groups and focused real-TV CPU. Corrected two earlier readings: types
5/6's rebuild fold reads text1 slot12 (not image1) and setScalingType gets
the stale isCreated result while create gets0x92/0x93. Build-tv-v3/
settings-row-render unit136416/12625ef29fda096965cb78d1c58d5b97;
reference1112368/efe772995d71a2f1f264f0ec8694b2a2. TV already awake,
no-reset flag kept, staging hash-verified, test exit0, staging removed, no
helper. Cases0xa..0xc,0x11..0x15,0x17..0x19, row construction, child
visibility internals, live resources, scrollbar/skin/module/display/startup/
visible acceptance remain open.

SETTINGS-ROW-BUTTON8-2026-11-03: renderItems type0x8 (plain button) passes43976
native/C comparisons (0x2eac/0x2eb0/0x2eb4/0x2eb8 names in slots1/0/3/2,
icon-0 create, created setText refresh, no images/glyphs/stores/draw-on-top)
plus full strict host, focused ASan/UBSan, ARM/qemu, all prior groups and
focused real-TV CPU. No new callbacks; no rebuild branch so no crash input.
Build-tv-v3/settings-row-render unit122876/a9960598965254a2454a35d1677ae2b0;
reference1090492/321845c147d592de59a292c8ad296523. TV already awake, no-reset
flag kept, staging hash-verified, test exit0, staging removed, no helper.
Cases0x9..0xc,0x11..0x15,0x17..0x19, row construction, child visibility
internals, live resources, scrollbar/skin/module/display/startup/visible
acceptance remain open.

SETTINGS-ROW-BANNER-2026-11-03: renderItems type0x7 (banner button) passes40904
native/C comparisons (direct "banner" string, type-5 names, four glyphs,
elipses/shadow, icon create, created refresh without setText, setDrawOnTop)
plus full strict host, focused ASan/UBSan, ARM/qemu, all prior groups and
focused real-TV CPU. No new callbacks; the rebuild path folds text1 and is
excluded when null. Build-tv-v3/settings-row-render unit117948/
556e9adbfe0f37fcb4e245c59d767fb1; reference1089004/
e8c29ea89e6b4c4d818a3eba678f87af. TV already awake, no-reset flag kept,
staging hash-verified, test exit0, staging removed, no helper.
Build-tv-v3/settings-row-render. Cases0x8..0xc,0x11..0x15,0x17..0x19, row
construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-BUTTON-2026-11-03: renderItems types0x5/0x6 (launcher button +
image1/image2) pass38216 native/C comparisons (destroy/reinit, type5 vs type6
name sets, four glyph ids from getImageId(row[0x2dd4],state), highlight/
sublauncher/elipses/shadow, icon-carrying create, inline0x92/0x93 stores,
setDrawOnTop) plus full strict host, focused ASan/UBSan, ARM/qemu, all prior
groups and focused real-TV CPU. Two ABI fixes: getImageId is two-arg
(op,state) and create_button carries the icon id; the rebuild path faults on
a null image1 and is excluded. Build-tv-v3/settings-row-render unit113460/
accfc355c08beec4f27529c75d2f3e28; reference1086648/
0ecf0c2d784ea39266e0d7db6229d3ed. TV already awake, no-reset flag kept,
staging size/hash separately verified, test exit0, staging removed and no
helper in /proc/*/exe. No screen/input/device/settings/network/service/boot/
persistent change. No pending job/helper. Cases0x7..0xc,0x11..0x15,0x17..0x19,
row construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-TEXTENTRY-2026-11-03: the text-entry family0xd/0xe/0xf/0x10
passes33608 native/C comparisons via one parameterised body (names1/0/3/2,
empty-label create, mode/max0/4,0xa/5,0xa/5,1/10, created numeric0x2e00 for
0xe/0xf vs value-text0x2e04 for0x10) plus full strict host, focused
ASan/UBSan, ARM/qemu, all prior groups and focused real-TV CPU. The
differential caught an un-stubbed SVGTextEntryBox::setText (0x10b8160) that
slid through zero halfwords into a neighbouring stub; all twelve callees in
the family are now installed and audited. Build-tv-v3/settings-row-render
unit84360/5478829a37dba33078596133b049a5da; reference1049500/
5c32008cb7758bcb3d3ddceb015533a5. TV already awake, no-reset flag kept,
staging size/hash separately verified, test exit0, staging removed and no
helper in /proc/*/exe. No screen/input/device/settings/network/service/boot/
persistent change. No pending job/helper. Cases5..12,0x11..0x15,0x17..0x19,
row construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-13-2026-11-03: renderItems type0xd (text-entry box) passes24392
native/C comparisons (names1/0/3/2 from0x2ebc/0x2ec0/0x2ec4/0x2ec8,
empty-label seven-arg create, entry-mode0/max-chars4/cursor-auto, created
setNumericContents from0x2e00) plus full strict host, focused ASan/UBSan,
ARM/qemu, all prior groups and focused real-TV CPU. Two harness gaps fixed:
the per-case "selectionbox" literal (0xd55140) and the empty-label literal
(0xd55144). Build-tv-v3/settings-row-render unit80200/
cc442f3a259d0edfbf8ada9ce22e2fa9; reference1048972/
0d006d8c71faac8c0f9a0ac7467b20d1. TV already awake, no-reset flag kept,
staging size/hash separately verified, test exit0, staging removed and no
helper in /proc/*/exe. No screen/input/device/settings/network/service/boot/
persistent change. No pending job/helper. Cases5..12,0xe..0x15,0x17..0x19,
row construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-SELECTION-2026-11-03: renderItems types0x2/0x3 pass21320
native/C comparisons (selection box names in slot order1/3/0/2, empty-label
create, setSelectedItemWithId; case0x3 image-id call between lookup and null
check) plus full strict host, focused ASan/UBSan, ARM/qemu, all prior groups
and focused real-TV CPU. setSelectionBoxImageId internals remain a stub.
Build-tv-v3/settings-row-render unit68940/de749fa24ece86e7b20de2fdcd06fabd;
reference1028772/806eee8791762ba808a2fd17233dd5e0. TV already awake, no-reset
flag kept, staging size/hash separately verified, test exit0, staging removed
and no helper in /proc/*/exe. No screen/input/device/settings/network/service/
boot/persistent change. No pending job/helper. Cases5..21,23..25, row
construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-16-2026-11-03: renderItems type 0x16 (checkbox) passes7592
native/C comparisons (names9/10/11/12, alignment5, empty-label seven-arg
create, 0x2e44 checked; created path setWindowRect/setText) plus full strict
host, focused ASan/UBSan, ARM/qemu, all prior groups and focused real-TV CPU.
Two harness fidelity fixes: row label slots are UString objects (first word
char*) and are now dereferenced everywhere; the checkbox create is a
seven-argument call needing a wide preserving trampoline. The presence grid
widened to five keys. Build-tv-v3/settings-row-render unit57908/
9a3996a5f7b7a557e6df98438d80837b; reference1018508/
b7d9fc934aeb87adb442af25c74289a7. TV already awake, no-reset flag kept,
staging size/hash separately verified, test exit0, staging removed and no
helper in /proc/*/exe. No screen/input/device/settings/network/service/boot/
persistent change. No pending job/helper. Cases 2..25 except0x16, row
construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-1A-2026-11-03: renderItems type 0x1a passes3032 native/C
comparisons (shared tail 0x2e8c x4 names without setFont, then the button
case body with 0x2ed0/0x2ecc/0x2ed8/0x2ed4) plus full strict host, focused
ASan/UBSan, ARM/qemu, all prior groups and focused real-TV CPU. The case's
own "button" literal (0xd54e24) needed an owned-string redirect. Build-tv-v3/
settings-row-render unit52876/83222713de62681546e6ca4f43fbccd2;
reference1006308/5c97553c04ccd4331190efb376342993. TV already awake, no-reset
flag kept, staging size/hash separately verified, test exit0, staging removed
and no helper in /proc/*/exe. No screen/input/device/settings/network/service/
boot/persistent change. No pending job/helper. Switch cases 2..25, row
construction, child visibility internals, live resources, scrollbar/skin/
module/display/startup/visible acceptance remain open.

SETTINGS-ROW-RENDER-2026-11-03: renderItems 0/1/4/shared-tail paths pass2264
native/C comparisons (prologue/base/3 rects, type-selected children, created
gates, always-create) plus full strict host, focused ASan/UBSan, ARM/qemu,
all prior groups and focused real-TV CPU. Jump-table dispatch, reconstruct
fallthrough into case 4, dead-temp button create and 0x2dec-label slot read
from the ELF/objdump, not decompile. Build-tv-v3/settings-row-render
unit49420/c2bf2da32e109d5914526814c13b98d0; reference1004372/
f60da58c6f420ab9a6bffe4090e0d6b7. TV already awake, no-reset flag kept, staging
size/hash separately verified, test exit0, staging removed and no helper in
/proc/*/exe. No screen/input/device/settings/network/service/boot/persistent
change. No pending job/helper. Switch cases 2..26, row construction, child
visibility internals, live resources, scrollbar/skin/module/display/startup/
visible acceptance remain open.

SETTINGS-ROW-SETTERS-2026-11-03: label/progress/range setters pass324 native/C
comparisons (child select/created gates/always-stores) plus full strict host,
focused ASan/UBSan, ARM/qemu, all prior groups and focused real-TV CPU. Saved
setType-adjacent bodies closed: empty-format decimal text, wrapped percent with
__aeabi_idiv edges, type-selected children. Build-tv-v3/settings-row-setters
unit33304/91fdb5c24bb31ce2344a7796e6ed6cb8; reference933404/
d295d9d4fb62e388a55e4cc0aa6eda46. TV already awake, no-reset flag kept, staging
size/hash separately verified, test exit0, staging removed and no helper in
/proc/*/exe. No screen/input/device/settings/network/service/boot/persistent
change. No pending job/helper. Actual renderItems/row construction/child
visibility internals/live resources/scrollbar/skin/module/display/startup/
visible acceptance remain open.

SETTINGS-ROW-TYPE-2026-11-03: setType cache/rebuild/render/query and exact child
setter ordering pass12480 shipped ARM/C cases plus prior groups, full host/Unix
IPC, ASan/UBSan, ARM/qemu and focused real-TV CPU. Model type event joins to owned
row cache/child fields; no full widget renderer. Type24 preserves untouched1ca8;
unknown types only change labels. Full renderItems and pending label/progress/
range sources saved. Build-tv-v3/settings-row unit73700/
18e83e5cd1d9a9126172f211a022ad66, reference884992/
6f7d199a7942cbeb9c082c0028feb360. TV already awake, no-reset/free space checked,
staging size/hash separately verified, CPU test exit0, staging removed and no
helper in /proc/*/exe; no-reset flag retained. No screen/input/device/settings/
network/service/boot/persistent change. No pending job/helper. Actual widget/
resource/skin/module/rendering/startup/visible acceptance remains open.

SETTINGS-MODEL-2026-11-03: owned item/text/value/image snapshot and draw providers
pass2048 dispatch calls plus fault injection/deep-copy/reentry checks on strict
host/full suite/actual Unix IPC, focused ASan/UBSan, ARM/qemu and real TV CPU.
Model type writeback retained; default OK is explicit localized provider input;
numeric endpoints use verified %d. Build-tv-v3/settings-model unit77080/
f72781ca27b1d9e525b2e76d676c7684, GLIBC_2.4 only. TV already awake; no-reset
flag/free space checked, staging size/hash separately verified, test exit0,
staging removed and /proc/*/exe found no helper. /conf/coredumpnoreset retained.
No display/input/device/settings/network/service/boot or persistent changes.
No pending job/helper. Actual live item/resource acquisition, module/skin/row
widget integration and rendering/startup/visible acceptance remain open.

SETTINGS-DRAW-2026-11-03: complete parent SettingItemsListBox::drawRow dispatcher
passes18441 shipped ARM/C comparisons plus all prior groups; full strict host/
Unix IPC, focused ASan/UBSan and fresh ARM/qemu units pass.36 types/16 styles/
32 variants/9 bounds exits cover type writeback, setter/image priorities and
temporary string/vector lifetimes. Typed context-bound callbacks and resource
failure unwind, no module/service/hardware integration. build-tv-v3/settings-draw:
unit69108/de6663fcf9ab09417d3da564a7540840;
reference865972/3475e6122271bb8f53986e0c078d7b22. No TV access/wake/staging/
deployment or hardware/boot/network changes; no pending job/helper. Actual
item/text/vector/widget providers, rendering/scanout/startup and visible parity
remain open. Owner reaffirmed TV testing authorization; this item stayed offline.

SVG-LIST-REFRESH-2026-11-03: base/manager updateRow join and row setChecked
pass2880+24 native/C cases and all prior groups, strict full host/Unix IPC,
focused ASan/UBSan and fresh ARM/qemu units. Refresh total+8/slots/wrapping
upper bound and checkbox set-before-cache order established. Row-view fixture
rv_slots[i*8] indexing corrected to rv_slots[i]. Parent virtual+0x60 resolves
to SettingItemsListBox::drawRow (d50f60), saved but still unimplemented. Build
build-tv-v3/list-refresh: update40576/74a98ddc619d6efc9864a8044e67ff41,
widgets50976/920cbb14407eecd17e09f42ab261ac69,
reference747480/7183f3a632fec0682e9ddf83fb016ede. No TV access/wake/staging/
deployment/hardware changes; no pending job/helper. Parent/rendering/backend/
startup/visible parity and full replacement remain open.

ARM-HARNESS-REVIEW-2026-11-03: reviewed0493f91;153 native animation comparisons
and every prior group pass from fresh build-tv-v3/arm-review. Full strict host/
Unix IPC, focused ASan/UBSan and ARM animation/widgets/ui.list pass. Owned string
handles/context, actual parse/free/equality order, signed counts, character bounds
and NULL-name empty construction corrected. Wrong-page scrollbar stub fixed and
all installers bounds-checked. Unpublished OAL1 payload checks are atomic; text
temporary max-aligned; routing prose corrected to vertical. ARM unit68848/
af5a22b097f815056718febb0ec0e1f0; reference727940/
155459ad850f1846299b9d45c2e94390; ui.list79760/
3cc7ac7bb20227ef76d0146571649fe2. Starting14-case binary passed; old SIGSEGV
had no retained core, exact historical cause unproven. Host GDB ARM support
verified; bundled GDB lacks ncurses5. AGENTS updated. No TV access/wake/
deployment/hardware mutation; no pending test/Ghidra/helper job. Full replacement
and native rendering/startup acceptance remain open.

SVG-LIST-ANIM-2026-11-03: startSelectAnimation gates/rect-snapshot/search/
start passes 14 native/C comparisons with exact call order/args, snapshot
words and started float bits, plus full strict host, focused ASan/UBSan,
ARM/qemu and all previous reference groups. ARM unit 41568/
55826924de76d0055dbd52381a71bcab; reference 742252/
a954cefaa3c3435bd604a9b403faa213, build-tv-v3/tab-policy. Disabled/
missing-row/unchanged-rect exits, the +0x24c/+0x25c archive/store, the +12
name/+28 duration fields, the (1,length-1) substring args and the 1000.0f
scale from the literal pool are asserted; both sides run the real atof on
the same bytes with float-bit compare. No new module/service/UI consumer.
Parent refresh, scrollbar construction/rendering internals, owned backend/
context and visible acceptance stay open. No TV access/wake/staging/
deployment or hardware mutation; no pending job/helper.

SVG-LIST-SLIDE-2026-11-03: setSlideValue progress/slider/text routing passes
72 native/C comparisons with exact call order/args, written values and stored
words, plus full strict host, focused ASan/UBSan, ARM/qemu and all previous
reference groups. ARM unit 41960/1e4a649908c226f42070078e4520aeb8;
reference 688504/3b0579ccd12a63417d447f78786c12a4, build-tv-v3/tab-policy.
Two flag bytes, the type word, +0x1498/+0x13c0 bar selection, the +0x1ca8
text box, the empty UString format and the +0x2df8 store are asserted;
progress paths store nothing. No new module/service/UI consumer. Parent
refresh, scrollbar construction/rendering internals, startSelectAnimation,
owned backend/context and visible acceptance stay open. No TV access/wake/
staging/deployment or hardware mutation; no pending job/helper.

SVG-LIST-WIDGETS-2026-11-03: widget-level row/list getters pass21 native/C
comparisons with exact call order/args and return values, plus full strict
host, focused ASan/UBSan, ARM/qemu and all previous reference groups. ARM
unit28016/1f3652f87e52c1b095b9373070778cd7; reference661432/
a6f10183566624e4b0c58788b9ef4d6f, build-tv-v3/tab-policy. Numeric forward
through the embedded +0x18d8 entry and the selected-id created/none gates
with the +0x28c cell-then-array walk are asserted. The differential fixed a
single-vs-double indirection misread at its source. No new module/service/UI
consumer. Row editing, parent refresh, scrollbar construction/rendering
internals, startSelectAnimation, owned backend/context and visible acceptance
stay open. No TV access/wake/staging/deployment or hardware mutation; no
pending job/helper.

SVG-LIST-MODULE-2026-11-03: first ui.list v1 consumer over the verified
layers passes host, focused ASan/UBSan, ARM/qemu and actual Unix-socket IPC.
ARM unit78640/b798fca761bba0127fc824e79be9c6dc; module477796/
cc7cb4a6e792b423a5e9e8015d956426, build-tv-v3/list-service. Owned 9/5/16
geometry; verified key chain with inert render targets; list-state/list-key
over socket/CLI; OAL1 reload persistence; suspend/shutdown gates; clean exit
with socket removal. Widget-level row getters/editing, parent refresh,
scrollbar construction/rendering internals, startSelectAnimation, owned
backend/context beyond words/counters and visible acceptance stay open.
No TV access/wake/staging/deployment or hardware mutation; no pending
job/helper.

SVG-LIST-UPDATE-2026-11-03: row-view getters and the full refresh pass100+20
native/C comparisons with exact call order/args and resulting list state,
plus full strict host, focused ASan/UBSan, ARM/qemu and all previous
reference groups. ARM unit37116/18e9b7c333c0b3453fb5d7d067a3eff1;
reference637612/44c48cac483ec8c83886d682760c8e7d, build-tv-v3/tab-policy.
Slot arithmetic, wrapping last-visible, item bounds/handles, the virtual
total, the stored count word and the refresh operand order are asserted.
The differential fixed a volatile-call harness rule for entry-point calls.
No new module/service/UI consumer. Widget-level row getters/editing, parent
refresh, scrollbar construction/rendering internals, startSelectAnimation,
owned backend/context and visible acceptance stay open. No TV access/wake/
staging/deployment or hardware mutation; no pending job/helper.

SVG-LIST-SCROLLBAR-2026-11-03: scrollbar create forward and setScrollPosition
gating/store/show pass294 native/C comparisons, plus full strict host,
focused ASan/UBSan, ARM/qemu and all previous reference groups. ARM unit
25168/e4ee0a5440050006f786a37a8d060b3e; reference605412/
fbab2d68f4d9e54abaf6a75741b82965, build-tv-v3/tab-policy. Signed
first-vs-vis gating, conditional background/foreground rendering and the
rect/arg forwarding are asserted. No new module/service/UI consumer. Row
getters/editing, parent refresh, scrollbar construction/rendering internals,
startSelectAnimation, owned backend/context and visible acceptance stay open.
No TV access/wake/staging/deployment or hardware mutation; no pending job/helper.

SVG-LIST-IMPL-2026-11-03: updateImpl and rotateRows pass6952 native/C
comparisons with exact call order/args and resulting manager/array/selection
state, plus full strict host, focused ASan/UBSan, ARM/qemu and all previous
reference groups. ARM unit39636/bb24bb2f323f2132bdc1dc37d3db45f0;
reference580456/5e7a254d0d315a088860cc206d03389e, build-tv-v3/tab-policy.
Proved row +0x1C setFocus identity and sret getWindowRect transport; start
clamping, unfocus gating, full/partial distinction, per-row show/update/draw,
hide sweeps, writeback/clamp/refocus and the empty-message path are asserted.
No new module/service/UI consumer. Row getters/editing, parent refresh,
scrollbar construction/position bodies, startSelectAnimation, owned
backend/context and visible acceptance stay open. No TV access/wake/staging/
deployment or hardware mutation; no pending job/helper.

SVG-LIST-SCROLL-2026-11-03: updateScrollBar gating/rect/show/position passes
4320 native/C comparisons, plus full strict host, focused ASan/UBSan,
ARM/qemu and all previous reference groups. ARM unit
33264/ea13fa9dbef0abeb09bb453a2a1a15d6; reference531912/
7e750cd4bc6bd0923809741e00ece1c0, build-tv-v3/tab-policy. Signed
size-vs-total gating, the literal x/y/w/h formula with wrapping arithmetic,
conditional show and the hide path are asserted. The differential caught a
stub-convention defect (r3-clobbering branch to 4-arg stubs); multi-arg
installers now preserve every argument register. No new module/service/UI
consumer. updateImpl, row widget internals, row getters/editing, parent
refresh, scrollbar construction/position bodies, startSelectAnimation, owned
backend/context and visible acceptance stay open. No TV access/wake/staging/
deployment or hardware mutation; no pending job/helper.

SVG-LIST-ROWS-2026-11-03: RowManager field layout, focusItem body and
updatePartial forward pass324 native/C comparisons, plus full strict host,
focused ASan/UBSan, ARM/qemu and all previous reference groups. ARM unit
31856/51f101743bf2de2025e947a8cfb45010; reference496352/
52391ba87be496651ede73f678d3d20f, build-tv-v3/tab-policy. Layout +12 first/
+16 count/+20 selected-rel/+28 rows, wrapping rel arithmetic, ordered row
(un)focus and rel store are asserted against real native bytes. No new
module/service/UI consumer. updateImpl, row widget internals, row getters/
editing, parent refresh, scrollbar/animation bodies, owned backend/context
and visible acceptance stay open. No TV access/wake/staging/deployment or
hardware mutation; no pending job/helper.

SVG-LIST-MOVE-2026-11-03: eight primitive list move/selection/window bodies
pass5040 native/C comparisons with exact call order/args, plus full strict
host, focused ASan/UBSan, ARM/qemu and all previous reference groups. ARM
unit29592/633ac2af584f29dfde1f1796cc74404d; reference476308/
84ac2e10c385693d50c8ea6d3d84f3f9, build-tv-v3/tab-policy. Window up/down
shift without animating; only to-top/to-bottom animate, and the receiver
split (list box versus this+0xf8 manager) is asserted. No new module/service/
UI consumer. Primitive move targets, RowManager internals, row getters/editing,
parent refresh, owned backend/context and visible acceptance stay open.
No TV access/wake/staging/deployment or hardware mutation; no pending job/helper.

SVG-LIST-BODIES-2026-11-03: seven lower SVGListBoxBase key bodies pass26048
native/C comparisons with exact call order/args and handled truth, plus full
strict host, focused ASan/UBSan, ARM/qemu and all previous reference groups.
ARM unit48868/a64cc362dbdb16fa2838f28d2922a16a; reference453928/
1c170d59ddd2622891e4b486501de5aa, build-tv-v3/tab-policy. The differential
caught a real page-down scroll/wrap inversion and proved move/window/animation
primitives target the list box while only manager queries and focusItem target
this+0xf8. No new module/service/UI consumer. Primitive move/getter/editing,
parent refresh, owned backend/context and visible acceptance stay open.
No TV access/wake/staging/deployment or hardware mutation; no pending job/helper.

SVG-LIST-INPUT-2026-11-03: SVG key/message and typed row-value callback routing
pass999216 native key cases,49344 message joins and68640 type/raw-value cases,
full strict host, focused ASan/UBSan, ARM unit and all previous reference groups.
ARM unit26824/9c01c0509bbedcf847a4ab550d2f3fc5; reference387540/
3277a450de22e3949b2f19e1df75eee5, build-tv-v3/tab-policy. Full-width widget
aliases/priority/original operands and separate raw row sources are preserved.
No new module/service/UI consumer. Lower movement/select/page, primitive getters/
editing, parent refresh, owned backend/context and visible acceptance stay open.
No TV access/wake/staging/deployment or hardware mutation; no pending job/helper.
Owner requested stopping after this item and its handoff/commit/push.

SETTINGS-LIST-2026-11-03: list map/Right-Left/Digit callbacks pass65600 shipped
map/matcher and138240 native method cases, full strict host, focused ASan/UBSan,
ARM unit and all prior groups. ARM unit30512/175c832887165a55aa210d4e0dc33742;
reference358336/3ac22cad43a7e8901ad147801d5e296b, build-tv-v3/tab-policy.
Original row and post-key record are separately pinned lifetimes; Digit consumes
iff row exists, mirrored events are fresh. No new module/service/UI consumer.
Typed row values/lower SVG movement/routing/parent refresh, owned backend/context
and visible acceptance remain open. No TV access/wake/staging/deployment,
actual keys/drawing/settings writes or pending Ghidra/build/helper job.

SETTINGS-NAVIGATION-2026-11-03: navigateInList callback body passes101384
shipped ARM state/action comparisons, including signed viewport wrap/captured
geometry and repaired cursors before read. Full strict host, focused ASan/UBSan,
ARM unit and all prior reference groups pass. ARM unit28652/
329ea6b5c147420d8b66e2ea4bd247b1; reference320444/
b26527ce44cd218d558d24f0ca7da126, build-tv-v3/tab-policy. Active entries are IDs,
not pointers/widget indices. Count/cursor safety outside valid native domain
is explicit. No new module/service/UI consumer. Widget keys/lower SVG/row,
parent refresh, owned backend/context and visible acceptance remain open. No
TV access/wake/staging/deployment, keys/drawing/settings change or pending job.

SETTINGS-INPUT-2026-11-03: qualified base map/five callback input bodies pass
65600 shipped map/matcher routes and86400 native method cases. Full strict host,
focused ASan/UBSan, ARM unit, metadata/token/vtable verifier and prior groups pass.
ARM unit33244/ad1ad9e636556ead378bc63c3afbcf02; reference293592/
4f6b30beff52c1ef1c70f23a79160beb, build-tv-v3/tab-policy. No new module/service
or UI consumer. navigateInList/list-key implementation, parent refresh, owned
backend/context and visible acceptance remain open. No TV access/wake/staging/
deployment, keys/drawing/mute/setting change or pending Ghidra/build/helper job.

TAB-LEAF-2026-11-03: six callback handler bodies pass238860 shipped ARM cases,
including the actual signed-division helper, logical raw digit writes, action/
event identity and handled truth. Full strict host suite, focused ASan/UBSan,
ARM unit and all previous reference groups pass. ARM unit34024/
88e3284f0522353e59df410462de86e9; reference239948/
242d1b0f249cadfd4a8ade6487f97768, build-tv-v3/tab-policy. Volume uses isShowing;
digit end is qualified SVG, others virtual. No new module/service or UI queue
consumer. Remaining settings map/list/mute bodies, backend/context integration
and visible acceptance remain open. No TV access/wake/staging/deployment or
persistent mutation; no pending Ghidra/build/helper job.

TAB-DISPATCH-2026-11-03: common fallback map and pre-first key join pass65600
actual shipped ARM map/matcher cases and16384 caller joins, plus full host,
focused ASan/UBSan, ARM/qemu unit and prior reference groups. The joins install
the separately verified C pre-handler as a virtual method, not a second
independent proof of that body. ARM test44532/e25592dbcfbb7146f83018ffba44815c;
reference205780/b36b2bd2db145f79b9ccd13b5999bea4, build-tv-v3/tab-policy.
No new module/service or attached UI consumer. Base/leaf handler implementation,
owned backend/context, permission/lifetime gates and display acceptance remain
open. No TV access/wake/deployment/persistent change or pending job/helper.

TAB-PREHANDLE-2026-11-03: full outer method body implemented via callbacks,
65664 valid-tree ARM trace/message/state/handled-truth cases match. vertical_tabs
marker and caller zero/nonzero contract verified. Invalid missing-child cases
return ENODATA safely, not claimed native parity. Full host/focused sanitizer/
ARM policy and prior reference groups pass. Native pure test41588/
a22c76e4c4d19719b0c646f7b74d2ae9; reference183692/
c7f77c6d0b832860186390ab7ca64262, build-tv-v3/tab-policy. No new module/service
or attached UI consumer. Fallback map was subsequently implemented by
TAB-DISPATCH; owned backend/runtime context, permissions
and display acceptance remain open. No TV access/wake/deployment/persistent
change or helper/Ghidra/build job pending after checks.

TAB-EVENTS-2026-11-03: common callback controller implements the recovered
onUpDownPress/onRightLeftPress method bodies;1008 shipped ARM action/argument/
state comparisons pass, including changes during callbacks and original-message
identity. Full host suite, focused sanitizer/ARM policy and prior reference
groups pass. ARM pure test21512/e032185de9554ef077d02570bb4a60aa;
reference113600/8f69a99ddbc17289e2433a15e3a6847e, build-tv-v3/tab-policy.
No new module/service or attached UI
consumer; outer pre-handler/focus/permission/backend integration remains open.
No TV access/wake/staging/deployment/persistent change or Ghidra/helper pending.

HOTEL-CONTEXT-2026-11-03: settings now publishes read-only settings.hotel v1;
hotel-state works through actual Unix IPC. HMCSettings mapping/column_int byte/
word semantics verified; saved DB150e2be30e5948571860a8c60c9da525 reads0/1/0
with content unchanged. Not a TV cache poll. Common helper composes ui.profile
with persisted hotel values, retaining app/media/entry. Peer-publication rollback/
removal lifetime and closed writable-state bugs fixed.345 shipped row-copy
cases, prior corpora, real vendor-SQLite ARM DSO/corpus, host/sanitizer/policy/IPC
and23-source-module host loader checks pass. Native build only settings24588/
636e6abd4526dcea42dd523d349bdc76/maxGLIBC2.4; test485204/
6705c8bb048fe4526d49b67a7c7b1902, build-tv-v3/tab-policy. CONTENT-HEADER-OWNERSHIP
also confirms show+50 clears content's own five full-screen pointers, not paint
or TabDialog's header. No TV access/wake/staging/deployment/installed ABI/recovery
change. No helper/build/Ghidra job pending after checks. Controller/native
defaults/writers/cache/app/permission/visible parity remain open.

TAB-WRITEBACK-2026-11-03: ui.navigation v2 now exposes independent tab/item/
raw submenu-byte setters via C/CLI/actual Unix IPC. v1 prefix retained; OAN2
preserves all raw state and reads OAN1.1792 shipped setter cases/1792 DSO reload
round-trips, prior corpora, full strict host, focused sanitizer/ARM-qemu DSO
checks and23-source-module host preflight pass. Native build only mod28936/
4e0364e286f500720dad7d917422190e/maxGLIBC2.4; test492940/
d686e6827c8b088e289c56ae9dce3929, build-tv-v3/tab-policy. Corrected picture
content+18 as isShowing and+8/+c show/hide; immediate destruction after false
is not proved. Full event/lifecycle/consumer/hardware acceptance remains open.
No TV access/wake/staging/deployment or installed ABI/recovery change; no job/
helper pending. Paused/resumed research did not leave a running experiment.

TAB-CONTEXT-2026-11-03: source now builds23 modules. New ui_profile/ui.profile
v1 snapshots explicit swprofile file and exposes copied immutable connectivity/
hotel-support facts (exact-one semantics/default0/1). Saved profile both1,
MD5 a1d9e5e80a926a6e0abd812c98852894; this is not active hotel mode/live state.
Network-tab37a0 source is menu-capability byte9, not Wi-Fi availability. Scalar
decoder/shipped ARM65281 hashes/27 profile lookups, preceding884736/9216
corpora, full strict host suite, focused sanitizers/ARM DSO lifecycle and host
23-module preflight pass. Native build only ui_profile36832/67e4491c6909c2a251d38d62131e9d54/
maxGLIBC2.4; test483612/8c15a5d5bc6acb1671b2ada0b7e3356b,
build-tv-v3/tab-policy. No TV access/wake/staging/deployment, installed ABI/
recovery change or job/helper pending. Mutable hotel/app/media/entry context,
permissions/writeback and navigation/UI consumer remain open.

TAB-SELECTION-2026-11-03: initial fallback/selection helper implemented from
UIGroup and TabDialog ARM. Missing requested tab retains group index0 ->first
created tab; empty group focused_tab=-1/content_tab=0.9216 actual group-code
qemu cases pass in addition to884736 availability cases. Full host suite and
focused sanitizer/ARM tests pass. Final ARM pure test25220/d797381250fd2b7955961b249bf27454;
reference49624/47edeca6b311b14de6e6d8d842ca8e29, build-tv-v3/tab-policy.
No TV connection/wake/deployment or current-state inference; no job/helper pending.
Runtime context, content permission/writeback and queue/UI consumer remain open.

TAB-NAVIGATION-2026-11-03: reviewed consumer prerequisites against fresh ARM.
common/tab_policy availability matches actual shipped168-byte predicate under
qemu in884736 cases, including raw byte XOR/default IDs. Initial focus request
is open!=0 or signed item>0, still subject to content/permission gates. Corrected
old key-handler address: d700a0, not d6faec. Full host suite and focused
sanitizer/ARM pure tests pass;
build-tv-v3/tab-policy is offline-only, no TV connection/wake/deployment.
Native context, unavailable-tab fallback, content/writeback and UI consumer
remain open. No current TV-state inference from the older suspended record.

NAVIGATION-SERVICE-2026-11-03: ui_navigation publishes ui.navigation v1,
retains firmware-default0 tab/row/open fields and queued route IDs, exposes
nav-* CLI/Unix IPC. take copies current selection; OAN1 reload preserves state
and pending IDs. Project queue16, no drawing/modal dispatch/provider calls.
Full strict host/focused sanitizers/ARM-qemu and actual local Unix IPC pass;
host loader preflight includes new module (source22). Native build only DSO
26488/da07746c756b32322156219eceb36142/maxGLIBC2.4; test482168/
feeab5416f0c2be1e926db176ece119c/maxGLIBC2.17, build-tv-v3/navigation-service.
No target loader/hardware acceptance, TV access/wake, staging, deployment or
settings/persistent change. Latest imported TV record still reports suspension;
no current-state inference. No helper/build/Ghidra job pending. UI consumer/
TabDialog writeback/queue pressure/native interaction and other parity open.

LAUNCHER-ROUTING-2026-11-03: common launcher_route adds full62-entry signed
operation result conversion and28 ordered unconditional settings plans.
Firmware main pre-handler active-item branch ends main dialog with converted
result; DemoApp modal return reaches OperationFinder under its guard, queues13
for settings -> TabDialog(true,false). Focus/edit/sublauncher branches prevent
universal Select claims. Missing writes retain row/open state; enqueue is not
immediate dispatch. Not yet service/skin/IPC integration;4b/a6 and provider/
runtime/modal branches open. Logging/telemetry excluded. Full host/focused
sanitizers/ARM-qemu and independent62/28 firmware corpora pass. Native test
build only26996/33b69aae50bed048b2c73a592d7c1acf/GLIBC2.4, build-tv-v3/
launcher-route. No TV connection/wake/keys/staging/deployment; last imported
TV session still reports suspended, not newly polled. No job/helper pending.

AGENT-WORK-REVIEW-2026-11-03: reviewed ff89e27..e7e7333. Scanner now confines
key reads to eligible heap/Aurora read-only maps, translates snapshot pointers,
requires complete printable terminated keys and escapes delimiters. +ac is a
possibly stale lookup key (fresh ARM); raw+a4 UString metadata always retained.
Original prefix/snapshot defects reproduced. Full host/focused sanitizers/
ARM-qemu pass. Native build only37392/dc22cd80cc1d6b0f5e26ba730c3186e9/
GLIBC2.7, build-tv-v3/review-latest; not staged or executed on TV.
154 navigation case-local call inventories and SubLauncher virtuals verified
offline. Corrected uninventoried TabDialog absence/tab ownership and identical
VK/ICE claims. Last imported navigation session reports TV suspended; no TV
connection, wake, keys or hardware/persistent change in this review. No current
TV state/persistence inferred from old snapshots. No helper/build job pending.

UI-GLYPH-ROUTING-2026-11-03: firmware conditional dispatch verified through
doExecute/renderText/drawCharacter; nonnull GPU getter selects GL drawText,
null selects CPU fallback. Saved vtables/context verify nonnull configuration,
not execution of the captured frame. Probe optional RGBA base/opaque-parent
regression passes host Mesa/focused sanitizers/TV. Opaque178,29,43,255 title
gives1318 white core/1072 differences vs vendor1446, not a parity fix. RGB sampled
from saved background; title alpha255 is an explicit comparison condition.
Actual parent/FBO/presentation remains open. Native161920/MD5
e59d750f830df849618c4898281778bd/maxGLIBC2.7, build-tv-v3/glyph-background;
output8294400/be8e8e0ae0abe3e2aeb1ee9937fa4614 downloaded/hash checked.
All staging/helpers gone; same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
fb1 pan0,1080/backing41735daa69dc137743b59efe12db4a19, hook/hosts/SSH22/
telnet2323/Samba REJECT. No window/swap/input/settings/restart/persistent change.
Owner requested handoff/commit/stop; no helper/build/Ghidra job pending.

UI-FONT-GPU-2026-11-03: lower scene_gles coverage-glyph/FBO composition API
implemented from shipped ARM/shaders. Full strict host, focused ASan/UBSan,
ARM-qemu geometry and owner-approved native private-pbuffer synthetic/title
tests pass. No service/wire/UI integration, no window/visible acceptance.
One-pass core1971 still differs from vendor1446 by525; two-pass core1228 differs
by1150, so this is NOT a closed pixel fix. Native-varying versus old-composition
shader gives identical two-pass full hash; do not fit offsets or blame fonts.
Persistent framework/task/client/renderer/context graph was read, but actual
title draw execution was not traced; the old Surface was freed/rejected.
Final native160268 bytes/MD5 772970e414644b47ec2c3b415a7a1cbd/maxGLIBC2.7,
build-tv-v3/glyph-gpu. Corpus8294400/25e09914c06a252c31cf92de49d419ef.
All staging/helper gone; same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
fb1 pan0,1080/full backing41735daa69dc137743b59efe12db4a19, hook/hosts,
SSH22/telnet2323 and /vendor/iptables Samba REJECT. No display drawing/input/
settings/restart/persistent mutation. ui.title stays default-off; no job pending.

FONT-X-OFFSET-2026-11-03: closed getter error: b42468 returns -signed font+5,
not zero. Guarded live font18 header reads0xe9 (-23); shipped font18/font7
minimum-lefts-23/-18 explain title23 and normal MODE-row18. common/font_box
now reserves the global offset and positions start/center/end accordingly.
Title origin23,5/ink136..388,31..85 matches saved vendor bounds; core pixels
still differ, so title default-off and complete raster/style parity remain open.
Full host/focused sanitizers/ARM-qemu/native memory-only corpus pass. Native
162884 bytes/MD5 22074919b41c8f0e789f0e3615ae3029/maxGLIBC2.7, build-tv-v3/font-x.
Separate size/hash verification before execution; staging gone/no helper/job.
Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/hook/hosts, SSH22/
telnet2323 and Samba REJECT; no display/input/device/settings/persistent change.

UI-TITLE-LIVE-GRAPH-2026-11-03: owner opened stock settings panel. Later ready
capture confirms red panel and title ink136..388/31..85. View-only heap scan plus
firmware-grounded pointer reads find foreground Surface108,5,691,95 and retained
LineObj23,5, exact Instellingen/zero margins/start1/center3/unit glyph ratio.
Flag+1b1 maps to CharClipping, not bold. X assignment remains open; no UI source
or coordinate change. Offline saved graph verifier passes; private data durable
in workbench/title-horizontal-20261103. Scanner32936 bytes/MD5
9d542f8a2cc4121590a18a45552871f1, separate verification before execution,
removed/independently checked. Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/
live1785/hook/hosts. No drawing/keys/settings/device/persistent change or helper/
job pending. Earlier launcher captures were stale relative to owner ready cue;
no MAU defect established, no fbregion execution. Owner can use TV normally.

UI-TITLE-BOX-2026-11-03: optional title now uses existing boxed alignment/clip
renderer, full583x90 viewport; font18 height80 gives local Y5. Corpus absolute
ink113,31..365,85 agrees vertically with dated vendor sample;23px X open, title
default-off. Full host/focused sanitizers/ARM-qemu and native memory-only corpus
pass. Native tests/font_box154344 bytes/MD5 4da2f04d1f5f857d2a69b69598cbf9da,
maxGLIBC2.7, build-tv-v3/title-box. Separate hash check before execution, staging
removed/absence checked and no helper/job pending. Current read-only sample:
same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785, firmware/hook/hosts
hashes match, SSH22/telnet2323 listening, Samba OUTPUT REJECT retained. No
display/input/device/settings/persistent operation; not reboot persistence proof.

UI-WORK-REVIEW-2026-11-02: reviewed through6822fce, fixed scanner read-span
overflow and ui.edit_init bypassing the lab flag. Full host/focused sanitizer/
ARM-qemu checks pass; build-tv-v3/review-20261102, details/hashes in report.
No TV connection, display operation or deployment in this review. The other
agent's saved live-scan-title-2026-11-02.txt records a created font18 title
candidate and actual list/row geometry; this supersedes the earlier absence
sample for those observations, not a current live poll. ui.title remains
default-off with unexplained draw/layout delta. Durable copy of that agent's
research: artifacts/workbench/agent-ui-review-20261102; no helper/job pending.

UI-ROW-LABELS-2026-11-02: offline label-source trace completed. Ordinary key
constructor/drawRow/setLabel add no prefix; langprofile Languages/Dictionary
contains41 locales/2435 records. MODE hash5aa5907f record987 resolves dut slot10
to exact Modus bytes4d6f64757300. Hash-checked bounded full-table evidence script
passes; live translation-fit flag and actual parent/resource/ratio state remain
open. No fitted-coordinate/renderer/persistent change and no TV connection in
this investigation. Owner's latest observation is awake HDMI1 external video;
last verified device/access/privacy state is the following historical sample.
Handoff updated and work stopped as requested; no helper/build/Ghidra job pending.

UI-LIVE-OBJECTS-2026-11-02: read-only vtable-candidate scanner added/tested.
Native32736 bytes/MD5 c7ac91ee12465bad32819cebfbe13c57/GLIBC2.7,
build-tv-v3/ui-scan. Full strict host/focused sanitizers/ARM-qemu pass; native
four-heap-range176934912-byte scan completes:6 uncreated textbox candidates,
no settings/row/list/dialog candidate in that subset (not global absence).
Owner reports awake HDMI1 external video. Blank OSD/WS OTHER do not mean standby
or crash; one Menu1048 POST201 produced no observed menu. No navigation/power/
device operation followed that report; this turn returned to view-only work.
Old wspoll.c not run due to concrete parser bugs; host Node WS queried state.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/
firmware64c252340d3af6c5d66072611b7bad7a/access/privacy; staging removed and
checked, no helper/job pending. No persistent change. Actual settings instance/
text/ratio/skin and visible acceptance remain open; fitted origins unchanged.

UI-TEXT-VIEWPORT-2026-11-02: common/font_box.c joins recovered widget alignment
mapping with vertical/local line rendering. Exact9-alignment pixels and actual
460->489/488/font7/20 render54 offscreen lines, Y2/1. Full strict host/focused
sanitizers/ARM-qemu/native TV CPU tests pass. Native152496 bytes/MD5
e0c91d6934f7803622016fd48bbb427a/GLIBC2.7, build-tv-v3/font-box. Same stock
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy;
staging removed/checked, no helper/job pending. No display/device/settings/
persistent operation. Settings/Picture/SVGDialog margin bindings and BitmapFont
X offsets were incorrectly called0; FONT-X-OFFSET corrects that to the negative
global minimum-left bearing. Base-list ctor insets0, setLabel adds no fixed prefix. Actual
parent/window/text/resource/ratio state still open: conditional origin31,184
does not establish old fitted49,184. No guessed offset or UI fit replacement.

UI-TEXTBOX-COMPOSE-2026-11-02: common/widget_layout.c composes fresh normal
single-line textPos surfaces, alignment/font marker policy; full-height row
slices, not top/bottom cap subtraction. Actual460->489/488 and20 live startup
font metrics yield heights66/70 and fonts7/20 for an explicit parent. Text
color strings are stroke, fill none. Pre-mask textPos rejected pending complete
second-pass/cache support. Full strict host/focused sanitizers/ARM-qemu/native
TV CPU tests pass. Final native124016 bytes/MD5 d1e11762fe72fc0d2314731bd953e2ad/
GLIBC2.7 in build-tv-v3/textbox. Same stock boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197/live1785/zombie1967/access/privacy; staging removed/checked,
no helper/job pending. No display/device/settings/persistent operation. Actual
parent/glyph-origin/ratio/clip and visible text integration remain open; fitted
UI coordinates and captured panels unchanged.

UI-ROW-WINDOWS-2026-11-02: ordered BinSVG group geometry now in
common/widget_layout.c; exact iterator rectangle accessor in common/binsvg.c.
Fresh firmware proves first mask raw/pre-mask identity/later cached transform
and normal rowItems/mRowM -> child-window composition. Actual460 tests derive
text1/labelLong/text2 for an explicit parent, not a live widget instance.
Full strict host/focused sanitizers/ARM-qemu/native TV CPU tests pass. Native
85492 bytes/MD5 7d40f2f7997459b0e683e82b5eb02a71/GLIBC2.4 in
build-tv-v3/row-windows. Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/
live1785/zombie1967/access/privacy; staging removed and checked. No display/
device/settings/persistent operation, no helper/job pending. Animation groups
explicitly unsupported; actual parent/textbox/text-origin/style integration
remains open and fitted UI origins unchanged.

UI-THEME-SKIN-2026-11-02: ui_vendor now holds/rebinds ui.theme and resolves
DialogLauncher animation markers from resource bytes. First numeric mode wins;
only copied mode survives selection/removal. Missing resource/markers means0,
missing optional provider retains stock4 fallback, ui.fs_type override retained.
ui.launch_anim remains default0/lab-only, not a vendor launcher transition.
Full strict host, focused sanitizers, ARM-qemu/native TV real-DSO policy/lifetime
tests pass, live resource selects60/type4. Native test479640 bytes/MD5
e876bcf71623a2ad6333402a7184827f; ui_vendor161632 bytes/MD5
72c2d2ba71d5751a8542aff979354ab9, both maxGLIBC2.17, build-tv-v3/theme.
Recorder display/injected progress: no visual/device acceptance claimed. Same
stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/
privacy; all staging/fixtures removed and checked, no helper/job pending.
Actual row/textbox geometry, styles and visible integration remain open.

UI-THEME-PROVIDER-2026-11-02: common/svg_pack.c owns a fully validated immutable
index/binary snapshot; mod_ui_theme publishes ui.theme v1 (record/named/count).
Borrowed views require a service hold. Full strict host, focused ASan/UBSan,
ARM-qemu and native TV real-DSO lifecycle+702-record corpus checks pass. Source
now21 modules; preserved16-module stage/installed ABI1 unchanged. Native test
508228 bytes/MD5 9975f90fcce4483dd9b2187d034b549e/GLIBC2.17; provider
47116 bytes/33609e021a52fab6c3e9b61b0459d6d4/GLIBC2.4 in build-tv-v3/theme.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/
access/privacy; staging and synthetic fixtures removed and independently checked.
No device/display/settings/persistent changes, no helper/job pending. Actual UI
service ownership/rebind and widget/text integration remain open; current panels
and text origins unchanged. Failed replacement still has no host binary rollback.

UI-BINSVG-VIEW-2026-11-02: common/binsvg.c adds bounded immutable record
validation/iteration and first-match named rectangle lookup, optionally scoped
to the first matching group (including descendants). All702 records/11888
elements/6733 rectangles/39 animations pass strict host/focused sanitizers/
ARM-qemu/native TV; full host suite passes. Actual477 slices now feed the
maintained geometry helpers from the pack, with an explicit test window.
Native75356 bytes/MD5 4ecee7b4a67391ff35dcc22ac47ed2cd/GLIBC2.7 in
build-tv-v3/binsvg. Live theme hashes match offline copies. Same stock boot
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy;
staging removed and checked. No device/display/persistent changes, no helper/
job pending. Indexed loader/theme provider/actual widget integration remain
open; current UI still uses fitted origins/captured panels.

UI-HORIZONTAL-ORIGIN-2026-11-02: common/font_text.c adds aligned line layout/
drawing with physical width, center overhang trim/reset-X and end right-bearing/
reverse paint order, correcting the previous advance interpretation. Whole-line
validation; explicit plain EF41 unit-ratio scope, no word justification/shaping.
Full strict host, focused sanitizers/corpus, ARM/qemu and native TV synthetic/
30-line font7/16 offscreen checks pass. Native82476 bytes,
MD5 bd1384ee414f673ad2858477ddcc7d3c, GLIBC2.7, build-tv-v3/font-line.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
hook/hosts/access/privacy; staging removed and checked. No device/display/
persistent changes, no helper/job pending. Actual widget style/resource/clip/
vertical-origin integration remains open; existing UI origins still fitted.

UI-TEXT-ORIGIN-2026-11-02: concrete list/row/textbox margin bindings resolve
the zero-margin base getter. Vertical logical placement and phase-preserving
ratio-floor helpers implemented in common/text_origin.c. Strict full host,
focused sanitizers, ARM/qemu and native TV contracts pass. Native30248 bytes,
MD5 d8dcf038231ff43f355573d7798f7b7a, GLIBC2.4, build-tv-v3/text-origin.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
hook/hosts/access/privacy; staging removed and independently checked. No
display/device/persistent change, no helper/job pending. Horizontal/direction/
actual style/coordinate/runtime theme integration remains open; UI origins
remain fitted. See UI-TEXT-ORIGIN for exact entries and rejected assumptions.

UI-SLICE-STRETCH-2026-11-02: retained double-ratio horizontal stretch and middle
destination construction implemented; composed raw477→measured/painted row test
passes without screenshot fits.49,538 VFP rectangle cases pass ARM/qemu and
native TV, full strict host/focused sanitizers pass. Native46836 bytes,
MD5 d38cfe511c7b0b6f3aaa8ba31c57b92d, GLIBC2.4, build-tv-v3/slice-stretch.
No device/display/persistent changes. Same boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197/live1785/zombie1967/access/privacy; staging removed and checked.
No helper/job pending. Runtime theme/client-window/text-origin integration
remains open; fitted UI coordinates unchanged. See UI-SLICE-STRETCH.

UI-LIST-METRICS-2026-11-02: raw slice measurement, painted-row cached metrics
and visible-count ordering implemented in common/text_layout.c. Raw475/477
slice heights give step70/top16/bottom10. Strict full host/focused sanitizers/
ARM-qemu and native TV list_metrics test pass. Native36008 bytes,
MD5 e888fe330468ff6b40e61b3e883d3c1d, GLIBC2.4, build-tv-v3/list-metrics.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
access/privacy, staging removed and checked. No device/display/persistent change.
Older widget-name expansion corrected at source; pointer chains retained.
Runtime transformed inputs/window step/text origin and UI integration remain
open; optional live label positions remain fitted. No helper/job pending.

UI-MASK-LAYOUT-2026-11-02: common/text_layout.c now also supplies explicit
mask positioning and vertical/horizontal/mirrored row arithmetic, recovered
from firmware. Strict host/full suite, focused sanitizers, ARM/qemu and TV
tests pass. Native26776 bytes/MD5 f45e0b8791697caa2cf09da628cb4d51,
GLIBC2.4 in build-tv-v3/mask-layout. No display/device operations. Same stock
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy;
staging removed and independently checked. No helper/job pending. Actual
runtime metrics/slice composition/text origin and full widget integration are
open; current optional live label origins remain fitted. See UI-MASK-LAYOUT.

UI-TEXT-GEOMETRY-2026-11-02: common/text_layout.c implements the recovered
single-line textPos height adjustment with float32/truncation and bounded errors.
Foreground consumes+1ec textPos; +254 is addition. This corrects the newer
research's width/round-half-away/unknown-consumer claims. Strict host/sanitizer/
full suite and49,728 ARM VFP comparisons on qemu and TV pass. Native17620 bytes,
MD5 873adb0528dc642df3fab21e6be64cc9, GLIBC2.4, build-tv-v3/text-geometry.
Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts/
access/privacy and safety markers; no device/framebuffer access. Staging removed
and independently checked. No pending helper/job. Goal resumed; actual row/mask
positioning, baseline and complete layout integration remain open.

UI-GLYPH-DRAW-2026-11-01: single-glyph RGBA compositor, clipping/signed bearings
and firmware coverage arithmetic implemented. All52022 shipped glyphs compared
to stored coverage offscreen; full host249 plus three font groups, focused
ASan/UBSan, ARM/qemu and TV pass. Native build-tv-v3/font-draw identities are in
the report; no device/framebuffer access. Same stock boot71f7eb63-727f-4b9a-
a9c7-2b04dc3f9197/live1785/zombie1967/access/privacy; staging removed and checked.
SVG marker mapping confirms focused488 is bold with a different text rectangle
from regular/default489; captured recoloring is not focus text parity. String
layout/font-data provider/UI text integration remain open. Owner requested
handoff and stop after this item; no helper/build/Ghidra job remains pending.

UI-FONT-LIVE-2026-11-01: common/font_pack.c reads the vendor index and one
record by ID (font7=847,332 bytes; font16 also loaded from the live /vendor2
pack; the 30 MB pack is not loaded whole). common/font_text.c renders one LTR
packed-UTF8 line with EF41 advances, explicit spacing, a clip and
common/font_draw coverage compositing; it validates the whole line before
drawing (missing/invalid draws nothing). Fits vs the corrected capture: row
labels font7 at (49|79,184+70*row) IoU0.9799..0.9840; text values font7
(Natuurlijk493,183; Uit553,533; Auto536,605; Aan545,673) IoU0.875..0.954; slider
digits font16 at (659,250+70*(row-1)) IoU0.907..0.921; disabled "Beeld zoomen"
font16 grey at (51,602). C measures font7 height61/advances118,143,302,168,48,
67,83 and font16 height68/digits49,51. ui_vendor ui.text=1 pre-renders 17
items, erases only the captured text strips (labels x40..486, text values
x480..680, digits x648..738) and blits live text; missing/invalid font falls
back to the captured panel. Title/tabs/slider graphics and focus overlays stay
captured; row8 uses the visible captured string. Host check-font-pack/
check-font-text/check-ui-settings-text (live+missing) pass; full make check 251
indented checks plus the font groups. Real fb1 (PID20201, ui_engine1952912c… v2,
ui_vendor e0e7a690…) vs the corrected panel: 24,545/2,073,600 any-channel
(1.184%), only22 px >8 (max11), RMSE0.2712/255; rows0-4/6 have no >8 diff
(row5 5, row7 7, row8 10). fb1 restored (16588800 bytes, offset0,1080), staging
removed. Next: title/tabs/slider graphics/focused labels (firmware geometry
only — analysis/ui-text-setup-2026-11-01 records the decoded state-indexed
resources and renderForeground line/font/alignment fields; transformed textPos
and per-widget setup are still undecoded, so no title/tab implementation from
fits), value/tab interaction and exact alignment/substitution/localization.
Evidence: artifacts/analysis/ui-font-live-2026-11-01/ and UI-FONT-LIVE-2026-11-01
report.

UI-FONT-SELECTION-2026-11-01: common/font_finder.c implements firmware-derived
stable typed maximum-height selection, signed height/line-count division and
global-smallest fallback. Startup20 IDs/heights verified from decoded theme.
Host full249 plus two font groups, focused sanitizers, ARM/qemu and native TV
tests pass. build-tv-v3/font-selection binaries37748/b4afdf02bc01759f17107257dd5c5204
and24240/c476f17d4f3d747e4ec4b429f55b16ff. Same stock boot/live1785/zombie1967,
access/privacy, all staging removed. No device/display operation. Widget text
style/layout/render integration remains open; see UI-FONT-SELECTION report.

UI-FONTS-2026-11-01: bounded EF41 grayscale bitmap reader and packed UTF8
lookup implemented; all38 live/offline font records pass host, ASan/UBSan,
ARM-qemu and TV checks. Full host suite exits0. UI text rendering/font selection
remains unimplemented; browser/FreeType native-row claim corrected. Native test
31640 bytes/MD5 1867d20017ecdc3e9c633afe9c7ba798, GLIBC<=2.7, isolated
build-tv-v3/font-bitmap. Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
live1785/zombie1967/access/privacy; no devices opened or drawing. Temporary
staging removed and independently checked. See UI-FONTS report/raw analysis.

Vendor UI is running on 192.168.178.87 / 70:54:b4:fe:70:c6. Following physical
USB recovery, fresh SSH key login and root telnet worked; Ethernet, routes,
hosts block, Samba OUTPUT REJECT and privacy/internet/WoL values were checked.
[Stability evidence](artifacts/reports/STABILITY-2026-10-31.md).

- Review's later view-only sample: boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
  uptime6min, stock Aurora PID1785 and zombie1967. The boot changed before our
  query; its cause was not established. Installed core/hook/hosts hashes match
  the values below; SSH22/telnet2323, Samba REJECT and safety markers remain.
  No restart or TV write was performed by the review. Raw review-tv-state.txt
  is in artifacts/analysis/cec-ownership-2026-11-01.
- Boot ID ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 was the earlier CEC-session
  id. A later same-day view-only read did not recheck that id. SSH and ping
  answered, uptime was 14h48, and load was about 15 from the usual uninterruptible
  driver threads. No process was named aurora. dmesg showed `[COREDUMP] SIGNR:6`,
  then `kcec: SW RX data full`. `sdb` was attached. `run.sh` was in `do_wait`.
  Vendor partitions stayed mounted. Nothing was restarted. The earlier PID 1737
  claim is superseded.
- /conf/oa_safe and /conf/coredumpnoreset present; /conf/oa_boot absent.
  Native and web launcher boot are disabled. No native test/helper was left running.
- Installed core is ABI 1, MD5 a898384df076572cf377749f25427689.
- Hook MD5 0b1cd32a9de2b0251f6252e6603db8b7; keepalive
  4ebd0f9d7ee9f470afde8b0f2807522b. One keeper repairs SSH/telnet permissions/
  listeners, hosts/Samba protection, privacy/internet/WoL and Ethernet/DHCP.
- Privacy: internet=1, ads/personalised/marketing/third-party/ACR=0,
  DNT=1, logging=0, wake-on=1. Internet=0 disables networking on this firmware.
- SSH 22 is key authenticated; telnet 2323 is unauthenticated root fallback.
  Standby suspends services; historical quick-standby WoL works. Deep standby
  and guaranteed always-on remote access have not been proved.

The tested recovery image is 994da7ff76a0b2fbafb8f6fa0c682627, with script
de1591298d633cf6422a1ade4e8835a9 (584 bytes). Loose payload and
recovery-stability copies match. Raw mtd13 is older; identities are in
[artifacts/README.md](artifacts/README.md). Do not rebuild/replace these during
ordinary development.

Outage lesson: an stdin upload in a backgrounded AND-list likely truncated the
hook. Separate upload, size/hash verification and subsequent actions. The exact
older USB script worked; the earlier failed recovery attempts do not prove a
syntax defect.

## Development build and evidence

SKIN-SELECTION-2026-11-01: host socket/stdin `skin NAME`/`skin` and startup
ui.skin policy now select modules and fall back to vendor on missing/start/
enter/first-frame failure, after safe failed-candidate cleanup (including NULL-
returning failed init). ui.skin v1 status is separate; drawing/registration stays
ui.engine v2. Runtime override is not persisted. Host249/focused ASan/UBSan/
Unix socket/native contracts pass. Real GPU PID8795 switches Omarchy/vendor,
recovers missing skin, verifies alpha and restores full backing hash
41735daa69dc137743b59efe12db4a19/pan0,1080. Same boot/PID1785/protections/access,
staging removed. Native build-tv-v3/skin-selection, exact final-tested.md5 and
stage.md5 in analysis identify artifacts. Production16/installed ABI1 unchanged.
Real OmarchyTV/content/exact visual/cold parity and crash recovery remain open.

UI-ENGINE-REVIEW-2026-11-01 supersedes the split's v1 dispatch contract below.
ui.engine v2 uses skin-owned trampolines and a bounded event queue; legacy
register_skin fails ENOTSUP. Forced unload/rebind, both skins' re-registration,
async/alpha companion return, failed receipts/readiness, timer failures, assets
and identity reset are covered. Host242 checks/focused ASan/UBSan/native skin
contracts pass. Actual GPU PID4365 passes vendor-instant Select,11 rail revisions,
reload/module cycle/BGRA24,23,22,153/full restoration. Backing MD5 remains
41735daa69dc137743b59efe12db4a19, pan0,1080, same boot/PID1785/access/privacy.
Staging/backups/helpers removed. Native build-tv-v3/ui-engine-review and report
identify final binaries; production16/installed ABI1 unchanged. Initial retained-
transform timeout was fully restored and corrected. Full content/visual parity
and automatic fallback remain open.

UI-TRANSPARENCY-2026-11-01: firmware rectangle reader + ARM fill/opacity
conversion recover portal RGBA36,38,40,153. Both display backends now publish
display.alpha v1; canvas source-over and CPU identity premultiplication preserve
alpha, ui_vendor owns companion hold. Host227 checks/focused sanitizer pass; real
GPU readback BGRA24,23,22,153, dialog/reload/module cycle and complete restore
pass. Independent real-video appearance and exact animation sequence/cadence
remain open (owner requests explicit audit). Native build-tv-v3/ui-alpha:
scene_display59752bytes/5d6a1d411c37b1769b2098c348bcd9c6, UI90184/
42aa9ed02e8cdeceff9fee818569b4cd(max2.4), probe500996/
6acfb7f39389f2fd5f91958df1b554ab(max2.17). Same stock boot/PID1785,
full framebuffer41735daa69dc137743b59efe12db4a19/pan0,0/access/privacy; staging
removed. Installed assets/core and matching production16-module stage unchanged.

RAIL-ASSETS-2026-11-01: verified eleven vendor APNG frames exported to ignored
artifacts/workbench/rail-ui-20261101 with provenance/hashes. Full-bundle regression
exposed MAX_ICONS24 truncating the loop to nine frames; capacity now29 includes
the maximum12 cards. Focused strict/sanitizer and full host226 checks pass.
Real TV private bundle, GPU dialog completion, UI reload and module suspend/
resume pass; seven image revisions observed in nominal660ms do not establish
visible frame order/30ms cadence. Full backing hash41735daa69dc137743b59efe12db4a19,
pan0,0 and same boot/PID1785/access/privacy/protections verified afterward.
Staging/backups/readiness/helpers removed; installed assets unchanged. Native
build-tv-v3/rail-assets: UI89704bytes/7f0106c721fcb478266f4166127081cb(max2.4),
diagnostic501052bytes/1d6120b69608b09e554b2313ddc87904(max2.17).
Production16-module stage unchanged. RAIL-ASSETS report distinguishes the
initial pre-fix run/manifest from the final tested DSO.

UI-ASYNC-2026-11-01: ui_vendor consumes display.async receipts, acknowledges only
actual success, owns its sampler/holds and caches transform-only dialog progress.
Four strict host/sanitizer/TV groups, full host226 checks pass. Actual TV portal
with installed assets, Down/Select/Exit, completed dialog, UI reload and host-only
module suspend/resume pass. Full snapshot/region/MAU/pan restored; independent
hash41735daa69dc137743b59efe12db4a19/pan0,0 unchanged. Same boot/PID1785,
hook/hosts/access/privacy/safety markers. All staging/backups/readiness/helpers
removed. Installed assets lack rail_hl01..11; no rail animation acceptance here.
Native build-tv-v3/ui-async: UI DSO89728bytes/0e29cd402c4aa9cacf678aaa85ea8522
(maxGLIBC2.4), final unit463032bytes/ffca0080be7e9a324fe09197774859cb and
tested diagnostic499600bytes/6da1edfe2327da62cb38988c34091eca (max2.17).
The old3-group unit identity is historical; final-tested.md5 owns current unit.
Source18 modules, matching production16 unchanged. Remaining visual/asset/input,
full feature parity and cold/TV standby acceptance remain open.

SCENE-DISPLAY-2026-11-01: optional scene_display canvas/coalescer publishes
display.async receipts; legacy present rejects ENOTSUP. Four strict host/
sanitizer/TV groups, full host222 checks pass. Real101-request burst retains
the final identity receipt; full diagnostic observes412 GPU completions, host
drain/unload and full snapshot/pan/MAU/region restoration. Independent backing
hash41735daa69dc137743b59efe12db4a19/pan0,0 unchanged. Same boot/PID1785,
hook/hosts/access/privacy/safety markers; staging/backup/helpers removed. Native
build-tv-v3/scene-display: DSO53236bytes/9293d421420b1ad0d7b03e41f3a57f1a
(maxGLIBC2.4), unit469528bytes/7f6f98030dcd8f40631713f2e369435a and
diagnostic487196bytes/a62146c477759bfb3d3e3d94d2dd055e (max2.17). Source18
modules, matching production16 unchanged. Subsequent UI-ASYNC above integrates
ui_vendor/readiness; full visual/cold acceptance remains open.

GRAPHICS-MODULE-2026-11-01: mod_graphics/display.gpu v1 adds owned watch/timer,
nonblocking quiescence, public host-id tickets and suspend/resume/holds. Four
strict host/sanitizer/TV groups pass; full host218 checks. Actual service
completes410 GPU frames, then the real host reactor retries cleanup until DSO
unload and full framebuffer/region/MAU/pan restoration. Native isolated artifacts
under build-tv-v3/graphics-module: DSO69900bytes/2afefd7ffaedc6bff9e411561e05ddc0
(maxGLIBC2.15), lifecycle478980bytes/f3ff7b116074fe7cee3f7440cb2fc065 and
diagnostic483724bytes/3e091ecdf8ef747c6c949d70b24d6415 (max2.17). Same stock
boot/PID1785, full hash41735daa69dc137743b59efe12db4a19/pan0,0, hook/hosts,
access/privacy/safety markers. Live Mali hash matches offline source. Staging/
backup/FIFOs/helpers removed. Source now17 modules; production stage remains16,
not rebuilt/deployed. Next: GPU display consumer/coalescing/ui_vendor/readiness.
Abnormal child exit latches unproven cleanup; automatic native recovery is open.

SCENE-CLIENT-2026-11-01: lower nonblocking parent (submit/pump/take/shutdown,
WNOHANG reap) passes five strict host/sanitizer/TV groups and410 actual GPU
frames through the diagnostic adapter. Blocked finish/close retains the child;
shutdown never waits/signals, canceled results stay collectable. Full host214
checks pass. Isolated build-tv-v3/scene-client: unit90108bytes/
4068c3b34d51bb38e0b340fdeb3a2762 (maxGLIBC2.15), diagnostic91068bytes/
df6428c2f968b617121474b15fc933c7 (max2.17); helper/renderer hashes unchanged
from SCENE-HELPER. Child exit0, full snapshot/pan/MAU/region restored; independent
hash41735daa69dc137743b59efe12db4a19/pan0,0 unchanged. Same boot/PID1785,
hook/hosts hashes/access/privacy/safety markers. Staging/backup/FIFOs/helpers
removed. Production stage unchanged. Next: host service/watch/reap timer,
coalescing, completion-driven readiness and UI/module lifecycle integration.

SCENE-HELPER-2026-11-01: separate EGL-free bootstrap plus late-loaded renderer
passes two process fixture groups on strict host/sanitizers/TV and410 actual GPU
frame requests on the TV. Constructor fd survives inherited-fd cleanup; child
drains/reaps before full snapshot/pan/MAU/region restoration. Independent full
hash41735daa69dc137743b59efe12db4a19 and pan0,0 unchanged. Same boot/PID1785,
installed hook/hosts/access/privacy/safety markers; staging/backup/helpers removed.
Full host209 checks pass. Isolated binaries under build-tv-v3/scene-helper:
bootstrap57304bytes/596643ed63c76231e6a7e31fd22d7efd, renderer68064bytes/
f4196fa2413cbf60efdfb136d8b56e4b, diagnostic74588bytes/
d875eff769e4b421dd6e6d4d70bf14fb; maxGLIBC2.4/2.4/2.17 respectively. No
production stage refresh. Next is nonblocking parent provider/completion/readiness
and module lifecycle; the diagnostic client blocks and must stay out of the host.

SCENE-BROKER-2026-11-01 adds the lower immutable-image IPC and blocking owner
server. Four injected-renderer groups pass strict host, ASan/UBSan, ARM/qemu and
actual TV; complete host suite207 checks pass. Native unit87760bytes/
ba89e0afce422f03e8254ae1b0c307cc, maxGLIBC2.4,
preserved under build-tv-v3/scene-broker. Target mmap_region rejects read-only
MAP_SHARED on sealed files; instruction-backed MAP_PRIVATE correction passes
with all seals retained. No EGL/hardware calls in this test. Staging removed;
same boot/PID1785, full framebuffer hash41735daa69dc137743b59efe12db4a19,
pan0,0, installed hook/hosts hashes, access/privacy and safety markers confirmed.
Production stage unchanged. The later SCENE-HELPER closes executable integration;
host service remains open. Mali must load after inherited-fd sanitation. See report for the
initial timeout and emulator-only ancillary truncation discrepancy.

EGL-WINDOW-2026-11-01: direct GPU display diagnostic passed, owner independently
confirmed upright/full-screen/smooth effects. Eight types each51 presents in
about1000.4ms at50Hz pacing. Full16588800-byte snapshot, MAU0, region and pan0
restored; full backing MD5before/after41735daa69dc137743b59efe12db4a19. Stock
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/PID1785 and protection/access hashes
remain; staging/backup/helpers removed. No production module integration or
stage refresh. Current native binaries under build-tv-v3/egl-window:
egl-window-probe88780bytes/5a66a935317e0623660151ffac96a926 and regression
egl-scene-probe74504bytes/aed778d42cc3aa4e3ff8ddc995b9767c, maxGLIBC2.17.
The regression also verifies cached textures, bounds, thread ownership and
pbuffer reopen. Next: nonblocking owning helper/service/completion/readiness
and module lifecycle integration. Synchronous EGL cannot run in quiesce.

SCENE-WARP-TV-2026-11-01: the display v2 software scene warp was exercised on
real fb1 with tools/display/scene-probe.c (ARM, staged /tmp, SHA-256 checked,
vendor Aurora left running). All eight types presented at p=0..1; the identity
endpoint readback matched BGRA (80,0,0,255) for every type; the probe restored
all 16,588,800 bytes with snapshot_match=1 and original yoffset 0. Per-present
timings: types 0/2/3/4/5/6 87-117 ms; type 1 keystone 483 ms; type 7 427 ms;
type 8 493 ms. Vendor PIDs 1967/1785 unchanged, staging removed, no helper left.
Readback is not scanout proof and these software timings are far below smooth
30/60 fps; the EGL path remains the parity target.

PANEL-OBSERVATION-2026-11-01: the owner watched the real-GPU UI on the panel for
two runs (~20 s). The rail highlight played once, but appeared to restart before
completing because the probe's 2000 ms rail window ends with a focus-changing
Down key that resets the sequence; the hold/observation window must extend past
the last frame before any key. The dialog transition visibly moved DOWN, not up:
we apply only the recovered full-screen type-4 shift (negative clip y) to the
whole scene, while the vendor's dialog has its own named-rect animation
(SVGDialog::drawAnimatedRectangles 0x10c5161 looks up the animation record's
rectangle in the dialog description and draws it). The full-screen surface and
the dialog therefore move separately; our single whole-scene shift sends the
dialog the wrong way. Next: recover the dialog's named animation rect/motion
(record name at +0xc, rectangle lookup at this+0xdc) and draw the dialog upward
over the type-4 background shift, then re-observe on the panel.

DIALOG-MOTION-AUDIT-2026-11-01 (partial): DialogLauncher.svg (resource 129,
span 57345..57634) contains the element `animation_slide_up_fade_in` followed by
four signed-16 values -109, 1096, 100, 100. SVGDialog::drawAnimatedRectangles
(0x10c5161) iterates the AnimationContainer at this+0xe8, reads the record name
at +0xc, builds a UString, calls an unnamed helper at 0xc86b4c and then 0xc8724c
with operands (1, len-1) before RectangleContainer::getRectangle at this+0xdc,
and draws the resulting rect through the facade. The exact meaning of the
substring step (the helper is not exported) and whether the four values are a
from/to pair or a rect is not resolved. Do not implement a dialog offset until
those are decoded; the whole-scene type-4 shift remains the only implemented
motion and is the observed wrong-direction defect.

DIALOG-HELPERS-2026-11-01: the two helpers are identified by their call-site
labels: 0xc86b4c = UString::length() and 0xc8724c = UString::getSubstring(uint,
uint). drawAnimatedRectangles calls getSubstring(name, 1, len-1), i.e. drops the
record name's first character before RectangleContainer::getRectangle. If the
record name carries a leading type marker, the looked-up name is exactly the
pack element "animation_slide_up_fade_in" (values -109,1096,100,100); that is
consistent with the name existing in DialogLauncher/MainMenu3/4/Tall, but the
record-name prefix and the four values' semantics (rect vs from/to) are still
unproven. Consequence: no dialog offset has been implemented; verify the
hypothesis against the AnimationContainer record construction or a live A/B
before changing the motion.

DIALOG-DIRECTION-2026-11-01: the firmware pipeline does not invert the type-4
motion. The vendor vertex shader is exactly `gl_Position = in_Position;
ex_Texture = in_Texture;` (source at 0x13f8b28..0x13f8bbc), the fragment shader
only multiplies alpha, and onRenderCompleted draws the same four-vertex quad with
UVs (0,0)(1,0)(1,1)(0,1). Our scene_gles applies the same oa_fs_apply type-4
rule (y -= (1-p)/6) and only inverts the texture v to compensate for top-down CPU
rows; that does not change the motion direction. With clip +y at the top (the UI
renders upright), p=0 is 1/6 lower and settles upward. The owner still reports
the dialog moving down, so the remaining difference is in what content is
transformed or when it is presented (single-surface screen swap / async frame
coalescing), not the recovered arithmetic. Resolve with frame-level panel
evidence (camera capture or a step-hold) before changing the transform sign;
do not invert the recovered vendor rule.

DIALOG-DIRECTION-MEASURED-2026-11-01: the GPU window path was measured on real
fb1 at held endpoints with a new probe diagnostic (first non-black row):
identity top=0, type-4 p=0.5 top=45, p=1 top=0. That is exactly the recovered
rule (dy = -(1-0.5)/6 clip = 1080/24 = 45 px): the surface starts lower and
slides up, and p=0 is fully transparent because the shader multiplies alpha by
progress, so an endpoint readback at 0 shows black. The transform arithmetic and
the GPU orientation are therefore correct; the owner's panel report of a
downward dialog is not the transform. Remaining suspects: the single-surface
screen swap at completion, or the owner observing the close/instant switch.
Resolve with a UI step-hold that pauses the dialog content at p=0.5 and p=1 for
direct viewing; do not invert the rule.

DIALOG-MOTION-FINAL-2026-11-01: firmware settles the question. The only caller of
AnimationContainer::addAnimation is AnimationHandler::handleAnimationElement
(0x00ef8fb8, call at 0x00ef90f2), and DialogLauncher/MainMenu resources contain
zero animation elements (named-animations.tsv has no rows for 129/136/139). There
is therefore no separate dialog named-rect motion to recover: the entire
transition is the full-screen type-4 rule (dy = -(1-p)/6 clip = 45 px at p=0.5
for 1080), the recovered 1000 ms controller, the cubic min(1,1+(p-1)^3) easing
and the alpha = progress fade. Our implementation applies exactly that to the
dialog content, and the real-fb1 measurement (identity=0, p0.5=45, p1=0) matches.
Do not add a guessed dialog offset or invert the rule; the residual panel
appearance is a presentation/hold question, not firmware timing.

SELECT-PATH-2026-11-01: firmware shows the launcher has no Select transition.
MainLauncherMenuDialog::onSelectPress (0xe48405) only calls
focusLauncher(false), and focusLauncher (0xe4831c) does index/logging/menu-active
bookkeeping with no animation controller or SVGDialog start. Our previous
type-4 dialog animation on Select was an invention and caused the visible
wrong-direction artifact. mod_ui_vendor now switches instantly by default; the
type-4 path is lab-only behind ui.launch_anim=1 (ui_nav and ui_async enable it to
keep animation coverage). Host make check 227 ok.

SUBL AUNCHER-ITEM-2026-11-01: sublauncheritemFA.svg (resource 552, span
232881..233354) recovers the vendor sub-launcher item: background (-2,0,1920,1080);
gRowM/mRowM (121,251,240,140); textPos (79,374,323,39); glyph (121,253,240,138);
scalerect (121,251,240,140). Its animation records are 300 ms linear:
#scalerect scale 1,1 -> 1.35,1.35; #glyph y 0 -> -24; #glyph x 0 -> -42 (the
-fvc variant 556 has the same three records). This is the next screen's tile
geometry and focus animation. Do not implement the card focus animation yet:
the scalerect's transform anchor/origin and the DialogSubLauncher (189) item
grid positions are not decoded, and our current card row is an invented layout.
Next: decode DialogSubLauncher 189 (item positions/count) and the animation
element's transform origin, then build the sub-launcher screen against them.

SUBL AUNCHER-LAYOUT-2026-11-01: the list/grid placement math is recovered.
FullScreenGrid::getFocusedButtonPosition (0xd206d0) tail-calls
SVGGrid::getButtonPosition (0x10ca8ac), which computes:
  x = column * this[+0x1e8] + this[+0x1fc]
  y = row    * this[+0x1e4] + this[+0x1f4]
  rect = (x, y, x + this[+0x200], y + this[+0x1ec])
The SVGGrid constructor (0x10cc605) zeroes +0x1d8..+0x200, so the six layout
members are filled later (create 0x10cacb9 / setWindowRect 0x10cacf1 or the
description lookup). Next decode where those stores happen and with which named
description rects, plus the #scalerect transform origin; then the sub-launcher
screen can be built without guessing.

SUBL AUNCHER-LAYOUT-2-2026-11-01: the six SVGGrid members are not set by
create/setWindowRect directly. setWindowRect (0x10cacf0) calls 0x10cabc8, which
calls SVGGrid::renderBackground (0x10c9ed0) among three others; renderBackground
computes the layout from the grid's area and margins using a division helper
(0x1285478) and reads/writes +0x1d8..+0x200: e.g. at 0x10ca194..0x10ca1dc it
derives +0x1dc/+0x1e8 from the area span minus +0x1f4/+0x1f8 and +0x1fc/+0x200,
and +0x1ec/+0x1fc from the other axis. The item-count members +0x1e4/+0x1e0 gate
the computation. Still undecoded: which description rects supply the margins and
the exact division constants, and the #scalerect transform origin. No screen code
was written; the next decode is renderBackground's inputs.

RAIL-CADENCE-TV-2026-11-01: real GPU UI run on the set with the preserved rail
stage refreshed to the current modules, our 10 ms-poll ui_vendor and the probe's
UI/rail sampling. Completion-paced advance is now implemented: the rail tick
waits for the async present serial to complete before advancing, so no frame is
coalesced away. Result: 11 image revisions over the 2000 ms measurement window
(the earlier 660 ms window under-counted because the full-frame async path
completes ~100 ms/frame), backdrop raw BGRA 24,23,22,153 matching the expected
premultiplied vendor colour, dialog type-4 completion, reload and host
suspend/resume passed, and the 16,588,800-byte snapshot restored
(snapshot_match=1, region_match=1); vendor PIDs 1967/1785 unchanged.
A first run with the stage's older modules showed BGRA 13,14,14,255: the
predates-alpha scene_display made ui_vendor fall back to the opaque clear. Reusing a
preserved stage requires refreshing every module, not just ui_vendor. Frame order and
scanout remain unproven.

EGL-SCENE-2026-11-01 adds common/scene_gles.c and opt-in context/scene probes.
The actual TV Mali-G51 passed offscreen pixel/orientation/fade/triangle and
close/reopen checks; no native window or fbdev writes. Scene helper70384bytes/
98e60a726b5169bf77614fd504ca17f1 (maxGLIBC2.17), context helper23032bytes/
b0f82f027ad28d251a5ac4ea95ff7609. The context helper is preserved there; the
70384byte scene helper is historical/source8a535b6 and that build path was rebuilt.
Five1080p uploads/renders/readbacks take301..374ms. EGL-WINDOW subsequently
validates visible native presentation; production service integration remains open.
Final TV read retains the same boot/PID1785/access/privacy/hashes and pan0,0;
sampled16580608-byte framebuffer prefix hash unchanged (last8192 backing bytes
not sampled). Staging/helpers removed. Target Mali source MD5d1aa5c1679a9467ec51865d4f2120112
matches TV. New Ghidra project is partially analyzed; see report/tool limits.

UI-SCENE-REVIEW-2026-11-01 corrects fan-triangle UV interpolation, GL_LINEAR
sampling and progress-alpha fade/premultiplied BGRA from fresh firmware reads.
Dialog progress now requests frames; provider aliases/interruption/failed-resume
cleanup are covered. Full strict host203 ok, focused ASan/UBSan and ARM/qemu
display regressions pass. Real-fb1 tests twice drew all types and restored
16588800 bytes and original yoffset0 (snapshot_match1), without stopping stock
Aurora or running HAL init. Final probe101324bytes, MD547dcd8ca304e67f42eed48c3a3172b33,
maxGLIBC2.17, under build-tv-v3/review-scene; full production stage unchanged.
Final live reads retain boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, PID1785,
installed hashes, listeners and privacy/safety markers. Staging/helper removed.
Five-present default y-slide batch improved4499.56->399.68ms; general transforms
remain slow. This does not close smooth animation, independent scanout/startup,
cold/resume or visual frame-sequence acceptance. See the report for exact scope.

UI-ANIM-SLIDE-2026-11-01: text-slide math implemented from instructions, not
inferred. `AnimationHelper::calculateSlidingDuration` 0xef0210 is
`1600 + trunc(distance/50)*1000` and `calculateSlidingMargin` 0xef0264 returns
0 through the 800 ms pause, then `(int)(distance*frac)` with the clamp only
above — forward ends at distance, reverse at zero; a short slide (distance < speed)
has a zero span and is immediate after the pause. Constants re-read from the
binary: pause 800.0f at 0xeefc8c, divisor 0x32 in the constructor 0xeefc54,
multiplier 1000.0f at 0xef0260. Added `oa_anim_slide_duration`/
`oa_anim_slide_margin` (common/ui_anim.c) and published them through `ui.anim`
v2. Exact vectors in tests/ui_anim_review.c pass on host (make check exit 0,
195 ok), ASan/UBSan and ARM/qemu against the TV libc. No TV access, no drawing,
no deployment; installed ABI 1, recovery images and hook/keeper hashes unchanged.
The report's "returns to zero" sentence was corrected at source and in FINDINGS.
Caller wiring is now in mod_ui_vendor as well: `SVGTextBox::isTextSlidingNeeded`
@0x10c76b0 sets the distance to text_width − box_width, the framework samples at
10 ms, and `SVGTextBox::onAnimationProgress` @0x10c8e48 arms a single 2000 ms
timer (progress > 0.99, constant at 0x10c8ee8) instead of a repeating one. The
module starts a pass when the loaded title exceeds `ui.slide_box` (default off),
offsets the title blit by 29 − margin, and re-arms after the 2000 ms rest.
tests/ui_slide drives two full passes through the real DSO with a settable
ui.clock and observes the clipped source column; it also covers the repeat armed
at progress 0.99, animation disabled, suspend/resume and stop/start. This is
caller behaviour verified offline, not a live on-TV visual observation.

REVIEW-FIXES-2026-11-01 (GPT-6.1 Sol review): fixed before accepting the
milestone. The slide now uses `oa_ui_time_elapsed` (90,000,000 ms period) instead
of raw subtraction; start/repeat are gated on `ui.anim` enablement; the title is
clipped to `ui.slide_box` via a reusable crop buffer (the old code blitted the
whole label); the repeat is armed at progress > 0.99 while the sampler still
runs; the sampler/repeat are deleted on suspend/stop and the slide state is
cleared (the old code could dereference a released display or suppress the next
repeat); allocation failures are logged; `oa_anim_slide_margin` clamps the
float-to-int cast; the wobble uses the double precision π from 0xb32aa8; and
`mod_ui_vendor`/`mod_ui_anim` link `-lm` (verified DT_NEEDED) because `sinf` was
undefined. The module now calls the v2 provider for duration/margin instead of
carrying a copy. Host, ASan/UBSan and ARM/qemu rerun after these fixes.
Initial limits, subsequently fixed by PROVIDER-REBIND and UI-SCENE-REVIEW:
late ui.anim/ui.clock discovery and stale pointers after forced provider reload.
Ordinary reload while held remains blocked by the host.

LIVE-SLIDE-PROBE-2026-11-01: the slide was exercised on the TV with real timers
and no display hardware: tests/mod_slide_probe.c published a recording display
plus a CLOCK_MONOTONIC-backed ui.clock, the real mod_ui_anim/mod_ui_vendor ran from
/tmp with ui.anim_enable=1 (default-off bootstrap lever), size/SHA-256 checked
and staging removed afterwards. 914 samples at a mean 10.08 ms cadence; first
nonzero margin at +822 ms, final 163 at +3808 ms, repeat restart at +6557 ms
(= 4554 + 2000, the 0.99 threshold), clean exit. Vendor PIDs 1967/1785 unchanged
and no helper left. This is not visual scanout evidence; fb1, keys, settings and
vendor modules were not touched.

RAIL-HIGHLIGHT-2026-11-01: `MainLauncherMenuDialog::init` calls
`SVGButton::setMainlauncherHighlightId` five times (0xe4981a/0xe498ec/0xe499e8/
0xe49ac6/0xe49b98), once per category. The setter stores the resource id at
+0x96c (0x10aeee5); `SVGButton::renderForeground` (0x10af894) creates an
SVGImage from it at `RectangleContainer::getRectangle("mainlauncher_highlight")`.
Resource 579 (myMenuLauncheritemFA.svg) contains that named rect, so the rail
buttons use the category resources, not launcheritemFA/FI (536/537). Resource
579's local signed-16 rects: glyph (119,240,150,150), mainlauncher_highlight
(119,212,199,210), background (-2,0,1920,1080). The highlight resource
`mainlauncher_item_highlight_animated.png` is an 11-frame, 30 ms APNG of an
orange bar at the image's left edge growing to full height; all 11 fcTL entries
use dispose_op=0 and blend_op=0 (full-canvas SOURCE replacement), so no frame
compositing is required. The APNG num_plays
field is metadata: PngImageDecoderImpl::decodeNextFrame (0xb30654) increments the
frame index and only decodes while index < count, with no rearm at the end, so
the last frame is held. The 0.3 s 536/537 records are launcher tiles, not the
rail.

Placement recovered: init creates five SVGButtons named HOME/VIVA/SETTINGS/
APPLICATIONS/SOURCES; updateCategoryButtonRects looks up description names
`button1`..`buttonN`; setMenuItemPositions walks the rect vector cyclically with
the type array at +0x39b4; setMenuItemPosition calls SVGButton::setWindowRect
(vtable slot 0x30 = 0x10ade19). DialogMainMenu.svg (136) defines button windows
(6,211/330/460/600/719,160,160). The capture shows the focused 150x150 tile at
(11,450) and the bar at x 6..10, y 458..591 (mid-growth), so the 210x210 image
is drawn at (focused_x-5, focused_y+10). Implemented in mod_ui_vendor: rail_hl01..11
OAI1 frames from the vendor APNG, a 10 ms decoder poll that advances a frame once
now-last >= the 30 ms fcTL delay (numerator used as ms; last stamped at decode)
while the portal shows (the last frame is held, no wrap), playhead reset on
focus change, scaled into the button window. tests/ui_rail_hl asserts frame
order, the poll/delay gate, the one-shot stop and the window; host make check
227 ok, focused ASan/UBSan pass.
Live probe on the set (fake display, no fb1/keys): 166 blits, mean gap 29.98 ms,
window x=6 y=461, clean exit; staging removed, vendor PIDs unchanged. Visible
panel appearance is still unverified.

PROVIDER-REBIND-2026-11-01: mod_ui_vendor now reacts to `module/changed` and
`service/removed`. Events only schedule a 0 ms rebind timer (event dispatch runs
with the subscriber as cur_module, so holds and timers belong to ui_vendor);
`service/removed` nulls the cached display/ui.anim/ui.clock pointers before they
dangle. The rebind reacquires missing services, restarts or holds the slide as
enablement allows, and ignores ui_vendor's own lifecycle events. Suspend defers
rebinding: events mark it pending and resume performs it, so no display hold is
taken while suspended. tests/ui_rebind covers late ui.clock/ui.anim publication,
the disabled gate, forced unload of ui_anim, and provider return. Host make
check 197 ok and ASan/UBSan 197 ok; the ARM/qemu rebind run passes; TV rerun
pending.

FULLSCREEN-ANIM-2026-11-01: the full-screen progress path is recovered.
`SVGDialog::startAnimation` @0x10c5009 returns for mode 0x38 and otherwise maps
modes 0x39..0x40 through the byte table at 0x153b80c (01..08) to HAL types 1..8,
starting a 1000 ms controller (0x447a0000 at 0x10c5094) when the HAL supports the
animation, an app exists and `UIApp::modalTargetsDepth() == 0`.
`SVGDialog::onAnimationProgress` @0x10c4dac feeds `min(1,1+(p-1)^3)` to
`UIFramework::setFullScreenAnimationProgress`, and `SVGDialog::destroy`
@0x10c4e10 forces 1.0 when the target is still below 1. The HAL
(`hal::OpenGLGraphicsContext`, type +0x78, progress +0x74, dirty +0xa4) clamps
and rewrites vertex positions in `iterateFullScreenAnimation` @0xb32910 with the
π/2-3/1-3/1-6 pool at 0xb32aa8. Correction 2026-11-01 from the instructions:
the surface is one 4-vertex clip-space quad built in `onRenderCompleted`
@0xb33075 (96 bytes, `GL_TRIANGLE_FAN`, positions (-1,-1)..(1,1), UV 0..1);
types 2/3 add to the x field and types 4/5 to the y field at 0xb329b6 (the
pointer+4 is bytes with a 0x18 stride), not a second vertex group. Type 1 scales
only the two right vertices, a keystone. Encoded as `oa_fs_apply` and the
scene-level `oa_fs_scene_transform` (clip-space affine, keystone flagged) with
cross-check vectors. The display now publishes v2 with
`set_scene_transform(type, progress)`: all draws go to an offscreen RGBA scene and
present() maps it through the recovered quad (corrected to triangle interpolation,
linear sampling and alpha fade by UI-SCENE-REVIEW, BGRA output);
fake-fb tests cover x-slide, scale, keystone and clearing. mod_ui_vendor is the
consumer: on dialog progress it pushes the selected type with the eased value and
clears on completion/suspend/stop. `SVGWindow::getWidgetStandardName` (0xeee798)
names the modes: 57 flip, 58 slide-right, 59 slide-left, 60 slide-up, 61
slide-down, 62 fade, 63 grow, 64 bounce; `SVGDialog::create` tries 57..64 in
order and starts the first widget present. DialogLauncher.svg (129) declares
only animation_slide_up_fade_in, so mod_ui_vendor selects type 4 by that rule;
ui.fs_type remains an explicit override. Evidence: host 200 ok, ASan/UBSan 200
ok; check-ui-nav asserts the type-4 selection. Subsequent UI-SCENE-REVIEW supplies
ARM/qemu and existing-layer real-fb1 execution. Smooth/visual independent
acceptance remains open.

RAIL-TIMING-2026-11-01: MainLauncherMenuDialog moves rail items synchronously
(setMenuItemPositions from scrollItems/updateButtonPositions/init/onExit/
preHandleKeyPressMessage; no controller call). The focus animation is per
SVGButton: drawButton starts the controller only when the window's animation is
enabled and getAnimationDuration() > 1.0 ms; getAnimationDuration
(atof(record+28) x 1000.0 double at 0x10af1f0, max over records, default 0)
supplies it. Generic launcher tiles launcheritemFA/FI (536/537) are 0.3 s
linear scale 1->1.32 with glyph x->-34, y->-37; service buttons use 1.35.
Category glyph resources 579..598 have no animation records, and whether the
rail buttons are built from 536/537 is unproven — do not copy the 300 ms onto
the rail. mainlauncher_item_highlight_animated.png is an 11-frame APNG at
30 ms/frame (330 ms); the sublauncher/launcher highlight files hold one frame.

SPEAKER-GAIN-2026-11-01 is the latest review: direct SetOPUGain operands are
0..0x8000 and bypass UI AQ/balance/ratio processing. GetOPUGain's cached100 to
0x8000 mapping makes it unsuitable for exact rollback. Lower codec/client
dispatch passes three wire and six session groups host/sanitizers/ARM-qemu/TV,
using injected I/O only; no actual gain/table/mute write occurred. Native units
37668bytes/a90d0a2c4635af1210a9ecb1467d9bf2 and
67756bytes/bcd8ad1eff1f78aaa0dbb97543e7dc2c are preserved in
speaker-wire-tested-tv.tar under analysis/cec-ownership-2026-11-01. Staging
removed; boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, live Aurora1785,
installed hashes, listeners and privacy protections unchanged. Broker/service
remain query-only. Only ARM unit tests were rebuilt in build-tv-v3/tests;
the production16-module set below retains its previously tested identities.

MIXER-MODULE-2026-11-01: mod_audio_mixer publishes
audio.mixer v1 and the core exposes mixer-query/result/cancel. Nonblocking IPC,
owned watch/reap timer, WNOHANG teardown, cancel/drain, held-provider protection,
resume failure cleanup and stale-ticket rejection are implemented. Five DSO
groups pass host/sanitizer/ARM-qemu/TV. Real TV socket query/reload/query returned
device0 L/R32768 twice; standalone core config/start/status/quit also passed.
No HAL init, setters, drawing or stock-UI restart. Both stages were removed;
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, Aurora1785, installed hashes,
listeners, safety markers and Samba REJECT remain. Rooted installed core ABI1
MD5a898384df076572cf377749f25427689 stays unchanged.

Current matching build-tv-v3: core08f549d4026cfb626c1ffc93b2e02fc6, all16
modules and helper/supervisor. All16 modules passed real-TV --check from a
verified temporary stage, removed afterward. stage.md5 identifies this complete
set. Previous ABI3 corefc9.../13-module set (including old test fixtures) is
archived at artifacts/archive/build-tv-v3-before-mixer-20261101.tar; SHA256 and
all old stage.md5 entries verified. ABI1/ABI2 and recovery files preserved.
Current exact tested mixer stage/tar and matching manifest are in analysis.
Target libc spawn's return0 followed by exec-failure exit127 is firmware behavior,
not a qemu defect; the adapter handles both immediate and asynchronous failure.

AUDIO-BROKER-2026-11-01: oa-audio-broker owns its
native PID-bound query socket in a separate executable, with strict32-byte local
seqpacket IPC and no HAL initialization/setters. Real posix_spawn execution on
TV returned device0 L/R32768, then closed/reaped cleanly; unrelated inherited
fds were absent. Parent85640bytes MD5bf227ea86b93b3a8c0c0055250fe0348;
helper62176bytes MD52e2856c667df2a40d2cbb38589a6753d, preserved in analysis.
Full host suite, focused sanitizer and ARM/qemu fixtures pass. Helper needs at
most GLIBC2.4; parent2.15. Fresh read-only firmware instructions confirm the
plat prelude's mixer initializer calls the PID-binding initializer. Parent
loop/module/CLI integration was subsequently added in MIXER-MODULE. No gain/table/profile/mute write.
Staging /tmp/oa-audio-broker-20261101 removed; boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197,
Aurora1785, installed core/hook/hosts hashes, listeners and Samba block unchanged.

CEC-ASYNC-2026-11-01: the lower CEC owner now has
nonblocking ticketed acquire/send/receive/release and copied take() results.
Four injected-I/O groups pass host/sanitizer/ARM-qemu/TV, including blocked open/
receive, copied path/tables/frames and failed-close generations retaining fd.
Test86172bytes/MD5d2ca064a491065637b78f9349c56e2b0 is preserved in analysis;
/tmp/oa-cec-async-20261101 removed. Same boot/PID1785, installed hashes,
listeners and Samba block remain. No live device, STBC or bus operation occurred.
Full host suite passes. Service adapters/native ownership/cancellation remain
open; both lower owners now support nonblocking submissions and value results.

AUDIO-ASYNC-2026-11-01: the lower audio owner accepts
nonblocking ticketed open/gain/close and returns copied results with take().
Three injected-I/O groups pass host, ASan/UBSan, ARM/qemu and TV. Full host suite
passes; no real socket, amp or CEC access occurs in those tests. Verified native
test75264bytes/MD571a53754d8c48fe97cfb65580c0f8d2c is preserved in analysis;
/tmp/oa-audio-async-20261101 removed. Boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197
and Aurora1785 were unchanged, as were core/hook/hosts hashes, listeners and
Samba protection. The module bind service still rejects all arguments: native
ownership and async service/lifetime integration remain open. CEC's synchronous
receive now treats positive1 as success instead of propagating stale worker errno.

New work uses ABI 3 in build-tv-v3, not the installed ABI 1/recovery build.
Core and all modules must move together at a future authorized deployment.
Historical build-tv-v2 remains preserved. ABI 3 adds a nonblocking quiesce gate
that can refuse destructive cleanup, including force and partial-start failures.
Six device-free DSO groups passed host, ASan/UBSan, ARM/qemu and actual TV.
The earlier core 8b06fd51c9ff662f13815ef6406dc184 plus 13 modules passed
--check on the TV, exit 0; temporary /tmp/oa-quiesce-20261101 was removed.
The archived previous build-tv-v3 core fc9eb71f924dd850c7ba4f1c9b8a02cd, max GLIBC2.17,
adds shutdown retry and was not checked on the TV. Its archived stage.md5 identifies
that historical set; modules were not rebuilt for that core change.
No production module was started in that earlier preflight. Stock boot/PID, installed core/hook/hosts
hashes, listeners and Samba block were reconfirmed unchanged. Host shutdown
now retries a quiescence veto offline (SHUTDOWN-2026-11-01). Offline CEC and
audio workers pass host, ASan/UBSan, ARM/qemu and the full host suite
(OWNER-WORKER-2026-11-01). mod_cec can suspend and resume its idle worker, and
mod_audio creates the mixer owner without binding a socket. Enabled HAL suspend
still fails closed. They were not in that previous13-module stage, not deployed,
and do not open live hardware. This core is not deployed.

Subsequent WORK-REVIEW-2026-11-01 fixes are ahead of that preserved stage set.
That review's source/host build had15 modules, including ui_anim and cec; the
that milestone's matching16-module build added audio_mixer (source is now21;
see UI-THEME-PROVIDER above). Review
fixes worker state ownership, join failure retention, fully nonblocking quiesce
and pending reload retry. The loop-thread bind_session service rejects every
argument with ENOTSUP until an async interface exists. Animation timer ownership,
resume, reentrant completion and extreme-coordinate conversion were fixed.
Full host make check and focused sanitizer/ARM-qemu checks pass. Review build
directories are build-review-san and build-review-arm. No review binary was
uploaded to the TV; no production module/hardware acceptance is implied.

| implemented milestone | evidence / limit |
|---|---|
| owned worker posts, descriptor-size checks, registry fixes | host, sanitizer and ARM/qemu tests; MODULE-SAFETY report |
| state-aware reload / restore failure handling | nine host/sanitizer/ARM-qemu regression groups; suspended activation deferred; no binary rollback |
| dependency-ordered lifecycle | nine host/sanitizer/ARM-qemu groups; service snapshots survive reload; CLI propagates failures; native device ownership remains open |
| sleep-cycle engine / power service+CLI | nine engine plus seven wrapped-power host/sanitizer/ARM-qemu groups; final entry/restoration and pinned notifications implemented; hardware acceptance remains open |
| bounded control replies / Unix transport | status heap overflow fixed; full-table/tiny-buffer checks plus five backpressure/partial-write/disconnect/input/cleanup groups pass host/sanitizer/ARM-qemu; stdout console remains synchronous |
| --check loader/ABI preflight | staged core plus 11 modules passed on TV, exit 0; staging removed; RUNTIME-CHECK report |
| present always commits FBIOPAN_DISPLAY | fake-device host/sanitizer/ARM tests; not deployed or tested drawing |
| maintained six-/four-word PWM declarations | decompile + return instructions; brightness still unverified independently |
| O_RDONLY fbregion diagnostic | four GET requests tested; staged fb1 query removed after use |
| oa-supervise/heartbeat/fallback | host and ARM fake-hardware smoke tests; real native takeover unverified |
| standby firmware recheck | outer/idtv/wake-network stages traced; monitor helper is task state and UART send helpers are semaphores; POWER-STANDBY-CONTRACT; no new hardware behavior |
| ui_vendor assets/rendering | historical framebuffer comparison; not full navigation or independent display parity |
| animation firmware/extractor | UI-ANIMATIONS: 39 named descriptors; all 702 resource IDs/names CRC-verified; list/dialog controller is mod_ui_anim offline; text slide wired in mod_ui_vendor; full-screen type/progress mapping encoded in oa_fs_apply; rail timing and renderer composite remain open |
| ui_clock / ticker paired hooks | FROST_Time arithmetic and reloadable UI time implemented; seven host/sanitizer/ARM-qemu DSO groups; absolute vendor clock phase unverified; ui.anim samples this clock |
| CEC/audio foundations | spec-based frame/status/ARC and identity/name/power validators; audio scalar ABI, prelude ownership, failure/cache/handoff fixes; six wire plus five fake-HAL groups pass host/sanitizer/ARM-qemu; transport/policy integration open |
| CEC packet/transport RE | raw/VPL ABI and 16-byte storage tested in four host/sanitizer/ARM-qemu groups; VPL status/filter/close/reopen hazards recovered; live open remains open |
| native raw CEC backend | PreInit/real cdev registration recovered; explicit blocking open/send/RX/close, failed-close retention and fd-zero/reopen logic pass three injected-I/O host/sanitizer/ARM-qemu groups; hardware acceptance open |
| offline CEC/audio owners | injected authority; mod_cec idle suspend/resume and mod_audio owner pass host, ASan/UBSan and ARM/qemu twice plus the full host suite; hardware_open_supported is 0; enabled HAL suspend returns failure; no live open; OWNER-WORKER |
| System Audio/ARC route | cec.audio v1 on mod_cec; injected 1.3a System Audio and 1.4 ARC; local volume/mute refused while the mode is on; 8-byte reload snapshot; host make check plus one ASan/UBSan and one ARM/qemu run; no live bus or amp write; OWNER-WORKER |
| portal navigation | mod_ir input.feed, mod_ui_vendor hit/open/home/back/source and mod_ui_anim list/dialog; 4-byte reload snapshot; host make check plus one ASan/UBSan and one ARM/qemu run; no ttyS2 or fbdev; live UART code byte unconfirmed; UI-PARITY |

The previously TV-tested preflight core MD5 was 424ef6ea213cb706acb51a7adb39f778.
The preserved historical offline ABI 2 build in build-tv-v2 has 13 modules, including ui_clock
and audio_curve. Core MD5 54a626f4c3a00b2e90490bc2626fe055;
ui_clock d9fa142605ef2a8fa75fa7f0fb1db7db;
ticker 9f1fdadf6f9ca721ed0a97f2d5da98eb. Core max GLIBC2.17.
Latest audio 4bb669961f0379575d98a5e2fcbb01eb and plat
b66b3a1eeff42ef77be782ad06674249 include the prior contract fixes and were
rebuilt after the service header addition. audio_curve a644c66e92bd6d53ef1032a990f8c4cc.
That ABI 2 full13-module set has build/host validation, not an on-TV preflight or
deployment; the separate curve/ticker real-TV runtime test is described below. Older
installed audio/panel/source have standalone unresolved V4L2 imports; the rebuilt
matching closure passes. --check proves loadability, not initialization/hardware.

Last fb1 read: MAU0 primary, PA0x35bbb000, size16588800, stride7680;
1920x1080 virtual1920x2160, offset0,1080; region
{0,0,1920,1080,0,0,3840,2160}. This does not diagnose previous invisible output.

Offline follow-up: both target NAND kernel slots confirm the GEM +0x70 size
store; fresh decompression matches reconstructed ELF segments. OSD binds that
allocation size separately from advertised dimensions. Update payload byte4
commits, byte5 requests gated vsync; 0x63 enables the region, not alpha. New raw
instruction logs and details are in OSD-BUFFER-CONTRACT. No TV access occurred.

Alpha/PM follow-up (OSD-KERNEL-INIT): normal-plane blend-enable callback does
no register write; SetPar global alpha is separate. Driver PM callbacks restore
cached registers/palettes, and PanDisplay can recover first-pan show state.
Existing fbdev is the first independent-validation candidate; cold/resume native
acceptance remains open. No new device call, deployment or TV access occurred.

## Settled decisions and next work

Owner focus is CEC/audio. Latest follow-up is CEC-OWNERSHIP-2026-11-01: PreInit
emulation and real registration are recovered, and a blocking raw backend now
exists/tested offline. Native authority/worker/reload integration and actual node/
hardware acceptance remain open. New PreInit project is durable in workbench.
November 1 follow-up: [CEC-LIVE](artifacts/reports/CEC-LIVE-2026-11-01.md).
The existing NDAL fd 83 maps to /dev/video28 (ndp-hdmi-switch); shared-owner
logical-address query returned TV address 0. Native ntkcec major 229 is registered
but has no node; a temporary node's open failed with EPERM while stock owns it.
No raw init/close, RX dequeue, authority write or takeover was attempted.
Vendor cache identifies Steam Deck at LA8 / PA1000, power On. Three standard
queries 83/46/8f to LA8 returned ioctl success AND driver acceptance. Cache
mirrored name/address/power into an unseen LA4 slot; this is not proof of a second
client or fresh replies to each query. No response payloads were captured.
Temporary files/node and probe processes are gone. Same boot/UI, Ethernet/MAC,
SSH/telnet, hosts/Samba blocks and hook/keeper hashes verified afterward.
No persistent deployment, reboot, audio write or recovery change occurred.
Audio follow-up: [AUDIO-ROUTING](artifacts/reports/AUDIO-ROUTING-2026-11-01.md).
Verified live amp descriptor has one instance at 033341c0, ESMT AD82587_D (id1),
with callbacks matching AMP_AD82587_Create. Earlier mod_audio AD82586C comment
was wrong and is corrected. AOUT output 0 owns present speakers 0,1,2,3,5;
outputs 1/2 have no handle/list. Decoder main volume, speaker EasySet gain and
amp register volume are separate layers. A new offline audio-state diagnostic
decodes the saved 612-byte cache; strict host/sanitizer/ARM outputs match.
These /proc reads preserved the same live UI/boot and made no device/audio write.
Native amp GPIO/I2C/scale/lifecycle and audible acceptance remain open.
Subsequent [AUDIO-BOARD](artifacts/reports/AUDIO-BOARD-2026-11-01.md) recovers
reset GPIO114, MCU mute GPIO208/STBC8, I2C bus3/address34 and actual register-read
ABI. A temporary ARM amp-probe read registers03..05=1b/05/05, ioctl count2/exit0;
it did not initialize/reset the chip or change gain/mute. Staging/probe removed;
same stock PID/boot, network/access and privacy/hook/keeper identities verified.
An incorrect fd-inspection cat blocked on vendor stdin; its surviving PID9821
was explicitly terminated and absence verified. Use readlink for fd targets.
Native lifecycle, volume conversion and audible acceptance remain open.
Latest [AUDIO-VOLUME](artifacts/reports/AUDIO-VOLUME-2026-11-01.md) traces UI
volume via speaker gain and101-entry AQ tables. New common/audio_curve.c decodes
normal/aggressive tables with verified low16-bit narrowing. All101 normal entries
match a202-byte live vendor table. Strict host/sanitizer/ARM-qemu and the same
test executed on the actual TV pass. Staging removed; same boot/UI, Ethernet,
SSH/telnet, hosts/Samba and hook/keeper hashes verified afterward. No gain/mute
write, module deployment or recovery change. The helper is not yet integrated
with mod_audio/HAL; routing, profile/default policy and native audio lifecycle
remain open. SetMainVolume's named wrapper only range-checks, with no device write.
Latest [AUDIO-CURVE-MODULE](artifacts/reports/AUDIO-CURVE-MODULE-2026-11-01.md):
audio_curve now publishes audio.curve v1 with caller-owned table copies and
bounded lookup. AQ path/profile are explicit; no guessed fallback. Six real-DSO
lifecycle/CLI/hold/collision/file/FIFO/suspended-reload groups pass host,
sanitizer, ARM/qemu and on the TV. Full host suite passes. Three temporary staged
test/module files and fixtures removed; stock boot/UI/access/privacy/hook/keeper
unchanged. This data provider is not yet connected to mod_audio gain application.
Latest [AUDIO-GAIN-TRANSPORT](artifacts/reports/AUDIO-GAIN-TRANSPORT-2026-11-01.md)
finds that message-backed Hal_Audio_Ctrl0 can hide sendmsg failure, while the
speaker ALSA setter updates a request cache and ignores lower failure results.
Getter returns that cache; no audible acceptance follows. A new read-only
mixer-probe passed on TV with ALSA_CONFIG_PATH=/vendor/etc/alsa.conf, count1,
range0..100, value0. Default fresh-SSH configuration first failed; vendor/run.sh
supplies this variable for the stock runtime. Helper removed and boot/UI/access/
privacy/hook/keeper state verified unchanged. New durable Alsa project and
separate manifest retain receiver/cache evidence. Native gain integration still
needs checked dispatch/completion/lifetime, not a blind HAL table call.
Latest [AUDIO-NETLINK](artifacts/reports/AUDIO-NETLINK-2026-11-01.md) resolves
timing: the target kernel invokes the callback synchronously; table data is
copied before the syscall returns. Operation errors are still discarded.
New bounded codec passes host/sanitizer/ARM-qemu. A fresh native query returned
device0/left32768/right32768, exit0, without gain/table writes or HAL init. Probe
and staging removed; same boot/UI, network/access/privacy/hook/keeper verified.
Process/socket ownership and owned blocking execution remain to integrate. Codec
is not in production audio yet. Neither reply nor ALSA cache proves DSP/audio.
Follow-up: common/audio_client.c now owns a process-bound blocking session with
explicit fd lifecycle, validated queries and checked table/selection sends
(dispatched only). Five injected-I/O/fork groups pass host/sanitizer/ARM-qemu;
full host suite passes. Refactored query probe (44556 bytes, MD5
bfef5a6123cbece7d17dfee92440f5e3) passed on TV with the same raw gain values.
Staging removed; same boot/UI/access/protections verified. No real table/selection
write was tested. An owning worker now serializes that session off the host
loop and rejects NULL I/O with ENOTSUP. It is not wired into mod_audio.
Production artifacts were not rebuilt/deployed. OWNER-WORKER, AUDIO-NETLINK.
CEC-TRANSPORT-CONTRACT records the preceding transport evidence:
normal traffic uses private V4L2/kernel CEC, not UART. VPL close/reopen is unsafe
for reload; driver filtering alone rejects some spec-valid messages, while stock
layers lose/ignore that result. Packet marshalling is implemented/tested offline;
offline authority and an owning worker now cover injected I/O and an idle
module. Live open and hardware acceptance remain open. OWNER-WORKER.
CEC-AUDIO-CONTRACT records ARC/System Audio requirements and audio ABI fixes.
Required /tmp/opencode material has moved to artifacts/workbench (4.2 GB at
migration), including firmware, Ghidra projects, toolchains and specification
copies. Active paths are durable; temporary compatibility links are not required.
See workbench/README and SHA256SUMS. New PreInit/HdmiSwitch projects have separate
manifests in analysis/cec-ownership; the historical migration manifest is retained.
Installed files and recovery are unchanged.

Latest power follow-up is POWER-STANDBY-CONTRACT. Target Ethernet private ioctl
opcode 1 sets a next-suspend policy (0 keeps PHY power, 1 powers down), not an
immediate link transition. Driver file from dump and imported copy match MD5
2efe1ee138ee29c2d490f6e4a667a38d. The new internal host cycle is tested with fake
entry. mod_power now routes standby through the host.sleep service and a final
RAM-entry callback; ordinary hooks do not sleep. CLI standby and power/state
ordering pass seven wrapped-I/O groups. Other modules need explicit quiescence
hooks and device/wake ownership. No TV execution/deployment; recovery unchanged.

APPS-GRID-2026-11-01: the full-screen apps portal is DialogFullScreenPortal.svg
(114): background/main_rectangle, top strip (0,0,1921,162) #481730, appsGrid
(183,180,1554,720), scrollbar (1750,180,8,720), dialogTitle (7,39,546,90),
customerLogo (1587,32,300,120), paging (893,984,223,60), portalMode
(1474,984,250,60). FullScreenPortalDialog::init gets the appsGrid description and
calls FullScreenGrid::create(id 0, rect, items, dialog); create passes 5 as the
column count (SVGGrid +0x1e0) and offsets the rect by the parent client origin.
FullScreenGrid uses FullScreenGridFA.svg (375 focused, 376 unfocused): mRowL
(156,122,0,230), mRowR (576,125,0,229), scrollPos (591,127,6,229), mRowM/rowPos
(225,121,320,230). gTopL/mTopL and gBotL/mBotL are zero-size path/polyline
elements (tags 7/6), not rectangles, so mTopL/mBotL yield no rectangle and the
top/bottom margins are 0. The button SVG is FullScreenGridButtonFA/UFA
(373/374): mRowL (149,0,36,180), mRowM (187,0,320,180), mRowR (509,0,36,180),
bottom strip (69,180,545,66); text is FullScreenGridTextboxFA/UFA (378/379).
SVGGrid::calculateHeight (0x10c9d00) zeroes +0x1f4/+0x1f8/+0x1e4, then sets top
= mTopL height, bottom = mBotL height, row step = mRowL height (rect fields
+8/+0xc/+0x10/+0x14 are x/y/x1/y1). SVGGrid::create (0x10cacb8) stores the
column count in +0x1e0. renderBackground (0x10c9ed0) computes visible rows =
(h-top-bottom)/row step and column step = (w-left-right)/cols with __aeabi_idiv
(0x1285478, truncated); item height = rowPos height; getButtonPosition
(0x10ca8ac) gives x = col*step + left, y = row*rowstep + top,
x2 = x+left+step+right, y2 = y+item_h. The per-cell background loop rewrites
left/right from rowPos and the mask rectangles, so after a paint they hold the
last cell's values; createListButtons (0x10ca924) then calls getButtonPosition
for every cell. A fresh grid has zeroes (col*310, width 310); which state the
vendor uses at createListButtons time is not established. Do not render the
grid with an assumed inset. Still undecoded: the HorizontalMaskScaleTransform
width evaluation and the widget-standard-name id->name map (0xeee798,
conditional).
common/vendor_grid.c encodes only the verified arithmetic and tests/vendor_grid.c
asserts the portal and gridFA numbers (make check-vgrid); no screen was rendered.

APPS-GRID-2-2026-11-01: decompiled with the new durable
artifacts/workbench/ghidra_scripts/DecompAt.java. SVGGrid::drawGrid (0x10cabc8)
is renderBackground -> createListButtons (0x10ca924) -> createScrollBar ->
update. createListButtons loops +0x1dc rows x +0x1e0 cols, calls
getButtonPosition and then makeButton/addButtonItem; getTotalNumberOfRows is
ceil(items/+0x1e0). SVGGridButton::renderBackground (0x10ccad0) resizes its
surface to the button window and renders the button SVG as a 3-slice
(left/right masks stretched vertically, mRowM stretched horizontally), so the
button window is exactly the getButtonPosition rect. Vendor assets for these
widgets are in artifacts/ui-assets: gridButton_fa.png 236x354, myapps_animated
320x180 (matches mRowM 320x180), app art 240x135/320x180. The runtime
contradiction stands: renderBackground's per-cell writes leave +0x1fc =
rowPos.x - cell.x and +0x200 = cell.x1 - rowPos.x1 (last cell: -1015/+1005 for
the portal), while a fresh grid gives the expected col*310/310. drawGrid calls
createListButtons after renderBackground, so static order does not explain the
vendor result; the RectangleContainer may return renderer-scaled rowPos. The
grid must not be rendered with an assumed inset; resolve with the vendor object
state (not while the set is in use) or the on-screen portal. commonCreateStep
(0x10cac10) sets the column count before drawGrid and SVGGridButton::create
(0x10ccdcc) uses the rect unchanged, so the last-cell fields really would put
columns 0..3 at negative x; nothing resets them. Working hypothesis: the
full-screen portal is broken or unused on this model (DialogNavigator only
reaches it through a portal navigation message), so do not port it as parity
without observing the vendor screen. SubLauncherDialog is instead a
SettingsDialog (SVG 189) created by LauncherDialog::showSubLauncher;
SettingsDialog is an AnyUIRefreshMessageFullScreenDialog (widget 175) with no
constructor window offset, so the p0/03 settings capture is not the plain
DialogSubLauncher listBox. The tested layout
now also exposes renderBackground's cell rect and the DialogFullScreenPortal
chrome plan (oa_vgrid_cell, oa_portal_plan_init).

TV-USE-2026-11-01: the owner reported the set showing HDMI1 after the idle test
window; return to view-only until a new idle window. Before that report one
DIAL KeyMenu (1048) was sent while both fb layers captured BLANK; it returned
201 and no layer change was captured. No further keys or wake attempts were
made. Vendor PIDs 1785/1967 were running.

SOURCE-SELECT-2026-11-01: the vendor source list ("Bronlijst") is a
PopUpSelectionDialog created by DialogNavigator::doNavigateSourceMenuDialog
(0xd17b10) with SVG 185, DialogSourceSelection.svg: background 1920x1080,
main_rectangle (0,0,780,1080), listBox (18,136,750,933), dialogTitle
(54,47,695,60). The p0/06 screenshot matches this panel/title geometry; its
focused first row is at ~(20,147) with ~63 px rows at ~70 px pitch, and the
rows list TV/BACK AV/HDMI1..3/YPbPr/VGA-PC/Wireless Display/Audio video delen.
The row template and exact row height/pitch are not in the dialog resource;
they belong to the popup list row and remain undecoded. This is a real, used
screen (unlike the likely-dead full-screen portal) and is the better next UI
target once the TV is free. No implementation yet.

SOURCE-SELECT-IMPL-2026-11-01: mod_ui_vendor renders DialogSourceSelection.svg 185
as a real screen. Rail Select on the source icon opens the 780x1080 panel
(#2D1EBE), the 748x66 focus bar (#1994FE) at 18,152 with a 75 px pitch, and the
vendor's own text extracted by tools/extract-source-selection.py
(src_title/src_0..src_8, ten new OAI1 blobs). Up/Down move the bar, Exit
returns; focused-row text scales between the capture's 34 px and 28 px ink
sizes. tests/ui_sources.c checks the state and fills; make check 232 ok. Text is
the Dutch capture; localization and exact row-template focus metrics remain
open. On-TV run pending (owner allows fb1).

UI-ENGINE-PLAN-2026-11-01: owner direction: the whole UI engine must be its own
module so ui.omarchy can replace the skin after parity, with ui.vendor as the
known working fallback. Design in docs/ARCHITECTURE.md: ui.engine owns display/
assets/present/ack/input/screen dispatch and publishes ui.engine v1; ui.vendor
registers the current screens and keeps OA_SVC_PORTAL. Not implemented yet;
TODO has the acceptance gates.

CCACHE-2026-11-01: ccache 4.14.1 installed. The Makefile wraps the default host
CC and the arm/tv cross compilers automatically; an explicit CC= is untouched
and CCACHE_DISABLE=1 disables. Most module/test rules compile+link in one
-shared call and are uncacheable; splitting them is a TODO. Build-only runs can
use make -j$(nproc) all; check stays sequential.

SOURCE-SELECT-TV-2026-11-01: the source-selection screen ran on the real set
under the owner's fb1 permission while HDMI1 stayed on the video plane. Native
ARM host PID17592 staged in /tmp/oa-src-stage (mod_ui_vendor md5
3c1b1bd562be3fdd79dd9a8946fc6790), a test plat stub (mod_display requires the
service; the real plat would re-run the vendor HAL), and mod_display with
set_region/mirror/pan 0. The legacy p0/06 capture is R/B swapped: its panel
(45,30,190) and bar (25,148,254) are the resource colours #BE1E2D and #FE9419
with R/B exchanged. The screen now uses the resource palette (panel #BE1E2D,
bar #FE9419, focused ink #7E000A, white unfocused rows). tvshot fb1 matched the
swap-corrected p0/06 panel (0,0,780,1080) with RMSE 181.535/65535 (0.28%) and
AE 61.7/842400 (0.0073%); flat colours exact. fb1 was backed up (16,588,800
bytes, yoffset1080) and restored (bytes 16588800, offset 0,1080); staging
removed; no openaurora left. src_0..src_8 and src_*_f are the vendor ink.
Evidence: artifacts/analysis/source-selection-2026-11-01/. The live RGBA-asset
-> BGRA-fb1 -> tvshot-BGRA path is consistent; the error was importing a legacy
swapped capture, not the render path.

UI-ENGINE-SPLIT-2026-11-01: mod_ui_engine publishes ui.engine v1 and now owns
display/alpha discovery and rebinding, lazy OAI1 assets (bounded cache), draw
ops, present/async receipts and the readiness receipt. It emits ui/ready when it
binds or rebinds a display; subscriber callbacks run in their own owner context,
so skin timers stay skin-owned. mod_ui_vendor is the vendor skin: it holds the
engine service (not display), keeps the portal/source screens, devctl and
OA_SVC_PORTAL, calls reset_ready on start and repaints on ui/ready. Every UI
test and tests/recovery.py load ui_engine before ui_vendor; full make check 232 ok.
The vendor skin is the known working fallback. Still pending: move key/screen
dispatch into the engine, rename the DSO/module to ui_vendor, add a
config/control-plane skin selection and write ui_omarchy against ui.engine.
The split also ran on fb1 with plat,display,ui_engine,ui_vendor (PID21135,
mod_ui_engine md5 d831bd64..., mod_ui_vendor 8056eb04...) and produced a
byte-identical capture to the pre-split screen (sha256 8f34f218...); fb1 was
restored (16588800 bytes, offset 0,1080), staging removed, no helper left.
Evidence in artifacts/analysis/source-selection-2026-11-01/.

UI-SKIN-SEAM-2026-11-01: ui.engine now subscribes OA_TOPIC_KEY and owns the
ui.fps repaint timer; skin enter/paint/key callbacks are posted to the skin's
owner token, so actions and timers stay skin-owned. register_skin/unregister_skin
plus ui.skin select the active skin. mod_ui_fb was renamed to ui_vendor (dir,
file, descriptor, tests, boot config, scene-module-client). ui_omarchy is a
skeleton skin (dark shell, accent rail, placeholder tiles, its own key handler)
with module ui_omarchy v0.1. tests/ui_skin switches vendor->omarchy->vendor by
unload/load with no rebuild; full make check 233 ok. The renamed ui_vendor also
ran on fb1 (PID24069, md5 283b5201...; engine 69470d50...) and produced the same
source-screen sha256 8f34f218...; fb1 restored, staging removed. Still open:
control-plane fallback loading when the selected skin fails and the real
OmarchyTV screens; the vendor skin remains the fallback.

SETTINGS-SCREEN-2026-11-01: p0/03 ("Beeld") is the SubLauncherDialog picture-
settings overlay, not a generic settings list. SubLauncherDialog::
initialiseFields (0xee3615) dispatches item type 2 to
initialisePictureSettingsShortcuts (0xee0ed0); that function builds the top
SettingItems and the category shortcuts. Resources: DialogSubLauncher.svg (189)
listBox (189,255,1774,222); DialogSettings_fullscreen.svg (177) and
DialogSettings.svg (175) are vertical panels with listBox (16/20,170/171,750,
826/895), bottomText, warningText; DialogSettings_tab.svg (179)
main_rectangle (0,162,780,918), listBox (20,170,750,826), dolbyVisionIcon;
DialogTabMenu.svg (191) tabs picture/sound/network/installation/system at
x=11,163,316,467,619, y=104,150x58, dialogTitle (57,5,634,90), glyph
(38,24,60,60). SettingHorizontalListBoxFA (467): selectionIndicator
(0,0,240,140), rowPos/mRowM (278,90,170,106), mRowL/R (225/329,90,10,140);
SettingHorizontalListBoxRowFA (471): mRowL/M/R (0/50/202,0,50/151/50,155).
SettingListBoxFA (475): rowPos/mRowM (244,178,75,70), mTop/mBot pieces;
SettingListBoxRowFA (479): mRowL/M/R (18/54/178,61,33/120/33,66) #FE9419.
SettingsDialog::createSettingsList (0xd5692d) creates the list from the
dialog's "listBox" description; setListBoxHorizontal (0xd56ebd) calls
SVGListBoxBase::makeHorizontalListBox. Row templates: SettingItemsListBoxRowItems
(460) is an 813x66 row (mRowM, text1 (11,-16,382,50), button (429,-17,348,64),
selectionbox/checkbox/slidebar/progressbar); its _sublauncher variant (464) is a
240x140 tile (mRowM (10,-140,240,140), button (10,-140,246,140), text1
(10,-134,240,54), banner (10,-170,960,180)); SettingListBoxRowSelected (483) is
a 296x71 #F7941E row. Still unresolved: whether p0/03's bottom row is the
launcher app row showing sub-launcher shortcuts and the top tiles are the
SubLauncherDialog settings list, or all of it is one SettingsDialog; the dialog
popup anchor/window offset and the SettingItemsListBox item placement on the
horizontal list are not decoded.

What ships is explicitly the captured state, not that interaction: ui_vendor
SCREEN_SETTINGS=3 paints three R/B-corrected p0/03 bands (settings_rail
0,180,176,660; settings_top 0,240,1920,180; settings_cat 0,440,1920,260 with
the "Beeld" caption) over background (13,14,14). ui.screen=settings and
Select on rail 4 enter it; Exit/Menu return to the portal. It is static: no
navigation, per-category lists or localization. tests/ui_settings.c passes,
and a real fb1 run (PID26424, mod_ui_vendor md5 524d58bf...) is pixel-identical
to the swap-corrected p0/03 over the whole 1920x1080 (0 differing pixels, max
channel delta 0, identical sha256 13125757...); fb1 was backed up/restored
(16588800 bytes, offset 0,1080) and staging removed. Evidence:
artifacts/analysis/settings-2026-11-01/.

SETTINGS-PANEL-2026-11-01: artifacts/protocol/shot-settings.png (Oct 30, before
the R/B correction) is the vendor SettingsDialog picture tab: a 780x1080 panel
with "Instellingen", the paintbrush glyph, Beeld/Geluid tabs and the rows
Modus Natuurlijk, Contrast 95, Helderheid 50, Scherpte 50, Kleur 60,
Energiebesparing Uit, Beeld zoomen Auto, Virtual HDR Aan, Geavanceerde
instellinge. Its (45,30,190) is #BE1E2D swapped. Rows use
SettingItemsListBoxRowItems (460) at a 65..66 px pitch; tabs are the
DialogSettings_tab (179)/DialogTabMenu (191) family. ui_vendor now has
SCREEN_SETTINGS_LIST=4 (settings_panel 780x1080 at 0,0), entered by Select on
the p0/03 quick tiles or ui.screen=settings_list; Exit returns to the tiles,
Exit again to the portal. The panel area is opaque; the rest of the OSD is
cleared transparent (clear_rgba 0,0,0,0) so the video stays visible. The
capture's black right side is an empty canvas, not a painted backdrop: the
dialog's full-screen "background" record is named and named backgrounds are
descriptions, not drawn (UI-TRANSPARENCY-2026-11-01). Raw fb1 confirms alpha 0
at (1000,500) and (900,900), alpha 255 at (100,500) BGRA 45,30,190,255. The
first run's opaque black clear was corrected. tests/ui_settings_panel.c covers
the flow and tests/ui_settings.c the tile entry. A real fb1 run (PID30540,
mod_ui_vendor md5 c12f800f..., panel asset 0979dbf2...) is pixel-identical to
the corrected capture over 1920x1080 (0/2,073,600 differing, max delta 0,
sha256 cf4a2296...); fb1 backed up/restored (16588800 bytes, offset 0,1080),
staging removed. Row focus selection now exists as a reconstruction, see
SETTINGS-ROWS-2026-11-01; the panel capture remains the unfocused reference.
Evidence: artifacts/analysis/settings-panel-2026-11-01/.

SETTINGS-ROWS-2026-11-01: SCREEN_SETTINGS_LIST now has Up/Down row selection
(sel, -1 none) at the captured 70 px pitch: FA overlay settings_focus_N 750x66
blitted at (20, 183+70*N), row 6 ("Beeld zoomen", grey/disabled in the capture)
skipped, clamped at both ends, ui.sel=N preselects for lab captures, Select
only logs (no value editing), Exit returns to the quick tiles. FA evidence:
state mapping 0=UFA/1=FA/2=UFI/3=FI from SettingItemsListBox (0xd518f1) and
makeRow (0xd5072d); SettingListBoxRowFA 479 is the #FE9419 66 px 3-slice and
SVGListBoxRow::renderBackground (0x10bec09) stretches it horizontally
(HorizontalMaskScaleTransform); settingListTextboxFA 488 / ButtonFA 496 /
SelectionBoxFA 504 use #7E000A textPos (UFA 489/497/506 are white).
tools/render-settings-focus.py reconstructs the overlays from the corrected
panel (recolor only white ink on the bg->white line; keep sliders/icons).
tests/ui_settings_rows.c covers selection/skip/clamp/Exit; make check 236 ok.
Real fb1 run (PID32567, mod_ui_vendor b82ddbe2..., focus overlay e92ee007...)
matches the reconstructed reference exactly (0/2,073,600, sha256 86b66c00...);
raw fb1 alpha 0 at (1000,500), BGRA 25,148,254,255 (#FE9419) at (100,500),
45,30,190,255 panel at (24,185); fb1 restored, staging removed. This verifies
the pipeline, not a vendor focused-row capture: bar centering/row rect and
focused slider/button graphics are inferred; value editing, tabs, scrolling,
localization and native font/text integration remain open. UI-FONTS below
corrects the earlier FreeType assumption. Evidence:
artifacts/analysis/settings-focus-2026-11-01/.

SETTINGS-FOCUS-LIVE-2026-11-02: row selection + live text + ui.theme composed
on fb1 (PID12364, mod_ui_vendor 41eea1bc..., engine e307de66..., theme
47d7579a...; ui.screen=settings_list, ui.text=1, ui.sel=4). Capture sha256
ade07e09... differs from the live unfocused capture only inside the row-4 bar
band (x20..769, y463..528; 48,584 px); bar BGRA 25,148,254,255, alpha 0 outside
the panel. fb1 restored (16588800 bytes, offset 0,1080), staging removed, no
helper left. Evidence: same directory, tv-fb1-focused-row-4-live-text.png.

SETTINGS-EDIT-LIVE-2026-11-02: lab slider editing (NOT parity) verified live.
CORRECTION 2026-11-02 (adversarial review): the first capture's digits were
the focus overlay's own glyphs, not the edited value — live text was painted
under an opaque 750x66 band. Paint order fixed (flat bar + focused ink);
verified by fixed runs ("49" = 283 px spanning x660..703, "7" = 121 px
spanning x660..679, same origin). Bar frame now firmware-derived
(listBox+insets → Y0 186, pitch 70): fb1 run (mod_ui_vendor eb3768ca...)
shows the fill at exactly y256..321 for sel=1. Old 33c719a1 capture
superseded. Shadow store only, no DB writes; range 0..100 invented, step 1
matches the firmware default. fb1 restored each run (16588800 bytes,
offset 0,1080), staging removed, no helper left. Evidence:
artifacts/analysis/settings-edit-2026-11-01/.

Firmware before experiments; instructions before inferred Ghidra signatures.
No telemetry/advertising in any form. All other TV features remain parity scope.
Vendor animations, transitions and UI/UX effects are included, not yet accepted.
Measured 32bpp OSD storage is BGRA; OAI1/blit inputs are RGBA and screenshot PNGs
are converted RGB. Keep each boundary explicit; older captures may be swapped.
Follow-up audit corrected tvshot's RGB565 raw decoder and legacy splash's BGRA
pixel writer. Synthetic capture tests and strict C in-memory pixel checks pass;
no TV read/deployment or native/recovery binary rebuild occurred for this audit.
Use -Werror -pedantic; edit with apply_patch; use tools/tvssh.sh for authorized reads.

[Findings](FINDINGS.md) consolidates the OSD/PWM/input corrections.
[TODO](TODO.md) owns unfinished work; [HANDOFF](HANDOFF.md) gives exact RE entry points.
Document cleanup changed local documentation and explanatory source comments;
no executable behavior, binaries or TV state changed. Raw logs were preserved,
with six previously temporary DRM traces copied into the analysis directory.
