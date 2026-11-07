REPLACEMENT-SYSTEM-2026-11-07 assembles the reproducible takeover UI:
`make replacement-bundle` emits `plat_stub` + display/panel/ir/settings/
service_db/image_decode/ui_engine/ui_vendor/ticker/ui_clock/ui_anim, oa-client
and menus. On the TV the portal painted, physical BLAST navigation changed
`band/rail/card`, `open:settings` reached the captured picture-settings screen,
and `reload ui_vendor` preserved it. `display.mirror=0` + `ui.fps=0` gives
46–51 ms scene + 6–12 ms pan with no idle repaints. The settings panel is the
firmware 780×1080 transparent overlay over the video plane (clear_rgba now
matches); rail assets are the vendor 11×30 ms APNG. The real `plat` DRM/KMS
bring-up leaves the vendor-enabled OSD black; the UI uses plat_stub until cold
OSD allocation/enable is recovered. Boot persistence is a separate gate. TV was
restored stock. Report: REPLACEMENT-SYSTEM-2026-11-07.

HW-ACCEPT-2026-11-07 is the first owner-confirmed hardware run covering the
fixed modules. **Amp:** a quiet 440 Hz default-PCM tone was audible, but a
focused A/B also heard it while Init left the amp at mute 1; the Init/mute
state does not gate that PCM path. The audible-amp acceptance is **retracted**;
mute gating, register readback, volume/balance and standby remain open.
**IR:** the physical remote produced real BLAST press and release records for
Right (0x3056), Down (0x3853), Up (0x3054) and Left (0x3855) through
`oa-ir-blast`; held keys arrived as `aux=01 flag=00` and are emitted as
`OA_KEY_PRESS` (`OA_KEY_REPEAT` is unimplemented; the vendor key path reads
only `flag`). No OK press was captured. **Negative:** `panel 50` and `panel 0`
returned `ok` and updated `panel-state`, but the owner saw no visible dimming
at either value — the direct `Hal_Pwm_Set_ByChannel` path is not this panel's
working backlight path; recover `DEV_PANEL`/MCU/`/dev/ntkled` before any
brightness claim. CEC with a created `/tmp/ntkcec` (229:0) finished acquire
with errno 1; no TX/RX. `tuner-scan` was a HAL-backend attempt with the
module's synthetic +25 progress counter and signal lock 1/strength 0; no RF
source, so no tune/lock acceptance. DVR, cold startup and broad UI parity were
not tested. The IR helper also needed `ioctl(FIOCLEX)` instead of `fcntl` to
avoid a `GLIBC_2.28` binding on the TV's glibc 2.23. Stock was restored by
`wdtctl set 10` (appmgr 1565/aurora 1767 after the first run; 1566/1770 after
the amp A/B), staging/ntkcec absent, profile and fb1 unchanged. Report:
HW-ACCEPT-2026-11-07; follow-up review: AGENT-REVIEW-2-2026-11-07.

AGENT-REVIEW-2026-11-05: the original vendor-decoder harness compared shipped
classes with Pillow/host TurboJPEG, not directly with our DSO on generated
variants. The maintained differential now runs both ARM implementations on all
same inputs, including CMYK rejection and the two durable foxxum JPEGs. All
match. Missing explicit extra inputs fail instead of silently skipping. The
new CLI lacked the service's axis/byte checks before hashing; bounded-provider
regressions now cover host/sanitizer/ARM. Grid firmware independently supports
zero side-mask insets; corrected a reversed right-edge formula in its evidence.
The actual getButtonPosition right edge includes the left inset a second time:
fresh10ca8ac ARM proves it; oa_vgrid_button and two wrong test oracles now agree.
Zero-inset portal appearance is unchanged; no native-getter execution claimed.
No complete decoder/frame/worker/display or native authority claim follows.

AMP-VIDEO-OWNER-2026-11-04 corrects the remaining endpoint ambiguity: video24
is extin, not established VPLT ownership. Its simple comp_release fd callback
is distinct from comp_release_func, which can disconnect input through s_input.
Use section-specific ELF relocations when Ghidra lacks callback data refs.
Private controls forward to EX_PLAT; successful ioctl still does not prove
MCU success. Full libv4l2 wrapper behavior/native authority remain open; no live
device open or MCU call, metadata read only after tvssh WoL. See report.

AMP-TRANSPORT-2026-11-07 recovers the write side used by amp Init.
`DEV_GPIO_SetLevel` sends pin <200 to `Hal_Gpio_Write` and pin >=200 to
`PLAT_SYS_SetStbcGpio((pin+0x38)&0xff, level?2:3)`; the wrappers are linkable
(`Hal_Gpio_*` in libNDAL3, `NDAL_VideoExt_STBC_SetStbcGpio` in libNDAL_EX).
`Hal_Gpio_Write/Config` are mutating: they issue the `670a` claim before
`670e`/`670c`. The STBC setter returns the ndcommon ioctl boolean (1 success,
0 failure) and uses the process-global fd installed by
`NDAL_VideoExt_OpenDevice(0,index)`, so it fails closed without the platform
video open. `common/amp_vendor.c` composes injected GPIO/STBC/I2C/delay into
`oa_amp_init_io`; `check-amp-vendor` passes the full sequencer. No hardware
run, no module consumer, and paired shutdown/ownership remain open.
Report: AMP-TRANSPORT-2026-11-07.

AMP-MODULE-2026-11-07 adds the gated `audio_amp` module and the missing
baseline. The vendor Init patches a retained 38-byte global whose initializer
is in aurora.elf `.data` at virtual `0x017640e8` (file `0x017440e8`), embedded
as `oa_ad82587_baseline` with FNV-1a-32 `0xab635b86`; the live 03..05
`1b/05/05` is not the baseline (`0e/0d/0d`). `audio.amp:v1` exposes
`amp-init` (submit only) and `amp-state` (nonblocking completion); enable=1
requires a ready plat service, loads the profile, opens `/dev/i2c-3` for
writing and creates `amp_owner` with the baseline, while enable=0 publishes a
disabled service and ENODEV. Quiesce unpublishes, detaches and joins/closes on
a worker. Host lifecycle/fail-closed test and TV `--check` pass; ARM
`mod_audio_amp.so` `41287a16...`. Hardware 2026-11-07 with the real
`mod_plat` + `audio_amp`: after rebuilding a stale core, `amp-init` completed
on the TV (`result 1`, `reset 0`, `mute 1`). Init deliberately ends muted;
in that state the amp NACKs the read-only I2C probe (`EPERM`). The new
`amp-mute 0` restores I2C and readback `1b/05/05`. The stock runtime image at
`0x17640e8` matches the embedded baseline plus the configure patches, so the
baseline is confirmed. This is register/GPIO/STBC hardware acceptance, not
audible acceptance; no owner listening confirmation, paired shutdown or
in-module readback yet. Report: AMP-MODULE-2026-11-07.

AMP-SHUTDOWN-2026-11-07 corrects "paired shutdown" for the AD82587 this board
actually uses. `AMP_AD82587_Create` @0x8755d0 installs Init/GetCap/Open/Close/
Reset/Mute/SoftMute/volume/balance/HP_Mute but leaves the Term (+0x54),
PowerDown (+0x68) and GetPowerDownStatus (+0x6c) slots NULL; `AMP_AD82587_Close`
is empty. `DEV_AMP_Term` and `DEV_AMP_PowerDown` therefore do nothing. There is
no vendor paired shutdown to reproduce; the module must not invent a
mute/reset-on-stop. Quiesce closes the I2C session; standby/amp-rail policy is a
separate MCU/panel/power-owner contract. Audible output remains the amp
acceptance gate. Report: AMP-SHUTDOWN-2026-11-07.

AMP-BALANCE-2026-11-07 recovers `AMP_AD82587_SetBalance` @0x875204. For
balance 0..100 the vendor writes reg4 then reg5: <50 -> 0, 127-(b*127)/50;
50 -> 0,0; >50 -> 127-((100-b)*127)/50, 0; >100 -> 127,0. The division is
signed/truncating. `oa_ad82587_balance_regs` and `audio.amp` `set_balance`
implement the 0..100 path; `amp-balance` exposes it. SetVolume is the reg3
prescale write plus 3-byte readback, not UI volume; SoftMute is reg2 bits
0x0e; HP_Mute is GPIO0xc3. Those need a general amp read/GPIO descriptor and
remain unwired. Mute/balance now run as typed owner worker jobs (AMP-JOBS);
a pending job or untaken result rejects with `-EBUSY`. Host tests pass; no
hardware balance run. Report: AMP-BALANCE, AMP-JOBS.

