# Handoff — 2026-11-04

Read AGENTS.md, MEMORY.md, FINDINGS.md and TODO.md. Documentation was consolidated
to remove competing status snapshots. Raw evidence and pre-cleanup docs remain
local; use artifacts/reports/README.md to distinguish current conclusions from
historical experiments. The original documentation cleanup was offline; later
authorized live checks are recorded below and in MEMORY.md.

## Current objective and safety

Latest owner instructions,2026-11-05: "You can test on the TV if you need to"
and "You can also use fb1." Temporary display tests are reauthorized with stock
state/access restored afterward. Historical offline/fb1 exclusions below describe
their original runs. Persistent replacement and boot-hook changes stay forbidden.
A temporary Aurora stop and, after a failed direct relaunch, a watchdog reboot
were authorized the same day. See TAKEOVER-2026-11-05.

Owner authorization 2026-11-05: "I have full control of the TV now. You can
test things and draw things on the screen." Temporary staged tests and display
drawing are authorized; preserve stock Aurora/access and remove staging.
Staging now goes under `/buffer` per owner request. Cold startup, boot-hook
changes and persistent replacement remain separate gates.

Lua skin on the panel, 2026-11-05: a temporary `/buffer/oa-lua-screen` stack
(`display_fb`, `ui_engine`, `lua`, `ui_omarchy`) painted the script theme.
The owner saw one green flash. A later fb1 capture was stock "Geen signaal"
with no green samples, so Aurora repainted the layer. `display_fb` stop stayed
`cleanup pending` and quit did not finish the snapshot restore. Staging was
removed. Stock PIDs 1967/1785 remained. This is not independent startup.

Temporary takeover, 2026-11-05: openaurora owned fb1 with stock Aurora stopped.
`skin vendor` showed Startpagina. After a publish-only plat service, `ir` opened
`/dev/ttyS2` and Right moved focus to Netflix. `skin absent` fell back to vendor
(`fallback 1`, error 5). Suspend and resume repainted that portal. A direct
relaunch of `aurora.elf` came up without the appmgr environment, left fb1 at
1280×1440, and the remote did not work. The owner asked to kill Aurora and let
the watchdog reboot. Stock boot returned (fb1 1920×2160). The owner confirmed
the set is in control. Staging `/buffer/oa-takeover` was removed. Not cold
startup. TAKEOVER-2026-11-05.

BACK AV IR follow-up, 2026-11-05: owner set BACK AV and confirmed the stock
remote works there. The first takeover found `/dev/ttyS2` `rx:0` and no BLAST
keys; the live stock process then showed the BLAST common fd **222 =
`/dev/video28`** and RC type **1** (protocol 4). `BLAST_RX` returns **0 on a
key** and needs a `v4l2_open` fd. A clean IR-only takeover decoded the physical
Right/Down/Up/Left keys as profile RC **22/19/20/21**; `mod_ir` now defaults to
BLAST/video28 and emits the RC code directly. Watchdog reboot restored stock
(PIDs 2186/1782, appmgr 1580), BACK AV (`m_current_source=1`); staging removed.
TAKEOVER-IR-2026-11-05.

Full portal physical-remote takeover, 2026-11-05: staged
`plat_stub/display_fb/ui_engine/settings/service_db/image_decode/ir/ui_vendor`
under `/buffer`, stopped stock and painted Startpagina on fb1. The physical
remote moved card/rail focus with 29 BLAST presses; TV-side `reload ir`
reopened `/dev/video28` without hang; `reload ui_vendor` restored state and
kept decoding keys. Display stop needed the known retry; clean quit, staging
removed, watchdog reboot restored stock (aurora 1769, appmgr 1569, pan0,0,
source5 unchanged). **Open defect: median full-portal repaint 617 ms**, making
the 11×30 ms rail highlight take ~6.8 s. TAKEOVER-FULL-2026-11-05; not cold
startup/scanout/animation parity.

GPU scene probe, 2026-11-05: first production `graphics`/`scene_display` run
on real fb1 with `oa-scene-broker` + `scene-renderer.so`; the 5-rail/12-card
portal painted and the physical remote produced 14 BLAST presses. Non-reload
paint intervals: median260 ms, min129 ms, 11/23 ≤200 ms (display_fb: 617 ms).
`scene_display` now uses the vendor mipmap sampler. Dual-open `reload ir`
stalled the host ~64 s; the run was killed, fb1 restored from a full snapshot
and watchdog reboot returned stock (1777/1572, pan0,0). GPU-SCENE-PROBE;
graceful GPU teardown, timed animation and 30 ms cadence remain open.

Firmware-shaped partial rail animation, 2026-11-05: `display.patch` v1
replacement RGBA fill, display_fb dirty-rect/page-rounded sync and a
`paint_rail_hl` path patch the portal rail strip `0,211 209x670` and redraw
only rail slots/highlight. Host tests pass. TV: 10 highlight frames at
scene3-5 ms/pan5-8 ms, ~50 ms cadence (~450 ms total) versus 617 ms full
repaint; key full paint scene52 ms/pan11 ms. Static/full path unchanged;
GPU partial upload and exact 30 ms cadence remain. RAIL-PARTIAL.

Animation bootstrap, 2026-11-06: `DemoApp::checkUIDesign` →
`SVGDesignChecker::checkUsabilityStyle` finds the `animation` rectangle in
`DesignStyle.svg` (resource79; this TV: `animation -480 -270 1920 1080`) and
enables `UIAnimationController`. `ui_vendor` now applies that policy from
`ui.theme` with `ui.anim_enable` override; synthetic marker/no-marker host tests
pass. ANIM-BOOTSTRAP. The 300 ms SVGButton/sub-launcher rendering and grid/
transform origin remain unported.

Sub-launcher grid/animation decode, 2026-11-06: SVGGrid counts are
`(area - offset - (button - pitch))/pitch` with +0x1e4/+0x1e8 pitches and
+0x1f4/+0x1fc offsets; SVGButton uses elapsed/duration, linear scale/glyph
from→to, and a top-left window anchor with the named #scalerect size. Pure
`launcher_anim` helpers/tests cover sublauncher 240x140/1→1.35 and glyph
0→-42/-24. Screen integration/vendor transition capture remain open.
SUBLAUNCHER-GRID-ANIM.

Sub-launcher menu tree, 2026-11-06: `LauncherDialog` opens
`SubLauncherDialog` after 750 ms or Down; `initialiseFields` dispatches by
launcher type to picture/sound/system/channel/AI branches. `DialogSubLauncher`
listBox 189,255 1774x222, item 240x140, 300 ms 1→1.35 + glyph −42/−24.
Runtime `menus/sub_launcher.json`; `tools/re/menu-tree.py` regenerates it from
the ELF/decompile/BinSVG; bounded JSON loader + action strings; reloadable
`ui.menu` v1 service. `oa_portal_iface` v2 `menu_action` opens vendor screens
for `open:<screen>`; v1 consumers keep the prefix. Host tree/action/service
and portal-action tests pass; ARM GLIBC 2.4. SUBLAUNCHER-MENU-TREE.

QEMU UI oracle, 2026-11-06 (TV off limits): `tools/display/sim-display.c`
(ARM GLIBC 2.4) publishes display/alpha and writes 1920x1080 RGBA frames;
`tools/qemu-ui-layout.sh` runs the real ARM `openaurora` + ui_engine/ui_menu/
ui_vendor under `qemu-arm-static`. P0/03 `SCREEN_SETTINGS` band is **AE=0** vs
`settings_top.rgba`. `ui_vendor` `SCREEN_SUBLAUNCHER` maps the JSON picture
branch to captured cards at checked layout 201/451/701,276 235x130, pitch 250,
item cell 199,274 240x140; unfocused row AE=0 and 300 ms card2 bounds
235x130 → 317x175 at p=0/0.5/1. Repeat with
`tools/qemu-sub-launcher-check.sh`. Live card text, focused vendor capture,
runtime key/timer and scanout remain. QEMU-UI-LAYOUT.

Live sub-launcher/i18n, 2026-11-06: font3 labels/font12 values; clean card
backgrounds + live Dutch text within4 px of the capture (most≤2 px).
`mod_i18n` resolves all41 langprofile locales with current→eng→key fallback;
U+202C etc. are stripped. The row box is the vendor 240x54 `text1` at
`runtime_item_x`199; the row sets `setElipses(false)` on the label (clip) and
`setElipses(true)` on the value (elide), no shrink-to-fit. Audit:0 missing
glyphs; at240 px only bul253 and ukr264 clip (lit239 fits), values max183.
Profile Hebrew is stored visual order, so draw it as stored (Dictionary's
reorder targets the vendor RTL engine); QEMU heb shows מצב/חסכון בחשמל/
קירוב והרחקה של תמונה/טבעי/מקסימום/16:9. Runtime Up/Down uses `ui.clock`
+10 ms timer for300 ms; QEMU `key 20` through the real IR parser card0
bottom405→450. `mod_i18n` has the ABI3 async quiesce gate; `ir`/`i18n` are the
async modules. Arabic/Persian run order is now handled by
`common/rtl_order.c` (QEMU ara/per verified); bracket mirroring, MODE
picture_modes, the real PQ/UserPreferences write, other async modules and
scanout remain. LIVE-SUBLAUNCHER-I18N.

Sub-launcher values/picture store, 2026-11-06: `DemoApp+0x318` is
`NovoPictureSettings*`; vptr=`_ZTV19NovoPictureSettings`+8. The per-source
0x240 config has mode+0x04, zoom +0x08 normally / +0x0c when format-0 full
mode applies, energy+0x44+mode*0x70+device3d*0x38. The fields are vendor
SettingValue ids, not JSON positions: zoom AUTO1/16:9=5/SUBTITLE6/14:9=3/
14:9zoom4/4:3=2/21:9=11/CINEMA8/FULL9; energy CUSTOM5/SCREEN_OFF6. Dump
source1 is NATURAL/AUTO/MAXIMUM; configs3/4/14 (zoom5) match p0/03 and
m_last_zoom_mode=5. The JSON carries ids and ui_vendor stores/reads them;
mod_settings publishes `picture.settings` (whole-blob UPDATE, EROFS read-only,
picture.full_mode) over m_current_source (external index+1, 0=Tuner); the
ui.state_file shadow is removed. QEMU: source3 NATURAL/MAXIMUM/16:9
(132/129/54); 16:9→Ondertitels155 and DB zoom id6, survives restart; setting
focus450; Down,Down,Right AUTO176/energy4; set:MODE=CINEMA writes mode2 and
applies energy0 (Cinema/Uit); m_current_source5 gives DYNAMIC/OFF151/36.
Host test checks the IDs, reload and the +0x08/+0x0c rule. The offline
PICTURE-OBJECT harness executes the shipped UserPreferences accessors under
qemu-arm-static: 20 configs/80 zoom formats/200 energy cases/242 setters
match the model; get/set config(index0)/mode/energy(non-HDR)/default zoom and
getZoomMode formats0/1/2 are executed. Bronlijst Select calls
source.select_type first (CVBS15, HDMI1/2/3 17/18/19, YPbPr20, VGA16, TV0;
WiDi/APM no VIN slot), then writes m_current_source and resyncs; host
fake-VIN IPC asserts ids3/3/66/3/2/1 and source.enable, QEMU HDMI1 row2 logs
VIN id3 and shows DYNAMIC/OFF151/36. Limits: serialized blob/offline object,
not a running vendor UserPreferences/PQ process; real TV VIN/hardware select
unaccepted; HDR variants/twin-source setter/setZoomMode need the live EASI
graph; TV DB write needs settings.readonly=0. SUBLAUNCHER-VALUES,
PICTURE-OBJECT and SOURCE-VIN.

ui_vendor quiesce, 2026-11-06: ABI3 gate cancels timers/dialog/slide,
unsubscribes, releases providers, unpublishes ui.portal and unregisters the
engine skin synchronously; font pack + settings/sub-launcher text caches are
detached and freed on a worker (`ui_vendor.cleanup_delay_ms`). The gate is
idempotent for reload/unload; restore now accepts screen7. Control test:
reload fails fast, portal republishes, retry completes, stop/unload clean.
QEMU text/value/static and full suite pass. ir/i18n/settings/ui_vendor are the
async modules. UI-VENDOR-QUIESCE.

ui_engine quiesce, 2026-11-06: extends the old draining gate to full teardown
(unpublish ui.skin/ui.engine, drop display/alpha/async holds, unsubscribe,
delete timers, clear skin/events) and frees the OAI1 icon cache on a worker
(`ui_engine.cleanup_delay_ms`). Idempotent for reload/unload; control
`ui-icon` loads a real asset. Test: settings_top 1920x180, fast-fail reload,
responsive status, retry, asset reload, clean stop/unload. Full suite/QEMU
pass. ir/i18n/settings/ui_vendor/ui_engine are async. UI-ENGINE-QUIESCE.

display quiesce, 2026-11-06: existing restore/close worker now has
`display.cleanup_delay_ms` and an explicit control proof in fake_fb_socket.cjs
(first reload fast-fail, status, retry, display-info 1920x1080 32bpp2); closing
rejects draw/present/info, m_stop unpublishes after join, errors are throttled.
Fake fb1 memfd; suspend/resume/stop/reload/reuse snapshot equality still
passes. Not a real driver/GPU/scanout teardown. Full suite/QEMU pass.
DISPLAY-QUIESCE.

mod_source quiesce, 2026-11-06: unpublish `source` and detach the VIN fd
synchronously, then close on a worker after `source.cleanup_delay_ms`;
tryjoin/idempotent for stop+unload, m_stop/m_destroy join. Fake-VIN control
test: reload fails fast, source-state ENODEV, status responsive, retry reopens
VIN (close before open), stop gate closes the second fd, source.enable holds.
ARM mod_source links libpthread+vendor libs. Full/QEMU pass. The fixture close
is log-only; VIN-DEVICE-SIM below executes the real Hal_Vin_Close. Real
kernel-driver/V4L2/HDMI/HAL thread safety unproven.
ir/i18n/settings/ui_vendor/ui_engine/display/source are async.
SOURCE-QUIESCE.

ui_menu quiesce, 2026-11-06: unpublish ui.menu and detach the parsed JSON
document (document/dialog/branch return NULL), then free it on a worker after
ui_menu.cleanup_delay_ms; tryjoin/idempotent, m_stop/m_destroy join. Control
`menu-state <dialog>` returns dialogs/branches. Test: reload fails fast,
menu-state ENODEV while pending, status responsive, retry republishes the
tree; stop uses the gate. ARM module links libpthread. Full/QEMU pass.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu are async.
UI-MENU-QUIESCE.

mod_audio quiesce, 2026-11-06: unpublish audio/release plat synchronously,
detach oa_audio_owner, then quiesce/poll-join/destroy on a worker after
audio.cleanup_delay_ms; m_stop/m_destroy join, m_start refuses if live,
settled suppressed while quiescing. Control audio-state/audio-volume.
Stub-HAL test: volume42 set, reload fails fast, audio-state ENODEV, retry
republishes with volume42 restored/reapplied, stop gate. Existing audio/cec
tests pass; ARM links vendor libs+libpthread. Full/QEMU pass. No audible/
hardware acceptance; a blocked owner RPC can keep cleanup pending.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio are async.
AUDIO-QUIESCE.

