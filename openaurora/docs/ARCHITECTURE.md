# openaurora architecture

A modular, reloadable replacement for vendor Aurora on this fixed MB230 firmware.
First reproduce vendor behavior/screens, then implement native OmarchyTV.
Telemetry, advertising, ACR/tracking/profiling and sponsored delivery are excluded
even as optional modules. All other TV functionality remains parity scope.
[TODO](../../TODO.md) owns unfinished work; [FINDINGS](../../FINDINGS.md) records
corrections to older design reports.

## Current status

The installed TV/recovery build is ABI 1, native boot disabled, vendor UI running.
Development uses ABI 3 in build-tv-v3; historical ABI 2 build-tv-v2 is preserved.
Do not mix them. Cold independent scanout,
complete input/power/audio/source/settings behavior and full native UI remain
unverified. [MEMORY](../../MEMORY.md) owns the last verified state.

Framebuffer drawing, a selected-frame comparison and runtime --check are separate
milestones. [Current OSD evidence](../../artifacts/reports/OSD-BUFFER-CONTRACT-2026-10-31.md)
supersedes the old dumb-buffer, palette-handshake and init-does-SETPLANE plans.

mod_ui_vendor paints the measured portal through the display service, moves
rail/card focus from input/key, and publishes ui.portal for the same rectangles.
Select opens that tile, Menu and Exit return to the portal, and Aux reads
m_current_source through settings. mod_ui_anim publishes ui.anim v2 and requires
ui.clock. mod_ir publishes input v1; feed() is the UART parser and an empty
ir.device leaves the port closed. UI-PARITY records the offline test. Labels,
other screens and cold scanout remain open.
UI parity includes animations/transitions, focus feedback, scrolling and other
vendor UI/UX effects. Firmware/assets must supply timing/easing/geometry and
interruption behavior. Static image similarity does not validate these effects;
acceptance needs frame sequences and navigation behavior.

`mod_ui_clock` requires ticker's raw `clock` and publishes `ui.clock` v1.
It preserves the recovered FROST_Time 90,000,000-ms rollover and 12,343 correction
quantum separately from ordinary 64-bit monotonic time. Owned sampling timers
quiesce through paired hooks; reload saves numeric state. Offline DSO tests pass;
mod_ui_anim is the list/dialog controller on this clock. Text-slide and rail
consumers and the display-v2 full-screen scene are implemented. UI-SCENE-REVIEW
corrects fan interpolation, linear filtering and alpha fade and records actual
fb1 draw/restore tests. General CPU transforms remain slow; independent startup,
smooth visible frame sequences and cold/resume acceptance remain open. See
[UI-ANIMATIONS](../../artifacts/reports/UI-ANIMATIONS-2026-10-31.md) for arithmetic,
strict boundaries and the unresolved absolute-phase/bootstrap limits.

Pixel formats are boundary-specific: all measured raw 32bpp OSD framebuffers are
BGRA (B,G,R,A), including fb1; OAI1 assets and display blit inputs are straight
RGBA. The display backend converts RGBA inputs into BGRA framebuffer bytes.

`common/scene_gles.c` is a lower synchronous owner, used by the separate native
renderer and opt-in diagnostics. EGL-SCENE establishes its pbuffer/pixel lifecycle;
EGL-WINDOW establishes direct display1 presentation with the retained4-byte
native window, process-local primary-buffer profile and cached texture. The
owner confirmed smooth full-screen effects at the diagnostic's50Hz pacing;
the full original framebuffer/region/MAU/pan were restored. GPU readback is
bottom-up premultiplied RGBA; native-window output was independently checked
as BGRA. Production display services still use the CPU backend.

`mod_graphics` now publishes `display.gpu` v1 over the separate EGL-free bootstrap
and late-loaded renderer. Explicit graphics.broker/graphics.renderer paths are
required; optional executable/renderer build targets are not a deployment.
OPEN/frame/take/cancel use copied completions, caller-owned immutable image fds,
cache revisions and host-module-id public tickets. A static DSO counter alone
cannot prevent reload aliases. Owned watch/reap timer keep ABI3 cleanup
nonblocking; abnormal child death retains a faulted provider until native release/
recovery is understood. Four host/sanitizer/TV lifecycle groups and actual GPU
service frames pass; see [GRAPHICS-MODULE](../../artifacts/reports/GRAPHICS-MODULE-2026-11-01.md).
Source builds18 modules; the preserved matching production ARM stage remains16.

