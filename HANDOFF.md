# Handoff — 2026-11-01

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

## Immediate continuation point

Owner resumed the goal and reaffirmed real display/hardware testing. Latest
milestone: [EGL-WINDOW](artifacts/reports/EGL-WINDOW-2026-11-01.md) proves direct
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
filter and progress-alpha fade into premultiplied BGRA. ui_fb now presents on
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
first-frame readiness before wiring ui_fb. Existing present() means commit,
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
consumer and its subsequent review are described above. Text-slide duration/margin/repeat are wired in mod_ui_fb with
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