mod_cec quiesce, 2026-11-06: unpublish cec.owner/cec.audio synchronously,
detach oa_cec_owner, then quiesce/poll/join/destroy on a worker after
cec.cleanup_delay_ms. A failed close retains the owner (-EAGAIN) for a new
generation; normal stop waits, force unload with a retained owner is outside
the safe contract. m_start refuses if live, settled suppressed while
quiescing. New cec-state probe. Test: reload fails fast, both services
ENODEV, status responsive, retry republishes; stop gate. Existing
retained-close/owner tests pass; ARM links libpthread. Full/QEMU pass. No real
CEC device/bus. ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/
audio/cec are async. CEC-QUIESCE.

audio_mixer quiesce, 2026-11-06: stop/reload unpublishes audio.mixer
synchronously, removes the watch and detaches the broker socketpair, then
closes on a worker after audio_mixer.cleanup_delay_ms; the50 ms reap timer
stays active to retire the child. Suspend keeps service/holds+timer; stop
joins/deletes timer; start refuses if live. Echo-broker control test:
query/collect left123/right456, reload fails fast, mixer-query ENODEV, status
responsive, retry republishes with fresh broker, child reaped before quit,
stop gate. Existing watch/timer/suspend/reload tests pass; ARM links
libpthread. Full/QEMU pass. Test broker only; no real mixer/ALSA.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer are async. AUDIO-MIXER-QUIESCE.

mod_tuner quiesce, 2026-11-06: stop an active scan and unpublish tuner
synchronously, then close the TS replay FILE* synchronously or on a worker
after tuner.cleanup_delay_ms. ts_open prevents the stop+unload double-quiesce
from spawning a second worker after the close. m_stop/m_destroy join and close
leftovers; m_start refuses if published or a worker is live. Control-socket
test with the generated EIT TS fixture: reload fails fast, tuner-state
ENODEV, status responsive, old /proc/<pid>/fd TS entry disappears, reload
reopens it, scan works, stop closes the second fd. Existing stub/TS tuner IPC
tests pass; ARM links libpthread. Full/QEMU pass. No NIM/frontend/demux or
PVR hardware. ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/
audio/cec/audio_mixer/tuner are async. TUNER-QUIESCE.

mod_epg quiesce, 2026-11-06: clear publication and unpublish epg.ingest/epg
synchronously, detach oa_eit_cache, then persist (when epg.path is set) and
destroy on a worker after epg.cleanup_delay_ms. m_stop/m_destroy join/free
leftovers; m_start refuses if live and recreates the cache for stop/start.
Control test: ingest two EIT events, reload fails fast, epg-count ENODEV, status
responsive, worker writes the two-record OAEPG1 file, retry completes and reloads
both events; clear/stop use the gate. Existing EPG persistence and tuner
handoff tests pass; ARM links libpthread. Full/QEMU pass. Project cache only,
not vendor EIT/SQLite. ir/i18n/settings/ui_vendor/ui_engine/display/source/
ui_menu/audio/cec/audio_mixer/tuner/epg are async. EPG-QUIESCE.

mod_pvr quiesce, 2026-11-06: clear publication and unpublish pvr synchronously,
then close the file-backed backend synchronously or on a worker after
pvr.cleanup_delay_ms. opened prevents the stop+unload double quiesce from
spawning a second worker. m_stop/m_destroy join/close leftovers; m_start
refuses if published, a worker is live or the backend is still open. Control
test: add two reservations, reload fails fast, pvr-list ENODEV, status
responsive, the two-record OAPVR1 file is present, retry completes and lists
both, a new add continues the persisted id sequence, record/stop still work,
stop gate. Existing PVR reload/cross-process test passes; ARM links libpthread.
Full/QEMU pass. Stub close is only an in-memory flag, not a slow storage
close. ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr are async. PVR-QUIESCE.

mod_channels quiesce, 2026-11-06: runtime active list moved from process-global
arrays to a heap-owned oa_channel_list per module instance (shared
oa_channel_runtime_iface stays for direct tests). Quiesce clears publication,
unpublishes channels synchronously, detaches the list, then destroys it
synchronously or on a worker after channels.cleanup_delay_ms. Stale iface fails
closed; m_stop/m_destroy join/free; m_start refuses if live and starts empty.
Control test: ingest two rows, reload fails fast, channel-state ENODEV, status
responsive, retry starts empty and ingests again, stop gate. Direct/IPC/UI
channel tests pass; ARM links libpthread. Full/QEMU pass. Project
ACTIVECHANNELLIST model, not vendor EASI; no LCN/visibility.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels are async. CHANNELS-QUIESCE.

service_db quiesce, 2026-11-06: clear publication and unpublish service.list
synchronously, detach the heap oa_service_list row array, then free it
synchronously or on a worker after service_db.cleanup_delay_ms. m_stop/
m_destroy join/free; m_start refuses if live. Control test with the 525-row
dump: reload fails fast, service-list ENODEV, status responsive, retry
re-reads 525 rows, sorted/name/channel probes work, stop gate. DB unit/module/
IPC tests pass; ARM links libsqlite3+libpthread. Full/QEMU pass. Project
read-only dump inventory, not a live vendor settingsDB writer or runtime EASI
filter; start-time open/read is still synchronous.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db are async.

image_decode quiesce, 2026-11-06: clear publication and unpublish image.decode
synchronously, detach libpng/zlib/TurboJPEG handles and clear TurboJPEG
pointers, then dlclose synchronously or on a worker after
image_decode.cleanup_delay_ms. m_stop/m_destroy join/close; m_start refuses if
live and reopens. Control test: decode PNG, reload fails fast, image-decode
ENODEV, status responsive, retry reopens and decodes the same bytes, stop
gate. Existing PNG/JPEG unit+IPC tests pass; ARM links libdl+libpthread.
Full/QEMU pass. Host codecs, not TV libraries; decode stays synchronous.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode are async.
IMAGE-DECODE-QUIESCE.

ui_theme quiesce, 2026-11-06: inventory found14 modules without a quiesce hook
(audio_amp_profile/audio_curve/panel/plat/power/ticker/ui_anim/ui_clock/
ui_console/ui_diag/ui_list/ui_navigation/ui_omarchy/ui_profile). Gate ui_theme:
clear publication and unpublish ui.theme synchronously, detach the parsed
oa_svg_pack, then free synchronously or on a worker after
ui_theme.cleanup_delay_ms. m_stop/m_destroy join/free; m_start refuses if
published/worker/pack live. New theme-state count probe. Control test: count 1,
reload fails fast, theme-state ENODEV, status responsive, fixture changed to 2
records while pending, retry reports count 2; stop gate. Existing 702-record
corpus/skin/hold/suspended-reload/fixture-IPC/ui-anim bootstrap tests pass;
ARM links libpthread. Full/QEMU pass. Synthetic pack only for the delayed
gate; borrowed views still require the host hold.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode/ui_theme are async.
UI-THEME-QUIESCE.

ui_list quiesce, 2026-11-06: owned state moved to heap. Quiesce clears
publication, unpublishes ui.list synchronously, snapshots the OAL1 handoff
blob, detaches state, then frees synchronously or on a worker after
ui_list.cleanup_delay_ms. Retry save() uses the snapshot after detach;
restore() allocates fresh state on a stopped instance after validating the
blob. m_stop/m_destroy join/free; m_start refuses if live. Control test: key
selection, reload fails fast, list-state ENODEV, status responsive, retry
preserves selected/animation state and still routes keys; stop gate. Direct/
IPC list tests pass; ARM links libpthread. Full/QEMU pass. Fixture list state,
not vendor SVGScrollList. 22/35 descriptors now have quiesce; 13 remain:
audio_amp_profile/audio_curve/panel/plat/power/ticker/ui_anim/ui_clock/
ui_console/ui_diag/ui_navigation/ui_omarchy/ui_profile.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode/ui_theme/ui_list
are async. UI-LIST-QUIESCE.

ui_navigation quiesce, 2026-11-06: selection/queue state moved to heap.
Quiesce clears publication, unpublishes ui.navigation synchronously, snapshots
the OAN2 handoff blob, detaches state, then frees synchronously or on a worker
after ui_navigation.cleanup_delay_ms. Retry save() uses the snapshot after
detach; a failed start/resume that triggers quiesce restores from the snapshot
on the next start. m_stop/m_destroy join/free; m_start refuses if live. Control
test: queue 2 requests, reload fails fast, nav-state ENODEV, status responsive,
retry preserves selection/queue and still delivers a request; stop gate.
Existing unit/IPC/matrix tests pass, including 1792 OAN roundtrips and
failed-resume retry; ARM links libpthread. Full/QEMU pass. Project navigation
model, not vendor DemoApp/EASI. 23/35 descriptors now have quiesce; 12 remain:
audio_amp_profile/audio_curve/panel/plat/power/ticker/ui_anim/ui_clock/
ui_console/ui_diag/ui_omarchy/ui_profile.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode/ui_theme/ui_list/
ui_navigation are async. UI-NAVIGATION-QUIESCE.

ui_profile quiesce, 2026-11-06: copied profile moved to heap. Quiesce clears
publication, unpublishes ui.profile synchronously, detaches the profile, then
frees synchronously or on a worker after ui_profile.cleanup_delay_ms. New
profile-state probe. Control test: connectivity1/hotel0, reload fails fast,
profile-state ENODEV, status responsive, fixture changed to connectivity2/
hotel1, retry reports new facts; stop gate. Also audited ui_list failed-start:
start rebuilds from the OAL1 snapshot after a publish-collision quiesce, and
fresh-state restore reinitializes fixture handles; new suspend/reload/peer/
resume regression in tests/ui_list.c. Unit/fixture tests pass; ARM links
libpthread. Full/QEMU pass. Project EFSL profile, not vendor UserPreferences/
PQ. 24/35 descriptors now have quiesce; 11 remain: audio_amp_profile/
audio_curve/panel/plat/power/ticker/ui_anim/ui_clock/ui_console/ui_diag/
ui_omarchy.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode/ui_theme/ui_list/
ui_navigation/ui_profile are async. UI-PROFILE-QUIESCE.

audio_curve quiesce, 2026-11-06: decoded 101-point table moved to heap.
Quiesce clears ready/published, unpublishes audio.curve synchronously,
detaches the table, then frees synchronously or on a worker after
audio_curve.cleanup_delay_ms. m_stop/m_destroy join/free; m_start refuses if
live and rereads the AQ file/profile, so no handoff snapshot is needed.
Control test: base100 curve110, reload fails fast, audio-curve ENODEV, status
responsive, AQ changed to base200, retry curve210; stop gate. Direct/module/
fixture tests pass; ARM links libpthread. Full/QEMU pass. DSP table entries,
not hardware gain; profile selection still explicit. 25/35 descriptors now
have quiesce; 10 remain: audio_amp_profile/panel/plat/power/ticker/ui_anim/
ui_clock/ui_console/ui_diag/ui_omarchy.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode/ui_theme/ui_list/
ui_navigation/ui_profile/audio_curve are async. AUDIO-CURVE-QUIESCE.

audio_amp_profile quiesce, 2026-11-06: profile+32-byte power moved into one
heap profile_data. Quiesce clears published, unpublishes audio.amp_profile
synchronously, detaches the data, then frees synchronously or on a worker
after audio_amp_profile.cleanup_delay_ms. m_stop/m_destroy join/free; m_start
refuses if live and rereads the hwprofile, so no handoff snapshot is needed.
Control test: prescale17, reload fails fast, amp-profile ENODEV, status
responsive, hwprofile changed to31, retry reports31; stop gate. Existing
provider/module/IPC/fixture tests pass; ARM links libpthread. Full/QEMU pass.
Immutable profile data only, not amp hardware. 26/35 descriptors now have
quiesce; 9 remain: panel/plat/power/ticker/ui_anim/ui_clock/ui_console/
ui_diag/ui_omarchy.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode/ui_theme/ui_list/
ui_navigation/ui_profile/audio_curve/audio_amp_profile are async.
AUDIO-AMP-PROFILE-QUIESCE.

panel quiesce, 2026-11-06: unpublish panel synchronously; one needs_cleanup arm
per start cycle; vendor Hal_Pwm_Close runs on a worker after
panel.cleanup_delay_ms (host stubs still honor the diagnostic delay). New
panel-state probe. Control test: percent40, reload fails fast, panel-state
ENODEV, status responsive, retry republishes40; stop gate. needs_cleanup fixes
the stop+unload double-quiesce that armed a second worker. Existing
plat-consumer IPC passes; ARM links vendor libs+libpthread. Full/QEMU pass. No
PWM/backlight hardware run; real close unaccepted. 27/35 descriptors now have
quiesce; 8 remain: plat/power/ticker/ui_anim/ui_clock/ui_console/ui_diag/
ui_omarchy.
ir/i18n/settings/ui_vendor/ui_engine/display/source/ui_menu/audio/cec/
audio_mixer/tuner/epg/pvr/channels/service_db/image_decode/ui_theme/ui_list/
ui_navigation/ui_profile/audio_curve/audio_amp_profile/panel are async.
PANEL-QUIESCE.

host sched first module use, 2026-11-06: ui_theme delayed pack free moved from
a pthread to one nonblocking sched slice (cleanup_task), preserving synchronous
unpublish, fail-fast EAGAIN while pending, delay0 sync free, cancel/free on
stop. ARM ui_theme no longer links libpthread. Existing 400ms gate/fixture/
skin/ui-anim tests pass; full suite ends all tests passed; QEMU pass. Only
pure-free cleanup migrated; blocking close/join paths stay workers. TV not
used. SCHED-UI-THEME.

host sched second use, 2026-11-06: channels delayed list destroy moved from a
pthread to one nonblocking sched slice (cleanup_task), preserving synchronous
unpublish, fail-fast EAGAIN while pending, delay0 sync destroy, and cancel/free
on stop. ARM channels no longer links libpthread. 400ms gate, XML/IPC tests,
full suite all tests passed, QEMU pass. Pure-free cleanup only; blocking paths
stay workers. TV not used. SCHED-CHANNELS.

ui_diag async gate, 2026-11-06: synchronously unsubscribe, unregister the diag
skin/release ui.engine, detach font pack/pixels, then free them from one sched
slice after ui_diag.cleanup_delay_ms. cancel/free on stop; m_start refuses
while pending. Engine skin query shows active - while pending and active diag
after reload. 400ms control test with nav_display/engine and extracted font
pack; diag/skin-socket tests, full suite all tests passed, QEMU pass. ARM links
libm/libc only. Recorder fixture, not scanout; no TV run. 28/35 descriptors now
have quiesce; 7 remain: plat/power/ticker/ui_anim/ui_clock/ui_console/
ui_omarchy. UI-DIAG-QUIESCE.