Optional `scene_display` consumes/holds display.gpu, publishes an RGBA drawing
canvas as display v2 and separate display.async request/latest receipts. Its one
inflight/one desired frame queue keeps immutable snapshots, caches image revisions
and drains final requests after animation timers stop. Consumer reload/activation
uses new receipt/revision namespaces even when the GPU session remains open.
Four host/sanitizer/TV groups and an actual101-request burst/final receipt pass;
see [SCENE-DISPLAY](../../artifacts/reports/SCENE-DISPLAY-2026-11-01.md).
Load it as an alternative to CPU display, never alongside it. No automatic
selection/display.renderer option exists.

Both display backends also publish optional display.alpha v1, a clear_rgba
companion that preserves existing display v1/v2 ABI and opaque clear semantics.
Canvases are straight RGBA with source-over including destination alpha; CPU
identity/transition and GPU output are premultiplied BGRA. ui_vendor owns/releases
the companion hold through its display lifecycle and uses the recovered portal
backdrop RGBA36,38,40,153. Legacy providers retain the old opaque fallback.
UI-TRANSPARENCY records firmware conversion, host/sanitizer coverage and actual
GPU BGRA24,23,22,153 readback; independent real-video visual parity remains open.

ui_vendor now holds display.async and owns its completion sampler. It tracks the
first accepted serial in the provider namespace and acknowledges ui.ok_file only
after an actual successful completion; queue/OPEN/errors/old receipts cannot
acknowledge. CPU present semantics remain unchanged. Async transform-only dialog
progress reuses pixels; real title/rail updates and completed screen changes
still repaint. Removal/suspend/failed resume clear both service aliases/holds.
[UI-ASYNC](../../artifacts/reports/UI-ASYNC-2026-11-01.md) records226 host checks,
sanitizer/TV contracts and actual portal/transition/UI reload/application-module
cycle with full restoration. Installed assets lack the recovered rail_hl frames;
that animation, full vendor visuals/actions, physical input, TV standby and cold
ownership remain open. The10ms sampler does not mandate100Hz scanout.
tvshot decodes raw BGRA and saves ordinary RGB PNGs, which must not be swapped
again. RGB565 and other vendor/API buffers retain their independently established
formats. Pre-correction tvshot captures have red/blue swapped; preserve provenance.

## Layout and dispatch