AGENT-REVIEW-2026-11-07 (Grok): the adversarial review found a CRITICAL CEC
authority bug. `mmap64` failure returns `MAP_FAILED` ((void*)-1), not NULL;
the opener only rejected NULL, so it published a bad `read_mirror` that would
fault on `MAP_FAILED+0x200`. Fixed in commit f323673 by normalizing/rejecting
MAP_FAILED with a regression test. The HIGH authority-lifetime bug is also
fixed: the `/dev/mem` mapping is retained while a quiesce generation returns
EAGAIN and unmapped only after a successful join; `check-cec-authority-lifetime`
forces a timed-out generation and then a completed one, and fails if the old
unconditional unmap is restored. The HIGH CEC suspend/resume bug is fixed too:
suspend clears opening/session/closing/job/reply state, hands the owner to the
cleanup worker and waits for it because the host has no retry for a SUSPENDED
module; resume reaps, reopens authority, restarts polling and re-acquires.
`check-cec-suspend-resume` fails without the state clear. `mod_cec`
c8f4138c.... The HIGH IR helper defects are fixed: reads are bounded to the
assembly buffer's free space (a 7+65-byte split used to reset/desync), the
header pins the 8-byte record (code@0/aux@4/flag@5) with `_Static_assert`
(ARM probe symbol size 8) and the writer zeroes padding, the forked helper
marks every fd above stderr close-on-exec (UART now `O_CLOEXEC`), and
`blast_helper_stop` uses `WNOHANG` only (100 ms after SIGTERM, 100 ms after
SIGKILL) with a retained-pid retry instead of a blocking `waitpid`.
`check-ir-helper` fails if the old read or a no-op CLOEXEC sweep is restored;
ARM `mod_ir` 87d2f16b..., `oa-ir-blast` 36ef3665.... The last review HIGH is
also fixed: amp mute/balance are typed `OA_AMP_JOB_MUTE`/`BALANCE` jobs on the
owner worker, using the mute record/level mapping and reg4-then-reg5 writes;
the single ordered slot still returns `-EBUSY` while Init is pending or its
result is untaken, and `poll_result` never overwrites the Init result with a
later mute/balance completion. `check-amp-owner-jobs` and
`check-audio-amp-jobs` pass (worker-only callback assertions; negative checks
for allowing a second pending submission and for overwriting the Init result);
ARM `mod_audio_amp` 67d34652.... The tuner p4 MEDIUM is fixed too:
`oa_nim_raw_channel_setup_dvbt` copies the recovered 0..2 operand into
`demod[4]` instead of a boolean and rejects p4>2; `check-tuner-backend`'s
p4=2 case fails with the boolean form. ARM `mod_tuner` 1896d578.... The panel
MEDIUM is fixed: Open/Set 0 is treated as a definite vendor failure, a failed
Open closes any cached fd and retries, failed Set keeps the last applied
percent, and `iface_get` returns -1 until a set succeeds; `check-panel-curve`
fails if the results are ignored. ARM `mod_panel` 46a3c3ff.... The CEC reply
queue is fixed: a failed submission drops only that record, replies are never
sent after close, and a successful release clears the queue;
`check-cec-reply-queue` fails with the old drop-all behavior. ARM `mod_cec`
5083aee9.... The last MEDIUM is fixed: amp Init may be retried after a failed
result (success stays final), and the module now has suspend/resume hooks that
tear down the owner/I2C and restart/republish; `check-audio-amp-jobs` covers
both. ARM `mod_audio_amp` 1c9553a3.... Only LOW review items remain. The
close-path wording was corrected to drop unproven "only reboot" claims.
Reports: AGENT-REVIEW-2026-11-07, AMP-BALANCE, AMP-JOBS, AMP-RETRY-SUSPEND,
TUNER-P4, PANEL-HAL-ERRORS, CEC-REPLY-QUEUE, IR-HELPER.

TAKEOVER-IR-2026-11-05: opening `/dev/ttyS2` is not sufficient for physical IR
under a replacement. The stock parser also polls `DEV_Ir_GetKey`
(`NDAL_VideoExt_BLAST_RX`, V4L2 control 0xf25). The BLAST common fd in the live
stock process is **222 = `/dev/video28`**, the RC type is **1** (protocol 4),
and `BLAST_RX` returns **0 when a key is present** (nonzero is the no-key/debug
path). A raw `open()` fd is not enough; use `v4l2_open`/`v4l2_close`. The BLAST
code carries the profile RC code in its low six bits (Right 22, Down 19, Up 20,
Left 21), bit `0x800` is the RC5 toggle and `flag` is press/release. Earlier
"no BLAST keys" and "MCU handshake missing" conclusions were implementation
errors, not hardware limits. Repeated protocol changes still leak kernel
opens (`DRV_BLAST_Close: over limit`) until reboot; a clean single open works.
The ttyS2/MCU `_HW_MCU_RcInit`/STBC path is a separate fallback, not required
for this remote.

STOCK-PARITY-2026-11-05: P1 stock portal is another pre-correction BGRA
capture and is blue-shifted; the corrected copy is recorded with raw/corrected
samples and geometry. capture_compare.py supports explicit BGR correction,
named regions and JSON, and exactly reproduces the prior portal AE/RMSE
measurements. A fresh apps-grid comparison is prepared with
apps-grid-regions.json and `make compare-stock-apps-grid`, which refuses until
the stock capture exists. Host404 ok, verify45. No stock apps-grid capture,
pixel parity or TV access; the geometry is firmware-grounded but not an
observed stock layout. Reachability audit: navigation code 0x80 constructs FullScreenPortalDialog;
senders are doNavigateFullScreenMarket (0xd1aa2c, market return 0x29a) and
OperationFinder::handleOperation case 0x79 (0xd3e6a0), proving conditional
compiled entry paths; a stock key path on this model is unverified. The
+0x1fc/+0x200 contradiction is resolved:
RectHandler transforms named rectangles before storage; rowPos is the
transform's own mask, so stored rowPos is the cell surface and the fields
reduce to zero-width horizontal masks, not -1015/+1005. check-vgrid covers
first/last cells and a nonzero mask. Operation-0x79 UI mapping and stock
capture remain open.

STOCK-APPS-GRID-PARITY-2026-11-05: the stock full-screen Apps page was
captured read-only after DIAL MENU/Apps navigation. Its layout is a solid
#481730 top strip, background_fullscreen gradient, 5 columns of 294x165 tiles
at cell+7,+7 on 310/230 pitch, focus painting only the 310x230 orange cell
backdrop (the image does not expand), and a white scrollbar over #811924 at
x1750,180..868. paint_apps now renders this stock mode when
background_fullscreen is present. Firmware-dump corpus comparison improved from
AE29.56%/MAE8 75.38 to AE0.0629%/MAE8 0.160. The tile scaler now matches the
firmware OpenGL path: createTexture 0xb32eb0 selects GL_LINEAR_MIPMAP_LINEAR
plus glGenerateMipmap for UIImageScalingType 1..3, and setApplicationImage
0xd21000 uses type 1. Host423 ok, verify45. Limits: exact vendor bitmap fonts;
JPEG staging uses TurboJPEG FASTDCT; Mali's mip kernel is box-assumed; residual
per-cell sampling/source differences; fb1 not HDMI/cold-start scanout.

CHANNEL-RUNTIME-ORDER-2026-11-05: the channel UI now has a runtime-order
path. A channels module parses the recovered ACTIVECHANNELLIST XML into an
order-preserving, bounded list (transactional replacement, entity decode) and
publishes it; ui_vendor prefers it with an amber marker naming the missing LCN/
visibility data, falling back to stored order with the red provisional marker.
channel-ingest/clear/state/list expose the mode over the socket. Host423 ok,
verify45; ARM/qemu passes. This is the ingest path, not live EASI: testtool is
still disabled, ACTIVECHANNELLIST lacks LCN/visibility, and no stock
channel-list geometry capture exists. The read-only fb1 check showed the stock
no-signal frame; stock apps-grid pixel parity still needs an owner-provided
apps-grid screen or HDMI capture.

PVR-MODULE-2026-11-05: the PVR service is now a real mod_pvr with a
vendor-shaped backend boundary (integer recording id, start/stop, index query)
and a persistent offline stub. pvr.path stores up to 256 jobs atomically with
monotonic ids; cancel only moves scheduled jobs; corrupt files fail closed.
Unit and real socket tests cover add/list/cancel/reload and cross-process
persistence; host421 ok, verify45; ARM/qemu passes. Stub only: no active
recording, timeshift/playback, storage/conflict/series engine, vendor SQLite
metadata or repeat-day expansion. No TV access.

LIVE-MODULE-ACCEPTANCE-2026-11-05: the ABI 3 ARM core, mod_epg and mod_tuner
were executed on the TV from /tmp under stock Aurora after on-device hash
verification. Real-socket load/start/scan/reload/stop/unload passed; the tuner
scan reached 12 services with lock/signal transitions and fed 12 EIT events
into mod_epg, whose query returned the expected event/metadata. Exact-PID
shutdown, socket/staging removal and unchanged stock pids (1967 1785) were
verified. This is CPU-only: no fb1/display/HAL/device access, no testtool, no
vendor backend or cold startup. Testtool TCP1986 is still disabled; the owner
decided on 2026-11-05 not to enable it now and to skip fb1/display until an
ownership window. No profile/process/watchdog/display state was changed.

TUNER-NIM-MODULE-2026-11-05: the tuner contract now has a vendor-shaped NIM
boundary (oa_nim_ops) mirroring DEV_NIM_DTV_StartTune(path,params,status),
MonitorTune, StopTune and the signal getters. Only an offline stub backend
exists: paths 1/2/3/5/7, status 3/4 lock words, frequency in kHz,
strength/quality uint32, float BER/SNR and UNC 0xffffffff. mod_tuner publishes
`tuner`, maps DVB-T/C/S/IP (analog ENOTSUP), advances 25% per status poll while
calling monitor/signal, and pushes synthetic EIT sections into `epg.ingest`
when present. A NIM unit test and real socket test cover metrics, scan,
cancel, reload and EPG query results; host419 ok, verify45; ARM/qemu passes.
Stub only: no tuner hardware, transponder scan, demux/PID assembly, real
service discovery, vendor execution or live backend. No TV access.

EPG-EIT-MODULE-2026-11-05: DVB EIT sections are now parsed (0x4e..0x6f,
section-syntax/length/MPEG-2 CRC checks, MJD/BCD time validity, short-event
descriptor name/text, actual/running/version) and fed into the EIT schedule
cache by a real mod_epg. It publishes `epg` (query) and `epg.ingest`
(feed/clear/count), clears on start/reload and replaces matching
(service,event) keys. Control exposes epg-ingest/epg-clear/epg-count, and a
real socket test covers ingest, replacement, bad CRC, non-current sections,
clear, reload and stop/unload. Host417 ok, verify45; ARM mod_epg and qemu
parser run pass. Limits: complete pre-assembled sections only, no PID/EIT
filters, section merging, CRID/first-showing, persistence or live tuner source.
No TV access.

