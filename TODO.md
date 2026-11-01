# TODO — active work, 2026-11-01

Read AGENTS.md for constraints and MEMORY.md for last verified state.
The full previous checklist is preserved in
[the pre-cleanup archive](artifacts/archive/README.md). Checked entries below
close specific milestones, not whole subsystems. Tiers order work; they do not
remove vendor features from scope. Telemetry and advertising are excluded.

## Current acceptance gates

- [ ] Independent startup: boot into openaurora without starting vendor Aurora;
      full-screen scanout, input, fallback and resume all verified. Owner authorized
      an idle hardware/CEC/audio test window on November 1; see AGENTS.md.
- [ ] Reloadable modules: state, service holds, callback ownership and dependencies
      behave correctly through start/stop/reload/suspend/resume and failures.
- [ ] Reproduce vendor screens, remote behavior, settings and the remaining TV
      functionality; only then change UI/UX to native OmarchyTV.

## Core and runtime

- [x] ABI 3 nonblocking quiescence gate retains busy modules through stop/unload/
      reload and failed init/start, including deferred activation; force cannot
      bypass it. Six real-DSO host/sanitizer/ARM-qemu/TV groups pass. QUIESCENCE.
- [x] Shutdown coordination: oa_host_run keeps dispatching while a quiescence
      veto is outstanding, retries stop/unload, and returns an error without
      dlclose for a non-quiescence hold with no module loop work. A busy gate
      can wait indefinitely for a future completion; absence of queued work
      does not prove a worker is unable to post.
      Nine real-DSO groups pass strict host, ASan/UBSan and ARM/qemu. SHUTDOWN.
      Not deployed. Offline owning workers are in OWNER-WORKER; they do not
      open live CEC or audio hardware.
- [x] Review fixes: worker-only session state, nonblocking idle quiescence,
      safe join-error handling, retryable pending reload, animation timer
      ownership/resume and reentrant completion; full host plus focused sanitizer/
      ARM-qemu checks. WORK-REVIEW-2026-11-01 records the scope.
- [x] Provider lifecycle rebinding: mod_ui_fb discovers services published after
      start (`module/changed`), drops cached pointers on `service/removed`, and
      rebinds in a 0 ms timer callback so holds and timers keep the consumer's
      identity. Suspend defers rebinding to resume (no display hold while
      suspended). A forced unload of ui.anim drops the pointer before dlclose; a
      later reload restarts the slide. tests/ui_rebind covers all three paths;
      host make check 197 ok, ASan/UBSan 197 ok and ARM/qemu pass. TV rerun pending.
- [ ] Add asynchronous audio/CEC service requests/completion before exposing
      worker RPCs on the host loop. bind_session currently rejects all arguments
      with ENOTSUP. Audio's lower owner now has ticketed async open/gain/close
      and copied take() results; wire it into a service after ownership/lifetime
      integration. CEC now has the corresponding lower async path; module
      adapters must account for borrowed backend code/context and blocking-I/O
      completion before release. No forced cancellation has been implemented.
- [x] Separate-PID oa-audio-broker owns a lazy native query session over strict
      local seqpacket IPC, drops inherited descriptors and closes on EOF. Four
      host/sanitizer groups, ARM/qemu fixtures and real spawned TV GetOPUGain
      pass. No HAL init or writes. AUDIO-BROKER.
- [x] audio_mixer publishes audio.mixer v1: nonblocking IPC, ticket validation,
      owned watch/reap timer, WNOHANG teardown, cancellation/drain and paired
      suspend/resume. mixer-query/result/cancel work through the real socket.
      Five real-DSO groups pass host/sanitizer/ARM-qemu/TV; native query/reload/
      stale-ticket rejection also pass on TV. MIXER-MODULE.
- [x] Recover direct SetOPUGain registration, channel operand bounds0..0x8000,
      cache fields and lossy GetOPUGain normalization from firmware. Lower codec/
      client dispatch passes three wire and six session groups on host/sanitizers/
      ARM-qemu/TV with injected I/O only. No real gain write. SPEAKER-GAIN.
- [ ] Integrate the proper UI speaker-volume path: ALSA speaker out gain reaches
      Mod_Cell_SetVol and AQ lookup/balance/ratio processing. Direct SetOPUGain
      bypasses that processing; its query cannot supply an exact rollback value.
      Extend broker/service for volume-table selection and UI gain. Preserve
      table ownership through dispatch, distinguish query/dispatch from applied
      audio, and complete amp/routing/native ownership before writable acceptance.
      The current broker/service remains GetOPUGain only, no hardware setters.
- [ ] Coordinate async suspend preparation with the sleep engine: mixer/worker
      hooks can return busy after beginning quiescence. Manual retry is tested,
      but failed-prepare rollback does not restore that partially quiesced hook.
      Do not call the paired-hook tests native standby/rollback acceptance.
- [x] Bounded asynchronous audio owner: one request/result slot, copied I/O table,
      ticketed value completion, backpressure and drain-before-close on shutdown.
      Three injected-I/O groups pass host, ASan/UBSan, ARM/qemu and actual TV;
      staging removed. No live socket or hardware access. AUDIO-ASYNC.