ui_anim async gate, 2026-11-06: unpublish ui.anim, remove cadence timer,
unbind callbacks and release ui.clock synchronously, detach heap controller,
then destroy from one sched slice after ui_anim.cleanup_delay_ms. Stale calls
fail closed; start recreates controller; stop/destroy cancel/free. New
anim-state probe. Control test: reload fails fast, anim-state ENODEV, status
responsive, retry republishes; stop gate. Animation review/bootstrap tests,
full suite all tests passed, QEMU pass. ARM links libm/libc only. No live
animation across reload; no TV run. 29/35 descriptors now have quiesce; 6
remain: plat/power/ticker/ui_clock/ui_console/ui_omarchy. UI-ANIM-QUIESCE.

ui_clock async gate, 2026-11-06: unpublish ui.clock, remove timer and release
raw clock synchronously, snapshot/detach FROST time state, then free from one
sched slice after ui_clock.cleanup_delay_ms. Retry save uses snapshot;
failed-activation start restores it; stop/destroy cancel/free. New clock-state
probe. Control test: reload fails fast, clock-state ENODEV, status responsive,
retry republishes; stop gate. Clock unit tests, full suite all tests passed,
QEMU pass. ARM links libc only. No live continuity across reload; no TV run.
30/35 descriptors now have quiesce; 5 remain: plat/power/ticker/ui_console/
ui_omarchy. UI-CLOCK-QUIESCE.

ticker async gate, 2026-11-06: remove timer and unpublish clock synchronously,
snapshot/detach tick counter, then free from one sched slice after
ticker.cleanup_delay_ms. Retry save uses snapshot; start restores counter;
stop/destroy cancel/free. New ticker-state probe (published clock + saved
ticks). Control test waits 1.1s for tick1, reload fails fast, ticker-state
ENODEV, status responsive, retry preserves tick1; stop gate. Clock/animation
dependencies, full suite all tests passed, QEMU pass. ARM links libc only.
Project diagnostic probe; on-TV follow-up below. 31/35 descriptors now have
quiesce; 4 remain: plat/power/ui_console/ui_omarchy. TICKER-QUIESCE.

ticker/clock/anim on-TV CPU-only check, 2026-11-06: staged under
/buffer/oa-sched-verify with hash-verified ARM host+mod_ticker/mod_ui_clock/
mod_ui_anim. ticker ticks0 then1 after1.5s; stop ui_anim/clock and reload
ticker each failed fast with ENODEV then completed after0.7s; tick2 restored;
unload clean. Staging removed, no helper left, stock Aurora PID1781 unchanged.
Only DSOs/socket/stdin opened; no fb/DVB/alsa/GPIO paths. CPU-only run, not
display/HAL/scanout/audio/tuner acceptance. /tmp staging removed separately.

ui_console async gate, 2026-11-06: clear active and unsubscribe KEY/TICK
synchronously, snapshot/detach the heap selection, then free from one sched
slice after ui_console.cleanup_delay_ms. Save uses the snapshot; start refuses
while pending and restores the selection; stop/destroy cancel/free. New
console-state and diagnostic emit-key probes. 400ms control test: selection
0->1, reload fails fast, status responsive, key during gate ignored, retry
preserves1 and a new key gives2, stop uses the gate. Focused ASan/UBSan clean,
full suite all tests passed, QEMU pass. ARM links libc only. On-TV CPU-only
/buffer/oa-console-verify run with hash-verified host+module reproduced the
sequence (restored1 byte), staging removed, stock Aurora PID1781 unchanged; no
fb/DVB/alsa/GPIO paths. Text UI only; 32/35 descriptors now have quiesce; 3
remain: plat/power/ui_omarchy. UI-CONSOLE-QUIESCE.

ui_omarchy async gate, 2026-11-06: clear active, remove the rebind timer and
subscriptions, unregister the engine skin and release engine/Lua synchronously,
snapshot/detach the cached colors/layout/screen state, then free from one sched
slice after ui_omarchy.cleanup_delay_ms. save uses the snapshot; a72-byte
magic/versioned blob restores it; start refuses while pending. New
omarchy-state probe. 400ms control test (nav_display+lua+ui_engine): script
theme9/8/7+6/5/4, layout0,0,40,100, screen1920x1080; reload fails fast, engine
reports active -, status responsive, retry restores72B and re-registers, stop
gate. Focused ASan/UBSan clean, skin/Lua suites pass, full suite all tests
passed, QEMU pass. ARM base glibc only. On-TV CPU-only
/buffer/oa-omarchy-verify run with hash-verified host+modules reproduced it,
staging removed, stock Aurora PID1781 unchanged; no fb/DVB/alsa/GPIO paths.
Recorder display only; 33/35 descriptors now have quiesce; 2 remain:
plat/power. UI-OMARCHY-QUIESCE.

plat/power quiescence decision, 2026-11-06: neither owns a worker, queued
callback, heap cleanup or reversible device close, so no fake async hook was
added. plat stop unpublishes plat; the vendor HAL bring-up is process-global
with no recovered de-init and NDAL_VideoExt_OpenDevice(0,10) installs a global
fd, so real-HAL reload is not a reset and remains unaccepted. power stop
unpublishes power and drops the host.sleep hold; its sysfs fd exists only inside
the final entry callback. New check-plat-power-sync-stop drives the real host
modules (OA_NO_VENDOR plat, power.enable=0): reload1ms/stop0ms, services
unpublished; focused ASan/UBSan clean; full suite all tests passed; QEMU pass.
No HAL/standby hardware, no TV run. 33/35 deferred/scheduler gates +2 audited
synchronous stops = all35 decided. PLAT-POWER-QUIESCE.

plat cold-start wait, 2026-11-06: firmware ICE_HardwareInit@0x7551a0 polls
access("/tmp/ntkdriver_completed",R_OK) every50ms (0x755498..0x7554ae, literal
0x755540->0x12930c5), logs elapsed and only then calls
PLAT_DRV_INI_Init@0x89b6e4 (0x75564c). mod_plat now waits before the audio
prelude: plat.driver_wait (unset auto via /dev/ntkled), driver_marker,
driver_timeout_ms60s (0=forever); timeout sets STAGE_DRIVER and refuses HAL,
retry works. New check-plat-startup-wait: pre-existing1ms, arriving151ms,
timeout401ms then retry; focused ASan/UBSan clean, full suite all tests passed,
QEMU pass. Cold config opts in. View-only TV: marker readable1, ntkled present,
/proc/fb five OSD planes, fb1 OSD_NORMAL2 1920x2160 stride7680, stock PIDs
1711/1781 unchanged. No cold boot/HAL/open-write; not scanout acceptance.
PLAT-STARTUP-WAIT.

display resume frame, 2026-11-06: vendor idtvResume@0x752b88 re-enables the OSD
plane and restores the STR bootlogo (0x752ca6/aa/b0, repeated0x752cfc/d00/d06);
kernel PM replays OSD state and first-pan shows plane/region. mod_display now
keeps the last RGBA scene across suspend, reuses it on reopen, saves the resume
snapshot before re-applying the region and recommits the scene via
FBIOPAN_DISPLAY ("resume: first frame committed"); failure restores/closes but
keeps the scene for retry; suspended quiesce frees only heap synchronously so
reload stays immediate. New fake-driver test clears its map on open and proves
re-render/region/commit/retry; display/lifecycle/fake-fb suites pass, ASan clean,
full suite all tests passed, QEMU pass. On-TV under stock: magenta first frame
read back at start and on4 consecutive samples across resume in the panned frame
(yoffset1080); stock repaints over it, so no panel scanout/cold acceptance.
Staging removed, stock PIDs1711/1781 unchanged. DISPLAY-RESUME-FRAME.

display scanout state check, 2026-11-06: opt-in display.verify_scanout re-reads
FBIOGET_VSCREENINFO yoffset, OSD region size/origin and MAU after start/resume
first frames, rejects a mismatch and restores/closes for retry. Fake test uses a
one-shot wrong yoffset after a successful pan to prove rejection/retry; display/
lifecycle/fake-fb suites, ASan, full suite and QEMU pass. On-TV
/buffer/oa-display-verify (mod_display_fb fc4780db...) logged scanout verified at
start and resume: yoffset0, region1920x1080 at0,0, mau0, stock PIDs1711/1781.
Driver-state evidence only; stable panel scanout and cold-ownership acceptance
still need authorized controlled ownership plus independent observation.

cold-start stage audit, 2026-11-06: firmware ICE_HardwareInit calls
PLAT_DRV_INI_Init then DEV_INI_Init@0x853a58 (0x75564c/50, 11 stages: PCB
layout, GPIO, source, panel, NIM, IIC, amp, SPI init/open, NIM-DTV, MTD). Each
maps to a modular owner; plat must not call it wholesale (duplicate ownership,
unrecovered device code under stock). SPI/MTD/NIM-DTV and amp/panel power stages
remain unimplemented. New plat-state probe reports ready and stage-error mask;
check-plat-startup-wait asserts ready/0x0 after start and ENODEV after stop; full
suite all tests passed. Offline host stubs only; no cold boot/hardware
acceptance. COLD-START-STAGE-AUDIT.

spi/mtd/nim stages, 2026-11-06: DEV_MTD_Init@0x85419c only registers a debug
menu, so no cold-start MTD owner is needed. DEV_SPI_Open@0x858d0c prefers
/conf/spi.bin (rb+/rb) and else /dev/ntkspi O_RDWR; read uses fseek/fread or
ioctl0x8018531e with {offset,size,buf,0,0,0}, returning1/0. Added
common/spi_dev + opt-in read-only mod_spi service (spi.enable=0, not
autoloaded); fake file/device tests wrap open/close/ioctl and cover
bounds/failure/zero-length. ASan clean, full suite all tests passed, QEMU pass,
ARM ELF32 libc-only. DEV_NIM_DTV_Init@0x861678 creates a DTV task and remains
hardware/tuner work; SPI writes/control and real /dev/ntkspi untested.
33/36 descriptors have deferred/scheduler gates; plat/power/spi are audited
synchronous (spi close is fclose/close, no worker). SPI-MTD-NIM-STAGE.

audio i2s mclk, 2026-11-06: DEV_AMP_Init@0x84f4ac zeroes a56-byte block, sets
params[0]=0x100 and calls Hal_Audio_Ctrl(0x3a) before creating the amp monitor
task (return ignored). mod_audio now optionally sends 0x3a/0x100 before
volume/mute (audio.i2s_mclk=1, default off; logs failure, continues). Fake-HAL
test asserts one call, zeroed remaining params and following0x33/50; focused
ASan/UBSan clean, full suite all tests passed, QEMU pass. No vendor HAL/I2S/
amp task/audible acceptance; default off. AUDIO-I2S-MCLK.

rtl bracket mirroring, 2026-11-06: Arabic/Persian visual-order helper now
applies UAX#9 L4 mirrored glyphs to RTL-resolved paired punctuation
(()[]{}/<>/guillemets/fraction/super-sub/<=/<= variants); LTR and no-Arabic
fallbacks unchanged, stored-visual Hebrew untouched. New cases
(ب)->(ب), ABC (ب)->(ب) ABC, [(ب)], (abc), plus the41-locale corpus; focused
ASan/UBSan clean, full suite all tests passed, QEMU live ara/per pass. Standard
BiDi rule, not instruction-level vendor text recovery; mixed Hebrew/Latin BiDi
and visible capture remain open. RTL-BRACKET-MIRROR.

controlled-ownership scanout, 2026-11-06: owner "you have full access to the TV";
AGENTS updated. Watchdog disable sent, stock aurora1781 stopped, staged
hash-verified display_fb (core025d1fb0/moduleb7ab1747) started on
vendor-initialized fb1 with snapshot+self-test+verify. Log: yoffset0
region1920x1080 at0,0 mau0. Three captures2s apart and three raw active-frame
reads1s apart byte-identical (raw656b0caf...); magenta band in active frame,
no stock repaint. Stop restored fb1 (band gone; region/pan/MAU same; manual
pre-snapshot differs only3 stock bytes from the later start state), staging
removed, wdtctl set10 rebooted. Stock returned: appmgr1582, aurora1784/zombie
1962, fb1 1920x2160 stride7680, no openaurora, oa_boot/oa_fail absent,
oa_safe/coredumpnoreset present, post-boot capture nonblank. Stable
controlled framebuffer/scanout-path evidence; no independent owner/HDMI panel
confirmation, not cold startup/MCU authority. Captures under
artifacts/analysis/controlled-scanout-2026-11-06. CONTROLLED-SCANOUT.

controlled scanout confirmation, 2026-11-06: second run with display.test=2
(8 full-screen bars, module5422a86c) under the same temporary ownership;
start log scanout verified yoffset0 region1920x1080 at0,0 mau0. Two captures5s
apart identical (a29ccecc...), sampled white/yellow/cyan/green/magenta/red/
blue/black across y540. Owner answered "Yes, I see the bars." Stop restored
fb1 (row10 zero), staging removed, wdtctl set10 rebooted. Stock returned:
uptime27.7s, appmgr1574, aurora1774/zombie1945, fb1 1920x2160 stride7680, no
openaurora, oa_boot/oa_fail absent, oa_safe/coredumpnoreset present,
post-boot capture nonblank. This confirms controlled panel scanout and
full-screen geometry; not cold startup, vendor UI pixel parity or MCU/standby
authority. Captures under artifacts/analysis/controlled-scanout-2026-11-06.

panel curve/nim dtv, 2026-11-06: DEV_PANEL_Init uses breakpoints40/60/80/100 and
scale1000; SetPWMBacklightLimit writes M=(1000*pct+50)/100 at
gstBacklightPara+4; SetPWMBacklightLv maps M+(level*(1000-M)+500)/1000 through
the midpoint table with vendor integer rounding. Added common/panel_curve +
opt-in panel.curve/min_percent/duty_max (default linear/4095 path unchanged);
fake-HAL tests cover identity/min20/p4=50 bend/percent-duty(4095 and1000
calibration)/bounds/degenerate. DEV_NIM_DTV_Init idempotent guard,
model0..3/type4 active1/else3, mutex and CreateTask(entry0x8601ad,
stack0x1000, priority14), Term0xff. Ghidra decompile of the raw Thumb task
entry: modulebuf9, NIM init, conditional HAL demod+connect, then
DEV_IIC_Read/Write(0,0,0x80,1,code,1,&byte): 0x15 clears bit3, 0x11 sets bit3,
0x40 sets bit1 (write result unchecked); active0xff/signal/unlock. Implemented
as an ordered fakeable state machine with read/write callbacks and failure
tests; HAL adapter packs {0,0x80}/code/1 byte and inverts Hal_Iic 0=success to
DEV 1=success, end-to-end fake-Hal test passes. PLAT_DRV_GetModuleBufInfo is
a literal return0 stub and is reproduced/tested; HAL_Demod_User_Initialize
pulls in the SAL tuner/LNB subsystem: HAL_TUNER_Init resolves four IIC bus IDs
via DEV_IIC_GetDescription, SAL_DEMOD_TUN_Initialize builds SI2151/MxL661 +
AV2018, tuner type comes from profile TunerTypes/TunerType (EFSL decoded in
EFSL-TUNER-IDENTITY: hwprofile value Si2157, vendor compares only Si2151, so
SAL 0xd/NdNim 0xc on layout14), and
SAL_LNB_Initialize calls RT5047. mod_tuner is intentionally not wired rather
than faking tuner init or touching IIC early. ASan/UBSan clean, full suite all
tests passed, ARM mod_panel builds. No panel/tuner hardware; curve default off.
PANEL-CURVE-NIM-DTV.