EIT-TS-DEMUX-2026-11-05: the tuner now has a real MPEG-2 TS EIT source
(`nim-ts-replay`, `tuner.ts_path`) instead of synthetic sections only. The
demux filters PID 0x12, validates 188-byte packets, handles adaptation fields,
continuity duplicates/discontinuities, PUSI pointer fields, split/back-to-back
sections and 0xff stuffing, and caps sections at 4096 bytes. mod_tuner opens it
on start and feeds epg.ingest during scans. Unit demux/replay tests and a real
Unix-socket replay produced epg-count 2 with both events; host423 ok, verify45,
ARM/qemu passes. Limits: replay file only; NIM tune/signal still stub; fixed
PID; no live demux/hardware, CRID or persistence; no TV access.


NDAL-TUNER-BACKEND-2026-11-05: the tuner boundary now has a selectable
live-capable NDAL adapter. It opens `/dev/dvb/adapterN/frontendN`, tunes DVB-T
with `FE_SET_FRONTEND`, reads lock/strength/SNR/BER/UNC with `FE_READ_*`, and
resolves the shipped `Nd_NIM_SetScanMode/GetFrequency/GetDeliverySystem/
GetSymbolRate/SetTsOutput/SetStandy` symbols from `libNDAL_EX.so` via dlopen.
`tuner.backend=ndal` selects it; the default remains the stub. Mock ops tests
cover open/tune/scan/frequency/status/metrics/close and error cleanup; host423
ok, verify45, ARM/qemu. Limits: mock only; DVB-T tune only; no live EIT/demux
or PVR stream; hardware lock/signal/scan and cold start/display remain
owner-gated.

EIT-DVB-DVR-2026-11-05: the tuner now has a DVB section-filter EIT source
(oa_eit_dvb_ops: demux open, PID 0x12 filter, section read/close) attached to
nim-ndal via `tuner.eit_demux=adapter:index[:pid]`, lazily opened and closed
on stop. The PVR side has a mockable DVR sink (oa_dvr_ops/open/write/flush/
close) with partial-write handling. Mock tests cover EIT filter/section/error/
lifecycle and DVR lifecycle; a new Unix-socket mod_tuner reload/re-scan test
keeps the TS EIT replay and EPG state. Host424 ok, verify45, ARM/qemu. The
first close path called POSIX close and skipped Hal_Demux_Dvb_Close; that
claim is superseded. The Ghidra ARM listing of libNDAL3.so (MD5
c842bad804302ec822076c3d9d3ce31d) shows Close at 4e7b9c48 does ioctl 0x6f2a
then close and returns close's r0, Set_Sectionfilter at 4e7ba12c takes the
PID-only seven-argument call (flag byte selects flags 5) and returns 0 or -1,
and Dvr_Open at 4e7b9f44 uses mode 0 as O_WRONLY. next now requires one
section (length field 4..4093, byte count 3+length). stop closes a lazy EIT
fd when the frontend is closed. HAL -1 with errno 0 is -EIO. nim_stub and
pvr_stub passed again on host, ASan/UBSan and ARM/qemu. Full host424/verify45
was not re-run. Limits: mock only; PID-only filter; DVB-T tune only; DVR
sink not scheduler-driven; hardware acceptance and cold start/display remain
owner-gated.

BROADCAST-VENDOR-FIELDS-2026-11-05: the broadcast contracts now carry the
recovered vendor fields. oa_tuner_status has lock/strength/quality/unc and
float BER/SNR (unc 0xffffffff unknown), oa_epg_event has
program_crid_id/running_status/first_showing/state/version/actual, and the PVR
request/job carry repeat days and start/end padding. The fixture and control
socket/CLI expose them, and pvr-add accepts an optional
`repeat start_pad end_pad` group. Added an in-memory EIT schedule cache keyed by
(service,event): seconds start+duration query, exact get, now/next, capacity and
bounded titles, built on common/eit_time.c. Host415 ok, verify45; ARM/qemu
passes. Fixture/project code only: no vendor execution or hardware, no CRID
string resolution, EIT filters, persistence, series/conflict handling, and no
EITEventRecordList find/remove semantics. No TV access.

VENDOR-BROADCAST-ABI-2026-11-05: aurora.elf grounds the broadcast contracts.
EIT ranges are uint32 Unix seconds decoded from MJD (>=40587) + BCD hh/mm/ss,
with duration appended; 0/0 is empty, 0/0xffffffff infinite. Range algebra
(merge/grow/shift/clip/moveInto) has vendor-specific sentinel and clip-edge
behaviour. Vendor EPG queries take (DateTime seconds, duration seconds, service
index), identity is (service id, EIT event id), and ScheduleEvent persists
StartTime/EndTime/FirstShowing/ProgramCridId. ScheduleEvent text is joined from
ProgramInformation. Tuning goes through DEV_NIM_DTV_StartTune(path, params,
status) with lock/strength/quality/BER/SNR/UNC getters; PVR uses
DvrController recording ids, index-based queries and EASIReminder+
explicit start/end scheduling, plus NebulaPvrRecordingScheduler handles with
repeat/padding. common/eit_time.c reimplements the recovered codec; host414 ok,
verify45; ARM/qemu passes. Static RE only, no vendor execution or TV access;
contracts not yet extended with the missing fields.

BROADCAST-IPC-2026-11-05: the tuner/epg/pvr fixture contracts are now exposed
through the control socket as tuner-state/scan/cancel/select, epg-query and
pvr-list/add/cancel. Argument validation happens before service lookup and
handles are released per call. tests/broadcast_ipc.cjs drives the real binary
and fixture through scan/select/cancel, EPG windows/caps, PVR ID/cancel/repeat,
reload reset, stop->ENODEV and shutdown; host413 ok, verify45. Fixture backend
only: no tuner/EIT/PVR hardware, vendor backend, UI or persistence. No TV
access.

BROADCAST-FOUNDATION-2026-11-05: added project-internal tuner/epg/pvr v1
contracts and a host fixture. Tuner scan state machine, EPG overlap/bounds/
caps, PVR add/list/cancel and reload/teardown pass; host412 ok, verify45. These
are fixture contracts, not vendor ABI or hardware parity: no tuner scan, EIT/
EPG semantics, recording engine, production module, UI or persistence;
CLI/socket coverage was added later in BROADCAST-IPC. No TV access.

TUNER-VENDOR-CHANNEL-2026-11-07: `HAL_Demod_User_Initialize` and
`HAL_Demod_User_SetChannel` are exported by **aurora.elf only**, not
libNDAL3/EX; `HAL_Demod_Set` is the linkable NDAL tail. A replacement cannot
call the Aurora-only symbols, so the DVB-T SetChannel body is reproduced:
build the recovered 0x5c record (freq kHz `+0`, bandwidth MHz `+4`, IF mode
`+0x10`, p4 `+0x14`, signal type 2 `+0x1c`, `ff ff ff 00` `+0x24`), run the
MxL661 DVB-T program sequence, then `HAL_Demod_Set` with
`demod[0]=0x12`, `[1]=bandwidth_MHz`, `[4]=p4`, `[5]=1`, `[0x14]=freq_kHz`.
`tuner.vendor_channel=1` selects it; no duplicate `program_tuner`. Host tests
and one no-RF TV run pass. No RF tune/lock acceptance, no execution of the
Aurora-only symbol, and the 6/7 MHz/platform-IF-mode variants remain
unvalidated. Report: TUNER-VENDOR-CHANNEL-2026-11-07.

PANEL-BACKLIGHT-2026-11-07: the stock process's populated descriptor table
shows backlight id `0x6c` -> pin 108 -> PWM channel 0 (`astPwmBasePin`
`0x175f320`); `gu32PanelFreqInHz` is 30000, `genPwmdimMode` 0 and the current
level 1000. `mod_panel` defaults were corrected from 20000 Hz/mode 1 to
30000 Hz/configurable `panel.mode=0`; the open request remains mode 1. A clean
50% run loaded `plat_stub` + `panel`, opened `/dev/ntkled` and reported
`panel-state 50`, but no measured/owner-confirmed brightness was obtained. A
first stage accidentally used the real `mod_plat`; it was restored by reboot
and is not panel acceptance. Panel power/resume remain open. Report:
PANEL-BACKLIGHT-2026-11-07.

AUDIO-CEC-TAKEOVER-2026-11-07: the first audio/CEC control-plane group is not
hardware acceptance. The `audio_amp_profile` config key is
`amp_profile.hwprofile`; using `audio_amp_profile.hwprofile` makes start return
EINVAL (the ARM parser on `/conf/mb230_hwprofile.bin` returns rc0, 12Watt,
prescale27). With the correct key, plat stub + audio_amp_profile + audio_curve +
audio + cec start; profile query 12Watt/27/5/5/128, curve50=8137,
`audio-state` enabled0 volume-1, `cec-state` supported0 local1. The host opens
no snd/i2c/video28/ntkcec fd; no volume/mute/amp or CEC bus operation. Read-only
mixer values0 and amp 1b/05/05 are unchanged. Native amp and CEC hardware
backends remain open. Report: AUDIO-CEC-TAKEOVER-2026-11-07.