- [x] Bounded asynchronous CEC owner: copied acquire path/tables and send frame,
      ticketed receive/empty/error results, retained failed-close ownership and
      drain-before-close. Four injected-I/O groups pass host, ASan/UBSan,
      ARM/qemu and actual TV. Staging removed; no live bus/device access. CEC-ASYNC.
- [ ] Make queued animation timer-allocation failure observable to the initiating
      consumer; the owned kick currently logs failure. No visual acceptance claim.
- [x] ABI 2 descriptor-size validation and module-owned worker posts implemented;
      host/sanitizer/ARM-qemu coverage recorded in MODULE-SAFETY-2026-10-31.
- [x] Registry full-hold rejection, held-provider stop/unload protection and
      failed-init cleanup fixed and regression-tested (TODO-AUDIT plus MODULE-SAFETY).
- [x] Socket/stdin list/status/load/start/stop/unload/reload/suspend/resume/loglevel/
      quit exist; ticker and console state-handoff smoke tests pass.
- [x] Bounded control reply formatting fixes a reproduced status heap overflow;
      full 32-module/48-service replies reach a Unix client without the old 1 KB
      output truncation. Tiny-buffer/full-table host, sanitizer and ARM/qemu tests pass.
- [x] Unix control transport: bounded nonblocking output/backpressure, partial
      writes/EINTR/EAGAIN, disconnected readers and half-close drain; whole-line
      overlong/NUL rejection and watch/socket cleanup. Five real-socket regression
      groups pass host, ASan/UBSan and ARM/qemu. MODULE-SAFETY.
- [ ] Console stdin/stdout delivery remains synchronous; blocked/broken stdout
      can still disrupt the host. Complete console output lifecycle and acceptance.
- [x] --check validates ELF/ABI note/entry and library/symbol/version closure without
      starting modules. Current matching ABI3 core + all16 modules pass on TV;
      temporary staging removed. MIXER-MODULE supersedes earlier11-module snapshot.
- [ ] Add runtime resource/environment validation alongside loader preflight.
      Real ALSA use also needs the stock ALSA_CONFIG_PATH; a loader-only pass
      cannot certify that configuration or hardware health. AUDIO-GAIN-TRANSPORT.
- [x] Host recovery integration, safety/registry, runtime and fake-display checks
      passed in prior implementation work. No tests establish full hardware parity.
- [x] Reload handles restore errors and preserves loaded/stopped/suspended state.
      Suspended replacements defer start until resume; failed deferred starts can
      retry. State allocation/size/growth failures preserve the old instance;
      rejected replacement/restore/start clears candidate registrations/posts.
      Nine regression groups pass host, ASan/UBSan and ARM/qemu. MODULE-SAFETY.
- [x] Dependency-ordered suspend/resume implemented from requires[] and lookup
      holds, preserving unpublished-service snapshots through reload. Cycles,
      active-consumer protection and callback errors covered by nine host/
      sanitizer/ARM-qemu regression groups. Socket/CLI reports transition failures.
- [x] Internal host sleep-cycle engine: final backend after paired hooks, automatic
      transition-ledger restoration, separate prepare/entry/restore results,
      preexisting suspended state retained, nested cycles/mutations rejected and
      shutdown cancellation. Nine additional host/sanitizer/ARM-qemu groups pass.
- [x] Power service/CLI use host.sleep coordinator; RAM entry split from ordinary
      hooks, notifications ordered around restoration, failed writes not counted.
      Seven wrapped-hardware host/sanitizer/ARM-qemu groups pass; not deployed.
- [ ] Finish broader lifecycle acceptance and UI selections; native device
      preparation/wake ownership and transactional binary rollback remain incomplete. Workers
      must still stop/join and reclaim undelivered userdata.
- [ ] Configuration is currently a global append-only table: repeated host init
      can retain earlier values. Define per-host/reinitialization semantics;
      multi-case power fixtures supply config through their test API override.
- [ ] Future authorized deployment: move a complete matching ABI 3 build, not
      individual modules. Older installed audio/panel/source standalone checks fail
      on unresolved V4L2 imports; rebuilt closure passes. Preserve ABI 1 recovery.

## Display / platform — acceptance remains open

Current evidence: OSD-BUFFER-CONTRACT, OSD-KERNEL-INIT, OSD-MAU-SELECTION and
DRM-PLANE-CONTRACT reports, indexed in artifacts/reports/README.md.

- [x] Palette 131/132 corrected; private GEM allocation, mapping/name table,
      ADDFB2/SETPLANE, OSD 68-byte records and enable path recovered.
- [x] KER_OSD_DrvInit registers five fbs; callbacks and stateful buffer order traced.
      Registration, device-node publication and visible presentation are separate.
- [x] Pool construction/bitmap allocation traced: front requests S, back S/2*S/shared
      according to count; Sync advertises 2*align4096(S). OSD binds GEM-derived
      address/size separately. Full cold lifecycle remains unverified.