| location | purpose |
|---|---|
| include/openaurora/module.h | host/module C ABI and compiled ELF note |
| include/openaurora/events.h / services.h | event payloads and service contracts |
| core/host.c | single-thread dispatch loop, registry, timers, watches, posts |
| core/module.c | dlopen/dlsym and lifecycle |
| core/control.c | Unix socket / stdin command plane |
| core/check.c | read-only ELF/loader preflight before host initialization |
| modules/* | mod_<name>.so implementations |
| tools/boot/oa-supervise.c | separate native-child recovery supervisor |

Callbacks run on the host event-loop thread. Workers may only call post(), using
an owner token captured during init through owner_id(). Queues are mutex
protected; dispatch restores owner attribution and pops one callback at a time.
After successful quiescence, stop/unload/failed init/start close acceptance and
purge queued callbacks. A nonzero optional quiesce() retains the instance, code,
posts and registrations for completion and a later stop/unload retry. It must
not block. Start/resume/suspend are rejected while cleanup is pending; reload
may retry the gate but cannot replace the instance while that gate is busy;
status reports cleanup_pending. --force cannot bypass the gate. Failed init
remains discoverable for unload and can never activate. Failed replacements
also remain rejected if cleanup cannot finish. Gates tolerate partial/NULL ctx
and repeated calls, including unload after stop. A veto may begin quiescence,
so it does not promise that ordinary module service continues.
Modules still must join workers and reclaim undelivered borrowed userdata before
cleanup returns. Never unload a module while its code is on the stack.

ABI 3 validates descriptor magic/version/size; old/short layouts reject before
unavailable fields are read, compatible extended prefixes are accepted. Use
OA_MODULE_EXPORT(descriptor) to emit oa_module_entry and the retained ABI note.
This is compatibility checking, not isolation from malicious pointers or crashes.
The version bump prevents an ABI 2 host from silently ignoring quiescence.
Host shutdown now stays on the loop thread. A quiescence veto is retried after
completion posts, module timers and module watches run. Return happens only
after every module unloads, or with an error for a non-quiescence hold without
module loop work. A busy gate may wait indefinitely for a future worker post;
the host cannot infer from an empty queue that no completion is possible.
That error does not dlclose. Load, start, reload, suspend and
resume refuse once shutdown has started. --force is not used. This is offline
lifecycle coverage. Offline device workers are a later injected-I/O layer.
See SHUTDOWN-2026-11-01, OWNER-WORKER-2026-11-01 and TODO.

## UI engine and skins

The generic UI engine is its own reloadable module (`modules/ui_engine`), so the
reverse-engineered vendor screens and a later OmarchyTV skin share one engine.

| piece | owns |
|---|---|
| ui.engine | display/alpha discovery and rebinding, OAI1 asset loading from `ui.dir` (lazy, bounded cache), clear/fill/blit/blit_scaled, present/completion receipts and the readiness receipt, the key topic, the `ui.fps` repaint timer, skin registration/`ui.skin` |
| ui.engine service v2 | v1 drawing prefix plus `register_skin_dispatch`/`dispatch_skin`; legacy `register_skin` rejects with ENOTSUP |
| ui_vendor | the renamed vendor skin: portal rail/cards, source selection, dialogs, animations, its own screen state and `OA_SVC_PORTAL` |
| ui_omarchy | the OmarchyTV skeleton over the same engine: dark shell, accent rail and placeholder tiles, with its own key semantics; no vendor assets and no published service yet |

Implemented in the 2026-11-01 split: the engine holds display/display.async/
display.alpha, writes the readiness receipt, subscribes the key topic and owns
the `ui.fps` repaint timer. The reviewed engine publishes ui.engine v2;
`ui.skin` selects by name. Each skin registers its own module-lifetime
trampoline/data with `register_skin_dispatch`; it invokes `dispatch_skin` under
the skin owner. Only skin code is queued under that owner, so forced engine
removal leaves no engine code or heap payloads in its queue. Registration resets
readiness and the scene transform; a serial epoch excludes old completions.
`present_pending` returns1 pending,0 completed,-1/errno failed. Build matching
v2 engine/skins; legacy registration fails ENOTSUP. UI-ENGINE-REVIEW records242
host checks, focused sanitizers/native contract tests and real GPU restoration.
The engine
loads icons lazily; the vendor skin probes `rail_hlNN` at start to keep the
vendor highlight's missing-asset behavior. `ui_vendor` is the renamed vendor
skin; `ui_omarchy` is a working OmarchyTV skeleton (dark shell, accent rail,
placeholder tiles, no vendor assets). tests/ui_skin.c switches both ways at
runtime with no rebuild; `make check` loads `ui_engine` before the skin in every
UI test and tests/recovery.py; all 233 checks pass.

Host control now supports `skin NAME` (pending request) and `skin` (query), using
the new copied ui.skin v1 status companion. After autoload, main selects
configured ui.skin, an existing skin, or vendor. Providers/ui_engine must already
be loaded; the host loads ui_NAME. Missing/start/enter/first-frame failures load
ui_vendor only after failed-candidate cleanup. Holds/quiescence are never forced.
ui.skin_timeout_ms defaults5000, accepts100..60000, and only bounds entry/first
frame. Busy cleanup remains pending until safe. Standby rejects pending selection;
shutdown cancels it. Runtime selection is memory only and does not rewrite config.
SKIN-SELECTION records249 host checks, sanitizer/native failures, actual Unix
socket startup/control and real GPU switching/fallback/restoration. This is
OpenAurora policy, not process-crash recovery or a vendor hardware behavior.

Still pending: real OmarchyTV screens and more general screen-stack policy.
The vendor skin remains the known working fallback. Example control replies:

```
skin omarchy
pending omarchy
skin
requested omarchy active omarchy state ready fallback 0 error 0
```

## Lifecycle and reload

init → optional restore → start → suspend/resume → stop → destroy → dlclose.
Start publishes services/watches/timers/subscriptions; stop drops holds and hardware.
The host tracks registrations and cleans owned entries, including failed init.

lookup records a provider hold; stop/unload refuse while consumers hold it.
Full hold tables fail lookup. --force deliberately bypasses that protection
and can leave consumers with stale pointers. It is not a safe-reload promise.

reload saves up to 64 KiB, unloads, loads and restores while preserving lifecycle
state. Running replacements start; loaded/stopped replacements remain loaded.
Suspended replacements run init/restore only, with first start deferred until
resume. They do not start and immediately suspend hardware during reload.
Starting an ordinary suspended instance resumes it rather than starting twice.
A failed deferred start cleans up and stays suspended for an explicit retry;
stopping/unloading before activation does not call stop on a never-started instance.

State allocation/oversize/growth failures cancel before unloading the old instance.
Restore errors, missing restore for nonempty state, changed module identity and
replacement startup failure reject and clean the candidate. After the old instance
has unloaded, failure leaves the module absent: there is no rollback to an old
binary or atomic replacement guarantee. --force still permits stale service
pointers. A module segfault still kills the process; hardware recovery has separate limits.

Suspend/resume order follows declared required services plus actual lookup holds.
The host snapshots service/provider names before callbacks release or unpublish
them, preserving dependencies through suspended reload into another slot. A
republished service resolves to its current provider. Consumers suspend first;
providers resume first. Active declared consumers also prevent normal provider
stop/unload/reload even without a lookup hold. Start/resume requires ready providers,
not merely a published interface belonging to a suspended module.

Cycles reject the transition before callbacks. A callback failure stops the
batch and returns error through the socket/CLI; earlier successful callbacks
remain transitioned, with no automatic rollback. A still-active consumer without
a suspend hook prevents its provider from sleeping. Snapshot capacity is bounded
by MAX_SVCS + OA_REQUIRES_MAX per module; it is host-internal, not an ABI change.

Manual suspend/resume order participating hooks; these commands do not enter RAM.
Coordinated standby uses a separate final entry operation after all active modules
have prepared. Full native device preparation/wake ownership and hardware
acceptance remain unfinished.

The internal oa_module_sleep_cycle engine now supplies a final entry callback,
per-cycle transition ledger and provider-first restore after preparation/entry
failure or wake. It returns separate preparation, entry-attempt/result and
restoration results. It leaves preexisting suspended modules alone, rejects
missing paired hooks before callbacks, pins all module lifecycles against nested
mutation, and restores after a pending shutdown cancels entry. Independent restores
continue after an error; consumers still require successfully restored providers.
The host publishes a per-host `host.sleep` v1 interface (one registry slot, owner
0); mod_power requires and holds it. Public module/host ABI layouts are unchanged.
Its power service and CLI standby command both use this coordinator. mod_power's
ordinary hooks own no persistent fd/worker and do not sleep; its final callback
syncs, opens /sys/power/state, writes mem once and closes. Only full writes count
as accepted requests; neither this count nor a return code proves physical sleep.

The notified cycle emits STANDBY before preparation while callbacks are still
active, then ON only after restoration of its prior state (including intentionally
suspended modules). Failed restoration emits RESUMING, never ON. Lifecycle mutation
and recursive standby are blocked through notifications. Preflight refusal emits
no events. Manual suspend/resume are hook-only and do not emit these notifications;
manual repair after a failed cycle does not yet announce a new ON state.

Seven integration groups run actual mod_power callbacks with wrapped power I/O
and dynamically loaded dependency fixtures on host/sanitizer/ARM-qemu. Actual
native device/MCU/network preparation, remaining paired hooks and hardware
acceptance are unfinished. Do not add guessed no-op hooks merely to make the
cycle preflight pass. No TV deployment or physical standby test was performed.

Control commands over stdin or Unix socket:
list, status NAME, load/start/stop NAME, unload/reload NAME [--force],
suspend, resume, standby, loglevel N, quit. `standby` invokes the power service;
`suspend`/`resume` only run module hooks.

List/status formatting tracks bytes actually stored, so caller buffer truncation
cannot turn snprintf's desired length into an out-of-bounds cursor. Socket/stdin
replies have a 16 KiB budget, sufficient for full current module/service tables
with ordinary version labels. Tiny caller buffers receive a terminated prefix
(zero capacity writes nothing).

Unix clients have nonblocking, close-on-exec fds and bounded 32 KiB output queues.
The host reserves room for a full reply before executing another command, pauses
reads when that space is unavailable, and resumes after output drains. Short
writes retain their suffix; EINTR retries, EAGAIN waits for write readiness, and
MSG_NOSIGNAL isolates broken clients. Half-close drains complete requests/replies;
incomplete EOF input is discarded. Quit remains immediate, with a best-effort
reply if the requester has a backlog.

Requests allow 511 bytes before newline. Overlong or NUL-containing lines are
rejected whole, including across reads; their tails cannot execute commands.
Stop removes client/listener/stdin watches and closes owned fds. Socket cleanup
checks inode identity; startup refuses live sockets and regular files but can
replace a refused stale socket. Startup failure is reported before autoload.
Console stdout still uses synchronous writes; blocked/broken-console handling is
separate unfinished work and is not covered by the Unix socket guarantees.

## Reusing fixed vendor libraries

TV modules directly link the firmware libraries. Host and musl sanity builds
use OA_NO_VENDOR stubs. Current VENDOR_MODULES in Makefile:
plat, panel, audio, source, display. The loader uses RTLD_LAZY | RTLD_LOCAL;
this is not permission to leave unresolved imports. --check forces relocation
diagnostics and parses failures even if the loader returns zero.

scripts/fetch-vendor.sh extracts NDAL3, NDAL_EX, asound, v4l2, v4lconvert, nteec,
mhash and sqlite from the dump. The complete HAL closure includes V4L2 imports
formerly supplied by the vendor app. TV RPATH is /vendor/lib:/vendor/lib/libs.
Settings links sqlite separately. Inspect Makefile for the exact current list.

Defined distinct dynamic functions: 321 Hal_* in NDAL3, 80 NDAL_* in NDAL_EX
(recount with nm -D --defined-only, type T/W, unique names). PLAT/DEV/ICE
implementations live inside Aurora; symbol names are not callable shared APIs.
The stable boundary still needs verified argument layouts, init ownership,
error semantics and device acceptance. Use the maintained vendor_pwm.h;
saved Ghidra prototypes previously gave incorrect word counts and return types.
vendor_audio.h now records the instruction-verified scalar audio returns.
plat owns the Protect/Mixer/Proxy prelude; audio holds the ready platform and
does not repeat initialization on reload. Partial-init restart, amp routing and
audible acceptance remain open. CEC must follow the applicable spec. Framing,
System Audio status and TV/Audio System ARC direction helpers stay in common/cec.c.
common/cec_policy.c is the route: mod_cec publishes cec.audio v1, and mod_audio
refuses local volume and mute while that service says local volume is not
permitted. Requests do not open ntkcec or call the HAL. Native transport and
electrical ARC remain unfinished. See CEC-AUDIO-CONTRACT and OWNER-WORKER.
CEC-TRANSPORT-CONTRACT records the private V4L2/kernel path and the tested
raw/VPL packet layouts in vendor_cec.h. VPL receive copies 16 bytes to the operand
buffer and its wrapper loses driver acceptance status. The driver filter also
rejects some spec-valid frames, while close/reopen has fd-sentinel defects.
Do not use blind NDAL Init/Close for reload; recover a stable device lifetime
and authority boundary. common/cec_device.c implements a blocking native raw
backend with explicit fd ownership, correct hardware-close argument and retained
state if hardware close fails. common/cec_authority.c and common/cec_owner.c
perform that authority step and the blocking calls on one worker, using only
injected I/O. mod_cec publishes cec.owner and cec.audio, and reports hardware open unsupported.
Suspend and resume join and recreate the idle worker; the service stays
published. A failed close waits for another quiesce generation and does not
post. Idle and open owner quiesce now return busy without waiting, until exit
is observed. A lifecycle client retries stop/unload/reload/suspend explicitly;
the host shutdown loop dispatches completions and retries internally. Host,
ASan/UBSan, ARM/qemu and the full host suite pass. Live open remains
unaccepted. OWNER-WORKER-2026-11-01.
CEC-OWNERSHIP-2026-11-01 records PreInit/real cdev registration and
the cleanup contract; a void module stop hook cannot erase a failed close.
CEC-LIVE-2026-11-01 records shared-owner Steam Deck queries and spec-based
identity validation; this is not independent backend/policy acceptance.
AUDIO-ROUTING-2026-11-01 corrects the assumed AD82586C main amp: the verified
live object/vtable selects AD82587_D. Decoder main volume, per-speaker EasySet
gain and amplifier register volume have different paths/scales. New audio-state
diagnostic decodes saved AOUT cache offline; board GPIO/I2C resources, lifecycle,
UI-volume conversion and audible acceptance remain open.
AUDIO-BOARD-2026-11-01 adds verified GPIO/I2C resources and a successful real-TV
amp-probe register read. It uses its own /dev/i2c-3 fd and the recovered PreInit
two-message transaction, requiring count2 instead of NDAL's nonnegative check.
It does not perform amp initialization/reset, gain/mute writes or standby.
AUDIO-VOLUME-2026-11-01 adds pure common/audio_curve.c: exact-size AQ normal/
aggressive curve decoding, low16-bit narrowing and bounded lookup. Unit tests
plus all101 real capture comparisons pass host/sanitizer/ARM-qemu and on TV.
Not yet integrated with mod_audio/HAL; existing decoder volume does not implement
the complete UI speaker-gain/table/output/profile/amp lifecycle path.
AUDIO-CURVE-MODULE-2026-11-01 adds mod_audio_curve / audio.curve v1. It reads an
explicit AQ path/profile, publishes immutable copied data and rereads on start/
reload; no HAL. Six real-DSO lifecycle/hold/failure/suspended-reload groups pass
host/sanitizer/ARM-qemu and on TV. examples/audio-curve.conf is a local data-only
example. Native gain application, profile/default handling and audio lifetime
remain work; module reload does not reload the host configuration file.
AUDIO-GAIN-TRANSPORT-2026-11-01 refines audio status/lifetime: the scalar ABI is
correct, but message-backed HAL0 can hide send failure; ALSA speaker get reflects
cached requests and cannot certify DSP/audible application. Receiver copies the
table into kernel storage; AUDIO-NETLINK subsequently proves synchronous dispatch.
Optional mixer-probe read succeeds with ALSA_CONFIG_PATH=/vendor/etc/alsa.conf.
That runtime resource is separate from loader-only --check coverage.
AUDIO-NETLINK-2026-11-01 adds common/audio_netlink.c and three wire regression
groups (host/sanitizer/ARM-qemu). Native GetOPUGain reply passed on TV. Keep table
buffers through the syscall; operation errors are discarded and no ACK exists.
Codec is not yet in production audio. A fresh same-process binding can conflict
with the vendor HAL's existing protocol2 socket.
common/audio_client.c provides process/fd ownership, checked dispatch and
bounded validated queries. Five injected-I/O/fork groups pass host/sanitizer/
ARM-qemu and the full host suite; the maintained query passes on TV.
SPEAKER-GAIN-2026-11-01 adds a lower direct SetOPUGain encoder/client primitive:
device0..5, channels0..0x8000, checked dispatch without ACK or EINTR replay.
Three wire and now six session groups pass host/sanitizers/ARM-qemu/TV with
injected I/O only. No broker/module/CLI setter is exposed. This raw path bypasses
UI speaker out gain/Mod_Cell_SetVol AQ lookup, balance and ratio processing.
GetOPUGain remaps cached100 to0x8000 and cannot provide an exact rollback snapshot.
The matching16-module production ARM stage retains its prior tested identities;
only ARM unit binaries were rebuilt for this follow-up.
common/audio_owner.c serializes that session on one worker and rejects NULL I/O
with ENOTSUP before any socket call. mod_audio creates that owner but the
loop-thread bind_session service rejects all arguments until its asynchronous
service adapter exists; legacy owner RPCs still block non-loop callers. The host build defines OA_NO_VENDOR, so the HAL stays
off and suspend joins the idle worker. An enabled HAL has no recovered standby
path: suspend returns failure and the module stays started. That path is
tested with tests/audio_hal_stub.c, not the vendor library. Real
table/selection writes remain untested. Session close consumes the fd on OS
error; no CEC-style hardware teardown ioctl is involved. OWNER-WORKER.

The lower audio owner now also has bounded nonblocking *_async submissions and
take() value results (AUDIO-ASYNC). One accepted request or uncollected result
occupies its slot; further submissions and synchronous RPCs return EBUSY.
Requests receive instance-scoped tickets, copy the callback table and keep a
stable session table across later gain jobs. Callback context/code remain
borrowed through close or quiesce+join. No user completion callback or output
pointer is retained. The loop can poll take(); quiesce drains accepted work
before closing, and untaken value results can be discarded at destruction.
Production service wiring and native socket ownership remain open. This is
injected-I/O lifecycle evidence, not native audio acceptance.

The native socket boundary now has a separate executable, oa-audio-broker
(AUDIO-BROKER). Firmware Mixer_Ctrl_Init calls the PID-binding IfCmd initializer,
so the broker uses its own PID and loads no HAL. It accepts strict32-byte local
seqpacket requests, lazily opens the maintained native session, supports only
read-only GetOPUGain, and closes on IPC EOF. Its entry point discards unrelated
inherited descriptors. A real spawned helper/query/reap passed on TV. This is
the blocking child side only: the host still needs a nonblocking module/client
adapter, fd-watch ownership, ticket matching and WNOHANG child reaping before
unload. The codec in audio_broker_wire.c is shared by that future parent.
make all/install now includes the helper. No writable audio parity is implied.

mod_audio_mixer now adapts that child to audio.mixer v1 and the control socket
(MIXER-MODULE). It requires an explicit audio_mixer.broker path, publishes no
hardware setters and depends on no HAL/platform initialization. Request/result
I/O uses MSG_DONTWAIT, one pending/uncollected slot, ticket+device validation
and cancel/drain. Its watch and50ms WNOHANG reap timer belong to the provider,
not the consumer. Quiesce closes IPC and refuses destruction until the child
is reaped. Suspend keeps the service/holds, closes the child, and resume creates
a fresh child while preserving the ticket counter. A failed resume closes any
partial channel; unlike start failure it is not automatically stopped by core.
Consumers hold the service and cancel abandoned tickets before releasing it.

The commands are `mixer-query 0`,
`mixer-result <printed-ticket>` and `mixer-cancel <printed-ticket>`. A result
contains error/device/left/right; zero error is a query result, not an applied
audio ACK or UI percentage. Reload invalidates old tickets. Busy lifecycle
operations require retry and normal lookup holds still protect the provider.
examples/audio-mixer.conf is a manual opt-in path example, not a deployment.
That milestone's core+all16 modules passed TV --check; real socket query/reload/query
and standalone config/start/status/quit passed with stock UI still running.

UI-THEME-PROVIDER-2026-11-02 adds ui_theme (source now21 modules), publishing
ui.theme v1. common/svg_pack.c reads explicit ui_theme.index/ui_theme.binary
regular files into a fully validated immutable snapshot; common/binsvg.c parses
records and named rectangles. Service record(index)/named(CRC32) return borrowed
views: consumers hold the provider through all view/string use and drop them
before release/stop. Service calls perform no file/device I/O. Start/reload
rereads the configured paths; ordinary suspend retains the snapshot, suspended
replacement defers first loading until resume. Invalid input/collisions publish
nothing; failed replacement still lacks host old-binary rollback. Real-DSO
lifecycle and full702-record corpus pass host/sanitizers/ARM-qemu/native TV.
examples/ui-theme.conf is a data-only example. UI-THEME-SKIN-2026-11-02 adds the
first ui_vendor consumer: DialogLauncher marker lookup, first numeric mode wins,
with optional hold/removal/rebind and copied active mode. Its ui.launch_anim
diagnostic remains default0, not a claimed vendor launcher transition. All8
policy modes/lifetime paths and the live resource pass host/sanitizers/ARM-qemu/
native TV recorder/injected-progress tests. Widget/style/font/clip integration
and visible acceptance remain open. The preserved16-module stage and installed
ABI1 artifacts are unchanged.

Target libc spawn can return0 and a PID before exec fails with child exit127.
Direct firmware instructions and matching live libc hash confirm this; it is
not a qemu defect. X_OK precheck catches missing paths but cannot guarantee exec
or helper health. Reply EOF/reaping handles delayed exec failure. Tests cover
both immediate and delayed failure (including a missing script interpreter).

The CEC owner has the same one-request/result-slot contract (CEC-ASYNC):
acquire copies bounded path (255 bytes plus NUL) and authority/device tables,
send copies a frame, and take returns an owned receive value with explicit
1 frame/0 empty/-1 error status. The session's copied device table stays stable
through later jobs and failed hardware closes. Context/code remain borrowed
through release or successful quiesce+join. Quiesce drains accepted I/O before
closing and retains any untaken result. It cannot forcibly cancel an executing
ioctl. Service integration/native ownership and safe blocking-I/O completion
remain acceptance gates; no live CEC capability is advertised by this API alone.

## Build without overwriting the installed/recovery baseline

From openaurora/:

```sh
make BUILD=build-strict
make BUILD=build-strict check
./build-strict/openaurora --check --modules build-strict/modules
make vendor
make TARGET=tv BUILD=build-tv-v3 \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc
```

Default flags include C11, -Wall -Wextra -Werror -pedantic; preserve them when
overriding CFLAGS. make tv hardcodes build-tv; make clean also removes build-tv.
Use explicit TARGET/BUILD for ABI 3. make arm produces musl stubs for sanity,
not TV runtime linkage. make install stages host files unless built otherwise;
it is not a TV deployment procedure.

ccache: when installed, the default host `CC` is wrapped automatically
(`ccache cc`) and `make arm`/`make tv` wrap their cross compiler. An explicit
`CC=...` on the command line or in the environment is left untouched;
`CCACHE_DISABLE=1` disables the cache. Only object compiles are cacheable:
most module and test rules here compile and link in one `-shared` invocation,
which ccache treats as a link and cannot cache. Splitting those rules into
`.o` + link steps is the follow-up if the cache hit rate matters; see TODO.

For build-only iteration, `make -j$(nproc) all` parallelizes independent
targets. Do not add -j to `check`: its test groups share staging directories
and fixed ports and must keep running in their current order.

The set runs glibc 2.23 with /lib/ld-linux-armhf.so.3. The known Arm GNU 10.2
toolchain has glibc2.31, but compatibility depends on each artifact's actual
required versions and closure, not just compiler age. GLIBC2.34 startup symbols
and GLIBC2.28 fcntl are known pitfalls. Prior rebuilt artifacts required at most
GLIBC2.17. Recheck readelf -V and --check after changes.

For ordinary ARM process tests, qemu-arm-static -L <TV-root> supplies guest libc;
it supplies no TV hardware. For --check, child execve uses the absolute loader:
a read-only bwrap namespace with guest /lib, /etc/ld.so.cache and /vendor/lib
was used, plus the host ARM binfmt handler. -L alone does not redirect that
child loader. See [RUNTIME-CHECK](RUNTIME-CHECK.md) and saved analysis logs.

## Hardware safety and recovery

Hardware APIs may have process-global state and no reference counting; module
reload must assign init/terminate ownership rather than repeat calls blindly.
Do not run hardware-mutating modules alongside stock Aurora. Read-only preflight
does not enter modules and is a separate, authorized operation.

[RECOVERY](RECOVERY.md) describes oa-supervise: a separate process monitors loop
heartbeat and first-present receipt, kills/reaps a failed child, restores watchdog
timeout and execs vendor fallback. Host/stub-hardware ARM tests passed. Neither
receipt proves visible scanout, and the supervisor cannot fix every kernel hang
or supervisor failure. Do not enable native boot before hardware acceptance.

## Implementation order, with full scope preserved

| phase | required work |
|---|---|
| hardware/core | display ownership, IR, power, panel, audio, source, settings, services and reliable reload |
| remaining TV features | CEC, apps/media, PQ/FRC, network/BT, tuner/EPG, PVR, teletext/MHEG/interactive TV, CI+ |
| remaining integrations | portal, voice, hotel, search, OTA, with telemetry/ad paths omitted |
| native UI | vendor screen/content/navigation parity first; native OmarchyTV afterwards |

The existing web launcher is a historical prototype and later UI reference;
its boot is currently disabled. The stock assets/labels/profile behavior are
the first parity reference. A one-frame visual metric, standalone dialserver
transport or successful HAL call does not close the corresponding subsystem.
