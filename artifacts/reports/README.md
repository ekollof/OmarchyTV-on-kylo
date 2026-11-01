# Report index — read status before conclusions

Current state is in ../../MEMORY.md, pending work in ../../TODO.md and corrections
in ../../FINDINGS.md. Reports preserve dated evidence; they are not live status
or authorization. A report's old “next” paragraph is not automatically work to do.

Status: **current evidence** supports the narrowly stated finding; **historical**
describes a dated experiment/deployment; **superseded plan** has been replaced;
**mixed** retains useful measurements but has corrected interpretations. None
establishes completion of the full replacement.

## Current implementation and firmware evidence

| report | status / use |
|---|---|
| [SPEAKER-GAIN](SPEAKER-GAIN-2026-11-01.md) | direct setter registration/operand bounds, distinction from UI AQ/balance/ratio path, lossy query prevents exact rollback; lower codec/client host/sanitizer/ARM-qemu/TV injected-I/O tests; broker remains query-only |
| [MIXER-MODULE](MIXER-MODULE-2026-11-01.md) | audio.mixer service, native query/CLI/reload/reaping, target libc spawn semantics; host/sanitizer/ARM-qemu/TV; refreshed16-module --check passes; writes/full audio parity open |
| [AUDIO-BROKER](AUDIO-BROKER-2026-11-01.md) | firmware prelude/PID-binding connection, separate executable and strict local query IPC; host/sanitizer/ARM-qemu and real spawned TV query/reap; subsequent MIXER-MODULE supplies parent integration |
| [CEC-ASYNC](CEC-ASYNC-2026-11-01.md) | copied ticketed acquire/send/receive/release, bounded slot and retained failed-close backend; four injected-I/O groups host/sanitizer/ARM-qemu/TV; module/native ownership still open |
| [AUDIO-ASYNC](AUDIO-ASYNC-2026-11-01.md) | bounded ticketed audio-owner submissions and copied results; three injected-I/O groups host/sanitizer/ARM-qemu/TV; module-service/native socket ownership still open |
| [WORK-REVIEW](WORK-REVIEW-2026-11-01.md) | owner concurrency/nonblocking release, pending reload retry, animation timer ownership/resume/reentrancy and test-dependency fixes; host/sanitizer/ARM-qemu; TV queried only |
| [OWNER-WORKER](OWNER-WORKER-2026-11-01.md) | offline CEC/audio workers; idle suspend/resume; injected cec.audio System Audio/ARC route; no live open or TV deploy |
| [SHUTDOWN](SHUTDOWN-2026-11-01.md) | host shutdown retries a quiescence veto until completion is dispatched; nine host/sanitizer/ARM-qemu groups; device workers are the later OWNER-WORKER layer |
| [QUIESCENCE](QUIESCENCE-2026-11-01.md) | ABI 3 busy-module cleanup veto; six host/sanitizer/ARM-qemu/TV groups and all 13 modules' TV loader preflight; host-loop gap superseded by SHUTDOWN |
| [STABILITY](STABILITY-2026-10-31.md) | current recovery/access/protection evidence; latest build state still in MEMORY |
| [RUNTIME-CHECK](RUNTIME-CHECK-2026-10-31.md) | loader/ABI preflight, installed failures and rebuilt real-TV pass; no hardware bring-up proof |
| [MODULE-SAFETY](MODULE-SAFETY-2026-10-31.md) | ABI 2 ownership/size/reload/dependency-order, Unix transport and coordinated power service/CLI tests; not deployed; device/wake/rollback/stdout limits remain |
| [TODO-AUDIT](TODO-AUDIT-2026-10-31.md) | historical audit inventory; later MODULE-SAFETY/PWM/OSD/runtime fixes supersede its open subitems |
| [OSD-BUFFER-CONTRACT](OSD-BUFFER-CONTRACT-2026-10-31.md) | current palette/private GEM/commit/allocation evidence; read the whole report, later sections resolve earlier questions |
| [OSD-KERNEL-INIT](OSD-KERNEL-INIT-2026-10-31.md) | current framebuffer registration/callback/order evidence |
| [OSD-MAU-SELECTION](OSD-MAU-SELECTION-2026-10-31.md) | current memory-bank query contract and dated fb1 observation |
| [DRM-PLANE-CONTRACT](DRM-PLANE-CONTRACT-2026-10-31.md) | generic NDAL record/discovery path; separate OSD cache is in OSD-BUFFER-CONTRACT |
| [PWM-CONTRACT-RECHECK](PWM-CONTRACT-RECHECK-2026-10-31.md) | current PWM layout/return instruction evidence; maintained vendor_pwm.h |
| [POWER-STANDBY-CONTRACT](POWER-STANDBY-CONTRACT-2026-10-31.md) | standby/device/wake-network order, target PHY policy and monitor/UART corrections; service/CLI tested; device preparation/wake/hardware acceptance remains open |
| [UI-ANIMATIONS](UI-ANIMATIONS-2026-10-31.md) | controller/easing/scrolling, 39 named descriptors, 702 CRC-verified names; ui.clock and ui.anim list/dialog tested offline; text slide, full-screen mapping and rail timing still open |
| [DOCUMENTATION-AUDIT](DOCUMENTATION-AUDIT-2026-10-31.md) | cleanup decisions, verification and remaining evidence limits |