- [x] Every present now commits FBIOPAN_DISPLAY, including display.pan=0/one frame;
      error/retry and resume-frame logic covered with fake devices on host/ARM.
      Subsequent UI-SCENE-REVIEW runs real fb1 draw/commit/readback and restores
      saved pixels/pan with stock running. Not deployed or independent cold/resume acceptance.
- [x] O_RDONLY fbregion helper GET request set tested; real fb1 query read MAU0
      primary with unchanged vendor boot/PIDs, staging removed.
- [x] Confirmed generic GEM +0x70 size store in both NAND kernel slots; fresh
      decompression matches reconstructed ELF load segments. OSD helper returns
      allocation size, not doubled metadata. See OSD-BUFFER-CONTRACT.
- [x] Update callback byte4 commit/byte5 conditional-vsync behavior traced;
      0x63 is region show, not alpha. Complete cold setup acceptance remains open.
- [x] Alpha callback/PM registration and register/palette resume paths traced.
      Normal-plane blend-enable is a no-op; SetPar global alpha is separate.
      First pan handles plane/region show. See OSD-KERNEL-INIT; static evidence
      supports existing fbdev as the first independent-validation candidate.
- [ ] Complete and verify the fbdev cold/resume lifecycle: initialization ordering,
      geometry/backing retention and first post-wake frame. Kernel PM restoration
      differs from reopening a userspace fd; do not add speculative replay calls.
- [ ] Finish alternate-MAU/separate-buffer/zero-buffer and later resize/surface
      switching behavior. Do not infer actual mapping size from framebuffer metadata.
- [ ] Verify independent scanout and full-screen geometry on panel at a future
      authorized takeover. Present success, pixel readback and oa.ready are insufficient.
- [x] Rechecked audio scalar return/no-argument ABI in ARM instructions; plat owns
      prelude, audio no longer repeats it. Fake-HAL failure gates/cache/handoff
      tests pass host/sanitizer/ARM-qemu. CEC-AUDIO-CONTRACT.
- [ ] Complete platform/audio init/close/restart ownership after partial startup.
      Copied VIN arrays alone do not reproduce routing; successful void stages
      do not prove hardware bring-up.

## Input, power, panel, audio, source and settings

- [ ] IR: integrate profile-derived map without mixing NEC/RC/ICE/DFB/DIAL codes.
      Confirm the actual UART fields, repeat/release/long-press behavior and native
      navigation. Current f[3]/0x5e assumptions are unverified. Reading ttyS2 steals
      stock input; live capture requires an authorized idle/takeover session.
- [ ] Power: complete native device preparation/wake/resume ownership and actual
      hardware acceptance. Service/CLI coordination is implemented offline;
      manual suspend/resume are hook-only commands and never enter RAM. Use
      POWER-STANDBY-CONTRACT; resolve remaining device/MCU questions before
      changing hardware behavior rather than repeating the recovered trace.
- [x] Rechecked outer standby plus idtvSuspend/idtvResume and wake policy against
      Aurora instructions. POWER-STANDBY-CONTRACT records omitted device/network
      stages, swapped WoW/WoL argument roles and monitor/UART ownership corrections.
- [x] Target ntkethmac 0x89f0 handler decoded from this TV's dump: opcode 1 stores
      next-suspend PHY policy; value 0 keeps PHY powered, 1 powers it down. Kernel
      suspend/resume handles MAC/DMA/magic-packet/PHY restore. POWER-STANDBY-CONTRACT.
- [ ] Assign paired hooks/device ownership to remaining modules; the coordinator
      deliberately refuses missing hooks. Native wake flags/MCU/device prep/restore
      remain work. Manual resume repairs hooks after a failed cycle but does not
      emit a new power/ON notification; complete recovery-state signaling with
      the native frontend. Raw mem/WoL is not this acceptance.
- [x] PWM six-/four-word requests and scalar return contracts corrected from ARM
      instructions; maintained vendor_pwm.h used by panel/fetch script.
- [ ] Panel: brightness/power curve, init/close ownership and resume under exclusive
      native ownership. Board frequency/mode are recovered; do not sweep channels
      as a replacement for firmware/profile analysis. Nonzero HAL return is not
      proof that an ioctl or brightness change happened.
- [ ] Audio: verify amp/speaker/soundbar volume, mute/routing and init ABI/ownership.
      Earlier non-crashing calls were not an audible acceptance test.
- [x] Confirmed selected live amplifier from verified descriptor/object/vtable:
      ESMT AD82587_D, one instance. Corrected the AD82586C assumption and recovered
      AOUT cache/gain-vs-decoder controls. Offline audio-state snapshot decoder
      passes host/sanitizer/ARM comparison and malformed-input checks. AUDIO-ROUTING.
- [x] Recovered/confirmed AD82587 reset GPIO114, MCU mute GPIO208/STBC8,
      I2C bus3/address34 and one-byte register subaddress. Fixed native register
      read returned 03..05=1b/05/05 with both messages complete on real TV.
      Probe removed, stock boot/access/privacy unchanged. AUDIO-BOARD.
- [ ] Implement AD82587 board-resource ownership, UI volume conversion/callers
      and paired init/close/resume. Cached routes, a successful register read and
      registered speaker 0 do not prove native audible output or complete amp
      mute; do not initialize AD82586C or call the MCU GPIO path cold.