EFSL tuner identity, 2026-11-06: ROMProfile container decoded (EFSL header,
root/entry tables, 20-byte hashed entries; getUniqueID seed0xf1d3b597 and
shift remainders from __aeabi_uidivmod). mb230_hwprofile/swprofile MD5s match
TV /conf (1b063b2c.../a1d9e5e8...). hwprofile TunerTypes/TunerType="Si2157";
FUN_0084abb8 compares only "Si2151" and DEV_INI_Init sets layout14, so the
firmware selects SAL0xd/NdNim0xc (not Si2151=8). common/efsl_profile.c +
common/tuner_identity.c reproduce that with bounds checks; host, MD5-pinned
corpus, ASan/UBSan, full suite and ARM/qemu pass. mod_tuner still unwired;
MxLWare601 init/HAL tuner/LNB backend open. EFSL-TUNER-IDENTITY.

MxL661 tuner init backend, 2026-11-06: recovered SAL type0xd: MxLWare command
buffer, Ctrl_Write/ReadRegister via DEV_IIC type4 addr0xc0 (0xfb pointer,
0x20000000 read), MxL_OverwriteDefaults 26x3 {reg,mask,value}, default config
11 words, and the exact 41-write/5-read order (reset, defaults+RMW31, xtal,
IF-out incl. stale cfg[3] reg6/7 branch, AGC RMW8/9/5e, power-up0b). Added
common/mxl661.c + common/tuner_hal.c (bus IDs from desc+0x19, Hal ctx
{bus,reg,0,0,400,0}, clz success); host, focused ASan/UBSan, full suite and
ARM/qemu pass. mod_tuner still unwired: AV2018/dvif/LNB + NIM chain remain.
TUNER-MXL661.

Tuner bring-up/ownership, 2026-11-06: read-only TV fds show stock aurora
PID1774 owns /dev/i2c-0..5, /dev/ntkiic0, /dev/ntgpio, /dev/ntkdvif,
frontend0; appmgr owns /dev/ntkgpu. AV2018 constructor data-only and
GetTunerResponse=return1; HAL_Demod_Init(0) opens/caches dvif, 0 normal/1
error; HAL_dvif_open returns cached handle; SetTunerType ioctl0xc0044485 maps
0xc->0xc,5/7->5,6->6,default8; RT5047=LNB_EN (GPIO index5) SetLevel, left
unbound (/dev/ntgpio owned). Corrected Hal IIC ctx to 24 bytes
{bus,addr,subaddr_len,0,timeout,0} (100 type0/400 type4), added MxL661 DEV
adapter (0xfb pointer), sequence layer and default-off tuner.hw_init gate
(host -ENOTSUP; TV builds mod_tuner.so sha256 8cd06b7b... linking libNDAL3).
Host/ASan/full/ARM-qemu pass. NIM state/Connect2HAL equivalents and RT5047
LNB_EN descriptor+Hal_Gpio adapter are wired behind the gate; the NIM tuning
subset (path map, open/close, lock/strength/quality) is wired into the
module table. MxL661 DVB-T/DVB-C case 9/0xd/0xb RF/IF and the typed
SetChannel/GetChannelStatus mapping are implemented (common/mxl661_rf.c,
common/nim_tune.c); a real tuner.backend=hal NIM backend (common/nim_hal.c,
requires tuner.hw_init=1, terrestrial only) wires the ops into mod_tuner;
the raw 0x5c channel record and the verified 0x180 lock/strength/quality
fields are parsed/tests; the 0x180 tbb dispatch/case targets are recovered and every
output map is implemented (types 1/2/3/6/7/8; types 4/5 return 0 with no output). The
earlier type 6/8 frame claim was corrected; vendor-stack output slots are zeroed
deterministically and type 2/7 out[14] is preserved. The analog NTSC/PAL/SECAM
case 9/0xd/0xb tails are implemented too (common/mxl661_atv.c); the vendor
tables are loaded at runtime from the TV's own aurora.elf, CRC32-checked, never
embedded or distributed. The analog tuning path is wired into the NIM HAL backend
(oa_nim_hal_set_atv -> program_atv -> oa_mxl661_tune_analog; module
config tuner.analog=1 with standard/if/xtal), and analog monitor/
signal use the recovered SAL_TUNER_MxL661_GetATVStatus 0x2b/0x2c
lock decode (strength/quality are not in that contract). Analog scan
stepping is wired (analog_scan/start/stop/step/dwell, host stub test);
the case-9 per-address calibration remains. First tuner takeover 2026-11-06:
libNDAL3 Hal_Iic_Init is required in a replacement process and was added;
MxL661 init, demod/tuner/AV2018/dvif/SetTunerType/LNB and HAL connect all
passed, but the NIM task saw a NIM-DTV IIC code-21 NACK (failed_step=5). The
module's own failed-step start gate was later corrected: the vendor's task
tolerates every IIC failure and still signals init-complete at 0x00860252, and
WaitInitComplete only waits on DAT_01936fb0. The helpers are
_dev_nim_dtv_IIC_Read/Write over DEV_IIC i2c-0/slave 0x80; +0x48 is the
HAL_Demod_User_PowerControl power-down path, not a power-up prerequisite.
Second run 2026-11-07 with mod_tuner sha256 cf361243...: service started
(status started, services tuner:v1), kernel logged ki2c [i2c-0] Dev NACK,
tuner-scan 1 (DVB-T) and tuner-scan 4 (analog) returned ok and tuner-state
advanced. No RF source: progress/services are the synthetic scan state
machine and lock 1 is not independent acceptance. Vendor-channel path
2026-11-07: HAL_Demod_User_Initialize/SetChannel are Aurora-only, so their
DVB-T body was reproduced with the recovered 0x5c record, MxL661 program and
libNDAL3 HAL_Demod_Set behind tuner.vendor_channel=1; host tests and a no-RF
TV run pass (tuner-scan 1 ok/25-50-75; analog retry ok after the DVB-T scan
reached DONE; the first analog request correctly returned -EBUSY). Restored
by wdtctl set 10; latest stock appmgr 1562/aurora 1765, staging/socket absent,
profile unchanged. TUNER-TAKEOVER/TUNER-VENDOR-CHANNEL. SI2151 is the full
Silicon Labs patch/property driver, not this profile's path.

Panel backlight 2026-11-07: read the stock process read-only for calibration:
GPIO descriptor 0x6c -> pin108 -> PWM channel0 via astPwmBasePin (0x175f320);
gu32PanelFreqInHz=30000, genPwmdimMode=0, gstBacklightPara level1000. Corrected
mod_panel defaults (30000 Hz, panel.mode0; open mode1) and tests. A first stage
mistakenly contained the real mod_plat (stale stub) and ran the full plat HAL;
it was restored by reboot and is not panel acceptance. Rebuilt the actual
plat_stub (37f8f320...; later named plat 97ef7656... for the VIN harness) and ran cleanly at 50%: panel loaded, /dev/ntkled fd6
open, panel-state50. Brightness is not measured/owner-confirmed; panel
power/resume remain open. Restored by wdtctl set10: latest stock appmgr1577/
aurora1774, level1000, freq/mode30000/0, profile/fb unchanged. PANEL-BACKLIGHT.

Audio/CEC read-only group 2026-11-07: stock pre-state captured (snd controlC0/
C2/C3, i2c-3, video28, CEC major229 no node; mixer values0; amp regs1b/05/05;
profile/AQ/fb/backlight). Staged d8d2006a... with plat stub + audio_amp_profile
+ audio_curve + audio + cec; all started. amp-profile 12Watt/27/5/5/128,
audio-curve50 8137, audio-state enabled0 volume-1, cec-state owner1 audio1
supported0 local1. No audio/CEC device fds and no volume/mute/amp/CEC bus
operations; read-only probes unchanged. Fixed config key amp_profile.hwprofile
(not audio_amp_profile.hwprofile); the typo caused EINVAL. Restored by wdtctl
set10: latest stock appmgr1564/aurora1764, same fds, probes0 and1b/05/05,
profile/AQ/fb/backlight unchanged. Not audible/CEC acceptance; native backends
open. AUDIO-CEC-TAKEOVER.

CEC native backend 2026-11-07: added cec_authority_mem (read-only /dev/mem page
0xfc040000+0x200; bit1 was set, mirror 0x000001ab), cec.enable/device/poll
config, async owner acquire/receive/send with policy reply queue, and
cec-open/cec-close/cec-send/cec-state. Host tests pass; ARM core 88bb2906...,
mod_cec 10ef4b50.... Hardware attempt stopped stock but the kernel thread
DRV_CEC_CALLBAC (pid2929) kept the exclusive KER_CEC lock; raw open returned
EPERM (CEC Try lock fail), so no frame TX/RX. Reboot restored stock aurora1740
with its own callback thread2937; no node/stage/socket. Close-path follow-up:
ICE_CECClose is a stub, PLAT_CEC_Close has no internal caller, and a new
process's NDAL_VideoExt_CEC_Init(NULL) (with or without open) reports success
but leaves the orphaned thread; no tested userspace stock-close trigger was
found. Raw takeover needs an early boot-order start or shared stock-owned
monitoring. CEC-RAW-BACKEND.

Amp transport 2026-11-07 (offline): recovered DEV_GPIO_SetLevel/SetDirection.
Pin<200 -> linkable Hal_Gpio_Write/Config (0 success; issue the mutating 670a
claim); pin>=200 -> (pin+0x38)&0xff with level0->STBC cmd3, level1->cmd2.
NDAL_VideoExt_STBC_SetStbcGpio returns the ndcommon ioctl boolean (1 success/0
failure) and needs the global fd from NDAL_VideoExt_OpenDevice(0,index).
common/amp_board.c + common/amp_vendor.c compose both with the I2C session into
oa_amp_init_io; check-amp-board/check-amp-vendor pass; ARM references only the
three exported vendor symbols. No module consumer, hardware run, paired
shutdown or audible acceptance. AMP-TRANSPORT.

Amp module 2026-11-07 (offline): audio_amp v0.1 publishes audio.amp:v1;
amp-init submits one Init to amp_owner, amp-state polls completion/caches.
audio_amp.enable=0 default; enable=1 needs plat ready, loads the profile, opens
/dev/i2c-3 and seeds the owner from the recovered aurora.elf cold-boot image
(vaddr 0x17640e8, FNV 0xab635b86). Quiesce unpublishes then joins/closes on a
worker. Host disabled/fail-closed lifecycle test, ARM build and TV `--check`
pass. First hardware runs 2026-11-07: a stale core run submitted nothing; with
the rebuilt core, real mod_plat + audio_amp ran, amp-init completed (`result 1`,
`reset 0`, `mute 1`), the init-muted amp NACKed the read-only probe, and the
new `amp-mute 0` restored I2C and readback `1b/05/05`. Stock runtime image
matched the embedded baseline plus configure patches. Audible/owner-listening
confirmation and paired shutdown remain. Restored by wdtctl set10: latest stock
appmgr1575/aurora1777, source5, profile/fb/backlight unchanged, no stage/
helper. AMP-MODULE. Amp shutdown 2026-11-07: the AD82587 vtable leaves Term
(+0x54), PowerDown (+0x68) and GetPowerDownStatus (+0x6c) NULL and Close is
empty, so this board has no vendor paired shutdown; DEV_AMP_Term/PowerDown are
no-ops. The module closes I2C on quiesce and does not invent a mute/reset on
stop; standby/amp-rail policy is a separate MCU/panel/power concern. Audible
confirmation remains. AMP-SHUTDOWN.
Amp balance 2026-11-07: recovered AMP_AD82587_SetBalance (regs4/5, 127/50
formula, 0/50/100 boundaries) and added amp-balance/set_balance with host
formula/CLI tests; mute/balance reject while Init is submitted. SetVolume is
reg3 prescale+readback, SoftMute reg2 0x0e, HP_Mute GPIO0xc3; unwired. ARM
mod_audio_amp 7334a236.... No hardware balance run. AMP-BALANCE.
Agent review 2026-11-07 (Grok): fixed CRITICAL MAP_FAILED authority mapping
(commit f323673, regression test), the HIGH authority lifetime across a
timed-out quiesce (keep the mapping until join succeeds; check-cec-authority-
lifetime), and the HIGH CEC suspend/resume state/worker join (clear session,
cleanup worker, resume reopens/re-acquires; check-cec-suspend-resume); mod_cec
c8f4138c.... Remaining high: amp mute/balance loop ordering, IR pipe/8-byte
record/bounded reap/CLOEXEC. AGENT-REVIEW.

Input/PVR group 2026-11-07: live settingsDB source5/previous4/volume0;
stock owns video28 BLAST, ntkdvif, video20/21/24, demux0 and DVB nodes.
Preflight found mod_ir missing -ldl on TV (fixed; libdl NEEDED). Two BLAST
runs hung the single host thread in blast_io_get_key; a poll-readiness gate
did not help, so both were killed and stock rebooted. Clean run with
ir.blast=0: plat/source/ir/pvr started; source-select 17 (vin3) then 0 (vin1);
PVR add id1/list/cancel state3; source stayed5. BLAST_RX now runs in the
killable oa-ir-blast helper, which owns /dev/video28 and writes fixed 6-byte
records to a pipe2 stdout; mod_ir emits on the loop and kills/reaps the child.
Host fake-blocking test passes. Hardware ir.blast=1: host main
poll_schedule_timeout, child oa-ir-blast blast_io_get_key; source/PVR commands
responsive; quit clean, no helper; reload changed helper 6277->6300 and quit
clean. Packaging verified: all/core/stage build and manifest oa-ir-blast,
install puts it in BINDIR, config `ir.blast_helper=oa-ir-blast` resolves via
/proc/self/exe. Physical remote press is still unverified. Final restore by
wdtctl set10: appmgr1562/aurora1766, source5, fds/profile/fb/backlight
unchanged. PVR is the offline stub, not DVR hardware. INPUT-PVR-TAKEOVER /
IR-BLAST-HELPER.

audio_amp_profile on-TV ARM check, 2026-11-06: TV /conf/mb230_hwprofile.bin
md5 1b063b2cc7faa8b92c0f843921e561c7/24514B matches local. Staged hash-verified
build-tv-v3 openaurora + mod_audio_amp_profile.so in /tmp/oa-amp-verify, ran
with only that module and the real profile (cleanup_delay_ms400): amp-profile
prescale27, first reload failed/ENODEV, retry after0.7s ok/prescale27, stop
gate failed then completed after0.7s, unload/quit ok. Staging removed, no
helper left, stock Aurora still PID1781 (TV woken from standby). Proves ARM/
glibc module + gate + real profile data, not amp writes or audible output.
SERVICE-DB-QUIESCE.