CEC-RAW-BACKEND-2026-11-07: the native CEC path is wired behind `cec.enable`
with a read-only `/dev/mem` authority check at `0xfc040200` (bit1 must be set;
the TV read `0x000001ab`), async owner acquire/receive/send, a 100 ms poll and
`cec-open`/`cec-close`/`cec-send`. A clear authority bit fails closed because
no userspace STBC `0xf011` write path is recovered. Hardware: after stock
SIGTERM, the kernel thread `DRV_CEC_CALLBACK` still held the KER_CEC exclusive
semaphore (`kcec: CEC Try lock fail`), so the raw open returned EPERM and no
CEC frame was sent or received. Stock restore by reboot created its own
callback thread. This confirms the close/open defect from CEC-OWNERSHIP:
process death is not a graceful CEC release. Follow-up: `ICE_CECClose`
@0x806e44 is a debug-only stub, `PLAT_CEC_Close` @0x88bff4 is never called
internally, and `NDAL_VideoExt_CEC_Init(NULL)` from a new process reports
success without stopping the orphaned kernel thread. No tested userspace
stock-close trigger was found for the SIGTERM case (indirect callers were not
exhaustively checked); a raw takeover still needs an early boot-order start or
shared stock-owned monitoring, not a driver-global patch.
Report: CEC-RAW-BACKEND-2026-11-07.

INPUT-PVR-TAKEOVER-2026-11-07: `mod_ir` used `dlopen` without `-ldl`; the TV
preflight rejected it (`undefined symbol: dlopen`, fixed to NEEDED libdl).
More importantly, `NDAL_VideoExt_BLAST_RX` blocks in `blast_io_get_key` and
was called from a 50 ms host timer. The single host thread then blocked and
every control command timed out; a poll-readiness gate did not help. BLAST RX
now runs in the killable `oa-ir-blast` child, which owns `/dev/video28` and
writes fixed 6-byte records to a pipe2 stdout; `mod_ir` emits on the loop and
SIGTERM/SIGKILL/reaps the helper. The fake blocking-lib test passes; real
BLAST shows host main `poll_schedule_timeout` and child `blast_io_get_key`,
source/PVR commands responsive, quit clean with no helper, and `reload ir`
changes the helper pid (6277->6300) cleanly. A `fcntl` binding required
GLIBC_2.28 on the TV's glibc 2.23; `pipe2` (GLIBC_2.9) fixed it. Physical
remote press/repeat/release remains unverified. Report: IR-BLAST-HELPER.
With BLAST off, source-select 17 (vin3) then 0 (vin1)
and offline PVR add/cancel work. Report: INPUT-PVR-TAKEOVER-2026-11-07.

EASI-PROBE-2026-11-05: testtool TCP1986 exposes read-only GETCHANNELLIST
(RSN/name) and ACTIVECHANNELLIST XML (RSN/name/handle/ONID/TSID/SID/type). A
whitelist-only probe client with bounded parsing, host fixture and ARM build
passes; host411 ok, verify45. The TV testtool server is disabled
(is_testtool_socket_enabled=0, TCP1986 refused). Enabling requires
profile+restart or a live-process pause, deferred without physical recovery.
Active LCN/visibility still need a runtime call path; no vendor process was
paused/restarted.

SERVICE-DB-2026-11-05: firmware SQLiteStreamStorage maps service table types
0xc9/0xca/0xcb to DVBSerTable/EASISerTable/AnalogSerTable. A new read-only
adapter and mod_service_db expose service.list v1 with 525 dumped rows
(445 DVB/67 EASI/13 analog), bounded text and missing-table failure. Exact
StoredDvbServiceData stored-flag predicates (visible/available/encrypted/
numeric/missing), stats, and name/channel_no query ordering with filters are
included; the dump totals 67/338/67/0/0. A read-only live DB fetch found
54 DVB/67 EASI/13 analog and m_lcn low16=0xffff sentinels. getLcn (0xefce65)
and isHiddenService (0xefba80) use runtime EASI, so active-list LCN/filter
parity needs a runtime probe or an explicit provisional-order decision.
Synthetic, real-dump, reload/stop and real socket lifecycles pass; host409 ok,
verify45. A provisional `ui_vendor` channel-list screen over the stored
channel_no/name query is recorder-tested, with an explicit "NOT RUNTIME EASI"
marker; host410 ok, verify45. Production `system()` calls are removed (display
mdev uses posix_spawn/waitpid; test cleanup uses nftw). Runtime EASI LCN/
visibility, stock channel-list
key path/geometry/typography/scrolling, tuner, EPG/PVR and stock parity remain
open. No persistent TV write; tvssh sent WoL for the read-only fetch. Cold
bundle not rebuilt.

PLAT-CONSUMERS-SOCKET-2026-11-05: a fake plat service provider starts the real
host-build audio/source/ir/panel modules over the socket with hardware paths
disabled. Services publish, plat reload is refused while they are active,
dependency-ordered provider reload and consumer reloads pass, and a fresh
unload/load/start cycle works. Five consecutive runs; host403 ok, verify45.
Fake plat only; real HAL/devices, hardware-module reload and cold startup
remain.

GRAPHICS-SOCKET-2026-11-05: real graphics + scene_display over the Unix socket
with oa-scene-broker and the fixture renderer. graphics publishes display.gpu;
scene_display publishes display/alpha/async. Reload and dependency-ordered
reload pass; graphics reload is refused while display.gpu is held; the broker
child is replaced then reaped; unload/load/start reuse works. Host402 ok,
verify45. Fixture renderer only; real EGL/Mali, physical fb1 and cold startup
remain.

FAKE-FB-SOCKET-2026-11-05: an LD_PRELOAD fake /dev/fb1 with the recovered
region/MAU/pan ioctls runs the real mod_display_fb over the socket. A self-test
paint changes yoffset/region/pixel FNV; suspend, stop, start/reload and
unload/load/start all restore the initial yoffset, MAU, eight region words and
pixel hash. Host401 ok, verify45. User-space fake only; real kernel/TDGA,
physical fb1, vendor coexistence and cold startup remain.

FIXTURE-MODULES-IPC-2026-11-05: a real `--sock` host runs ui_theme (synthetic
BinSVG), ui_profile (synthetic EFSL), audio_amp_profile (extracted hwprofile),
audio_curve (synthetic 1668-byte AQ/profile0) and audio_mixer (host broker).
All publish services, reload, stop/start or unload/load, and exit cleanly.
Host400 ok, verify45. Host fixtures only; hardware modules, physical fb1 and
cold startup remain.

SKIN-SOCKET-2026-11-05: the existing skin_socket.sh test is wired into make
check with recorder display, ui_engine, ui_vendor and ui_omarchy. Missing/
invalid startup skins fall back to vendor; runtime omarchy/vendor switching,
enter-failure fallback and clean quit pass. Host399 ok, verify45. Recorder-only;
physical fb1 and cold startup remain.

MODULE-MATRIX-IPC-2026-11-05: a real `--sock` host runs ticker ->
ui_clock -> ui_anim, plus ui_navigation, ui_list, power and cec with no
display/HAL. All load/start; nav/list queries and dependency-ordered reloads
pass, including CEC's asynchronous cleanup retry. Host398 ok, verify45.
Hardware modules, physical fb1 and cold startup remain open.

UI-STACK-IPC-2026-11-05: a real `--sock`/stdin non-HAL stack runs
settings/launcher.db + image.decode + recorder display + ui_engine + ui_vendor.
Launcher rows, decode hash, ui-state and dependency-safe reloads of all five
providers pass. Host397 ok, verify45. Non-HAL only; physical fb1, full hardware
module set, all-consumer holds and cold startup remain.

UI-VENDOR-IPC-2026-11-05: `ui-state` queries the real ui.portal screen; a
host with mod_nav_display (recorder display), ui_engine and ui_vendor is driven
over `--sock` and stdin through start/use/stop/start/reload/unload/reload,
including display-provider reload and consumer restart. No TV/HAL/framebuffer;
host396 ok, verify45. Recorder-only, not physical fb1/cold startup.

IMAGE-DECODE-IPC-2026-11-04: the real host `--sock` control plane (plus stdin)
runs image.decode through `image-decode <path>`: load/start, PNG/JPEG FNV,
invalid-file errors, status, suspend/EAGAIN/resume, stop/ENODEV/start reuse,
reload and unload/reload. Empty autoload, no display/HAL/TV. Host395 ok and
verify45. This closes the host socket/CLI lifecycle milestone, not
ui_vendor/display reload or cold startup.

VENDOR-DECODER-DIFF-2026-11-04: an ARM LD_PRELOAD probe runs the shipped
PngImageDecoderImpl/JpegImageDecoder under qemu before vendor main, using
vendor MemoryBufferSource/MutableImage and requested RGBA ImageProperties
(model3; model2 is BGRA). Nine corpus PNGs plus gray1/2/4/8, gray+alpha,
palette, palette+tRNS, RGB16, RGBA16, interlaced and gAMA variants are
byte-exact RGBA. DMR-small/large JPEG are byte-exact against TurboJPEG
`TJFLAG_FASTDCT`, now used by image.decode; ImageMagick accurate DCT differed
by up to8. A progressive JPEG is exact, a generated CMYK JPEG is rejected by
both classes, and two available real foxxum JPEGs are exact; the other two were
not offline. No TV/HAL/UI. Remaining two foxxum files, YCCK/APNG,
worker/frame lifetime and stock parity remain.

STOCK-PARITY-2026-11-04: old shot-launcher-right.png is an R/B-swapped
pre-correction capture; after RGB swap its Prime/Canal colors match the
current downloaded sources. The corrected stock portal and offline portal
share 171x95 card geometry and rail focus color, but card artwork/crop differs
(Netflix RMSE0.376, Prime0.216, others0.066..0.104), background differs
(13,14,14 vs36,38,40) and stock status chrome is absent. It is not an apps-grid
or pixel oracle; a fresh corrected same-session stock apps-grid capture is
still required. No TV access.