- [x] Traced UI volume through speaker gain and recovered both AQ101-entry table
      layouts/word-to-halfword narrowing. New pure audio_curve decoder and tests
      match all101 entries captured from live Aurora; strict host/sanitizer/
      ARM-qemu and real-TV runtime comparison pass. AUDIO-VOLUME.
- [x] Reloadable audio_curve module publishes copied immutable AQ data through
      audio.curve v1. Six real-DSO/CLI, hold/collision, malformed-file/FIFO and
      suspend/reload/retry groups pass host/sanitizer/ARM-qemu and on the TV;
      full host suite passes. No HAL/gain application. AUDIO-CURVE-MODULE.
- [ ] Integrate AQ/profile/default handling, speaker gain/table application,
      balance/output/DAP policy and amp lifecycle into native audio. Existing
      decoder-volume opcode33 is not full UI volume parity; do not invent an
      amplifier-register conversion or rely on the no-op SetMainVolume wrapper.
- [x] Recovered message-backed HAL false-success, receiver table-copy behavior
      and speaker ALSA request-cache semantics. Read-only mixer query passed on
      real TV with stock ALSA_CONFIG_PATH; cached gain0/range0..100. AUDIO-GAIN-TRANSPORT.
- [x] Traced synchronous netlink callback/table-copy lifetime; no operation ACK
      exists. Bounded GetOPUGain/VOLCfg/SetOPUVolCfg codec passes three host/
      sanitizer/ARM-qemu groups. Native read-only GetOPUGain exchange passed on
      real TV; staging removed. AUDIO-NETLINK.
- [x] Owned blocking audio_client session implements fd/bind/fork/close ownership,
      checked table/selection dispatch and bounded validated queries. Five host/
      sanitizer/ARM-qemu groups and full host suite pass; maintained-session query
      passed on TV. Real table/selection writes untested. AUDIO-NETLINK.
- [x] Owning audio worker serializes open/gain/close off the host loop and
      rejects a NULL I/O table with ENOTSUP before any socket call. Quiesce
      closes the session and consumes its fd. Host, ASan/UBSan, ARM/qemu and
      the full host suite pass. mod_audio owns its idle worker, but no live
      socket binding or asynchronous command path is wired. Not deployed.
      OWNER-WORKER.
- [ ] Wire that owner into one process that does not share the vendor HAL
      socket. Retain table through the syscall, never replay EINTR, and
      distinguish dispatch from operation/DSP/audio acceptance. No gain, table
      or profile write is accepted.
- [ ] Source: implement companion VPLT operation, Hal_Video_Set_V4l2Info, audio
      routing, supported source list and names; verify switching. source.enable
      remains off by default; VIN open alone is not source parity.
- [ ] Settings: typed/string/BLOB access, profile/NVRAM semantics, authoritative
      cache/update behavior and lifecycle. Existing generic integer service defaults
      read-only and has a real-TV read milestone only.
- [ ] Services: reproduce DIAL, state WebSocket and DLNA independently of Aurora.
      Standalone dialserver restored transport; it still delegates TV operations
      to the vendor engine and does not prove a standalone replacement service.

## Remaining vendor parity

- [ ] CEC transport, OSD-name caching, RCP and firmware workarounds. The injected
      System Audio/ARC route is not wire-level or electrical acceptance.
- [x] Specification takes precedence over vendor CEC defects. Identity/name/power
      grammar now checks directed 46/47/83/8f/90 and broadcast 84, printable
      1..14-byte names and defined operands. Regressions also preserve valid
      broadcast 72 rejected by the vendor filter. Host/sanitizer/ARM-qemu pass;
      helpers are not yet a complete policy module. CEC-LIVE-2026-11-01.
- [x] Confirmed HDMI1 Steam Deck at logical address 8 / physical 1.0.0.0 in
      vendor cache; standard 83/46/8f queries received driver TX acceptance with
      stock owner active. No response payload capture or independent native
      ownership acceptance. Temporary staging removed; stock boot unchanged.
- [x] Spec-based CEC framing/System Audio status and ARC-direction helpers,
      with explicit unsupported scope, tested offline on host/sanitizer/ARM-qemu.
      No CEC transport/module or hardware integration yet. CEC-AUDIO-CONTRACT.
- [x] Recovered private V4L2/kernel CEC transport and raw/VPL packet layouts;
      bounded marshalling passes four host/sanitizer/ARM-qemu groups. Found
      16-byte receive copy, lost driver-result byte, spec-incompatible driver
      filter entries and close/reopen hazards. CEC-TRANSPORT-CONTRACT.
- [x] Recovered PreInit emulation plus real character-device registration;
      implemented blocking raw CEC backend with explicit fd ownership, correct
      close argument, retained failed-close state and raw RX. Three injected-I/O
      groups pass host/sanitizer/ARM-qemu. CEC-OWNERSHIP-2026-11-01.