Real VIN HAL simulation, 2026-11-06: run the shipped ARM libNDAL3.so
Hal_Vin wrappers and the real mod_source.so under qemu against an LD_PRELOAD
fake /dev/video20. Hal_Vin_Open(0) opens /dev/video20 (not /dev/ntkdvif);
SelectInput ioctl0xc0045627 (id3/id66), SetStreamStart ioctl0x40045612 (on1),
Close close. The real module's reload/stop gate closes before reopening.
libNDAL_EX supplies gs32Ndp_PQ_fd; vendor libs staged. check-vin-real. Not the
kernel driver, V4L2/HDMI routing or TV acceptance. VIN-DEVICE-SIM.

Stock Apps-grid pulse capture, 2026-11-06: read-only fb1 sampler over
Markt/Netflix (700x280, ~37ms effective) captured one redraw at t=53ms
(bbox 33,30 620x230) and 16 identical post frames through 621ms. Apps focus is
an instant redraw, not a 300ms scale/glyph pulse; do not wire the sublauncher
records there. Corrected BGRA→RGB PNGs; sublauncher/settings screen capture
remains. APPS-PULSE.

Partial GPU scene uploads, 2026-11-06: OGS1 FRAME rects + renderer ABI2 +
`display.gpu` v2 `frame_rect`; `scene_display` sends only dirty images. Host
protocol/patch/UI tests pass. TV: full upload 73 ms; ten rail frames uploaded
only `0,211 209x670`, ~20 ms request-to-done, ~50 ms cadence. Exact 30 ms,
CPU canvas, graceful teardown and cold startup remain. GPU-PARTIAL.

Control client, 2026-11-05: `oa-client` has a default 5 s timeout and
`--no-wait`; `mod_ir` keeps the UART closed when BLAST succeeds unless
`ir.device` is explicit. Silent-server test passes 1.02 s timeout / 0.00 s
no-wait. `mod_ir` also has the ABI 3 worker cleanup gate: a slow UART/BLAST
close returns EAGAIN, the first reload fails fast, and a retry completes; the
pty + 400 ms-delay test passes. Later gates: i18n, settings, ui_vendor,
ui_engine, display, source. OA-CLIENT-ASYNC. The host now also has additive
`load-batch`/`start-batch`/`stop-batch`/`unload-batch`/`reload-batch`: one
lifecycle operation per loop turn, an immediate `queued N` reply (64-entry
FIFO, `dropped M` on overflow), and `poll_once` skips its wait while entries
remain. Teardown/reload order is consumers first because a loaded consumer
keeps its provider hold. Fixture and real-socket tests pass; the scheduler is
not used because its contract forbids lifecycle calls. Limit: one slow
lifecycle call still blocks that turn.

Offline IR IPC, 2026-11-05: `tests/ir_ipc.cjs` now drives `mod_ir` over the
real Unix control socket with plat/recorder/ui_engine/ui_vendor fixtures
(`ir.device=`, `ir.blast=0`). It verifies `input:v1`, key navigation, stop
→ENODEV, start, `reload ir`, live `reload ui_vendor`, ui_vendor stop/start,
unload/load ir and clean exit. `make -j24 check-ir-ipc`,
`check-ui-vendor-ipc` and `check-ui-nav` pass; no TV/HAL device was opened.
The BLAST/v4l2 physical path remains TV-only.

Quick picture-settings offline capture, 2026-11-05:
`tests/ui_settings_quick.c` plus `make capture-settings-quick-real` paint the
workbench `settings_rail/top/cat` at 0,180/0,240/0,440 over (13,14,14). The
recorder snapshot is byte-identical to the on-TV fb1 quick-settings capture
(AE=0, max=0, 0/2,073,600 changed). Full settings panel, live text/focus,
Mesa/transformed frames and scanout remain open.

Full settings panel capture, 2026-11-05: `tests/ui_settings_panel.c` with
`OA_SETTINGS_REAL=1` uses the real workbench UI dir; `make
capture-settings-panel-real` clears transparent, blits the 780x1080 OAI1 panel
at 0,0 with no focus row, and compares the recorder crop to
`settings_panel.rgba` (AE=0, max=0, 0/842,400 changed). `OA_SETTINGS_SEL=0`
plus `make capture-settings-focus-real` compares the focused 20,186 750x66
row region to `settings_focus_0.rgba`: AE=0, max=0, 0/49,500 changed.
`OA_SETTINGS_TEXT=1` plus `make capture-settings-text-real` enables the live
labels/values/fonts and matches the full `settings_panel.rgba`: AE=0, max=0,
0/842,400 changed. Navigation panel ->
quick tiles -> panel -> portal still passes. Settings acquisition/editing,
other focused rows, animation, Mesa/transforms and scanout remain open.

Startpagina offline capture, 2026-11-05: `tests/ui_portal_quick.c` plus
`make capture-portal-real` paint the workbench title/rail/card assets with
focus rail2 and the default nine-card row. The recorder snapshot, composited
over black, is byte-identical to the on-TV fb1 portal capture (AE=0, max=0,
0/2,073,600 changed). `capture-portal-card-real` drives two Right keys and
matches the on-TV Prime Video focused-card capture (accent fill 394,473,
AE=0). Live extra cards, rail-highlight/title-slide animation and GPU/scanout
remain open.

Bronlijst offline capture, 2026-11-05: `tests/ui_sources_quick.c` plus
`make capture-sources-real` paint the 780x1080 source panel, focused row 0 at
18,152 and the workbench `src_*` assets. The composited recorder frame is
byte-identical to the on-TV `ui-sources.png` (AE=0, max=0, 0/2,073,600
changed). Row 0 only; other rows, animation/interaction and GPU/scanout remain
open.

Latest live acceptance: [LIVE-TV-ACCEPTANCE](artifacts/reports/LIVE-TV-ACCEPTANCE-2026-11-05.md)
staged the ABI 3 tree under `/buffer`, ran epg/tuner/pvr/lua, and loaded
`display_fb` on vendor-initialized fb1. It saved a 16,588,800-byte snapshot,
painted a magenta self-test band, and restored the blank pre-test frame after
the async cleanup retry. The full display_fb/ui_engine/settings/service_db/
image_decode/ui_vendor stack painted the "Startpagina" portal (5 rail items,
12 cards), hot-reloaded `ui_vendor` with state restore/repaint, and restored
fb1 on shutdown. The new `key` command fed real NEC frames through mod_ir
(UART empty), moving card focus, opening the Bronlijst source list and the
Beeld settings screen (full asset set); Exit returned home. NDAL tuning
returned EBUSY against stock; direct DVB
EIT opened but saw no tuned sections. Staging removed; stock PIDs 1967/1785
intact. Limits: temporary display binding, no cold startup/native scanout,
live EIT or DVR recording.

Offline scripting entry point: [openaurora/docs/LUA.md](openaurora/docs/LUA.md).
Module `lua` publishes `ui.lua` v1, one Lua 5.4.9 state shared by every skin.
It is not a skin. Host `check-ui-lua` and `check-ui-skin` pass, and the
leak-enabled ASan/UBSan `check-ui-lua` passes; full host `make check` is
434 ok and `verify.cjs` 45 groups. ARM/qemu `tests/ui_lua` passes
on the TV glibc; the http body itself is proven on the host, because qemu-user
does not exec host python or curl. `oa.aurora` is an opt-in, gated
`ui.lua.control` bridge for read-only queries or deferred control commands; no
raw shell/device access and no MCP listener. This does not accept on-set
startup.

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

Latest apps-grid parity:
[STOCK-APPS-GRID-PARITY](artifacts/reports/STOCK-APPS-GRID-PARITY-2026-11-05.md)
captures the real stock Apps page via DIAL navigation + read-only fb1 and adds
a stock-layout renderer mode. Manifest comparison improved from AE29.56% to
0.0629% (MAE8 75.38 -> 0.160; top 0.0035%, grid0.1125%, scrollbar0.0168%).
The tile scaler is the recovered GL_LINEAR_MIPMAP_LINEAR path (createTexture
0xb32eb0, setApplicationImage scaling type 1), implemented in common/canvas.c.
Host423 ok, verify45. Limits: exact vendor bitmap fonts; JPEG staging uses
TurboJPEG FASTDCT; Mali's mip kernel is box-assumed; residual per-cell
sampling/source differences; fb1 not HDMI/cold-start. Follow-up: a BACK AV
stock Twitter oracle, firmware one-row scroll (`first=10` for selection 22),
Markt insertion and EF41 font-4 focused labels; live-DB AE0.0619%/MAE8 0.158;
both focused labels are 2px left of stock (residual).

Latest channel runtime order:
[CHANNEL-RUNTIME-ORDER](artifacts/reports/CHANNEL-RUNTIME-ORDER-2026-11-05.md)
adds an ACTIVECHANNELLIST XML parser/module and makes ui_vendor prefer a
runtime snapshot with an amber LCN/visibility caveat, falling back to stored
order. channel-ingest/clear/state/list are socket-tested; host423 ok, verify45,
ARM/qemu passes. No live EASI feed (testtool still disabled) and no LCN/
visibility in the XML. Stock apps-grid pixel parity is blocked on an
owner-provided apps-grid screen or HDMI capture; the TV showed no-signal and
no keys were sent.

Latest PVR module: [PVR-MODULE](artifacts/reports/PVR-MODULE-2026-11-05.md)
adds a vendor-shaped PVR boundary (recording id, start/stop, index query) with
a persistent offline stub. mod_pvr stores up to 256 jobs atomically at
pvr.path, assigns monotonic ids, cancels only scheduled jobs and fails closed
on corrupt files; unit and socket tests cover reload and cross-process
persistence. Host421 ok, verify45; ARM/qemu passes. Stub only: no active
recording, timeshift/playback, storage/conflict engine or vendor metadata.
PVR follow-up: **v2** adds a fixture-only one-window-per-service padded
conflict rule (`EBUSY`), `pvr-record`/`pvr-stop` scheduled→recording→completed
transitions and persisted states; socket and ARM/qemu tests pass
(PVR-CONFLICT-STATE). Tuner-count/vendor conflict semantics and the real
recording engine remain open.

Latest live acceptance:
[LIVE-MODULE-ACCEPTANCE](artifacts/reports/LIVE-MODULE-ACCEPTANCE-2026-11-05.md)
is the first on-TV run of the ABI 3 ARM build: staged core/mod_epg/mod_tuner
with on-device hash verification, then real-socket load/start/scan/reload/
stop/unload. Tuner scan fed 12 EIT events into mod_epg and queries matched;
exact-PID shutdown/staging removal and unchanged stock pids were verified.
CPU-only: no fb1/display/HAL/device access, no testtool or cold startup.
Testtool TCP1986 remains disabled: the owner chose on 2026-11-05 not to enable
it now and to skip fb1/display until an ownership window. No profile/process/
watchdog/display state was changed.

Latest tuner module: [TUNER-NIM-MODULE](artifacts/reports/TUNER-NIM-MODULE-2026-11-05.md)
adds a vendor-shaped NIM boundary (DEV_NIM_DTV_StartTune/MonitorTune/signal)
with an offline stub backend. mod_tuner publishes `tuner`, runs the 25% scan
state machine with lock/signal metrics, maps DVB-T/C/S/IP (analog ENOTSUP) and
pushes synthetic EIT sections into `epg.ingest`. Host419 ok, verify45;
ARM/qemu passes. Stub only: no tuner hardware, transponder scan, demux/PID
assembly, real service discovery or live backend.

Latest EIT/EPG module: [EPG-EIT-MODULE](artifacts/reports/EPG-EIT-MODULE-2026-11-05.md)
adds a DVB EIT section parser (CRC/structure/time validation, short-event
descriptors) feeding the EIT cache through a real mod_epg. It publishes
`epg`/`epg.ingest`; control adds epg-ingest/epg-clear/epg-count, covered over
the Unix socket including key replacement, bad CRC, stale sections, reload and
stop. Host417 ok, verify45; ARM/qemu passes. Still no PID/EIT filters, section
merging, CRID/first-showing or live tuner source.
EPG-PERSISTENCE-2026-11-05 adds opt-in `epg.path` with atomic `OAEPG1`
load/save, `oa_eit_cache_get_at`, and corrupt-file fail-closed. Real-socket
reload/process-restart/clear tests and qemu `eit_cache` pass. Project store,
not vendor persistence; no filtering/CRID/retention, save per section, no TV.
EPG-NOWNEXT-2026-11-05 adds `epg-nownext <svc> <now_ms>` over the existing epg
v1 query: earliest current (`start<=now<end`) and earliest next (`start>now`),
`-` for missing. Real-socket boundary/clear/stop/invalid cases pass; ARM core
links. Project query over the 512-event cache; no new vendor ABI or TV.

Latest EIT demux: [EIT-TS-DEMUX](artifacts/reports/EIT-TS-DEMUX-2026-11-05.md)
adds a real MPEG-2 TS EIT demux (PID 0x12, continuity, PUSI pointer, split/
back-to-back sections, stuffing) and the `nim-ts-replay` backend selected by
`tuner.ts_path`. mod_tuner opens/closes the replay source and feeds
`epg.ingest`; unit and real-socket replay tests pass (epg-count 2), host423 ok,
verify45, ARM/qemu. Limits: replay file only; NIM tune/signal remains stub; no
live demux/hardware, CRID or persistence; TV work owner-gated.

Latest NDAL tuner backend: [NDAL-TUNER-BACKEND](artifacts/reports/NDAL-TUNER-BACKEND-2026-11-05.md)
adds a selectable `nim-ndal` frontend adapter: `/dev/dvb/...`,`FE_SET_FRONTEND`
DVB-T tune, `FE_READ_*` status/metrics, and `Nd_NIM_*` getters/setters from
`libNDAL_EX.so`; `tuner.backend=ndal`. Mock contract tests only; host423 ok,
verify45, ARM/qemu. Hardware lock/signal/scan, live EIT/PVR and cold start/
display remain owner-gated.

Latest DVB EIT/DVR boundaries:
[EIT-DVB-DVR](artifacts/reports/EIT-DVB-DVR-2026-11-05.md) adds a section-
filter EIT source (`oa_eit_dvb_ops`, `/dev/dvb/.../demuxN`, PID 0x12) attached
to `nim-ndal` via `tuner.eit_demux`, plus a mockable DVR sink
(`oa_dvr_ops`, open/write/flush/close). Mock tests, ARM/qemu and a new
Unix-socket `mod_tuner` reload/re-scan test pass; host424 ok, verify45. The
review of that commit, checked against the Ghidra listing of `libNDAL3.so`,
routes demux close through `Hal_Demux_Dvb_Close`, rejects a read that is not
one complete section, and closes a lazy EIT filter from `nim->stop` when the
frontend is not open. `nim_stub` and `pvr_stub` were re-run on host,
ASan/UBSan and ARM/qemu. Limits: mock only, PID-only filter, DVB-T tune only,
DVR sink not scheduler-driven; hardware acceptance/cold start/display remain
owner-gated.

