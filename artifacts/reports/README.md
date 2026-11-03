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
| [TAB-LEAF](TAB-LEAF-2026-11-03.md) | six handler bodies, isShowing volume gate, signed digit wrap/remainder and direct versus virtual end;238860 shipped ARM comparisons with real division plus host/sanitizer/ARM; remaining settings map/list/mute/backend/visible integration open |
| [TAB-DISPATCH](TAB-DISPATCH-2026-11-03.md) | concrete full-width TabDialog fallback map and pre-first key join;65600 shipped map/matcher routes and16384 caller joins plus host/sanitizer/ARM; base/leaf implementations/backend/visible integration still open |
| [TAB-PREHANDLE](TAB-PREHANDLE-2026-11-03.md) | full method-body callback controller, vertical_tabs/source caller truth resolved;65664 shipped ARM trace/message/state matches plus host/sanitizer/ARM tests; invalid-tree safety explicit, fallback dispatcher/backend/visible acceptance still open |
| [TAB-EVENTS](TAB-EVENTS-2026-11-03.md) | reusable ordered Up/Down and Left/Right controller methods,1008 shipped ARM trace/state matches plus host/sanitizer/ARM tests; callbacks remain unwired to ui_vendor, outer pre-handler/full consumer/hardware acceptance open |
| [HOTEL-CONTEXT](HOTEL-CONTEXT-2026-11-03.md) | firmware HMCSettings mapping; read-only settings.hotel/CLI/actual Unix IPC, profile-context composition, settings publication/closed-state fixes;345 shipped row cases and full host/sanitizer/ARM vendor-SQLite tests; full defaults/controller/TV acceptance open |
| [CONTENT-HEADER-OWNERSHIP](CONTENT-HEADER-OWNERSHIP-2026-11-03.md) | resolves show+50 as removal of content's own five full-screen component pointers, separate parent header; source only, no new renderer/visual acceptance |
| [TAB-WRITEBACK](TAB-WRITEBACK-2026-11-03.md) | v2 independent retained setters/CLI/Unix IPC, OAN2 raw-state reload/OAN1 migration;1792 shipped setters/1792 DSO round-trips plus full host/sanitizer/ARM pass; corrects showing/hide interface; full consumer/lifecycle/TV acceptance open |
| [TAB-CONTEXT](TAB-CONTEXT-2026-11-03.md) | resolved network-tab/menu-capability/profile chain; bounded scalar decoder and reloadable ui.profile,65281 shipped hashes/27 actual lookups plus host/sanitizer/ARM DSO lifecycle and23-module host loader pass; mutable context/consumer/TV acceptance open |
| [TAB-SELECTION](TAB-SELECTION-2026-11-03.md) | initial missing-tab fallback and raw-ID/child-index distinction, empty-group/content0 behavior;9216 actual shipped group-code qemu comparisons; no native context, queue consumer or visible acceptance |
| [TAB-NAVIGATION](TAB-NAVIGATION-2026-11-03.md) | availability/focus prerequisites from fresh ARM; predicate matches shipped function in884736 qemu cases; corrects d6faec key-handler claim; native context/consumer/content/writeback remain open, no TV access |
| [NAVIGATION-SERVICE](NAVIGATION-SERVICE-2026-11-03.md) | reloadable ui.navigation state/queue + nav-* CLI/actual Unix IPC, explicit OAN1 pending reload; host/sanitizer/ARM-qemu/host-loader pass; no TV or UI consumer; queue pressure/TabDialog writeback open |
| [LAUNCHER-ROUTING](LAUNCHER-ROUTING-2026-11-03.md) | full62 operation conversion/28 ordered settings plans, conditional main-modal result route to nav13, host/sanitizers/ARM-qemu and firmware corpus; no UI/service integration or live keys;4b/a6 gates open |
| [AGENT-WORK-REVIEW](AGENT-WORK-REVIEW-2026-11-03.md) | review through e7e7333: scanner key bounds/completeness/snapshot/stale-key fixes; host/sanitizers/ARM-qemu;154 navigation call inventories/SubLauncher slots verified; no TV access |
| [Menu flow evidence](../analysis/ui-menu-flow-2026-11-04/README.md) | mixed dated session: positive portal/shortcut observations and conditional firmware routing; corrected VK/ICE equality, TabDialog absence/ownership and unconditional full-list claims; unchanged1043 attempts had no visible effect |
| [Navigation session](../analysis/settings-nav-2026-11-04/README.md) | dated screenshot/key/candidate record; no TabDialog identity inventory, no matched full-list proof or settings DB differential; session reports suspension, not current state |
| [UI-GLYPH-ROUTING](UI-GLYPH-ROUTING-2026-11-03.md) | conditional GPU/CPU dispatch and saved-context verifier; opaque-parent native comparison still differs, host/sanitizer/TV checks pass; actual scene/presentation parity open; owner requested handoff/stop |
| [UI-FONT-GPU](UI-FONT-GPU-2026-11-03.md) | native coverage-glyph/FBO primitive; strict host/sanitizers/ARM-qemu and real private-pbuffer checks pass; title pixel mismatch persists, no service/wire/visible parity; stale Surface rejected |
| [FONT-X-OFFSET](FONT-X-OFFSET-2026-11-03.md) | corrects false zero-getter claim: -signed global minimum-left gives font18/title23 and font7/MODE18; boxed viewport/placement implemented, host/sanitizers/ARM-qemu/TV CPU checks pass; exact bounds, full raster/colour gate open |
| [UI-TITLE-LIVE-GRAPH](UI-TITLE-LIVE-GRAPH-2026-11-03.md) | owner-opened stock panel/read-only graph: Surface108,5,691,95, LineObj23,5, zero margins/exact text/unit glyph ratio; +1b1 is CharClipping, not bold; X assignment remains open, no renderer change |
| [UI-TITLE-BOX](UI-TITLE-BOX-2026-11-03.md) | optional title consumes boxed text alignment/clip; font18 center Y5 explains historical vertical delta; host/sanitizers/ARM-qemu/native CPU corpus pass; horizontal23px/complete controls and visual parity open |
| [UI-WORK-REVIEW](UI-WORK-REVIEW-2026-11-02.md) | subsequent-agent review: ASan-proven scanner span fix, lab preset gate, default-off title/partial-subscription coverage; full host/focused sanitizers/ARM-qemu; saved live title/list geometry and limits, durable research snapshot |
| [UI-ROW-LABELS](UI-ROW-LABELS-2026-11-02.md) | offline ordinary-label source trace and exact MODE Dutch lookup; hash-checked bounded2435x41 profile traversal; no hidden padding in stored Modus, actual instance/transform/visual parity remains open |
| [UI-LIVE-OBJECTS](UI-LIVE-OBJECTS-2026-11-02.md) | bounded view-only heap candidate scanner, native complete sample and tests; no settings instance proved; owner confirms HDMI video despite blank OSD/OTHER; unsafe historical wspoll flagged |
| [UI-TEXT-VIEWPORT](UI-TEXT-VIEWPORT-2026-11-02.md) | alignment bridge and unit-ratio boxed EF41 renderer; exact9-alignment/54-line actual resource/font CPU tests pass host/sanitizers/ARM-qemu/TV; absolute placement/text/ratio/visual acceptance still open |
| [UI-TEXTBOX-COMPOSE](UI-TEXTBOX-COMPOSE-2026-11-02.md) | single-line full-height slice/textPos/style composition; actual460->489/488/live font7/20 tests pass host/sanitizers/ARM-qemu/TV; actual parent/glyph/clip/visible integration open |
| [UI-ROW-WINDOWS](UI-ROW-WINDOWS-2026-11-02.md) | ordered container/mask discovery and rowItems child windows; actual460 composition and host/sanitizers/ARM-qemu/TV CPU tests; actual parent/textbox/font/UI integration open |
| [UI-THEME-SKIN](UI-THEME-SKIN-2026-11-02.md) | first held theme consumer: actual dialog marker priority and removal/rebind/suspended/failed-start lifetime; host/sanitizers/ARM-qemu/TV policy tests; lab-only launcher path and actual widget geometry/visual acceptance remain open |
| [UI-THEME-PROVIDER](UI-THEME-PROVIDER-2026-11-02.md) | owned indexed BinSVG snapshot and reloadable ui.theme v1; CRC/index lookup, held views, lifecycle/failure/standby and full-pack tests pass host/sanitizers/ARM-qemu/TV; actual UI consumer/rendering open |
| [UI-BINSVG-VIEW](UI-BINSVG-VIEW-2026-11-02.md) | bounded runtime record/element/named-rectangle reader; nested/first-group and duplicate semantics; all702 records and actual477 geometry composition pass host/sanitizers/ARM-qemu/TV; indexed provider/widget integration open |
| [UI-HORIZONTAL-ORIGIN](UI-HORIZONTAL-ORIGIN-2026-11-02.md) | physical width, center trim/reset-X and end right-bearing/reverse traversal; aligned font-text renderer with host/sanitizer/ARM-qemu/native TV offscreen tests; full style/shaping/widget integration open |
| [UI-TEXT-ORIGIN](UI-TEXT-ORIGIN-2026-11-02.md) | concrete zero-client-margin bindings, vertical placement/clipping-capacity and phase-preserving ratio floor; host/sanitizer/ARM-qemu/TV helpers; horizontal/actual widget/text integration open |
| [UI-SLICE-STRETCH](UI-SLICE-STRETCH-2026-11-02.md) | retained double-ratio horizontal transform, independent offset/width truncation and composed477 row path;49,538 VFP cases qemu/TV plus host/sanitizer/full checks; parent/client findings, runtime/text integration open |
| [UI-LIST-METRICS](UI-LIST-METRICS-2026-11-02.md) | raw slice measurement, visible-count ordering and painted-row cached fields; corrected widget pointer names; host/sanitizer/ARM-qemu/native TV arithmetic tests; theme/resource/window/text integration open |
| [UI-MASK-LAYOUT](UI-MASK-LAYOUT-2026-11-02.md) | firmware mask alignment/translation and vertical/horizontal/mirrored row formulas; bounded helpers, host/sanitizer/ARM-qemu/TV pass; runtime metric population/full widget integration open |
| [UI-TEXT-GEOMETRY](UI-TEXT-GEOMETRY-2026-11-02.md) | single-line textPos consumption/axis/rounding corrections, bounded height helper;49,728 VFP comparisons qemu/TV plus host/sanitizer/full checks; row/mask/baseline integration open |
| [UI-FONT-LIVE](UI-FONT-LIVE-2026-11-01.md) | font pack loader + packed-UTF8 line renderer; font7/font16 labels, text values, slider digits and the disabled row drawn live on the settings panel (IoU 0.85..0.98, fb1 RMSE 0.106%, 22 px >8); title/tabs/sliders/focus text still captured |
| [UI-GLYPH-DRAW](UI-GLYPH-DRAW-2026-11-01.md) | signed glyph bearings/clipping and firmware coverage compositing;52022 glyphs tested offscreen on host/sanitizer/ARM-qemu/TV; SVG font-marker priority and focus font-type change recovered; string layout/UI integration open |
| [UI-FONT-SELECTION](UI-FONT-SELECTION-2026-11-01.md) | startup20 font IDs/heights, signed height/line-count division, stable typed selection/global fallback; pure selector and host/sanitizer/ARM-qemu/TV tests; widget styles/layout/render integration open |
| [UI-FONTS](UI-FONTS-2026-11-01.md) | native EF41 bitmap/font lookup contract, bounded reader and all38 records on host/sanitizers/ARM-qemu/TV; corrects browser/FreeType conflation; font selection/layout/UI integration open |
| [SKIN-SELECTION](SKIN-SELECTION-2026-11-01.md) | host skin command/startup policy, committed-frame readiness and safe vendor fallback; host249/sanitizer/native contracts, real Unix socket and actual GPU switching/fallback/full restore; content/visual/cold parity open |
| [UI-ENGINE-REVIEW](UI-ENGINE-REVIEW-2026-11-01.md) | v2 skin-owned dispatch, queued-code/payload lifetime fixes, rebind/receipt/timer/asset/identity regressions; host242/sanitizer/native contracts and actual GPU lifecycle/full restore; content/fallback/visual parity open |
| [APPS-GRID](APPS-GRID-2026-11-01.md) | DialogFullScreenPortal/appsGrid geometry, FullScreenGridFA/Button rectangles, recovered SVGGrid rows/step/button arithmetic with tests; portal insets/mask evaluation and list widget-name map remain open |
| [UI-TRANSPARENCY](UI-TRANSPARENCY-2026-11-01.md) | firmware-derived RGBA36,38,40,153; display.alpha/source-over/CPU premultiplication, host227/focused sanitizer and actual GPU alpha/lifecycle/full restore pass; independent real-video appearance/exact animation acceptance remain open |
| [RAIL-ASSETS](RAIL-ASSETS-2026-11-01.md) | verified11-frame vendor export, full-bundle icon-capacity regression/fix, real GPU navigation/reload/module-cycle/full restore; visible frame order/30ms cadence remain open |
| [UI-ASYNC](UI-ASYNC-2026-11-01.md) | ui_fb completion/readiness, cached dialog progress and service lifetime; four host/sanitizer/TV groups plus actual portal/transition/reload/module-cycle/full restoration; rail assets/full visuals/input/cold acceptance remain open |
| [SCENE-DISPLAY](SCENE-DISPLAY-2026-11-01.md) | RGBA canvas + one latest desired frame + exact async receipts; four host/sanitizer/TV groups; real101-request burst final receipt and412 GPU completions/full restoration; subsequent UI-ASYNC supplies ui_fb/readiness |
| [GRAPHICS-MODULE](GRAPHICS-MODULE-2026-11-01.md) | reloadable display.gpu v1, owned watch/timer/quiescence, public reload-safe tickets; four host/sanitizer/TV groups and410 actual service/GPU frames/full restoration; subsequent SCENE-DISPLAY adds coalescing, ui_fb/readiness remain open |
| [SCENE-CLIENT](SCENE-CLIENT-2026-11-01.md) | nonblocking parent submit/pump/take/shutdown and WNOHANG lifetime; five host/sanitizer/TV groups plus410 real GPU frames/full restoration; subsequent GRAPHICS-MODULE adds provider, UI readiness/coalescing remain open |
| [SCENE-HELPER](SCENE-HELPER-2026-11-01.md) | EGL-free bootstrap loads Mali renderer after fd cleanup; host/sanitizer/TV process tests and410 real GPU frame completions, child reap and full restoration; asynchronous host service/module lifecycle remain open |
| [SCENE-BROKER](SCENE-BROKER-2026-11-01.md) | immutable sealed-image IPC and blocking owner server; host/sanitizer/ARM-qemu/TV injected-renderer tests; target read-only shared mapping EPERM resolved from kernel instructions; subsequent SCENE-HELPER supplies executable/GPU acceptance, host module integration remains open |
| [EGL-WINDOW](EGL-WINDOW-2026-11-01.md) | direct native presentation, eight effects at50Hz with owner-confirmed upright/full-screen/smooth output; complete backing/pan/MAU/region restoration; async module/worker/readiness integration still open |
| [EGL-SCENE](EGL-SCENE-2026-11-01.md) | native Mali pbuffer ownership and scene rendering; real GPU pixel/lifecycle/readback tests, framebuffer prefix/pan unchanged; direct-window/module/scanout integration remains open |
| [UI-SCENE-REVIEW](UI-SCENE-REVIEW-2026-11-01.md) | fan-triangle UVs, linear filtering, alpha fade and UI lifetime/frame fixes; host/sanitizer/ARM-qemu, two real fb1 draw/restore runs; CPU performance and independent visual/startup acceptance remain open |
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
| [UI-ANIMATIONS](UI-ANIMATIONS-2026-10-31.md) | controller/easing/scrolling, 39 named descriptors, 702 CRC-verified names; later text-slide/rail/mode recovery and consumer implementation; UI-SCENE-REVIEW corrects scene sampling/fade and adds actual fb1 tests; full visual acceptance open |
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