- [x] Offline CEC authority helper and owning worker. A clear mirror bit 1 writes
      injected register 0xf011 with the low byte plus bit 1; a set bit does not
      write. The worker keeps a failed hardware close until a later quiesce
      generation and does not post on that failure. mod_cec starts idle,
      publishes cec.owner, and hardware_open_supported() is 0. Host, ASan/UBSan,
      ARM/qemu and the full host suite pass. Not deployed. OWNER-WORKER.
- [x] Idle cec suspend/resume joins and recreates its worker without opening
      ntkcec. mod_audio creates the mixer owner; bind_session(NULL) is ENOTSUP
      and does not bind a socket. Enabled HAL suspend returns failure and leaves
      the module started. Review removed the 500ms idle wait: all quiesce calls
      report busy immediately until exit, with explicit lifecycle retry. Host,
      ASan/UBSan and ARM/qemu suites passed
      twice. Not deployed. OWNER-WORKER lifecycle follow-up.
- [x] System Audio and ARC route on mod_cec as cec.audio v1, injected frames only.
      Mode on blocks local volume and mute before Hal_Audio_Ctrl. External Report
      Audio Status is stored and is not a speaker write. Volume keys go to logical
      address 5 as User Control Pressed/Released. The TV sends Request ARC
      Initiation/Termination and answers the amplifier's Initiate/Terminate ARC.
      A direction-valid Initiate ARC does not set arc until the injected link can
      transmit and the Report ARC Initiated reply is stored. An 8-byte snapshot
      survives reload. hardware_open_supported() stays 0. Host make check, one
      ASan/UBSan run and one ARM/qemu run pass. No live bus, amp write, or deploy.
      OWNER-WORKER route follow-up.
- [ ] Install a firmware-backed CEC authority and exclusive open only after stock
      releases the device. Keep failed-close retention. Native-node tests remain
      open. Do not use NDAL Init/Init(NULL) for reload or strict VPL acceptance
      as end-to-end specification validation.
- [ ] Apps/browser/media integration, picture quality/FRC, network and Bluetooth.
- [ ] Tuner/DVB-T/DVB-S, EPG/service lists, teletext/MHEG/interactive TV, PVR/
      timeshift/playback, CI+/conditional access and associated persistence.
- [ ] Portal, voice, hotel mode, search and OTA user-facing behavior as applicable
      to vendor parity; omit telemetry/ad components. Do not silently descope
      broadcast features because this set is currently used with HDMI.

## Native UI, assets and OmarchyTV

- [x] Theme/index/layout files and 964 PNGs extracted; initial OAI1 conversion and
      ui_fb rendering pipeline exist. Source and provenance: UI-ASSETS report.
- [x] BGRA capture bug fixed; selected portal-frame comparison ~1.62/255 recorded.
      This is one frame, not full screen/navigation or startup parity.
- [x] Follow-up format audit: tvshot uses explicit BGRA/RGBA/RGB565 raw decoding
      with offline known-colour PNG tests; legacy bootsplash source now writes
      BGRA, checked in memory under strict C flags. Stale report claims corrected.
      No TV capture, deployment or legacy/recovery binary rebuild occurred.
- [x] String inventory extracted (56,715 lines); it is not an aligned localized table.
- [ ] Decode layout property IDs/records and original language IDs/grouping, including
      Dutch selection; do not guess slot numbers from the flattened string inventory.
- [x] Portal rail/card navigation on mod_ui_fb from mod_ir frames. Profile RC
      Up/Down/Left/Right/Select/Exit and NEC OK 0x8a. Focus chrome is observed
      through the display service. The reload snapshot is now 4 bytes (band, rail,
      card, screen); a 3-byte blob no longer restores. Host make check, one
      ASan/UBSan run and one ARM/qemu run pass. No ttyS2, fbdev, CEC or amp open.
      The live UART code byte is still unconfirmed. UI-PARITY.
- [x] Tile hit testing uses the same rectangles as paint. Select opens the focused
      tile, Menu and Exit return to the portal, and Aux emits source with the
      index read from settings m_current_source. Hit still resolves tiles hidden
      by the open screen. Proven by tests/ui_nav.c on host make check, one
      ASan/UBSan run and one ARM/qemu run. The test database is not the live
      settings store. UI-PARITY.
- [ ] Complete the remaining vendor screens and labels. The injected portal
      above is not all-screen parity or a confirmed live remote capture.
- [ ] Reproduce vendor animations, transitions, focus/selection feedback, scrolling
      and other UI/UX flair. Recover effects/timing/easing/interrupt and resume
      behavior from firmware/assets; validate frame sequences and interaction,
      not just a selected screenshot. These effects are required parity scope.
- [x] Recovered animation controller/cadence, target-specific linear vs cubic
      easing, text-slide pauses and restart/cancel/finalise distinctions. Bounded
      BinSVG reader consumes 702 records/11,888 elements and extracts 39 descriptors;
      host/sanitizer/ARM output matches. UI-ANIMATIONS. Runtime/caller mapping remains open.
- [x] Resolved all 702 BinSVG IDs/names from ELF svg_name_list and verified every
      CRC32 against the index. Named all 39 descriptors, flagged the ad-banner
      effect as excluded, and recovered internal widget states/category remap.
      Host/sanitizer/ARM name-reader output matches. UI-ANIMATIONS.