Latest vendor-field contracts:
[BROADCAST-VENDOR-FIELDS](artifacts/reports/BROADCAST-VENDOR-FIELDS-2026-11-05.md)
extends tuner/EPG/PVR records with lock/strength/quality/BER/SNR/UNC,
CRID/first-showing/state/version/actual and repeat/padding; the fixture and
control socket expose them. Adds an in-memory EIT schedule cache keyed by
service+event with seconds start+duration query and now/next, built on
common/eit_time.c. Host415 ok, verify45; ARM/qemu passes. Still fixture/project
code: no vendor execution/hardware, CRID resolution, filters, persistence,
series/conflict handling or find/remove semantics.

Latest vendor ABI grounding:
[VENDOR-BROADCAST-ABI](artifacts/reports/VENDOR-BROADCAST-ABI-2026-11-05.md)
recovers the EIT time codec/range algebra, event key, ScheduleEvent schema,
querySchedule/ScheduleCache shape, DEV_NIM_DTV tuner paths and signal getters,
and DvrController/NebulaPvrRecordingScheduler recording APIs. Adds
common/eit_time.c with host and ARM/qemu tests; host414 ok, verify45. Static RE
only; contracts still need signal/CRID/scheduler fields. No TV access.

Latest broadcast CLI/socket: [BROADCAST-IPC](artifacts/reports/BROADCAST-IPC-2026-11-05.md)
exposes the fixture tuner/epg/pvr contracts over the control socket and tests
them end-to-end with the real binary: scan/select/cancel, EPG windows/caps,
PVR add/list/cancel/repeat, reload, stop->ENODEV and shutdown. Host413 ok,
verify45. Fixture backend only; no tuner/EIT/PVR hardware, vendor backend, UI
or persistence.

Latest broadcast foundation: [BROADCAST-FOUNDATION](artifacts/reports/BROADCAST-FOUNDATION-2026-11-05.md)
adds project-internal tuner/epg/pvr v1 contracts and a host fixture: scan state
machine, EPG overlap/bounds/caps, PVR add/list/cancel, reload/teardown. Host412
ok, verify45. Fixture only; no tuner hardware/scan, EIT/EPG semantics,
recording engine, production module, UI or persistence (CLI/socket added later
in BROADCAST-IPC).

Latest runtime probe: [EASI-PROBE](artifacts/reports/EASI-PROBE-2026-11-05.md)
implements the read-only testtool client (TCP1986, GETCHANNELLIST/
ACTIVECHANNELLIST XML) with fixture/ARM coverage. The TV testtool server is
disabled (`is_testtool_socket_enabled=0`, TCP1986 refused); enabling requires
profile+restart or pausing the live vendor process, deferred without physical
recovery. Active LCN/visibility and provisional UI replacement remain open.

Latest core-TV data: [SERVICE-DB](artifacts/reports/SERVICE-DB-2026-11-05.md)
adds a read-only settingsDB broadcast inventory (`service.list` v1) over
DVBSerTable/EASISerTable/AnalogSerTable: 525 dumped rows (445 DVB/67 EASI/13
analog), bounded text, table order, missing-table failure, CLI and real socket
lifecycle. Exact StoredDvbServiceData stored-flag predicates, stats and a
name/channel_no stored query with filters are exposed (dump totals visible67/
available338/encrypted67/numeric0/missing0). A read-only live DB fetch found
54 DVB/67 EASI/13 analog and m_lcn low16=0xffff; getLcn/isHiddenService use
runtime EASI, so active-list LCN/filter parity needs a runtime probe or an
explicit provisional-order decision. A provisional `ui_vendor`
`SCREEN_CHANNELS` (`ui.screen=channels`) now shows stored channel_no/name rows
with a visible "NOT RUNTIME EASI" marker; recorder tests with synthetic/real
font packs pass. Host410 ok, verify45. Production `system()` calls are removed
(display mdev uses posix_spawn/waitpid; test cleanup uses nftw). Runtime EASI
LCN/visibility, stock
channel-list key path/geometry/typography/scrolling, tuner, EPG/PVR and stock
parity remain open. No persistent TV write; tvssh sent WoL for the read-only
fetch. Cold bundle not rebuilt.

## Immediate continuation point

Current owner requested review/fixes and TODO verification after40f2f66. Read
[AGENT-REVIEW](artifacts/reports/AGENT-REVIEW-2026-11-05.md) for the direct
production/vendor decoder comparison, CLI dimension guard, missing second left
inset in grid button right edges and checked-milestone
audit. The prior stop below is superseded by that review request. It closed only
the video24 static trace, not the replacement goal. Audio continuation remains
[AMP-VIDEO-OWNER](artifacts/reports/AMP-VIDEO-OWNER-2026-11-04.md): extin's
actual fd open/close callbacks are simple; the mutating release_func is reached
through input selection. ELF .rodata relocations prove the callback binding
despite absent Ghidra refs. Private controls forward to EX_PLAT. Next trace
shipped libv4l2_fd_open before choosing a native backend, then coherent board/
fd/retained-image/worker/module lifetime. Do not retry cold MCU calls or infer
exclusive authority from static callbacks. New ghidra_extin project is durable.
That earlier video24 item changed docs/evidence only and had no runtime suite;
its metadata read followed WoL with no device/MCU/fb1 access or continuity sample.
The current review does include decoder CPU/lifecycle tests on TV and fresh
host/sanitizer/ARM checks; staging removed/no helper, boot/vendor pair/no-reset
retained. No framebuffer was used. See AGENT-REVIEW for identities and limits.

[DISPLAY-CLEANUP](artifacts/reports/DISPLAY-CLEANUP-2026-11-04.md) fixes display
snapshot failure discard. Suspend remains synchronous but returns errors and
retains state; stop/unload restore/unmap/close runs on an owning thread, with
nonblocking actual join in ABI3 quiesce. New failure and blocked-driver tests
pass both display variants on host/leak-enabled sanitizer/fresh ARM-qemu; full
host382 passes. TDGA teardown, NDAL_EX GPU close and kernel mapping/pan/PMI
wrappers were decompiled/checked against instructions: recovery/retry is project
policy, not a cloned vendor lifecycle. [DISPLAY-DSO](artifacts/reports/DISPLAY-DSO-2026-11-04.md)
now adds actual host/loader/control-dispatch hold, blocked/forced reload,
deferred resume, partial-start failure and shutdown tests for display_fb. Five
groups pass host/leak-enabled sanitizer/fresh ARM-qemu; full host387 passes.
GDB reproduced a completed-worker shutdown poll(-1) hang; shutdown now caps
poll at20ms so join gates retry without a later notification. No TV/fb1 use.
Next establish exclusive authority and native state equality when authorized;
external socket-client/display integration remains untested by this fixture.
This does not provide kernel power parity or move
ordinary start/draw/suspend off the loop. Read the report before more changes.

[AMP-INIT](artifacts/reports/AMP-INIT-2026-11-04.md) adds an injected blocking
AD82587 Init sequencer.29952 cases match actual Init/Reset/Mute instructions,
including present/absent/changing GPIO resources, combined errors, delay args,
cache writes and every I2C failure.45 total reference groups and host/sanitizer/
ARM/full host366 pass. Verifier defaults to build-tv-v3/amp-init.
[AMP-OWNER](artifacts/reports/AMP-OWNER-2026-11-04.md) now supplies the injected
owning thread: copied inputs/value completions, retained state, bounded slot,
drain and actual nonblocking join through TLS destructors. Four strict host/
sanitizer/ARM groups and full host370 pass; target tryjoin export/instructions
checked offline. The exact unit now also passed all four groups on the TV's
actual pthread/glibc after non-fb1 test authorization; staging removed, no helper,
boot/vendor PIDs unchanged. All I/O remains injected and no device/fb1 was
opened. Next establish native resource/image ownership and wire the
audio consumer/provider holds, completion polling, module gate and backends;
there is no module hookup or paired shutdown yet. Never run the blocking helper
in the host loop. No native amplifier bring-up or persistent deployment.
Create's private reset/mute cache seeds are0/1; its image is process-global
and not reset by Create. Preserve retained image across reload, rather than
assuming a fresh ELF baseline represents the running hardware/owner state.

[AMP-I2C](artifacts/reports/AMP-I2C-2026-11-04.md) now supplies the fixed MB230
bus3/address34 fd session and refactors amp-probe onto its read-only mode.
Fresh Aurora/NDAL3/PreInit decompilation/instructions preceded implementation.
Writes are explicit/injection-tested, including their lower10ms delay and
Init boolean adapter. Three host/sanitizer/ARM groups and full host390 pass;
native read03..05 returned1b/05/05 after verified staging. File removed/no
helper, boot/vendor PIDs unchanged, no fb1 access or audio writes. Exact-count
rejection is an explicit stricter error policy than vendor HAL. Next recover
coordinated GPIO/MCU/device/image lifetime and wire the owning consumer/module;
fd ownership alone is not amplifier authority or paired shutdown.

[AMP-GPIO-QUERY](artifacts/reports/AMP-GPIO-QUERY-2026-11-04.md) recovers a
getter hazard before implementing direct114 GET670d/670f: HAL GetDirection
marks the pin open, and ordinary HAL Read exports via sysfs. Use the maintained
query instead; no claims/setters/global GPIO init/close or MCU access. Two
host/sanitizer/ARM groups and full host392 pass; native query returned level1/
cached kernel direction0(output), verified staging removed/no helper and boot/
vendor PIDs unchanged. No fb1 use. Direction/latch are cached/selected values,
not voltage or exclusive ownership. Next establish coordinated GPIO/MCU and
paired lifetime before mutators/native amp bring-up. AGENTS records the trap.

[AMP-STBC](artifacts/reports/AMP-STBC-2026-11-04.md) now traces the MCU side
through NDAL_EX helpers and kernel EX_PLAT/DRV_STBC calls. Getter has three
arguments and two raw output bytes, distinct from its device-success status.
Injected fixed-MCU8 codec passes two host/sanitizer/ARM groups/full host394;
no native fallback, MCU/TV/fb1 call or ownership claim in that milestone.
AMP-VIDEO-OWNER above now recovers driver callbacks; full wrapper behavior and
coherent authority remain open before connecting a native transport: the
vendor OpenDevice(type10) maps video24 and uses mutable process-global state.
The new NDAL_EX Ghidra project is durable/catalogued. Do not infer prior reset
causes from ABI recovery or call the getter with only one output pointer.

[AMP-PROFILE-MODULE](artifacts/reports/AMP-PROFILE-MODULE-2026-11-04.md) now
exposes the recovered amp inputs through reloadable audio.amp_profile v1 and
amp-profile CLI/Unix query. DSO holds/removal/failure/fd/suspend/reload cases
pass host/sanitizer/ARM; real host/ARM socket tests include the saved profile;
full host361 passes. Next wire the audio consumer and retained image owner,
then coordinated worker/GPIO/MCU/I2C lifecycle. Provider performs no hardware
operations; no TV access or deployment. Owner offline-only restriction remains.

[AUDIO-AMP-PROFILE](artifacts/reports/AUDIO-AMP-PROFILE-2026-11-04.md) now
decodes actual hwprofile inputs into caller-owned amp snapshots; defaults,
string/legacy power and malformed-input policies come from firmware. Host/
sanitizer/ARM corpus units and full host356 pass;44 reference groups include
2312 native payload cases. The verifier defaults to build-tv-v3/amp-profile.
Next connect a reloadable data provider/consumer and retained image ownership
before worker/GPIO/MCU/I2C lifecycle. All work remains offline; no TV access.

[AUDIO-AMP-NATIVE](artifacts/reports/AUDIO-AMP-NATIVE-2026-11-04.md) now compares
the image helper against the complete shipped Init body:6000 profiles and all38
write failures match under ARM/qemu;43 total reference groups pass. Hardware
callees are controlled and GPIO descriptions absent. No helper correction or
TV operation. The maintained verifier requires this group and defaults to the
fresh audio-amp-native build. Continue with actual profile/retained-image
acquisition and worker/backend lifecycle; audio hardware acceptance stays open.

Latest stock parity: [STOCK-PARITY-2026-11-05](artifacts/reports/STOCK-PARITY-2026-11-05.md)
corrects the blue-shifted P1 stock portal and adds
openaurora/tools/capture_compare.py (explicit BGR correction, regions, JSON;
reproduces prior AE/RMSE exactly). A fresh apps-grid comparison is prepared
with apps-grid-regions.json and `make compare-stock-apps-grid`, which refuses
until artifacts/protocol/stock-apps-grid.png exists. Host404 ok, verify45. No
stock apps-grid capture, pixel parity or TV access. Reachability audit:
navigation code 0x80 constructs FullScreenPortalDialog; senders are
doNavigateFullScreenMarket (0xd1aa2c, market return 0x29a) and
OperationFinder::handleOperation case 0x79 (0xd3e6a0), so the screen is
compiled with conditional entry paths; a working stock key path remains
unverified on this model. The +0x1fc/+0x200 contradiction is resolved:
RectHandler transforms named rectangles before storage; rowPos is the
transform's own mask, so stored rowPos is the cell surface and the fields
reduce to zero-width horizontal masks, not -1015/+1005. check-vgrid covers
first/last cells and a nonzero mask. Operation-0x79 UI mapping and stock
capture remain open.

Latest plat consumers: [PLAT-CONSUMERS-SOCKET](artifacts/reports/PLAT-CONSUMERS-SOCKET-2026-11-05.md)
starts audio/source/ir/panel under a host-only plat fixture with hardware paths
disabled. Services, plat hold refusal, dependency-ordered provider reload,
consumer reloads and unload/load reuse pass in five consecutive runs. Host403
ok, verify45. Fake plat only; real HAL/devices, hardware-module reload and cold
startup remain.

Latest graphics: [GRAPHICS-SOCKET](artifacts/reports/GRAPHICS-SOCKET-2026-11-05.md)
runs real graphics + scene_display over the socket with oa-scene-broker and the
fixture renderer. display.gpu/display/alpha/async services, reload,
hold-refusal, dependency-ordered reload, broker child replacement/reap and
unload/load reuse pass. Host402 ok, verify45. Fixture renderer only; real
EGL/Mali, physical fb1 and cold startup remain.

Latest fake fb: [FAKE-FB-SOCKET](artifacts/reports/FAKE-FB-SOCKET-2026-11-05.md)
runs the real mod_display_fb over the socket with an LD_PRELOAD fake /dev/fb1
and recovered region/MAU/pan ioctls. A self-test paint changes yoffset/region/
pixel FNV; suspend, stop, start/reload and unload/load/start all restore the
initial yoffset, MAU, eight region words and pixel hash. Host401 ok, verify45.
User-space fake; real kernel/TDGA, physical fb1, vendor coexistence and cold
startup remain.

Latest fixtures: [FIXTURE-MODULES-IPC](artifacts/reports/FIXTURE-MODULES-IPC-2026-11-05.md)
runs ui_theme, ui_profile, audio_amp_profile, audio_curve and audio_mixer over
the real socket with synthetic BinSVG/EFSL/AQ fixtures, extracted hwprofile and
host broker. Services, reloads, stop/start or unload/load and clean exit pass.
Host400 ok, verify45. Host fixtures only; hardware modules, physical fb1 and
cold startup remain.