FB-SNAPSHOT-COLD-BUNDLE-2026-11-04: common/fb_snapshot snapshots pixels,
var/fix, the8-word OSD region (RD0x8020c86c/WR0x4020c86b), read-only MAU and
pan; restore verifies MAU and restores pixels/region/pan. display_fb config
`display.restore_snapshot=1` snapshots before region change and restores on
stop/suspend, with resume re-snapshot. fb-restore stores FBR2. Fake-fb
library/tool/actual-display tests pass, host354 ok, focused ASan/UBSan clean.
`make cold-bundle` builds an ARM bundle (25 modules, oa-supervise, fb-restore,
cold config) and manifest-verifies29 entries. No TV execution; live-driver
equality, cold startup and MCU/watchdog authority remain unverified. TV
access was revoked for that original run. FB-RESTORE-REVIEW corrects "full"
restoration: var/fix are saved metadata; changed backing/layout/mode/MAU now
rejects before writes, mmap failure still attempts region/pan and failed file
close cannot report successful backup. Strict host/sanitizer/fresh ARM-qemu
library/tool/both-display tests pass. DISPLAY-CLEANUP subsequently fixes the
module's snapshot discard: failed synchronous suspend retains state and returns
error; stop/unload cleanup uses an owning worker and actual nonblocking join.
Both variants pass host/sanitizer/ARM and full host382. Vendor TDGA RemoveRegion
is a no-op; pool destruction unmaps and driver close releases GPU/fds, while
kernel PMI restores registers separately. New snapshot/retry behavior is
project reload policy, not vendor lifecycle parity. DISPLAY-DSO now verifies
actual display_fb host/loader/control-dispatch hold, blocked/forced reload,
deferred resume, failed start and shutdown: five host/sanitizer/ARM groups and
full host387 pass. It reproduced a completed-worker poll(-1) shutdown hang
after the last timer disappeared; GDB evidence drove a20ms shutdown poll cap.
Quiescence is now retried without a later notification; permanent failure still
retains the module. Exclusive authority/native equality remain open. No TV access;
current permission excludes active fb1.

VISIBLE-IMAGES-2026-11-04: the supervised no-plat candidate painted the live
apps grid on vendor-initialized fb1 from an empty ui.dir and the real
/buffer/downloaded_images (18 items,15 visible, scrollbar0/600). The capture
contains two visible JPEG foxxum cards (3753/1685,384x216) and13 PNG cards;
the two JPEG crops match nearest-scaled originals within≈2.1%/1.1% AE. fb1
pixels/yoffset were snapshotted and restored; bundle removed, /proc scan0,
vendor PIDs intact. Supervised only; no full region/MAU/pan equality or
independent startup. The current owner override revoked TV access after this
run; no TV command followed. See VISIBLE-IMAGES.

AMP-INIT-2026-11-04 adds injected blocking Init control policy.29952 cases
match complete native Init/Reset/Mute traces/image/cache/return;45 total native
groups and host/sanitizer/ARM/full host366 pass. Raw reset-pin operations leave
the reset cache untouched; GPIO failures do not stop Init; only nonzero level
returns update cache bytes. No default native I/O, ownership, module/worker
hookup, paired shutdown or audible acceptance in that milestone. AMP-OWNER
now supplies an injected owning thread, copied input/value completion and
retained state; shutdown drains and nonblockingly joins through real TLS
destructors. Four host/sanitizer/ARM groups and full host370 pass. Backend
context/code stays borrowed until join, not just a published completion;
joining alone performs no physical shutdown. Module/consumer/native backend
integration remains open. The worker's four injected-I/O groups subsequently
passed on actual TV glibc/pthread under the owner's non-fb1 test permission;
temporary staging removed/no helper and boot/vendor PIDs unchanged. No devices
or framebuffer opened. Create seeds private reset/mute cache0/1 and does not
reset the global image; a fresh ELF image is not a running-state snapshot.

AMP-I2C-2026-11-04 supplies an owned blocking bus3/address34 fd session,
explicit write permission and correct errno-to-Init-boolean adaptation. Fresh
default-write/NDAL3/PreInit instructions establish two-message packets and10ms
lower post-write delay; no duplicate delay belongs in the orchestration loop.
Exact-count checking deliberately rejects the HAL's false-success cases and
does not claim vendor failure parity. Three host/sanitizer/ARM groups and full
host390 pass; the refactored native read03..05 returned1b/05/05 on TV. Staging
removed/no helper, boot/vendor PIDs unchanged; no fb1 or audio write. Writes
are injection-tested only. Fd ownership is not exclusive amp authority, and
GPIO/MCU/module/image/paired lifetime acceptance remain open.

AMP-GPIO-QUERY-2026-11-04 corrects a getter trap: Hal_Gpio_GetDirection issues
mutating670a before GET670f; normal Hal_Gpio_Read exports through sysfs. Direct
kernel670d/670f for114 do not claim/change pins; an8-byte packet contains a
one-byte reply at+4. The new helper/probe passes two host/sanitizer/ARM groups,
full host392 and native level1/cached direction0(output). Staging removed/no
helper, boot/vendor PIDs unchanged, no fb1/config/level/MCU call. The GPIO
fd's fops has no open/release callbacks; fd close is not global ioctl6702.
Cached direction/output latch are not independent voltage or ownership proof.
Do not repair missing pin context by claiming it under stock Aurora.

AMP-STBC-2026-11-04 establishes a different GPIO domain: MCU getter has three
arguments, pin plus TWO uint32 output pointers; no historical reset cause is
inferred from that fact. Driver replies are raw bits23..16/15..8; Aurora's
GetLevel consumes the first, not a proved direction/level tuple. Separate
control0 device status can be0 even with successful ioctl/NDAL transport bool.
The injected codec checks it and returns stricter errors; no failure-parity
claim. Two host/sanitizer/ARM groups/full host394 pass, no TV/MCU operation or
native fallback. OpenDevice type10 resolves video24/process-global fd state;
that helper alone does not establish native acquisition/release/MCU authority.

AMP-PROFILE-MODULE-2026-11-04 adds reloadable audio.amp_profile v1 and the
amp-profile query over actual local Unix IPC. Caller-owned copies/names and
provider holds/removal/failure/fd/suspend/reload lifecycle pass host/sanitizer/
ARM; socket tests include the saved target profile. Full host361 passes.
Audio consumer/retained image/worker/hardware lifecycle still open. No TV
access, deployment or hardware operations; offline-only restriction remains.

AUDIO-AMP-PROFILE-2026-11-04 adds bounded hwprofile inputs for the amp image:
power string/legacy enum, empty-name8Watt lookup, scalar defaultc0/DRC default-1.
The saved profile selects12Watt with1b/05/05/fine80. Host/sanitizer/ARM and full
host356 pass;2312 native payload comparisons make44 total reference groups.
Values and name bytes copy into caller storage; the struct borrows that name
buffer. No vendor getter cache or hardware ownership is claimed. Offline only;
TV restriction remains in force.

AUDIO-AMP-NATIVE-2026-11-04 supersedes static-only image verification:6000
profile/current-image cases and all38 write failures match the complete shipped
Init body under ARM/qemu (controlled hardware callees, absent GPIO descriptions).
All43 reference groups pass in a fresh identified binary. Real profile/image
ownership, GPIO/MCU/I2C/worker lifecycle and audible/standby acceptance remain
open. No TV access; owner offline-only restriction remains in force.

AUDIO-AMP-IMAGE-2026-11-04 adds pure AD82587 profile-image conversion from
decompiled ARM. Retain the preceding image between inits; state-control OR bits
are persistent. Open/Close only return object!=NULL, ResumeDevice starts a task,
and its completion flag does not carry init success. Strict host/sanitizer/ARM
units and full host349 checks pass; no native Init differential, hardware
ownership or audible/standby acceptance yet. No TV access or deployment.

HOST-CONFIG-2026-11-04 fixes the process-global append-only configuration table:
snapshot entries/count now belong to each host. Cross-host isolation and caller-
torn-down reinitialization pass strict host, leak-enabled ASan/UBSan and fresh
ARM/qemu tests;54 core checks pass. First duplicate still wins, and module
reload does not reread config. No TV access or vendor persistence parity claim.

TODO-REAUDIT-2026-11-04 reviewed all183 checks at172cd57;182 were retained at
bounded scope and PNG was reopened at that snapshot. The reopened item is now
narrowed by the classic-libpng/JPEG follow-up: non-image/truncated inputs,
suspend/resume/reload and provider-copy release are covered; a vendor-class
differential, APNG/CMYK policy, worker scheduling and on-TV decode remain open.
Recorder alpha composition passes; scene transforms are only metadata in its
captures. Full host and42 maintained ARM reference groups pass. No TV access
or deployment.

IMAGE-DECODE-JPEG-2026-11-04: image.decode v2 uses the shipped libpng1.6
classic API with Aurora b30918 setConversions for PNG and the vendor
libjpeg-turbo2.0.1 TurboJPEG API for JPEG, returning straight RGBA. Nine real
downloaded PNGs (RGB/RGBA/colormap,400x225) and two real vendor JPEGs (48/120)
match independent ImageMagick SHA256/FNV exactly; truncated/non-image inputs
reject, suspend returns EAGAIN and reload republishes. ui_engine tries PNG then
JPEG in ui.download_dir (default /buffer/downloaded_images), copies pixels then
releases the provider under hold. check-ui-png/jpeg-cards prove the source
dimensions/first pixels. Host395 ok, focused decoder/UI ASan/UBSan clean and
ARM/qemu passes with vendor libpng/zlib and libjpeg-turbo. Limits: actual
foxxum JPEGs were not copied offline; APNG/CMYK/YCCK reject; the vendor-class
differential is separate and covers PNG variants/JPEG fast-DCT; worker, on-TV
decode and stock pixel parity remain. No TV access after the owner stop
instruction.