## Historical hardware work and design surveys

| report | status / replacement guidance |
|---|---|
| [OSD-INIT-RE](OSD-INIT-RE-2026-10-30.md) | mixed: useful old measurements; palette handshake, generic dumb-buffer backend and scanout exclusions withdrawn. Use current OSD reports. |
| [BOOTSPLASH](BOOTSPLASH-2026-10-30.md) | historical negative result; later quarter-screen boot paint supersedes global impossibility claim |
| [PANEL-TAKEOVER](PANEL-TAKEOVER-2026-10-31.md) | mixed: watchdog/core-handler evidence; old deployment/init/scanout/recovery claims superseded by MEMORY/RECOVERY/current OSD reports |
| [PANEL-BACKLIGHT](PANEL-BACKLIGHT-2026-10-30.md) | mixed: useful call chain/constants; void-return interpretation replaced by PWM-CONTRACT-RECHECK |
| [UI-PARITY](UI-PARITY-2026-10-31.md) | mixed: BGRA fix/one-frame metric valid; injected portal hit, actions and list/dialog animation pass offline; no cold scanout proof |
| [UI-ASSETS](UI-ASSETS-2026-10-30.md) | extracted theme/index evidence; full layout/string alignment remains open |
| [IR-KEYMAP](IR-KEYMAP-2026-10-30.md) | profile map recovered; UART code-space/repeat/release integration still open |
| [IR-UART](IR-UART-2026-10-30.md) | partial static protocol evidence; 0x5e/repeat claims tentative; tty reads consume UI input |
| [UI-STACK](UI-STACK-2026-10-30.md) | historical stack/process survey; old RGBA/current-boot descriptions superseded |
| [AURORA-INIT](AURORA-INIT-2026-10-30.md) | static init call inventory; function counts are analysis-specific; not init success proof |
| [OPENAURORA-T1-PLANS](OPENAURORA-T1-PLANS-2026-10-30.md) | superseded plan: obsolete counts, void ABIs, dumb backend, “power solved” and channel-sweep/test order; use ARCHITECTURE/TODO/current contracts |
| [OPENAURORA-VENDOR-HAL](OPENAURORA-VENDOR-HAL-2026-10-30.md) | superseded initial design: current TV modules directly link complete closure; old dlopen/PWM/count advice invalid |
| [OPENAURORA-PARITY](OPENAURORA-PARITY-2026-10-30.md) | useful census; proposed HDMI-only descope and IR completion rejected; all non-telemetry/ad features remain scope |

## Protocol and other firmware research