Latest skin socket: [SKIN-SOCKET](artifacts/reports/SKIN-SOCKET-2026-11-05.md)
wires the existing skin_socket.sh test into make check with recorder display,
ui_engine, ui_vendor and ui_omarchy. Missing/invalid startup skins fall back to
vendor; runtime omarchy/vendor switching, enter-failure fallback and clean quit
pass. Host399 ok, verify45. Recorder-only; physical fb1 and cold startup
remain.

Diagnostic skin: [UI-DIAG](artifacts/reports/UI-DIAG-2026-11-05.md) adds the
reloadable `ui_diag` skin over ui.engine. It probes 12 project services,
paints green/red rows with details, supports Up/Down/Select refresh and
module-change repaint, and uses optional EF41 text with bar fallback. Recorder
rows/selection/reload/teardown pass; `skin_socket.sh` switches
omarchy→diag→vendor; ARM module builds. Fixed probe list, recorder-only; no
TV/HAL/cold acceptance.

Full-stack rehearsal: [FULL-STACK-REHEARSAL](artifacts/reports/FULL-STACK-REHEARSAL-2026-11-05.md)
runs the exact deferred takeover module order offline: plat, recorder display,
settings, service_db, image_decode, ir, ui_engine, ui_vendor; live dump DB and
workbench UI; key 22/21, `reload ir`, `reload ui_vendor`, reverse teardown.
`make -j24 check-full-stack-ipc` passes. No UART/BLAST/fb1/HAL/TV; the TV run
still needs explicit authorization.

Latest matrix: [MODULE-MATRIX-IPC](artifacts/reports/MODULE-MATRIX-IPC-2026-11-05.md)
runs ticker -> ui_clock -> ui_anim plus ui_navigation, ui_list, power and cec
over the real socket with no display/HAL. Nav/list queries and dependency-
ordered reloads pass, including CEC's async cleanup retry. Host398 ok,
verify45. Hardware modules, physical fb1 and cold startup remain.

Latest host stack: [UI-STACK-IPC](artifacts/reports/UI-STACK-IPC-2026-11-05.md)
runs settings/launcher.db + image.decode + recorder display + ui_engine +
ui_vendor over the real `--sock`/stdin control plane. Launcher rows, decode
hash, ui-state and dependency-safe reloads of all five providers pass. Host397
ok, verify45. Non-HAL only; physical fb1/full hardware module set/all-consumer
holds/cold startup remain.

Latest host lifecycle: [UI-VENDOR-IPC](artifacts/reports/UI-VENDOR-IPC-2026-11-05.md)
adds `ui-state` and drives mod_nav_display (recorder display), ui_engine and
ui_vendor over the real `--sock` control plane and stdin through
start/use/stop/start/reload/unload/reload, including a display-provider reload
and consumer restart. No TV/HAL/framebuffer; host396 ok, verify45.
Recorder-only; physical fb1, full module set/holds and cold startup remain.

Latest image lifecycle: [IMAGE-DECODE-IPC](artifacts/reports/IMAGE-DECODE-IPC-2026-11-04.md)
drives image.decode through the real `--sock` control plane and stdin with an
empty autoload config: load/start, PNG/JPEG FNV, invalid errors, status,
suspend/EAGAIN/resume, stop/ENODEV/start reuse, reload and unload/reload all
pass. No display/HAL/TV module. Host395 ok, verify45. Limits:
ui_vendor/display reload, held-provider rejection and cold startup remain.

Latest offline differential: [VENDOR-DECODER-DIFF](artifacts/reports/VENDOR-DECODER-DIFF-2026-11-04.md)
runs the shipped PngImageDecoderImpl/JpegImageDecoder under qemu through an ARM
LD_PRELOAD probe before vendor main, with requested RGBA ImageProperties.
Nine corpus PNGs plus gray1/2/4/8, gray+alpha, palette, palette+tRNS, RGB16,
RGBA16, interlaced and gAMA variants are byte-exact RGBA; DMR JPEG and a
progressive JPEG are byte-exact against TurboJPEG `TJFLAG_FASTDCT`, now used by
image.decode. ImageMagick accurate DCT differed by up to8. A generated CMYK
JPEG is rejected by both classes; two available real foxxum JPEGs are exact.
The probe is tests/vendor_decoder_probe.c and the checker is
tests/vendor_decoder.py. No TV/HAL/UI. Remaining two foxxum files, YCCK/APNG,
worker/frame lifetime and stock parity remain.

Latest offline analysis: [STOCK-PARITY](artifacts/reports/STOCK-PARITY-2026-11-04.md)
confirms the old `shot-launcher-right.png` is R/B-swapped; the corrected copy
matches the current Prime/Canal color orientation. Same-geometry portal cards
still differ in artwork/crop, background color and missing status chrome, so
the old capture cannot certify stock apps-grid pixel parity. A fresh
same-session corrected stock apps-grid capture remains required. No TV access.

Latest offline implementation: [FB-SNAPSHOT-COLD-BUNDLE](artifacts/reports/FB-SNAPSHOT-COLD-BUNDLE-2026-11-04.md)
adds fb1 snapshot/restore (pixels/var/fix metadata, 8-word region, read-only MAU,
pan) and wires `display.restore_snapshot=1` into display_fb; fb-restore uses
an FBR2 file. Fake-fb library/tool/actual-display tests pass, host354 ok,
focused ASan/UBSan clean. `make cold-bundle` builds and manifest-verifies an
ARM bundle with25 modules, oa-supervise, fb-restore and a cold config. Nothing
was executed on the TV; live driver equality, cold startup and MCU/watchdog
authority remain unverified. [FB-RESTORE-REVIEW](artifacts/reports/FB-RESTORE-REVIEW-2026-11-04.md)
now fixes changed backing/layout acceptance, skipped region/pan after mmap
failure and false backup-file save success on close failure. Strict host,
leak-enabled sanitizer and fresh ARM-qemu library/tool/both-display tests pass;
full strict host372 passes.
No TV access in this review; fb1 remains excluded. Mode/allocation/MAU are not
restored. DISPLAY-CLEANUP above supplies retained failures and owning cleanup
for stop/unload; DISPLAY-DSO covers controlled host/loader cases, while native
acceptance remains open.
Do not put blocking restore in the ABI3 gate. The historical cold bundle needs
rebuilding before any future authorized experiment and has no native acceptance.

Latest visible test: [VISIBLE-IMAGES](artifacts/reports/VISIBLE-IMAGES-2026-11-04.md)
ran under the preceding TV-access authorization. The supervised no-plat
candidate painted the live apps grid on vendor-initialized fb1 from an empty
ui.dir and the real /buffer/downloaded_images:18 items,15 visible, scrollbar
0/600, including two visible JPEG foxxum cards (384x216) and13 PNGs; JPEG
crops match nearest-scaled originals within≈2.1%/1.1% AE. fb1 pixels/yoffset
were saved/restored, bundle and ready files removed, /proc scan0, vendor PIDs
1967/1785 intact. Supervised only; full region/MAU/pan ownership and
independent startup not proven. The owner then revoked all TV access
("Don't touch the TV"); no TV command ran after the override. Independent
startup is blocked until explicit reauthorization.

Audio prerequisite: [AUDIO-AMP-IMAGE](artifacts/reports/AUDIO-AMP-IMAGE-2026-11-04.md)
adds pure AD82587 profile-to38-register conversion with retained-image semantics.
Host/sanitizer/fresh ARM units and full host349 checks pass; comparison is static
ARM evidence, not execution of vendor Init. Continue with native differential,
actual profile/image acquisition and worker/GPIO/MCU/I2C ownership. Open/Close
are object!=NULL no-ops; ResumeDevice task creation/flag clearing do not prove
amp init succeeded. No TV operation or audio/amp lifecycle gate closed.

[HOST-CONFIG](artifacts/reports/HOST-CONFIG-2026-11-04.md) fixes the unchecked
global-config bug independently of the ongoing PNG/JPEG edits. Configuration
now belongs to each host, with existing first-wins/snapshot policy. Reproduced
before-fix assertion; strict host, leak-enabled sanitizers, fresh ARM/qemu and
54 core checks pass. No TV operation. Runtime config reload and a complete
host teardown API are not added. Continue PNG acceptance from its current
worktree/evidence rather than assuming the separate edits are already tested.

Read [TODO-REAUDIT](artifacts/reports/TODO-REAUDIT-2026-11-04.md) for the
history: PNG acceptance was reopened at172cd57. The current follow-up below
fixes the non-image/truncated/reload/suspend tests and provider-copy ownership
and moves PNG to the classic libpng conversion sequence. A vendor-class
differential, APNG/CMYK policy, worker scheduling and on-TV decode remain open.
Recorder alpha is verified, but its captures do not apply scene transforms.

Latest implementation: [IMAGE-DECODE](artifacts/reports/IMAGE-DECODE-2026-11-04.md)
now publishes image.decode v2. PNG uses the shipped libpng1.6 classic API with
Aurora b30918 setConversions; JPEG uses the vendor libjpeg-turbo2.0.1 TurboJPEG
API. Nine real RGB/RGBA/colormap400x225 PNGs and two real vendor JPEGs (48/120)
match independent ImageMagick SHA256/FNV exactly; truncated/non-image inputs
reject, suspend returns EAGAIN and reload republishes. ui_engine keeps OAI1
first, tries PNG then JPEG in ui.download_dir (default
/buffer/downloaded_images), copies decoded pixels then releases the provider
under hold. check-ui-png/jpeg-cards verify source dimensions and first pixels.
Host395 ok, focused decoder/UI ASan/UBSan clean, decoder/UI ARM-qemu pass with
vendor libpng/zlib and libjpeg-turbo. Tools/boot autoloads image_decode.
Limits: actual foxxum downloaded JPEGs were not copied offline; APNG/CMYK/YCCK
reject; the vendor-class differential is separate and covers PNG variants/JPEG
fast-DCT; worker, on-TV decode/display and stock pixel parity remain. No TV
access after the owner stop instruction; all decode/UI evidence is offline.

Latest checklist audit: [TODO-AUDIT](artifacts/reports/TODO-AUDIT-2026-11-04.md)
reviews all182 previously checked items. Its one reopened item, recorder
transparent RGBA fidelity, is now closed: tests/ui_nav_display.c uses
oa_canvas_over and publishes display.alpha; check-ui-sources checks transparent
clear, opaque panel and partial composition, with host273 ok, focused
ASan/UBSan and ARM/qemu. Evidence is in
artifacts/analysis/alpha-recorder-2026-11-04/. This is production-canvas
fidelity, not vendor-Mali pixel/scanout parity. AGENTS.md now requires a TODO
item to be implemented, tested and firmware-compared for parity work before it
is checked. Remaining checks are bounded research/helper/test/historical
milestones, not complete TV features; stale missing-helper/scanout/version/count
claims were corrected. Full host,42 ARM firmware groups, host/ARM font/theme
corpora and Mesa checks pass. No TV access or deployment in this audit. The
claim inventory has per-item dispositions and source/test/evidence locators.
Implementation next steps remain the native integration/visual/ownership gates
below; do not repeat completed lower-helper recovery.

Latest implementation review: [WORK-REVIEW](artifacts/reports/WORK-REVIEW-2026-11-04.md)
checks cf78486..4ba1599. Fixed scrollbar signed overflow, first/second-window
query ordering and the mistaken two-orientation cache model: +6c is the four
SVGWindow states, now32 skin/36 render native cases plus14,576 geometry/state.
Real shipped division replaces both new arithmetic stubs. Removed advertising/
tracking SQL fields and propagation; native comparisons explicitly separate
that privacy deviation from ordinary-field parity. Fixed image+0xb0 byte truth,
callback guards, apps reload/stop-start state, sorting before the64-card cap,
hidden-scrollbar raster and insufficient shared icon cache. Full strict host,
focused ASan/UBSan and42 ARM/qemu groups pass; focused TV CPU/read-only DB/loader
checks pass, staging removed/no helper, no-reset retained. No new draw or
takeover. See MEMORY and the report for limits. Next: native widget/PNG/live
resources and stock visual/interaction acceptance; independent startup still
needs an evidenced ownership/watchdog/fallback and restoration design.

Previous: [OA-CANDIDATE](artifacts/reports/OA-CANDIDATE-2026-11-04.md) adds
mod_display_fb (display.c with -DOA_DISPLAY_NO_PLAT) and binds a candidate
openaurora host to the vendor-initialized fb1 without plat. The host ran
settings,display_fb,ui_engine,ui_vendor with the live
/conf/sqlite_db/settingsDB.db and ui.screen=apps; it logged
"painted apps grid: 18 items, first 0, scrollbar 0/600" repeatedly.
candidate-apps_1.png is a real 1920x1080 fb1 capture of the apps grid and
scrollbar. Vendor Aurora stayed owner; the candidate was stopped,
/buffer/oa_candidate and /tmp staging removed, and the stock no-signal screen
verified. This is the supervised display/scanout binding, not a cold
independent startup. WORK-REVIEW withdraws the inference that a historical
FreshWDT return1 proves rejection or makes native watchdog maintenance
impossible. Vendor stop/watchdog disable remain unauthorized; the native
ownership/watchdog/fallback contract is unverified. The stock screenshot is
not full state-restoration proof. No cold takeover was executed.
Real downloaded-PNG decoding, stock apps-grid pixel parity and
independent startup remain open. No persistent change.

Previous: [UI-APPS-GRID](artifacts/reports/UI-APPS-GRID-2026-11-04.md) adds
SCREEN_APPS (5, ui.screen=apps) to ui_vendor: the live parent-0 type-2
LauncherItem rows page through the 183,180,1554x720 grid of 5x3 310x230
cells; top strip 0,0,1921,162 #481730; scrollbar 1750,180,8x720.
paint_apps feeds the recovered oa_svg_scrollbar_render_background/render_bar
ops into a rasterizer (track #811924, bar #FFFFFF, 20/16 caps, min 36;
geometry from oa_svg_scrollbar_calculate). Recorder tests: synthetic 20 rows
-> 15 cells/bar 540; dumped 18 rows -> 15 cells/bar 600; offline and TV-CPU
(ARM) rasters are in artifacts/analysis/ui-apps-grid-2026-11-04/. The visible
scene-module-probe apps mode confirmed `PORTAL hold 6s screen=5 apps=1`, but
the layer-1 capture returned the vendor "Geen signaal" frame and the probe
restore refused after the vendor region changed; the stock screen was verified
and the stale backup removed, so no visible apps scanout is claimed. There is
no stock apps-grid capture; the chrome/cell fitting/scrollbar colors are a
geometry/resource reconstruction. Display/scanout binding, independent
startup, the live-DB visible run and real downloaded-PNG decoding remain open.
Host check 265 ok, verify.cjs 42 ok; all /buffer and /tmp staging removed, no
helper, no persistent change.