ALPHA-RECORDER-2026-11-04 corrects the offline recorder: cap_blend now uses
oa_canvas_over (straight source-over, destination alpha preserved) and the
test display publishes display.alpha, so clear_rgba keeps transparent regions
instead of ui_engine's opaque fallback. check-ui-sources verifies transparent
and opaque panel regions, invalid clear alpha and half-alpha composition; host,
focused ASan/UBSan and ARM/qemu pass. This is production-canvas fidelity, not
vendor-Mali pixel or stock-scanout parity. It closes the TODO-AUDIT reopened
item; the earlier opaque-only note below is superseded.

UI-FB-SIM-2026-11-04 (addendum) adds offline layout capture: the host test
display recorder rasterizes its draw calls into 1920x1080 RGBA frames under
OA_NAV_CAPTURE, and check-ui-settings-panel writes a .panel snapshot. The
captured portal (ui-layout-portal.png) comes from the real ui_vendor draw
calls, no TV or GPU. The synthetic panel test paints only 4x4 stand-ins and no
background, so a real-asset preview needs the captured panel as OAI1 and a
real ui.dir; GPU composition still needs the Mesa-wrapped renderer DSO. This
is layout evidence, not scanout/vendor-render parity.

UI-FB-SIM-2026-11-04 establishes what can replace a TV run. The real fbdev
display module can run against a fake framebuffer (link-wrapped open/mmap/
ioctl) on the host and as an ARM binary under qemu-arm-static: eight
composition/pan/failure groups pass. The GLES scene code runs offline on host
Mesa llvmpipe by redirecting the production `eglGetDisplay(1)` contract to
EGL_DEFAULT_DISPLAY (`make scene-gles-mesa`); all nine transition types are
pixel-exact. User-mode qemu cannot emulate Mali or the ntk `/dev/mali`
ioctls, and the sysroot has no ARM EGL other than the vendor blob, so vendor
rasterization and stock-vs-ours scanout comparison still require the set.
Next offline step: a 1920x1080 fake-fb capture harness and a Mesa-wrapped
renderer DSO so the full settings preview can render without the TV.

# Current findings and correction map

Initial offline audit:2026-10-31. Subsequent dated reports add implementation
and TV evidence, including UI-SCENE-REVIEW on2026-11-01. This guides conclusions;
topic reports retain detailed provenance. See [report index](artifacts/reports/README.md).

## Evidence rules

TODO-AUDIT-2026-11-04 reviewed all182 checked milestones. Reopened recorder
RGBA fidelity: cap_blend forces alpha255 and no display.alpha service is
published, so captures are opaque layout approximations.181 checks remain
as bounded research/helper/test/historical milestones;66 descriptions remove
stale missing-helper, timing, version, snapshot or hardware-acceptance claims.
The full source/font/theme/ARM/Mesa checks pass; no new TV observation or
complete native-feature acceptance is inferred. See the per-item audit ledger.

WORK-REVIEW-2026-11-04 corrects the later work: scrollbar+6c is SVGWindow's
four states, not orientation; two-entry caches omitted inactive states.
Calculate uses wrapped32-bit math and the first window query, while render
places from the second. Both reference groups now use the real divider;
14,576 geometry/state,32 skin and36 render cases pass. imageCreate+0xb0 is a
byte, not word truth. Launcher ad/tracking columns and propagation are removed;
the three deliberate privacy differences are explicit in native comparisons.
Apps selection survives reload, new rows replace old rows on restart, sorting
precedes the64-card cap, and hidden tracks/shared-cache exhaustion are fixed.
Strict host/sanitizer/ARM and focused TV CPU/GET-only checks pass. Revised
display_fb cannot initialize HAL or run mdev and has no HAL dependency, but
full hardware restoration/ownership/startup remain open. A historical
FreshWDT return1 did not prove refusal or native-watchdog impossibility.

UI-DISPLAY-REVIEW-2026-11-04: the preview's clean focus bar erased the captured
slider/selection children; live text alone did not restore them. Skin now draws
shipped sliderFA/UFA tracks/PNG knobs and focused choice chevrons, with explicit
reconstruction row/range inputs. Firmware10c4530/10c41ec/10c4af0/10c4a80 and
10b3798/10b4020 supply the composition rules. Seven real GPU states captured;
independent stock Contrast/Modus widget regions agree within2/255 per channel,
full framebuffer/region/MAU/pan restoration verified. Stock Contrast was in
buffer0 while buffer1 retained Modus: never assume a fixed active buffer.
This closes missing preview children only, not native settings, focused font,
tabs, general SVG raster or independent startup. No persistent deployment.

UI-REVIEW-2026-11-04 corrects the recent work: UIImageResource is32 bytes,
not16 (supplier copies through1c); create now has aligned full-size storage and
full-payload tests. Renderer percentage subtraction/division had UB; shared
checked wrap math fixes it. All native zero divisors reach unconditional
raise(8), invalidating the old INT_MAX/MIN return claim. EDOM is an explicit
replacement error policy, not crash parity. Native divider now runs in both
reference groups. Raw checked bytes and independently varied paired child
states close additional coverage gaps.1771396 renderer/276 setter/56 image
cases plus host/IPC/sanitizer/ARM-qemu/TV CPU pass; real rendering/startup open.

SVG-SCROLLBAR-RENDER implements renderBackground (0x10c3658) and renderBar
(0x10c3bac) as ops-driven sequences. Both cache the supplier image per
orientation, resize the background (this+0x98) or bar (this+0xa4) surface,
configure the top/bottom masks with align3/4, draw gTopM/gBotM/gRowM (or
bar_gTopM/bar_gBotM/bar_gRowM) under the mTopM/mBotM/mRowM (bar_m*) masks, and
stretch the middle row rect {0,top_cap,width,height-bottom_cap}. renderBar
calls calculateBarSizeAndPosition first, so it queries the window rect twice;
an empty bar clears both surfaces and clears +0xc8. The +0xc8 byte is a
background-rendered flag (renderBackground sets it; setScrollPosition renders
the background only when clear), not a hide flag; the API name is corrected
from background_hidden to background_rendered. 8 native/C cases compare the
full sequence, cache and flag with host/ASan/ARM-qemu/TV CPU and verify.cjs42
ok. The vendor mask/surface/renderer calls remain stubbed; no pixels, apps-grid
screen or rasterizer are wired, so no visible scrollbar parity is claimed.

SVG-SCROLLBAR-SKIN implements SVGScrollBar::calculateMinBarSize (0x10c397c,
CRC0xd9fdda32). Widget IDs0x20/0x21/0x22/0x23 resolve bar_gTopM/bar_gBotM/
bar_mTopM/bar_mBotM; the BinSVG elements give cap heights20 and16, so the
apps-grid scrollbar minimum is36px. The body caches the supplier image per
orientation (+0x74/+0x84), leaves +0xb8 unchanged if there is no image, resets
it otherwise, calls the renderer vtable slot+0x14 for the two background names
and adds the two moving-cap heights.16 native/C cases compare min/cache/trace
with fake supplier/renderer/RectangleContainer/name stubs; host/ASan/ARM-qemu/
TV CPU and verify.cjs41 ok. The renderBackground0x10c3658/renderBar0x10c3bac
bodies (mask transforms/SVGSurface), the apps-grid screen, display/scanout
binding and independent startup remain open; the home portal has no scrollbar
and there is no stock apps-grid capture.

SVG-SCROLLBAR implements the shipped scrollbar state/geometry.
setScrollPosition (0x10c4138) stores total+0xbc, visible+0xc0 and first+0xc4,
rejects not-created or first>total without storing, calls renderBackground only
when +0xc8 is clear, then renderBar, and returns1. calculateBarSizeAndPosition
(0x10c3af0) gets the track height from UIWindow::getWindowRect (vtable slot
0x28, rect bottom-top): total==0 -> size=height then hidden; total<=visible ->
0/0; negative total preserves; otherwise size=height*visible/total clamped to
min+0xb8 and height, position=(height-size)*first/(total-visible) when
total>visible+first else height-size, and a size==height bar is stored as0.
The apps-grid scrollbar is 1750,180,8x720. 3120 native/C cases compare fields
and reject writes outside+0xb0..+0xc8; host/ASan/ARM-qemu/TV CPU and
verify.cjs40 ok. The skin calculateMinBarSize (widget IDs0x20/0x22/0x21/0x23
through the SVG supplier/renderer/RectangleContainer), background/bar drawing
and the apps-grid screen remain unimplemented; the home portal has no
scrollbar, so no visible scrollbar parity is claimed.

OA-CANDIDATE builds modules/display/display.c with -DOA_DISPLAY_NO_PLAT
(mod_display_fb.so), so a candidate host can bind to the vendor-initialized
fb1 without OA_SVC_PLAT. Running settings,display_fb,ui_engine,ui_vendor with
the live /conf/sqlite_db/settingsDB.db and ui.screen=apps painted
"apps grid: 18 items, first 0, scrollbar 0/600"; candidate-apps_1.png is a
real 1920x1080 fb1 capture of the grid/scrollbar. Vendor Aurora remained
owner; the candidate was stopped, /buffer/oa_candidate and /tmp staging
removed, and the stock no-signal screen verified. This verifies the supervised
display/scanout binding to an existing vendor-initialized fb1, not a cold
independent startup. WORK-REVIEW withdraws the inference that a FreshWDT
return1 proves kick refusal or makes native watchdog maintenance impossible.
Ownership/watchdog/fallback remain unverified and vendor stop/watchdog disable
remain unauthorized. The stock screenshot was not a full state-restore proof.
That cold takeover was not executed. Real downloaded-PNG decoding, stock apps-grid pixel
parity and the independent startup remain open.