- [x] Implemented separate ui.clock service from FROST_Time instruction evidence:
      90,000,000-ms period, 12,343 correction quantum and strict boundaries.
      Seven real-DSO host/sanitizer/ARM-qemu groups cover arithmetic, reload,
      failure cleanup and paired ticker/ui_clock lifecycle. Not deployed.
- [x] mod_ui_anim publishes ui.anim v1 on ui.clock. Enablement starts off and
      set_enabled only stores 0 or 1. Start rejects a duration below 1, replaces
      the same id, and keeps four slots. Cancel drops an entry; finalise applies
      the end state first. List coordinates truncate; dialog progress uses
      min(1, 1+(p-1)^3) and the 1000 ms select path. sample(now) is the 0 ms then
      10 ms update. tests/ui_nav.c passed host make check, one ASan/UBSan run and
      one ARM/qemu run. UI-ANIMATIONS.
- [x] Text-slide math from AnimationHelper instructions: `oa_anim_slide_duration` (1600 +
      trunc(distance/50)×1000) and `oa_anim_slide_margin` (pause 800 ms, clamp above only; forward ends
      at distance, reverse at zero). Published as `ui.anim` v2 and asserted with exact vectors for
      distances 0/49/50/100/400 and elapsed 0/800/801/4800/9600/90000 plus reverse and short-slide
      cases. UI-ANIMATIONS.
- [x] Text-slide caller wiring in mod_ui_fb: distance from isTextSlidingNeeded @0x10c76b0
      (text_width − box_width), 10 ms sampler, and the single 2000 ms repeat armed at progress > 0.99
      while the sampler still runs (SVGTextBox::onAnimationProgress @0x10c8e48, constant 0x10c8ee8).
      The label is clipped to `ui.slide_box` (crop blit at the box origin), start/repeat are gated on
      `ui.anim` enablement, elapsed uses the 90,000,000 ms ui.clock period, and suspend/stop delete
      both timers and clear the runner state. Duration/margin come from the v2 provider. The module
      links `-lm` for sinf (DT_NEEDED verified). tests/ui_slide drives two complete passes and the
      repeat threshold, disablement, suspend/resume and stop/start; oa_slide_* has deterministic
      vectors including the vendor period rollover. Host `make check` exit 0 (196 ok), ASan/UBSan and
      ARM/qemu pass. Scheduling is exercised by the live probe below; visible-panel behaviour is not
      yet verified. UI-ANIMATIONS.
- [x] Live TV scheduling probe for the text slide: a /tmp-staged TV build with the probe
      (recording display, CLOCK_MONOTONIC ui.clock) and the real ui_anim/ui_fb measured a 10.08 ms
      mean sampler cadence, first nonzero margin at +822 ms, 163 px at +3808 ms, and the repeat
      restart at +6557 ms (= 4554 + 2000, the 0.99 arm), clean exit. No fb1, vendor module, key,
      settings or network access; staging removed and vendor PIDs unchanged. This is not visual
      scanout or pixel parity evidence. UI-ANIMATIONS.
- [x] Rail focus highlight: placement recovered (DialogMainMenu.svg button windows (6,y,160,160);
      capture-derived offsets); implemented in mod_ui_fb as 11 vendor frames at 30 ms looping in the
      160x160 window, playhead reset on focus change. tests/ui_rail_hl asserts order/wrap/timer/window;
      host 198 ok, ASan 198 ok; live probe 166 blits at 29.98 ms mean, x=6 y=461. Visible-panel
      appearance still unverified. UI-ANIMATIONS.
- [x] Full-screen transform consumer: display v2 publishes `set_scene_transform`; all draws render
      to an offscreen RGBA scene and present() maps it through the recovered four-vertex clip-space
      quad (x-slide, y-slide, scale, type-1 keystone). mod_ui_fb pushes `ui.fs_type` with the eased
      dialog progress and clears on completion/suspend/stop. UI-SCENE-REVIEW corrects fan-triangle
      interpolation, linear texel filtering, progress-alpha fade and transparent outside pixels;
      it fixes per-progress frame requests, provider aliases and interrupted-dialog cleanup.
      Full host203 ok, focused ASan/UBSan, ARM/qemu display checks and two actual fb1 draw/commit/
      restore runs pass. These do not establish smooth/visual independent acceptance.
      The vendor per-mode names are recovered
      (57 flip, 58 right, 59 left, 60 up, 61 down, 62 fade, 63 grow, 64 bounce) and
      getWidgetStandardName's first-present rule selects type 4 for DialogLauncher.svg
      (animation_slide_up_fade_in); ui.fs_type remains an override.
