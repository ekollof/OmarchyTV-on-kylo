# Current project state — last verified 2026-11-01

This is the canonical state summary. Latest TV confirmation is the authorized
November 1 CEC session described below. Earlier chronological notes are preserved in
[the archive](artifacts/archive/README.md); they contain superseded claims.

## TV and persistent recovery

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
Caller wiring is now in mod_ui_fb as well: `SVGTextBox::isTextSlidingNeeded`
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
`mod_ui_fb`/`mod_ui_anim` link `-lm` (verified DT_NEEDED) because `sinf` was
undefined. The module now calls the v2 provider for duration/margin instead of
carrying a copy. Host, ASan/UBSan and ARM/qemu rerun after these fixes.
Known limits kept explicit: a ui.anim/ui.clock published only after ui_fb
starts is not discovered until restart, and a forced provider reload leaves the
module's borrowed pointers stale (the host blocks ordinary reload while held).

LIVE-SLIDE-PROBE-2026-11-01: the slide was exercised on the TV with real timers
and no display hardware: tests/mod_slide_probe.c published a recording display
plus a CLOCK_MONOTONIC-backed ui.clock, the real mod_ui_anim/mod_ui_fb ran from
/tmp with ui.anim_enable=1 (default-off bootstrap lever), size/SHA-256 checked
and staging removed afterwards. 914 samples at a mean 10.08 ms cadence; first
nonzero margin at +822 ms, final 163 at +3808 ms, repeat restart at +6557 ms
(= 4554 + 2000, the 0.99 threshold), clean exit. Vendor PIDs 1967/1785 unchanged
and no helper left. This is not visual scanout evidence; fb1, keys, settings and
vendor modules were not touched.

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
π/2-3/1-3/1-6 pool at 0xb32aa8. Encoded as `oa_fs_apply` (common/ui_anim.c) with
exact vectors for all eight types, both slide directions/vertex sets, clamping
and short/NULL arrays; nothing calls it from the renderer yet. Still open:
rail-specific duration, whole-scene composite application, frame sequences.

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
current matching16-module build adds audio_mixer. Review
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
| ui_fb assets/rendering | historical framebuffer comparison; not full navigation or independent display parity |
| animation firmware/extractor | UI-ANIMATIONS: 39 named descriptors; all 702 resource IDs/names CRC-verified; list/dialog controller is mod_ui_anim offline; text slide wired in mod_ui_fb; full-screen type/progress mapping encoded in oa_fs_apply; rail timing and renderer composite remain open |
| ui_clock / ticker paired hooks | FROST_Time arithmetic and reloadable UI time implemented; seven host/sanitizer/ARM-qemu DSO groups; absolute vendor clock phase unverified; ui.anim samples this clock |
| CEC/audio foundations | spec-based frame/status/ARC and identity/name/power validators; audio scalar ABI, prelude ownership, failure/cache/handoff fixes; six wire plus five fake-HAL groups pass host/sanitizer/ARM-qemu; transport/policy integration open |
| CEC packet/transport RE | raw/VPL ABI and 16-byte storage tested in four host/sanitizer/ARM-qemu groups; VPL status/filter/close/reopen hazards recovered; live open remains open |
| native raw CEC backend | PreInit/real cdev registration recovered; explicit blocking open/send/RX/close, failed-close retention and fd-zero/reopen logic pass three injected-I/O host/sanitizer/ARM-qemu groups; hardware acceptance open |
| offline CEC/audio owners | injected authority; mod_cec idle suspend/resume and mod_audio owner pass host, ASan/UBSan and ARM/qemu twice plus the full host suite; hardware_open_supported is 0; enabled HAL suspend returns failure; no live open; OWNER-WORKER |
| System Audio/ARC route | cec.audio v1 on mod_cec; injected 1.3a System Audio and 1.4 ARC; local volume/mute refused while the mode is on; 8-byte reload snapshot; host make check plus one ASan/UBSan and one ARM/qemu run; no live bus or amp write; OWNER-WORKER |
| portal navigation | mod_ir input.feed, mod_ui_fb hit/open/home/back/source and mod_ui_anim list/dialog; 4-byte reload snapshot; host make check plus one ASan/UBSan and one ARM/qemu run; no ttyS2 or fbdev; live UART code byte unconfirmed; UI-PARITY |

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