| report | status / use |
|---|---|
| [DIAL-KEYS-SCREEN](DIAL-KEYS-SCREEN-2026-10-30.md) | historical protocol/key tests; 201 is not execution proof; standalone transport still uses vendor engine |
| [SOURCES-CEC](SOURCES-CEC-2026-10-30.md) | source/CEC static call paths; implementation/routing incomplete |
| [CEC-INPUT-RENAME](CEC-INPUT-RENAME-2026-10-30.md) | static cached-name policy, not wire-level reproduction |
| [CEC-SPEC](CEC-SPEC-2026-10-30.md) | external spec notes, not a complete conformance certification |
| [CEC-SPEC-VS-AURORA](CEC-SPEC-VS-AURORA-2026-10-30.md) | static opcode comparison; don't generalize to all CEC revisions or live behavior |
| [CEC-AUDIO-CONTRACT](CEC-AUDIO-CONTRACT-2026-10-31.md) | spec/ABI recheck: corrected ARC direction, tested wire/status helpers and audio prelude/cache/handoff fixes; injected route is the later OWNER-WORKER follow-up; transport/hardware acceptance open |
| [CEC-TRANSPORT-CONTRACT](CEC-TRANSPORT-CONTRACT-2026-10-31.md) | private V4L2/kernel packet ABI, tested marshalling and status/filter/copy/reopen hazards; CEC-OWNERSHIP is the subsequent backend/lifetime follow-up |
| [CEC-OWNERSHIP](CEC-OWNERSHIP-2026-11-01.md) | latest PreInit/cdev registration and tested blocking raw backend; explicit failed-close retention; authority/worker/module/hardware integration open |
| [CEC-LIVE](CEC-LIVE-2026-11-01.md) | spec overrides and tested identity grammar; shared-owner Steam Deck queries accepted, native exclusive-open refusal confirmed; stock boot/access retained; injected route is the later OWNER-WORKER follow-up |
| [AUDIO-ROUTING](AUDIO-ROUTING-2026-11-01.md) | live main amp is AD82587_D, correcting AD82586C assumption; distinct decoder/speaker/amp controls, AOUT cache and checked offline decoder; native board/lifecycle/audio acceptance open |
| [AUDIO-BOARD](AUDIO-BOARD-2026-11-01.md) | confirmed reset/MCU-mute/I2C resources and successful fixed register read on TV; staging removed, stock boot/access/privacy intact; native lifecycle/scale/audible acceptance open |
| [AUDIO-VOLUME](AUDIO-VOLUME-2026-11-01.md) | UI speaker-gain path and implemented AQ decoder; all101 live entries match, host/sanitizer/ARM-qemu/real-TV tests pass; native policy/gain application/lifecycle still open |
| [AUDIO-CURVE-MODULE](AUDIO-CURVE-MODULE-2026-11-01.md) | reloadable audio.curve v1 data provider; six real-DSO lifecycle/hold/failure groups pass including on TV; full host suite passes; hardware gain/profile/default/lifecycle integration remains open |
| [AUDIO-GAIN-TRANSPORT](AUDIO-GAIN-TRANSPORT-2026-11-01.md) | HAL false-success, receiver table copy and ALSA request-cache limits; configured read-only query passed on TV; subsequent AUDIO-NETLINK resolves dispatch timing; native gain/audio acceptance open |
| [AUDIO-NETLINK](AUDIO-NETLINK-2026-11-01.md) | synchronous lifetime/no operation ACK; tested codec plus owned blocking session (five offline groups), refactored query passes on TV; host worker/broker, real gain/table writes and audio acceptance open |
| [TELEMETRY](TELEMETRY-2026-10-30.md) | pre-remediation egress/consent evidence; last verified protections in STABILITY; excluded from replacement |
| [OTA](OTA-2026-10-30.md) | firmware Callisto research; transport/TLS claims apply to specific paths; no update/flash authorization |
| [AURORA-INFLUENCE](AURORA-INFLUENCE-2026-10-30.md) | old attack-surface survey/dead ends; root already obtained; not current task queue |

## Web launcher milestones — historical, boot currently disabled

| report | status / use |
|---|---|
| [LAUNCHER-PLAN](LAUNCHER-PLAN-2026-10-30.md) | superseded initial plan; follow P0–P3 history for what was built |
| [P0-BROWSER-PROBE](P0-BROWSER-PROBE-2026-10-30.md) | dated browser/viewport/key/CORS observations |
| [P1-MOCK-LAUNCHER](P1-MOCK-LAUNCHER-2026-10-30.md) | mock CGI/DIAL round-trip milestone |
| [P2-BOOT-INTEGRATION](P2-BOOT-INTEGRATION-2026-10-30.md) | historical opt-in boot/guard; not current boot state |
| [P2b-PORTAL-TILE](P2b-PORTAL-TILE-2026-10-30.md) | historical DB tile/re-entry test; not permission to edit live DB |
| [P3-OMARCHYTV-PORT](P3-OMARCHYTV-PORT-2026-10-30.md) | historical web port, not native module parity |
| [TVSIM](TVSIM-2026-10-30.md) | offline web/control-plane simulator; no TV hardware fidelity claim |
| [UPSTREAM-ISSUE](UPSTREAM-ISSUE-omarchytv-vestel.md) | draft field report, not evidence of publication or latest state |

Raw logs remain under ghidra/ and ../analysis/. The pre-cleanup root chronology
is in ../archive/. Existing report paths are stable so evidence links keep working.