- [ ] Complete accelerated scene presentation and visible animation acceptance.
      EGL-WINDOW now proves direct native presentation on the initialized layer.
      SCENE-HELPER now tests the separate bootstrap/renderer on actual GPU;
      GRAPHICS-MODULE now tests the asynchronous display.gpu provider on actual
      GPU with owned watch/reap timer and nonblocking quiescence. SCENE-DISPLAY
      adds the coalescing canvas and async receipts. UI-ASYNC wires ui_fb receipts,
      readiness and cached transform-only progress; actual portal/transition/UI
      reload/application-module cycle pass. Remaining visual/assets/input and
      independent cold/TV standby acceptance are still open.
      No display.renderer switch exists yet. Preserve failed-close
      ownership; test module reload/suspend/resume/failures on hardware.
      CPU reference transforms remain slow; direct-window diagnostic meets its
      own50Hz target. Measure controller sampling, renderer and physical refresh
      separately;10ms sampling is not100Hz scanout. Verify vendor frame sequences,
      alpha/edge precision, interaction and cold/resume ownership.
- [x] Lower graphics IPC and retained owner/image lifetime: fixed OGS1 records,
      sealed immutable RGBA memfd transfer, cached revisions, completion after
      injected finish and failed-close retry. Four groups pass strict host,
      ASan/UBSan, ARM/qemu and actual TV without rendering/hardware calls.
      Target shared sealed mapping EPERM resolved using kernel ARM instructions;
      use private read-only mapping with all seals retained. Helper/GPU acceptance
      is the subsequent SCENE-HELPER milestone; host service/readiness and module
      lifecycle remain open.
      SCENE-BROKER-2026-11-01.
- [x] Separate graphics bootstrap/renderer: close inherited descriptors before
      Mali loads; keep renderer loaded until child exit. Two process groups pass
      strict host/sanitizers/TV. Actual native helper completes410 GPU frames
      (identity plus eight effects), child exits/reaps, full16588800-byte backing,
      region/MAU/pan restored and independently hashed. Full host209 checks pass.
      Blocking diagnostic client only; async host service/module lifecycle and
      first-frame readiness remain pending. SCENE-HELPER-2026-11-01.
- [x] Lower nonblocking graphics parent: one pending/result slot, generated
      instance tickets, revision validation, MSG_DONTWAIT I/O, copied completion,
      EOF shutdown and WNOHANG reap/destroy gate. Five strict host/sanitizer/TV
      groups pass, including blocked child work/cleanup and target exec failure.
      Maintained client also completes410 actual GPU frames through the full
      snapshot diagnostic, with child exit0/restoration/independent hash match.
      Full host214 checks pass at that milestone. GRAPHICS-MODULE subsequently
      adds the provider/watch/timer; coalescing/readiness and UI integration remain
      pending. SCENE-CLIENT-2026-11-01.
- [x] Reloadable graphics provider: display.gpu v1 with owned completion watch,
      reap timer, retained failed-start cleanup and nonblocking quiescence.
      Public tickets use host module ids across reload. Four host/sanitizer/TV
      lifecycle groups pass (holds, suspend/resume, failures, blocked close and
      abnormal-peer fault latch). Actual service completes410 GPU frames; host
      reactor drains/unloads before full snapshot restoration/hash match.
      Full host218 checks pass. Source17 modules; preserved production stage16.
      UI coalescing/readiness and actual UI reload/power behavior still open.
      GRAPHICS-MODULE-2026-11-01.
- [ ] Recover native GPU abnormal-process release/recovery semantics from
      firmware/driver before adding automatic recovery or clearing the graphics
      cleanup_unproven latch. Reap/signal status is not successful EGL teardown.
- [x] Coalescing scene display: display v2 canvas plus distinct display.async v1
      request/latest receipts, immutable snapshots, cached transform revisions,
      one inflight/one desired frame, retained final request and namespace-safe
      consumer reload. Four host/sanitizer/TV groups pass. Actual101-request
      burst drains its final identity receipt; full run observes412 GPU frame
      completions, host drain/unload and full restoration/hash match. Host222
      checks pass. Source18 modules; matching production16 unchanged.
      Subsequent UI-ASYNC supplies ui_fb integration; full visual/cold acceptance
      remains separate.
      SCENE-DISPLAY-2026-11-01.
- [x] Async ui_fb presentation/readiness: owned receipt sampler, both service
      holds/aliases, namespace/serial gating, cached dialog progress and final
      selected-screen repaint. Four host/sanitizer/TV groups and full host226
      checks pass, including CPU regressions. Real portal assets, project-local
      navigation, transition, UI reload and host-only module suspend/resume pass
      on actual display with full restoration/hash match. This is not full vendor
      UI/app execution, physical input or TV standby/cold ownership. UI-ASYNC.
- [x] Export/hash-verify vendor rail_hl01..11 and stage a complete temporary UI
      bundle on the real TV. Fix the24-icon capacity that truncated the loop;
      full-bundle regression, sanitizer and full host226 checks pass. Real GPU
      navigation/reload/module-cycle/restoration pass. RAIL-ASSETS-2026-11-01.
      Persistent installed assets remain unchanged and lack these frames.
- [ ] Accept visible rail frame order,30ms cadence and vendor appearance. The
      real-GPU test observed seven completed image revisions in a nominal660ms
      window; this is not eleven-frame scanout/cadence proof. Measure repaint/
      upload/coalescing costs and verify on the panel before closing parity.