UI-APPS-GRID adds SCREEN_APPS5 to ui_vendor (ui.screen=apps): the live
parent-0 type2 LauncherItem rows page through a 183,180,1554x720 grid of 5x3
310x230 cells; the top strip is0,0,1921,162 #481730 and the scrollbar is
1750,180,8x720. paint_apps feeds the recovered
oa_svg_scrollbar_render_background/render_bar ops into a rasterizer: track
surface0 #811924, bar surface1 #FFFFFF, caps20/16, row from the shipped
formula, min36; the bar size/position use oa_svg_scrollbar_calculate. Synthetic
20 rows ->15 cells/bar540; dumped18 rows ->15/bar600; offline and TV-CPU
recorder rasters exist. The visible probe confirmed screen=5 and held, but the
layer-1 capture returned the vendor "Geen signaal" frame and the probe restore
refused after the vendor region changed, so no visible apps scanout is claimed.
There is no stock apps-grid capture: grid chrome/cell fitting/scrollbar colors
are a geometry/resource reconstruction, not pixel parity. Display/scanout
binding, independent startup, live-DB visible and real PNG decoding remain open.

UI-LIVE-CARDS wires the settingsDB rows into the portal. launcher.db now
exposes a borrowed row view/get; ui_vendor reads parent0 with read_parent(0,0,0),
keeps item_type2 rows with a non-empty thumbnail name (skipping slash names),
sorts by m_order_index then id and loads each named OAI1 asset. The explicit
ui.cards override wins and ui.live_cards=0 restores DEFAULT_CARDS; missing
service/DB/rows also fall back. Recorder tests: synthetic3 rows/3 visible;
dumped and live DB12 selected/9 visible (the dump has20 parent0 type2 rows, the
first12 selected and the first9 fit the row). Offline portal capture and a
visible TV layer-1 capture show the stock order (Netflix, Prime, YouTube, tabii,
Canal Digitaal, TV VLAANDEREN, TELESAT, Digiturk, GAIN). The visible probe
restored bytes=16588800 yoffset=1080 MAU=0 region_match=1 snapshot_match=1 on the
dumped-DB run; a live-DB visible attempt reached the hold but captured the stock
frame and refused restore after the vendor region changed, so it is not claimed
as a visible live capture (the live rows were rendered in the recorder test).
The rail is still static; PNG decoding, live refresh, scrollbar/skin and scanout
remain open.

LAUNCHER-DB implements the settingsDB LauncherItem read half. The member map
from NovoLauncherItemInfo::launcher_item_info_struct (0xfabf50) has16 scalar
columns at+4..+0x34 and10 fixed text columns at+0x38..+0x93e with sizes
300/60/150; the DB blobs are exactly those sizes. copyLauncherInfoToItem
(0xfac334, 0xf4 bytes, CRC0xa089d47e) decodes one record: id/parent/order/
type/original/server/menu-return into item+0/+4/+8/+0x14/+0xc/+0x10/+0x18,
ten UString assignments in a fixed order, update times/image CRC at+0x44/
+0x48/+0x4c, five flag bytes at+0x50..+0x54 and channel at+0x90. 8 native/C
cases pass. getAllLauncherItems (0xfacff1) reads every row; 
getLauncherItemsByParentId (0xfacef9) keeps the requested parent and skips
m_is_deleted/m_is_out_of_sync unless requested; NovoLauncherDataProvider::
getLauncherItems (0x107a0c4) is only a vtable forward. common/launcher_db.c
reads the real table read-only (dumped50 rows, live TV54 rows) and mod_settings
publishes a reloadable launcher.db service plus the launcher-items CLI. The
shipped SequentialQuery/MemberAccess engine is not ported and no UI consumes
the rows yet.

LAUNCHER-ITEM implements LauncherItem::LauncherItem(NovoLauncherItem const&)
(0xe4794c, Thumb, 500 bytes): the settingsDB resource conversion used by
LauncherDialog::getLauncherItemsList (calls at0xedb2ac/0xedb40a) after
NovoLauncherDataProvider::getLauncherItems reads the 26-column LauncherItem
table. It copies17 word fields and4 bytes, constructs10 UStrings plus one
default at+0x2c, copies the shared_ptr control/raw pair at+0x8c/+0x90 with a
control+4 refcount increment, initialises+0x94=-1 and+0x98..+0xa0=0, and when
the thumbnail-name UString at source+0x3c is non-empty builds
"/buffer/downloaded_images"+"/"+name at+0x2c. The body does cbz on isEmpty, so
the path block runs for non-empty names; an initial inverted reading was caught
by the differential. 8 native/C cases compare trace,164-byte output and
refcount; host/ASan/ARM-qemu/TV CPU and verify.cjs38 ok. The SQLite
adapter/provider is not implemented; UString ops and the shared_count
destructor remain stubs. The C side always takes the atomic refcount branch.

SVG-WIDGET-STATE implements the widget-state qualification renderItems uses:
SVGWindow::setFocus (0xeeebd4) remaps 0->1/2->3 when focused and 1->0/3->2 when
unfocused at +0x6c; SVGWindow::setActive (0xeee4ec) has an inverted argument
(zero activates 0->2/1->3, non-zero deactivates 2->0/3->1); SVGTextBox::setFocus
(0x10c8a28) saves the state, forwards, and only on a state change with
UIWindow::isCreated calls drawTextBox then performTextSlideAnimation;
SVGTextBox::setElipses (0x10c6f48) ignores switching on when the +0x220 mode
word is5 but still stores off at +0x1b1. 36 native/C cases compare traces and
resulting state after 0xAA poisoning; host/ASan/ARM-qemu/TV CPU and verify.cjs37
ok. All four bodies are Thumb (force-thumb required). Vendor redraw/slide calls
remain stubs; live resources, scrollbar/skin and display gates remain open.

SETTINGS-ROW-CONSTRUCT implements SettingItemsListBoxRow's constructor
(0xd53d0c, Thumb, 0x4cd bytes): base SVGListBoxRow plus 17 children at fixed
offsets (textboxes 0x104/0x3c4/0x1ca8/0x1f68/0x2228/0x24e8, button0x684,
selection0x1014, slidebars0x13c0/0x1498, progressbars0x1570/0x1724,
textentry0x18d8, checkbox0x27a8, images0x2b28/0x2c0c/0x2cf0), 10 UStrings
(0x2dd8..0x2e3c, 0x2e24 default), and 47 default fields including per-child
name-id defaults and 0x2dec=27/0x2df0=-1/0x2df4=1/0x2e49=1/0x2e68=0x234. The
mirror probe rewrites 0x2e4c/0x2e50 to120/119. 96 native/C checks (2 mirror
states x48) and the ordered call trace pass; host/ASan/ARM-qemu/TV CPU and
verify.cjs36 ok. Child ctor internals/vtables remain outside the differential.
Visibility internals, live resources, scrollbar/skin and display gates remain
open.

SVG-IMAGE-POINT-IDM implements SVGImage::create(int,EASIPoint const&,UIWindow*)
(0x10c9554): setId, supplier resource gate, rectangle x/y from the point; with
a parent, three getClientRect calls add the client origin and clamp
width/height (resource words +8/+12) to client.w-x / client.h-y; without a
parent the resource sizes are added directly; then imageCreate. Also implements
LauncherHelper::convertOperationIdToIDM (0xe4ae0c): a 62-entry signed 16-bit
table at 0x14efd84 with -1 for unknown operations. svg-image is now 80
native/C cases and the launcher group 536 comparisons; the point body
(CRC 0x09f01190) and IDM body (CRC 0xf2156f40) are copied verbatim. Vendor
getClientRect/EASIRect ctor/imageCreate remain stubs. Row construction, child
visibility internals, live resources, scrollbar/skin and display gates remain
open.

SVG-IMAGE-CREATE implements SVGImage::imageCreate (0x10c9430): set parent
window and window rect; when created, resizeUISurface + targetHandle +
drawImageResource(res,0,0,byte0xa8,target,word0xac,float0xb4,float0xb8,
word0xbc) + release; when uncreated, createSurface(this,rect,1,0) +
setUISurface, targetHandle and either drawImageResourceScaled (byte0xb0 set)
or the full draw, release, then UIWindow::create. 72 svg-image native/C cases
(16 new) pass. The differential retargets three targetHandle BLs into the
body page because 0x1149000 is held by an earlier group, and dereferences the
shared_ptr pointer so both sides compare the token. Vendor surface/draw calls
remain stubs. A second overload, SVGImage::create(int,EASIPoint const&,
UIWindow*) at 0x10c9554, is recovered but unimplemented.

SVG-IMAGE implements SVGImage::create (0x10c9638) and SVGImage::destroy
(0x10c8ef4) orchestration in common/svg_image.c. create: UIWindow::setId,
SVGContext::getImageSupplier, the supplier's vtable slot2 resource lookup
(which gates everything), a copy of the rectangle, an optional parent
client-rect offset into words 0/2 (x) and 1/3 (y), then imageCreate. destroy:
SVGWindow::destroy then SVGSurface::setCleanUISurface at this+0x9c. Verified by
18 native/C cases against the verbatim bodies copied at their original
addresses, with a fake supplier object/vtable and the shipped EASIRect::offset
fragment. Boundary: imageCreate (0x10c9430, the SVGSurface/UISurface/
drawImageResource raster and actor subsystem) remains an ops callback, as do
SVGWindow::destroy, setCleanUISurface, setId, the supplier and getClientRect.
Row construction, child visibility internals, live resources, scrollbar/skin
and display gates remain open.