Previous: [SVG-SCROLLBAR-RENDER](artifacts/reports/SVG-SCROLLBAR-RENDER-2026-11-04.md)
implements renderBackground (0x10c3658) and renderBar (0x10c3bac) as
ops-driven sequences. Both cache the supplier image per orientation, resize
the background (this+0x98) or bar (this+0xa4) surface, configure the
top/bottom masks with align3/4, draw gTopM/gBotM/gRowM (or
bar_gTopM/bar_gBotM/bar_gRowM) under the mTopM/mBotM/mRowM (bar_m*) masks,
and stretch the row rect {0,top_cap,width,height-bottom_cap}. renderBar calls
calculateBarSizeAndPosition first (two window-rect queries); an empty bar
clears both surfaces and clears +0xc8. The +0xc8 byte is background_rendered
(renamed from background_hidden): renderBackground sets it and
setScrollPosition renders the background only when clear. 8 native/C cases
compare sequence/cache/flag; host/ASan/UBSan, ARM/qemu, full verify.cjs
(42 ok), full make check (265 ok) and a focused TV CPU run pass. The vendor
mask/surface/renderer calls remain stubs, so this is a call/geometry
differential, not pixels: no apps-grid screen or scrollbar rasterizer is
wired and the 1750,180,8x720 scrollbar is not visible in ui_vendor.
Display/scanout binding, independent startup, the live-DB visible run and
real downloaded-PNG decoding remain open. No device/screen/settings/network/
boot change; TV staging removed.

Previous: [SVG-SCROLLBAR-SKIN](artifacts/reports/SVG-SCROLLBAR-SKIN-2026-11-04.md)
implements SVGScrollBar::calculateMinBarSize (0x10c397c). The standard widget
names are bar_gTopM/bar_gBotM (0x20/0x21) and bar_mTopM/bar_mBotM (0x22/0x23);
their BinSVG cap heights are 20 and 16, so the apps-grid minimum is 36 px. The
body caches the supplier image per orientation, leaves +0xb8 unchanged without
an image, and adds the two moving-cap heights through the renderer vtable
slot +0x14 and RectangleContainer. 16 native/C cases pass with host/ASan/
UBSan, ARM/qemu, full verify.cjs (41 ok), full make check (265 ok) and a
focused TV CPU run (unit 26996/94e26f39...). The geometry test now uses the
36 px minimum for the 20-item/15-visible apps-grid cases. The
renderBackground (0x10c3658)/renderBar (0x10c3bac) bodies, the apps-grid
screen with the 1750,180,8x720 scrollbar, display/scanout binding and
independent startup remain open; the home portal has no scrollbar and there is
no stock apps-grid capture. Live-DB visible and real PNG decoding remain
acceptance gates. No device/screen/settings/network/boot change; TV staging
removed.

Previous: [SVG-SCROLLBAR](artifacts/reports/SVG-SCROLLBAR-2026-11-04.md)
implements SVGScrollBar::setScrollPosition (0x10c4138) and
calculateBarSizeAndPosition (0x10c3af0). The state stores total/visible/first
at +0xbc/+0xc0/+0xc4, rejects not-created or first>total, gates the background
call on +0xc8 and always renders the bar on acceptance. The geometry uses
UIWindow::getWindowRect's bottom-top track height: size=height*visible/total
clamped to min_bar and height, position by first/(total-visible) or the
bottom, a full-height bar hidden, and the shipped negative-total preserve.
3120 native/C cases (2880 geometry+240 state) pass with host/ASan/UBSan,
ARM/qemu, full verify.cjs (40 ok), full make check (265 ok) and a focused TV CPU
run. The apps-grid scrollbar is 1750,180,8x720; the home portal has no
scrollbar. calculateMinBarSize0x10c397c (skin IDs 0x20/0x22/0x21/0x23 through
the SVG supplier/renderer/RectangleContainer), the background/bar render
bodies and the apps-grid screen remain open. Next: the scrollbar skin
rendering/apps-grid, then display/scanout binding and independent startup. The
live-DB visible run and real downloaded-PNG decoding remain explicit acceptance
gates. No device/screen/settings/network/boot change; TV staging removed.

Previous: [UI-LIVE-CARDS](artifacts/reports/UI-LIVE-CARDS-2026-11-04.md) makes
ui_vendor draw the live parent-0 LauncherItem rows from the reloadable
launcher.db service. Rows are filtered to item_type 2 with a non-empty
thumbnail name, sorted by m_order_index then id, and each card is loaded by its
named OAI1 asset. ui.cards still overrides; ui.live_cards=0 restores
DEFAULT_CARDS; missing service/DB/rows fall back. Recorder tests: synthetic
3 rows/3 visible, dumped and live 12 selected/9 visible on the TV (the dump has
20 parent-0 type-2 rows). The offline portal capture and a visible TV layer-1
capture show the stock nine-card order. The visible dumped-DB probe restored
bytes=16588800 yoffset=1080 MAU=0 region_match=1 snapshot_match=1. A live-DB
visible attempt reached the 6 s hold but captured the stock frame and the probe
refused restore after the vendor region changed; the stock no-signal screen was
verified and the stale backup removed, so no visible live capture is claimed
(the live rows were rendered in the recorder test). Staging used
/buffer/oa_portal_20261104 and /buffer/oa_recorder (owner suggestion) and /tmp;
all removed, no helper process, no-reset flag intact. The TV was asleep and
tvssh.sh woke it. Next: scrollbar/skin, display/scanout binding, independent
startup, live refresh and real PNG decoding. Host check 265 ok, verify.cjs 39 ok.

Previous: [LAUNCHER-DB](artifacts/reports/LAUNCHER-DB-2026-11-04.md) implements
the settingsDB LauncherItem read half. The member map
(launcher_item_info_struct0xfabf50) gives 16 scalar and 10 text columns
(300/60/150); the shipped copyLauncherInfoToItem0xfac334 decode is implemented
and differentially verified (8 cases). common/launcher_db.c reads the table
read-only with the getAll/by-parent plus deleted/out-of-sync semantics;
NovoLauncherDataProvider::getLauncherItems0x107a0c4 is only a vtable forward.
mod_settings publishes a reloadable launcher.db service and the
launcher-items CLI. The dumped DB has50 rows; the live TV probe read-only
reported54 rows. Host, ASan/UBSan, ARM/qemu, verify.cjs39 ok, full make check
(265 ok) and four focused TV runs pass; staging hash-verified/removed. The
shipped SequentialQuery/MemberAccess engine is not ported, and ui_vendor still
draws static DEFAULT_CARDS. Next: feed these rows into ui_vendor, then
scrollbar/skin, display/scanout binding and independent startup. The TV was
asleep and tvssh.sh sent WoL for the read-only runs; no
device/screen/settings/network/boot change.

Previous: [LAUNCHER-ITEM](artifacts/reports/LAUNCHER-ITEM-2026-11-04.md)
implements LauncherItem::LauncherItem(NovoLauncherItem const&) (0xe4794c,
Thumb, 500 bytes), the settingsDB row to UI object conversion used by
LauncherDialog::getLauncherItemsList. It copies 17 word fields and 4 bytes,
constructs 10 UStrings plus one default, copies the shared_ptr pair with a
control+4 refcount increment, initialises +0x94=-1 and +0x98..+0xa0=0, and when
the thumbnail name at source +0x3c is non-empty builds
"/buffer/downloaded_images/" + name at +0x2c. The body does cbz on isEmpty, so
the path block runs for non-empty names; the differential caught an inverted
first reading. 8 native/C cases compare trace, 164-byte output and refcount;
host, focused ASan/UBSan, ARM/qemu, full verify.cjs (38 ok) and full make check
pass, plus a focused real-TV CPU run (exit 0, staging hash-verified and
removed; the 32-bit refcount branch is asserted there). The settingsDB
adapter/query and NovoLauncherDataProvider are still unimplemented, so this is
the conversion half of live resource loading. Next: the DB adapter/provider,
then scrollbar/skin, display/scanout binding and independent startup. No
device/screen/settings/network/boot change.

Previous: [SVG-WIDGET-STATE](artifacts/reports/SVG-WIDGET-STATE-2026-11-04.md)
implements the widget-state qualification renderItems uses on every child:
SVGWindow::setFocus (0xeeebd4, state at +0x6c: 0->1/2->3 on, 1->0/3->2 off),
SVGWindow::setActive (0xeee4ec, inverted argument: 0 activates 0->2/1->3,
non-zero deactivates 2->0/3->1), SVGTextBox::setFocus (0x10c8a28, redraw and
text-slide only on a state change when UIWindow::isCreated) and
setElipses (0x10c6f48, mode 5 ignores switching on but off still stores at
+0x1b1). 36 native/C cases compare ordered call traces and the resulting state
after 0xAA poisoning against the four verbatim Thumb bodies; vendor
redraw/slide/isCreated calls remain stubs. Host, focused ASan/UBSan, ARM/qemu,
full verify.cjs (37 ok) and full make check pass, plus a focused real-TV CPU
run (exit 0, staging hash-verified and removed). Next: live settings resources,
scrollbar/skin, display/scanout binding and independent startup. No
device/screen/settings/network/boot change.

Previous: [SETTINGS-ROW-CONSTRUCT](artifacts/reports/SETTINGS-ROW-CONSTRUCT-2026-11-04.md)
implements SettingItemsListBoxRow's constructor (0xd53d0c, Thumb, 0x4cd
bytes): base SVGListBoxRow +17 children at fixed offsets, 10 UStrings, and 47
default fields/name-id defaults; the OSD-mirror probe rewrites 0x2e4c/0x2e50
to120/119. 96 native/C checks (2 mirror states x48) compare the ordered
call trace and the default field bytes against the verbatim body; child
constructor internals remain stubs. Host, focused ASan/UBSan, ARM/qemu, full
verify.cjs (36 ok) and full make check pass, plus a focused real-TV CPU run
(exit 0, staging hash-verified and removed). The constructor is Thumb, so
objdump needs force-thumb. Next: child visibility internals and live settings
resources, then scrollbar/skin, display/scanout binding and independent
startup. No device/screen/settings/network/boot change.

Previous: [SVG-IMAGE-POINT-IDM](artifacts/reports/SVG-IMAGE-POINT-IDM-2026-11-04.md)
implements the remaining SVGImage::create(int,EASIPoint const&,UIWindow*)
overload (0x10c9554) and LauncherHelper::convertOperationIdToIDM (0xe4ae0c).
The point overload does setId, the supplier resource gate, rect x/y from the
point, then with a parent three getClientRect calls for the client origin and
clamping of the resource size words (+8/+12) to client.w-x/client.h-y, or
without a parent adds the sizes directly, then imageCreate. IDM is a 62-entry
signed-16 table at 0x14efd84 with -1 for unknown operations. svg-image is now
80 native/C cases (8 new) and the launcher group 536 comparisons (67 new); the
bodies (CRCs 0x09f01190/0xf2156f40) are copied verbatim, and the EASIRect
ctor fragment is copied too. Host, focused ASan/UBSan, ARM/qemu, full
verify.cjs (35 ok) and full make check pass, plus a focused real-TV CPU run
(exit 0, staging hash-verified and removed). Vendor callees remain stubs.
Next: row construction, child visibility internals/live resources,
scrollbar/skin, display/scanout binding and independent startup. No
device/screen/settings/network/boot change.

Previous: [SVG-IMAGE-CREATE](artifacts/reports/SVG-IMAGE-CREATE-2026-11-04.md)
implements SVGImage::imageCreate (0x10c9430): set parent window/window rect;
created resizes the UI surface, takes targetHandle, draws with field words
0xa8/0xac/0xb4/0xb8/0xbc and releases; uncreated creates and sets the surface,
takes targetHandle, chooses the scaled draw by byte 0xb0 or the full draw,
releases and calls UIWindow::create. The svg-image differential is now72
native/C cases (16 new) against the verbatim body copied at its original
address; three targetHandle BLs are retargeted into the body page because
0x1149000 is held by an earlier group. Host, focused ASan/UBSan, ARM/qemu,
full verify.cjs (35 ok) and full make check pass, plus a focused real-TV CPU
run (exit 0, staging hash-verified and removed). Vendor surface/draw calls
remain stubs. Also found: the EASIPoint create overload at 0x10c9554
(CRC 0x09f01190) is unimplemented. Next: that overload, then
convertOperationIdToIDM, row construction, child visibility/live resources,
scrollbar/skin, display/scanout and independent startup. No device/screen/
settings/network/boot change.

Previous: [UI-FB-SIM](artifacts/reports/UI-FB-SIM-2026-11-04.md) now also covers
offline layout capture. The host test display recorder rasterizes its
clear/fill/blit calls into 1920x1080 RGBA frames when `OA_NAV_CAPTURE` is set,
writing numbered present frames; `check-ui-settings-panel` additionally writes
a `.panel` snapshot. `artifacts/analysis/fb-sim-2026-11-04/ui-layout-portal.png`
is the captured portal from the real ui_vendor draw calls, no TV or GPU. The
synthetic panel test paints no background fill, so its panel snapshot is black;
a real-asset preview needs the captured panel exported as OAI1 plus a real
ui.dir, and GPU composition still needs the Mesa-wrapped renderer DSO. This is
layout evidence, not scanout or vendor-render parity. The pending firmware
feature remains SVGImage::imageCreate; no device/screen change, TV untouched.

Previous: [UI-FB-SIM-2026-11-04 original offline GPU/fbdev assessment](artifacts/reports/UI-FB-SIM-2026-11-04.md) answers whether
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
surface S; back shares it (count 1), requests S (count2) or 2*S (count3).
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
AUDIO-CURVE-IPC-2026-11-05 adds read-only `audio-curve <volume>` and
`audio-curve-dump` control queries over the real socket (values 110/200,
reload 210, 101-entry dump). It remains data-only: no gain/amp/ALSA write,
mute/routing/balance, profile/default fallback or audible acceptance.
AUDIO-SPEAKER-MAP-2026-11-05 adds the pure
`PLAT_VAUD_SetSpeakerVolumeBalance` clamp/state mapping (level 100, balance
±50, HAL balance balance+50, equal left/right) and the read-only
`audio-speaker <level> <balance>` query. Unit and real-socket assertions pass;
the ARM core links. No speaker-id/HAL/ALSA/amp write, mute/routing or audible
acceptance.
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

Module base/stages + skin hot-switch, 2026-11-06: 36 modules categorised
hardware12/middleware11/utilities2/UI11. Core=base platform 10 (hardware access
+ settings/image_decode, no TV/UI); stage-bundle STAGE=tv|av|ui|dev|full is
15/19/31/34/36 with generated autoload, install-stage/cold-bundle-core, and
module-categories prints the taxonomy. Autoload parser is 64 names/511-byte
values (tests/autoload.cjs). Skin switching: `api->control("skin <name>")` from
a skin callback is deferred via skin_pending and executed by oa_skin_pump after
the stack unwinds; ui_omarchy/ui_diag demo key240, tests/skin_switch_ipc.cjs
covers omarchy->diag->omarchy. Full suite passes. MODULE-CONSOLIDATION,
SKIN-SWITCH.