- [x] Correct opaque portal canvas: recover unnamed theme#242628/raw opacity6
      through ARM conversion to alpha153; add display.alpha companion to CPU/
      GPU backends, preserve straight source-over and premultiplied scanout.
      Host227 checks/focused sanitizer pass. Real GPU UI readback24,23,22,153,
      navigation/reload/module-cycle/full restoration pass. UI-TRANSPARENCY.
- [ ] Independently compare translucent backdrop over real video with vendor
      appearance. Native framebuffer alpha is verified; visual parity is not.
- [ ] Owner-requested exact animation audit: compare timing/easing/geometry,
      APNG scheduling/frame order, focus reset, interruption/finalise and render
      sampling against firmware. Separate offline formula matches from actual
      vendor/native frame sequences; rail cadence and complete screens remain open.
- [x] Direct EGL native-window diagnostic: recovered4-byte dimensions, fb1/
      primary-buffer profile and swap/MAU contract. Eight effects at50Hz pacing
      passed on actual display, owner confirmed upright/full-screen/smooth;
      full16588800-byte snapshot/region/MAU/pan restored and separately hashed.
      Native cached-texture/pbuffer/thread-ownership regressions also pass.
      Prototype only; not the production display module or independent startup.
      EGL-WINDOW-2026-11-01.
- [ ] Audit pixel-format boundaries as UI work expands: raw 32bpp OSD is BGRA;
      OAI1/blit inputs are RGBA; saved screenshots are RGB. Preserve provenance
      of pre-fix red/blue-swapped captures and verify other API formats explicitly.
- [ ] Implement native OmarchyTV layout/theme on those services after vendor parity.
      Use the web port as a later UI reference, not evidence of native acceptance.
- [ ] Retire the browser launcher after native acceptance. Avoid measured web defects:
      silent freezes/stale heartbeat, slow boot/wake, missing HOME re-entry and
      browser-consumed TV-control keys.

## Final system replacement — deferred until implementation is complete

Owner-requested parity work: investigate how to replace the current Aurora system
with openaurora as the normal operating system application stack. Begin this
integration investigation once the implementation is complete; it is not the
next hardware experiment or authorization to deploy now.

- [ ] Recover the complete vendor boot/appmgr/service/dependency contract from
      firmware and determine how openaurora takes over its responsibilities,
      including startup order, hardware ownership, watchdogs, IPC and helper services.
- [ ] Design a persistent, reversible replacement using the supported unsigned
      startup path; preserve signed firmware and USB recovery. Identify any
      remaining runtime dependency on vendor Aurora and replace it explicitly.
- [ ] Validate normal operation with vendor Aurora absent: cold boot, restart,
      standby/wake, module reload, TV features/settings/services, network access
      and privacy protections. A one-off supervised takeover is not this acceptance.
- [ ] Verify migration of existing settings/device identity, failure recovery and
      rollback before making openaurora the default. Telemetry/advertising remain
      excluded; all other normal-operation parity requirements still apply.

## Access, recovery and deferred investigations

Current access/protection state is in MEMORY, evidence in STABILITY; no further
TV mutation is required now. The following are parked, not default next steps.

- [x] Recovery succeeded using the exact 584-byte script/current UBIFS; SSH/telnet,
      privacy blocks, settings/network and keeper verified afterward.
- [x] Documentation consolidated, correction/report/artifact indexes added; original
      chronology retained. See DOCUMENTATION-AUDIT-2026-10-31.
- [x] Required /tmp/opencode firmware extracts, analysed Ghidra projects,
      toolchains, Python tools and research assets moved to artifacts/workbench;
      active build/extractor paths updated and checked with empty /tmp.
- [ ] If payload maintenance is needed, budget /conf space, preserve keys/numeric
      owners and backups, verify extracted contents, script and image identities.
      Raw mtd13/chunk artifacts are older. Do not run write tests just to update docs.
- [ ] If full restore is actually needed, prepare/revalidate n00..n63.bin and exact
      device/mount identity. Current USB recovery is the tested /conf-only pair.
- [ ] Live dd flash path is unverified and uses an older image; not a routine task.
- [ ] Deep versus quick standby PHY/WoL behavior and access hardening policy
      (unauthenticated telnet). Disabling fallback is not currently authorized.
- [ ] Optional /vendor runtime overlay: cost/space, flag/revert, app/telemetry effects.
      Never modify signed on-flash volumes; an overlay is not needed for current DIAL.
- [ ] Distinguish logging-flag versus hosts-block effects, portal/search blocking
      impacts, OTA allowance and any router-side policy. No other LAN targets.
- [ ] Physical-access SPI dump/xboot/U-Boot signature-chain research if resumed.
      Existing NAND already supplies runtime firmware; SPI unlocking is not a native
      module prerequisite. Do not write bootloaders or sacrifice USB recovery.
- [ ] Exact VESxxx panel identification/official firmware request if ever needed.
      No sibling image flashing. OTA descriptor capture/Callisto USB verification
      questions remain separate research, not a reason to trigger an update now.
- [ ] Optional no-signal timeout overlay if owner wants it; current ~3-minute suspend
      is known behavior, not evidence of networking failure.