SETTINGS-ROW-SELIMAGE implements the row-level setSelectionBoxImageId
(0xd54374) instead of a stub: store the id at row+0x2e54, look up the "image"
rectangle, destroy row+0x2cf0 unconditionally, then re-read the id and create
from rect+8 when non-zero. The shipped 140-byte body is copied verbatim and
called by the copied renderItems case-0x3 path; only its stack-guard and key
literals are redirected. A 15th presence key ("image") widens the sweep to
1770796 native/C comparisons. Two harness issues were fixed: the key literal
is a pointer-to-pointer (redirect must target a const char* slot) and
verify.cjs's 30s qemu timeout was too short for the doubled sweep (raised to
180s; ~33s run). SVGImage::destroy/create internals remain stubs, as do row
construction, child visibility internals, live resources, scrollbar/skin and
display gates.

LAUNCHER-IMAGEID implements the real LauncherHelper::getImageId
(common/launcher_image_id.c): a 62-entry tbh switch where op>61 returns -1,
state==1 selects the alternate id and every other state takes the default.
Ops 30/31/36/37 share the -1 block, 42/56/57 ignore state, 20-23 and 43-44
share 781/782, 32/50 share 799/800, 46/47 share 763/764, and op52 is
asymmetric (825 vs 790). 469 native/C comparisons call the verbatim shipped
body (guard literal 0x01791d68 backed by page 0x1791000) and match exactly;
the render ops glyph_id hook is proven to accept the real function. The
adjacent convertOperationIdToIDM (0xe4ae0c) is separate and still open, as are
setSelectionBoxImageId/SVGImage internals, row construction, child visibility
internals, live resources, scrollbar/skin and display gates.

SETTINGS-ROW-17-19 closes the renderItems switch: types 0x17/0x18 are
progressbar+text2 and 0x19 adds image1/image2 first. Percentage is
100*(row[0x2dfc]-row[0x2df0])/(row[0x2df4]-row[0x2df0]) with 32-bit wrap and
truncating signed division (the shipped __aeabi_idiv at 0x1285478); bars are
row+0x1570 (0x17) or row+0x1724 (0x18/0x19). 0x17 alone consults the prologue
alexa rect (setFont 0x1e on the text box; four 0x26a names + ball image 892 vs
names 0x35/0x38/0x36/0x37 + images 169/169/170/170), and row byte 0x2e46
gates names/balls. 0x18/0x19 always write names 0x1bd/0x1be/0x1c0/0x1bf and
apply setDoubleDirectionalBar(1) when 0x2e46 is set. 0x19's image refresh sets
setVisible(1) on both, unlike 0xb. 886060 native/C cases pass; every
renderItems case 0..0x1a is now implemented. getImageId's real mapping, image
internals, row construction, child-visibility internals, live resources,
scrollbar/skin and display gates remain open.

SETTINGS-ROW-11-15 closes the remaining text-entry variants 0x11..0x15
through the shared parameterised body: selectionbox key via their own
literals (0xd55498, and 0xd5574c for 0x14), child row+0x18d8, names
0x2ec0/0x2ebc/0x2ec8/0x2ec4 in slots 1/0/3/2, create(0,0xc,0xd,empty). Mode/max
pairs are 3/10, 4/5, 5/none, 2/4, 9/15 — 0x13 returns right after setEntryMode
with no max or cursor call — and the created source is setText on row UStrings
0x2e0c/0x2e14/0x2e1c/0x2e24 except 0x15, which calls
setNumericContents(row[0x2e00]). 394384 native/C cases pass. The ops surface
now routes every row UString through the generic row_ustring. Cases 0x17..0x19,
construction, child-visibility internals, live resources, scrollbar/skin/
visible gates remain open.

SETTINGS-ROW-C implements renderItems type 0xc: four lookups
(slidebar/text2/text3/text4) through its own literals; text2 drives row+0x1ca8
with the formatted row[0x2df8] value; text3+text4 (both rects required) drive
row+0x1f68 and row+0x2228 from row UStrings 0x2e2c/0x2e34, with alignment 6
and setElipses only on 0x1f68; the shared row+0x1498 slider, when uncreated,
does setFocus(0)+create+value with NO name writes (0xd55872). 312224 native/C
cases pass under the full 13-key presence mask and separate image/textbox
state passes. The callee audit caught SVGTextBox::setTextAlignment
(0x10c72a8) having no trampoline. Cases 0x11..0x15, 0x17..0x19, construction,
child-visibility internals, live resources, scrollbar/skin/visible gates
remain open.

SETTINGS-ROW-B implements renderItems type 0xb: "slidebar" then its own
"image1"/"image2" literals; the image block requires BOTH rectangles and
forks on each image's own isCreated, refreshing with plain setWindowRect (no
point offset) or re-creating from row[0x2e4c]/[0x2e50] — including the quirk
that a created image1 plus an uncreated image2 re-creates image1. The
row+0x1498 slider uses 0x214/0x216/0x215/0x217 and finishes with
setSliderValue(row[0x2df8]). 74604 native/C cases pass. The harness now
distinguishes the two image children's created state from the row child, with
a dedicated type-0xb pass covering the mixed state. Case 0xc, 0x11..0x15 and
0x17..0x19, construction, child-visibility internals, live resources,
scrollbar/skin/visible gates remain open.

SETTINGS-ROW-A implements renderItems type 0xa, the second slider row: its
own "slidebar"/"text2" literals, the 0x1e8/0x1e9/0x1ea/0x1eb text-box names in
slots 1/0/3/2 with no font, the formatted row[0x2df8] value, and the
row+0x1498 slider with 0x214/0x216/0x215/0x217 names plus
setSliderRangeValues on the created path. 70456 native/C cases pass. A full
callee audit preceded the sweep (fourteen direct call sites plus both shared
created blocks) and needed only the case's own key/label literal redirects.

SETTINGS-ROW-COVERAGE corrects a silent harness hole: the presence mask
iterated only 7 bits, so image2/banner/slidebar/text2 were never present and
type 0x9's case body never executed. The sweep now has a full 11-key pass and
a separate point/word/label/flag pass, and type 0x9's created slider
(setWindowRect + setSliderRangeValues(row[0x2df0],[0x2df4]) +
setSliderValue(row[0x2df8])) is implemented. 66312 genuine comparisons pass.
A third NUL-sled was caught: SVGSlideBar::setWindowRect (0x10c4790) had no
trampoline and slid into the range stub. Enumerate every callee before
sweeping a new case.

SETTINGS-ROW-SLIDER implements renderItems type 0x9: the "slidebar" then
"text2" lookups (that order), a row+0x1ca8 text box created with the empty
char* label and extra 1 that receives the %d-formatted row[0x2df8] value, and
a row+0x13c0 slide bar created from row[0x2df0]/[0x2df4] then setSliderValue
from 0x2df8. 95624 native/C cases pass. Corrections landed: types 5/6's
rebuild fold reads text1 (slot 12), not image1, and setScalingType receives
the stale isCreated result while only create gets 0x92/0x93. Cases 0xa..0xc,
0x11..0x15, 0x17..0x19, construction, child-visibility internals, live
resources, scrollbar/skin/visible gates remain open.

SETTINGS-ROW-BUTTON8 implements renderItems type 0x8, the type-4
key1-missing button branch as its own case: "button" lookup, row+0x684 child,
0x2eac/0x2eb0/0x2eb4/0x2eb8 names in slots 1/0/3/2, icon-0 create and a
created setWindowRect+setText refresh, with no images, glyphs, stores or
draw-on-top and no rebuild branch (so no crash input). 43976 native/C cases
pass; no new callbacks were needed. Cases 0x9..0xc, 0x11..0x15, 0x17..0x19,
construction, child-visibility internals, live resources, scrollbar/skin/
visible gates remain open.

SETTINGS-ROW-BANNER implements renderItems type 0x7: a direct "banner"
string lookup (0xd54b30 holds the char*, unlike the pointer-to-pointer keys),
row+0x684 button, no destroy/images/stores, type-5 names, four glyph ids,
elipses 1 and shadow 0x94, icon-carrying create, created refresh with
setWindowRect only (no setText) and setDrawOnTop(1). 40904 native/C cases
pass. The rebuild path folds the text1 rectangle and faults when it is null.
No new callbacks were needed. Cases 0x8..0xc, 0x11..0x15, 0x17..0x19,
construction, child-visibility internals, live resources, scrollbar/skin/
visible gates remain open.

SETTINGS-ROW-BUTTON implements renderItems types 0x5/0x6, the launcher
button plus two images: three rectangle lookups, refresh-or-destroy+reinit,
type-5 vs type-6 name sets in slots 1/3/0/2, four glyph ids from
LauncherHelper::getImageId(row[0x2dd4], state), highlight/sub-launcher/
elipses/shadow ids, a create carrying the icon id, inline 0x92/0x93 scaling
stores and setDrawOnTop(1). 38216 native/C cases pass. Two ABI corrections:
getImageId takes only (op,state), and SVGButton::create needs the icon
argument. The rebuild path faults with a null image1, so that input is
excluded. getImageId's real 62-way mapping stays a controlled stub. Cases
0x7..0xc, 0x11..0x15, 0x17..0x19, construction, child-visibility internals,
live resources, scrollbar/skin/visible gates remain open.

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
empty string. The old divide-zero claim is withdrawn by UI-REVIEW: every zero
divisor raises; replacement returns EDOM explicitly. Case bodies 2..26, construction/visibility-
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
