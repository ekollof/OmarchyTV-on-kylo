# TODO — audited current checklist, 2026-11-04

Read AGENTS.md for constraints and MEMORY.md for last verified state.
The full previous checklist is preserved in
[the pre-cleanup archive](artifacts/archive/README.md). Checked entries below
close specific milestones, not whole subsystems. Tiers order work; they do not
remove vendor features from scope. Telemetry and advertising are excluded.

Audit: [TODO-AUDIT-2026-11-04](artifacts/reports/TODO-AUDIT-2026-11-04.md)
checks all182 previously checked entries against source, tests and saved evidence.
A check closes the stated bounded milestone: research, helpers, controlled tests
and dated hardware observations are not interchangeable. Historical test counts
and module counts/TV samples identify their original run, not the current whole source build.
The preserved claim inventory and per-item locators are linked from the audit.

Follow-up: [TODO-REAUDIT-2026-11-04](artifacts/reports/TODO-REAUDIT-2026-11-04.md)
accounts for all183 checks present at172cd57.182 were retained at their stated
boundaries; the added PNG milestone is reopened pending firmware comparison
and lifecycle/error acceptance. Recorder alpha composition is verified, but
its captures do not apply scene transforms. The report distinguishes the
passing committed build from the separate unfinished source edits.

2026-11-05 follow-up review covers all added/changed checks since8ebacd2,
including the socket fixtures, decoder comparison and grid/capture research.
See AGENT-REVIEW-2026-11-05 for corrections and current verification. Earlier
test counts remain dated evidence; no full decoder/display/ownership gate closes.

## Current acceptance gates

- [x] Offline display simulation feasibility: actual fbdev module with wrapped
      device calls on host/ARM qemu, and actual scene_gles with the host Mesa
      adapter. UI-FB-SIM; this audit reran all nine transform/orientation checks.
      Pixel-exact checks concern the test oracle, not complete frames versus
      vendor Mali or stock TV output.

- [x] Offline RGBA primitive composition using the production
      canvas: tests/ui_nav_display.c now composes with oa_canvas_over instead
      of forcing alpha255 and publishes display.alpha, so clear_rgba keeps
      transparent regions and partial source-over. check-ui-sources verifies
      the transparent panel surround, the opaque panel, invalid clear alpha,
      and half-alpha source over transparent/opaque destinations; full host
      check273 ok, focused ASan/UBSan and the same test under ARM/qemu pass.
      Evidence: artifacts/analysis/alpha-recorder-2026-11-04/ (frame2 mean
      alpha0.40625 =780/1920, corner alpha0). This is production-canvas
      fidelity, not vendor-Mali pixel or stock-scanout parity. Closes the
      TODO-AUDIT-2026-11-04 alpha defect. Scene transforms are recorded as
      metadata only; the recorder does not apply them to captured pixels.

- [ ] Offline full settings preview: wire a real-asset ui.dir capture and
      Mesa renderer integration, including transformed frames. Recorder alpha
      composition is corrected; its set_scene_transform only records arguments.
      The settings_panel OAI1 and slider/selection exporter already exist;
      the synthetic panel test's4x4 stand-ins are not a real-content preview.
      Vendor Mali rasterization/scanout comparison still requires the set.


- [ ] Independent startup: boot into openaurora without starting vendor Aurora;
      full-screen scanout, input, fallback and resume all verified. Owner authorized
      an idle hardware/CEC/audio test window on November 1; see AGENTS.md.
- [ ] Reloadable modules: state, service holds, callback ownership and dependencies
      behave correctly through start/stop/reload/suspend/resume and failures.
- [ ] Reproduce vendor screens, remote behavior, settings and the remaining TV
      functionality; only then change UI/UX to native OmarchyTV.

## Core and runtime

- [x] Review later UI/launcher/display work (WORK-REVIEW-2026-11-04): ARM
      scrollbar wrap/query ordering/four-state caches; explicit launcher ad/
      tracking exclusion; image byte flag/guards; apps reload/restart/global
      sorting/cache/hidden-track fixes. Full host, focused ASan/UBSan and42
      ARM reference groups pass, plus TV CPU/read-only DB/loader checks.
      These close the named bugs, not full rendering or independent startup.
- [x] Review UI engine/skin lifetime: skin-owned posted trampolines, bounded
      queue, v2 registration, forced unload/rebind, failed receipts/readiness,
      companion return, timer failure and new-skin identity. Host242/focused
      sanitizer/native contracts and actual GPU lifecycle/restoration pass.
      UI-ENGINE-REVIEW. Content/fallback/visual acceptance remain open.
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
- [x] Provider lifecycle rebinding: mod_ui_vendor discovers services published after
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
- [x] Descriptor-size validation and module-owned worker posts were introduced
      in ABI2 and are retained in current ABI3. Historical ABI1/2 artifacts
      are preserved, not accepted as ABI3 modules. Host/sanitizer/ARM coverage
      is recorded in MODULE-SAFETY; current host safety checks pass.
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
- [x] --check validates ELF/ABI metadata and library/symbol/version closure
      without module start. core/check.c and runtime_check.sh are implemented
      and pass this audit's host checks. The matching ABI3 core +16-module
      TV preflight in MIXER-MODULE was a dated production-stage test, not
      preflight of every module now present in source. ABI1 recovery remains
      installed; latest development builds are separate ABI3 subdirectories.

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
- [x] Host configuration is an immutable per-host initialization snapshot;
      initializing another host cannot leak settings, exhaust the first host's
      capacity or invalidate borrowed values. Reinitialization after caller
      teardown clears prior values, including NULL/missing-file config. Existing
      48-entry bounds, first-duplicate-wins and module-reload snapshot behavior
      are retained. Regression fails against the old global table; strict host,
      ASan/UBSan with leak detection and ARM/qemu pass, plus54 core lifecycle/
      power/control/skin checks. HOST-CONFIG-2026-11-04. Internal host policy,
      not vendor-setting persistence parity or a new host teardown API.
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
      AUDIO-AMP-IMAGE now supplies the data-only38-register profile conversion,
      with host/sanitizer/ARM tests and6000 native Init/C profile comparisons,
      plus all38 write-failure/retained-image cases. GPIO descriptions are
      absent and hardware callees controlled in that differential. Open/Close
      only return object!=NULL; ResumeDevice only starts a task and its task
      ignores the init result. AUDIO-AMP-PROFILE supplies bounded hwprofile
      input snapshots with the recovered power/default rules;2312 native
      byte-payload cases and44 total reference groups pass. AMP-PROFILE-MODULE
      now publishes reloadable audio.amp_profile v1 plus amp-profile CLI/Unix
      query; real DSO holds/failures/removal/suspend/reload and actual socket
      tests pass host/sanitizer/ARM, including the saved profile. AMP-INIT
      adds a blocking injection-only Init sequencer:29952 native
      Init/Reset/Mute trace/image/cache cases and45 total reference groups pass.
      AMP-OWNER adds an injected owning thread with copied inputs/completions,
      retained image and draining/nonblocking join, including TLS destructors;
      four host/sanitizer/ARM groups and the same injected-I/O groups on the TV
      pass (no device/fb1 access; staging removed). This is not a native backend or paired
      shutdown. AMP-I2C adds an owned blocking bus3/address34 session with
      explicit write permission and Init boolean adapter. Fresh Aurora/NDAL3/
      PreInit instruction review establishes packets and10ms post-write delay;
      three host/sanitizer/ARM groups and the refactored native read03..05 pass
      (1b/05/05, staging removed, no fb1 access). Writes remain injection-tested;
      exact-message-count rejection is stricter than the vendor false-success
      policy and not claimed as bug-for-bug error parity. Fd ownership is not
      amplifier authority. AMP-GPIO-QUERY recovers HAL getter side effects and
      supplies fixed direct114 GET670d/670f, with no pin claim/export/config.
      Two host/sanitizer/ARM groups and native query pass: level1, cached kernel
      direction0(output), staging removed/no fb1 use. Cached direction/latch
      are not voltage or ownership evidence; reset/mute/MCU writes remain open.
      AMP-STBC recovers the three-argument MCU getter, separate device status
      and default video24/process-global fd mapping. Injected fixed-MCU8 GET/
      level-command codec passes two host/sanitizer/ARM groups/full host394;
      byte16/byte8 remain raw, device failure is stricter than vendor transport-
      only success. No native fallback, fd authority or MCU operation tested.
      AMP-VIDEO-OWNER identifies extin's simple actual open/close callbacks
      through relocations, distinct from mutating input-selection cleanup.
      Metadata-only endpoint read followed helper WoL; no video24 open/MCU/fb1
      operation. Trace libv4l2_fd_open before choosing a native backend; static
      callbacks do not establish coherent authority or cold readiness.
      Finish audio consumer/provider holds and module integration,
      retained image acquisition, coordinated GPIO/MCU/I2C/worker
      ownership and hardware lifecycle acceptance; wrapper results and controlled
      callbacks do not establish native bring-up.
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
      Later mod_cec routes injected frames; these helpers do not establish
      native transport integration or hardware acceptance. CEC-AUDIO-CONTRACT.
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
- [x] Broadcast service DB inventory: firmware table mapping from
      SQLiteStreamStorage, read-only adapter for DVBSerTable/EASISerTable/
      AnalogSerTable, bounded text, table-order rows, missing-table failure;
      `service.list` v1 and `service-list` CLI; 525-row dump (445 DVB/67 EASI/
      13 analog), synthetic edge cases, reload/stop and real socket lifecycle.
      Exact stored flags predicates (visible/available/encrypted/numeric/
      missing), `service-list stats`, and name/channel_no query ordering with
      source/predicate filters are included. Host409 ok, verify45. SERVICE-DB.
      Limits: the live DB's m_lcn is 0x....ffff sentinels; getLcn and
      isHiddenService use runtime EASI, so active-list LCN/filter parity
      requires a runtime probe or an explicit provisional-order decision.
      Tuner, EPG/PVR, channel-list UI and stock parity remain open.
- [x] Remove production `system()` calls: display mdev node repair now uses
      `posix_spawn("/sbin/mdev", "-s")` plus `waitpid`; the channel-list test
      uses `nftw` cleanup instead of a shell command. The display test wrappers
      remain as no-system regression guards. Host410 ok, verify45. No TV access.
- [x] Broadcast service foundations: project-internal tuner/epg/pvr v1
      contracts and a host-only fixture. Tuner scan state machine, EPG overlap
      queries with bounds/caps, PVR add/list/cancel, reload and teardown are
      host-tested. Host412 ok, verify45. BROADCAST-FOUNDATION. Limits: fixture
      only; no tuner hardware/scan, EIT/EPG semantics, recording engine,
      production module, CLI/socket/UI or persistence. No TV access.

- [x] Read-only testtool/EASI probe client: recovered testtool TCP1986,
      `GETCHANNELLIST` RSN/name format and `ACTIVECHANNELLIST` XML; added a
      whitelist-only probe (ACTIVECHANNELLIST/GETCHANNELLIST) with bounded XML
      parsing and setter rejection, plus host fixture and ARM build. Host411
      ok, verify45. EASI-PROBE. Blocker: TV testtool server is disabled
      (`is_testtool_socket_enabled=0`, TCP1986 refused). Enabling it requires a
      profile change plus vendor restart or a live-process pause; both are
      deferred without physical recovery. Active LCN/visibility still need a
      runtime call path; provisional UI remains.

- [x] Provisional channel-list UI: `ui_vendor` `SCREEN_CHANNELS`
      (`ui.screen=channels`) consumes the stored `service.list` channel_no/name
      query, shows a visible "PROVISIONAL STORED ORDER - NOT RUNTIME EASI"
      marker, supports Up/Down/Select/Exit and is recorder-tested with
      synthetic and real font packs. Host410 ok, verify45. CHANNEL-LIST-UI.
      Limits: no runtime EASI LCN/visibility, stock channel-list key path,
      measured stock geometry/typography/scrolling, tuner/EPG/PVR or scanout.
- [ ] Apps/browser/media integration, picture quality/FRC, network and Bluetooth.
- [ ] Tuner/DVB-T/DVB-S, EPG/service lists, teletext/MHEG/interactive TV, PVR/
      timeshift/playback, CI+/conditional access and associated persistence.
- [ ] Portal, voice, hotel mode, search and OTA user-facing behavior as applicable
      to vendor parity; omit telemetry/ad components. Do not silently descope
      broadcast features because this set is currently used with HDMI.

## Native UI, assets and OmarchyTV

The callback-driven common/ helpers below are implemented and independently
tested at their stated boundary. They are not all connected to ui_vendor or
a native widget backend. Current shared integration gates are live item/style/
resource acquisition, child rendering/visibility, complete SVG/PNG raster,
settings/navigation consumers, scanout ownership, startup and visual/interaction
acceptance. Dated case counts identify development milestones; current aggregate
ARM checks are1771396 row renders,276 setters,536 image-ID/IDM,80 SVGImage,
14576 scrollbar geometry/state,32 skin and36 scrollbar render cases.

- [x] Review recent row/image work (UI-REVIEW-2026-11-04): fix32-byte image
      resource capacity/alignment, renderer arithmetic UB, zero-divide claim,
      raw checked byte and independent paired-child test coverage. Actual
      divider and1771396 renderer/276 setter/56 image comparisons pass; full
      host/Unix IPC, ASan/UBSan, ARM/qemu and focused real-TV CPU units pass.
- [ ] Resolve/accept malformed zero-range fault policy before claiming full
      bug-for-bug fault parity: firmware raises SIGFPE for all zero divisors;
      current replacement returns EDOM and preserves earlier rendering effects.
- [x] Correct the renderItems differential's presence coverage (11 keys, not
      7) and implement type 0x9's created slider path:66312 genuine native/C
      comparisons plus host/sanitizer/ARM-qemu/TV CPU pass.
      SETTINGS-ROW-COVERAGE. A third NUL-sled (SVGSlideBar::setWindowRect)
      was caught; enumerate every callee before sweeping a new case.
      This is the dated11-key milestone; current render coverage uses15 keys,
      including independent paired-image states (UI-REVIEW).
- [x] Implement SettingItemsListBoxRow::renderItems type 0xa (second slider
      row):70456 native/C comparisons and prior groups pass; full host/Unix IPC,
      ASan/UBSan, ARM/qemu and focused real TV CPU pass. SETTINGS-ROW-A. Full
      callee audit preceded the sweep; 0x1e8.. text-box names, the row+0x1498
      slider names and the created setSliderRangeValues are asserted.

- [x] Implement SettingItemsListBoxRow::renderItems type 0xb (slider bar +
      images):74604 native/C comparisons and prior groups pass; full host/Unix
      IPC, ASan/UBSan, ARM/qemu and focused real TV CPU pass. SETTINGS-ROW-B.
      Both-rect gating, the image-state fork, the mixed-state image1
      re-create quirk and the slider name/range/value tail are asserted; the
      harness separates the image children's created state.
- [x] Implement SettingItemsListBoxRow::renderItems type 0xc (text2/text3/
      text4 + slider):312224 native/C comparisons and prior groups pass; full
      host/Unix IPC, ASan/UBSan, ARM/qemu and focused real TV CPU pass.
      SETTINGS-ROW-C. Four own literal redirects, alignment 6 on the 0x1f68 box
      only, both row UStrings, and the nameless uncreated slider are asserted;
      the new SVGTextBox::setTextAlignment trampoline was added after the callee
      audit.

- [x] Implement SettingItemsListBoxRow::renderItems text-entry variants
      0x11..0x15:394384 native/C comparisons and prior groups pass; full
      host/Unix IPC, ASan/UBSan, ARM/qemu and focused real TV CPU pass.
      SETTINGS-ROW-11-15. Mode/max pairs, 0x13's omitted max/cursor, the four
      new UString offsets and 0x15's numeric source are asserted; all
      row-side UString reads now go through row_ustring.
- [x] Implement renderItems types0x17..0x19 (progress rows), including ball,
      double-directional, alexa-style and type0x19 visibility branches.
      SETTINGS-ROW-17-19 records the original886060-case run. Current aggregate
      render coverage is1771396 comparisons and uses the shipped divider,
      superseding the old C arithmetic trampoline. UI-REVIEW explicitly
      retains EDOM as a zero-divisor policy difference. All cases0..0x1a
      exist as callback-driven code; real child drawing is a separate gate.

- [x] Real LauncherHelper::getImageId 62-way mapping: `common/
      launcher_image_id.c` matches the verbatim shipped body over469
      op/state pairs; host structural checks, ASan/UBSan, ARM/qemu, full
      verify.cjs and focused TV CPU pass. LAUNCHER-IMAGEID. The render ops
      glyph_id hook is proven to accept the real function.
- [x] setSelectionBoxImageId internals (0xd54374): store0x2e54, "image"
      lookup, unconditional destroy at0x2cf0, create when id nonzero;
      1770796 native/C comparisons and prior groups pass, plus host,
      ASan/UBSan, ARM/qemu, verify.cjs and focused TV CPU.
      SETTINGS-ROW-SELIMAGE. The shipped body is copied verbatim; only its
      guard/key literals are redirected.
- [x] SVGImage::create/destroy orchestration: `common/svg_image.c` matches
      the verbatim shipped bodies over18 cases; host, ASan/UBSan, ARM/qemu,
      full verify.cjs (35 ok) and a focused TV CPU run pass. SVG-IMAGE.
- [x] SVGImage::imageCreate branching/arguments (0x10c9430):72 svg-image
      native/C cases and prior groups pass; host, ASan/UBSan, ARM/qemu,
      verify.cjs and focused TV CPU pass. SVG-IMAGE-CREATE. Vendor surface/
      draw calls remain stubs.
- [x] SVGImage::create(int,EASIPoint const&,UIWindow*) overload (0x10c9554)
      and LauncherHelper::convertOperationIdToIDM (0xe4ae0c): svg-image80
      native/C cases and launcher536 comparisons pass, with host, ASan/UBSan,
      ARM/qemu, verify.cjs and focused TV CPU. SVG-IMAGE-POINT-IDM. Vendor
      callees remain stubs.
- [x] SettingItemsListBoxRow construction (0xd53d0c): base+17 children,
      10 UStrings, 47 default fields and the mirror branch;96 native/C checks
      plus host/ASan/ARM-qemu/verify.cjs/TV CPU. SETTINGS-ROW-CONSTRUCT.
      Child constructor internals remain vendor stubs.
- [x] Child widget-state qualification (SVGWindow::setFocus/setActive and the
      SVGTextBox setFocus/setElipses overrides):36 native/C cases plus host,
      ASan/UBSan, ARM/qemu, verify.cjs37 ok and focused TV CPU.
      SVG-WIDGET-STATE. Vendor redraw calls remain stubs.
- [x] LauncherItem resource conversion: ordinary row fields, thumbnail-path
      prefix and shared refcount match shipped0xe4794c over8 native cases.
      WORK-REVIEW deliberately replaces the3 advertising/tracking copies
      with empty UStrings. The fixture identifies those policy differences;
      it does not claim complete vendor-object byte parity. LAUNCHER-ITEM.

- [x] settingsDB LauncherItem read adapter and reloadable launcher.db/CLI:
      read-only all/by-parent filters and ordinary fields are implemented.
      WORK-REVIEW excludes the3 advertising/tracking SQL columns and clears
      their conversion destinations. Current source does not port those fields.
      Host/service/ARM tests pass; dated live TV query returned54 rows.
      LAUNCHER-DB. Vendor SequentialQuery/cache/default semantics are not ported.

- [x] ui_vendor consumes ordinary parent0/type2 launcher rows, orders globally
      by order/id and uses named local OAI1 thumbnails. WORK-REVIEW fixes
      cap-before-sort, restart/reload state and shared cache exhaustion; the
      70-row corpus retains the earliest64 and browses all pages with other
      assets cached. UI-LIVE-CARDS preserves the earlier dumped-DB portal test;
      OA-CANDIDATE subsequently captured the live-DB apps grid. Dynamic DB
      refresh and stock layout parity remain open; downloaded PNGs are handled
      by the image.decode prototype below, whose acceptance remains open.

- [x] SVGScrollBar setScrollPosition/calculateBarSizeAndPosition helpers.
      WORK-REVIEW supersedes SVG-SCROLLBAR's3120-case baseline with14576
      native comparisons, explicit32-bit wrap and the shipped signed divider.
      Apps track1750,180,8x720 is reconstructed from the resource; this lower
      geometry/state milestone is not complete native widget rendering.

- [x] image.decode JPEG extension: the module now also uses the vendor
      libjpeg-turbo TurboJPEG API (2.0.1 on the TV) after the classic-libpng
      PNG path. Two real JPEGs (DMR-small 48x48, DMR-large 120x120) match the
      shipped JpegImageDecoder class exactly under TJFLAG_FASTDCT; ImageMagick
      accurate-DCT hashes differ by up to8. Truncated/missing/non-image inputs
      reject; ui_engine tries PNG then JPEG and copies decoded pixels before
      provider release. check-ui-jpeg-cards renders both 48/120 px sources with
      first-pixel checks; host395 ok, focused ASan/UBSan clean, decoder/UI tests
      pass ARM/qemu with the vendor libpng/zlib and libjpeg-turbo2.0.1.
      IMAGE-DECODE. Limits: CMYK/YCCK and APNG reject; the
      later direct vendor/production differential includes two durable foxxum
      JPEGs as well as the DMR files. Two other foxxum files, YCCK/APNG policy,
      worker/frame lifetime and stock parity remain separate acceptance work.

- [x] On-TV visible PNG/JPEG apps grid through the supervised fb1 binding:
      the current image_decode v2/ui_vendor v0.2 candidate painted18 live DB
      items (15 visible, scrollbar0/600) from an empty ui.dir and the real
      /buffer/downloaded_images. The capture shows two visible JPEG cards
      (foxxum_thumb_3753/1685, 384x216) plus13 PNG cards; the two visible JPEG
      crops match nearest-scaled originals within≈2.1%/1.1% AE. fb1 pixels/
      yoffset were snapshotted and restored, staging removed, /proc scan0,
      vendor PIDs intact. VISIBLE-IMAGES. Limits: supervised only; full
      region/MAU/pan ownership equality and independent startup not proven.

- [x] Real socket/CLI image.decode lifecycle: a host started with an empty
      autoload config is driven through the actual `--sock` control plane and
      stdin: load/start, PNG and JPEG CLI decode hashes, invalid-file errors,
      status, suspend/EAGAIN/resume, stop/ENODEV/start reuse, reload, and
      unload/reload. `image-decode` is a real control command; no display/HAL/
      TV module is loaded. Full host395 ok, verify.cjs45. IMAGE-DECODE-IPC.
      Limits: host-only; held-provider rejection and cold startup remain
      separate.

- [x] Real ui_vendor/recorder-display socket lifecycle: `ui-state` exposes
      `ui.portal screen`, and a host with mod_nav_display + ui_engine +
      ui_vendor is driven over `--sock` and stdin through start/use/stop/start/
      reload/unload/reload, including a display-provider reload and consumer
      restart. No TV/HAL/framebuffer. Full host396 ok, verify45.
      UI-VENDOR-IPC. Limits: recorder display, not physical fb1; full module
      set/holds and cold startup remain.

- [x] Real non-HAL UI stack socket lifecycle: settings/launcher.db,
      image.decode, recorder display, ui_engine and ui_vendor run as a real
      dependency chain over `--sock` and stdin. Launcher rows, decode hashes,
      ui-state, and reloads of image_decode/settings/ui_engine/nav_display/
      ui_vendor pass in dependency-safe order. Host397 ok, verify45.
      UI-STACK-IPC. Limits: non-HAL modules only; physical fb1, full hardware
      module set, holds under all consumers and cold startup remain.

- [x] Fake-fbdev display_fb socket lifecycle: an LD_PRELOAD fake /dev/fb1 and
      recovered region/MAU/pan ioctls run the real mod_display_fb over the
      socket. Self-test paint changes yoffset/region/pixel FNV; suspend, stop,
      start/reload/stop and unload/load/start/stop all restore the initial
      yoffset, MAU, eight region words and pixel hash. Host401 ok, verify45.
      FAKE-FB-SOCKET. Limits: user-space fake; real kernel/driver behavior,
      physical fb1/TDGA, vendor coexistence and cold startup remain.

- [x] Apps-grid compiled-path audit: navigation code 0x80 constructs
      FullScreenPortalDialog; senders are doNavigateFullScreenMarket
      (0xd1aa2c, market return 0x29a) and OperationFinder::handleOperation
      case 0x79 (0xd3e6a0). Conditional compiled entry paths exist; a working
      stock key path on this model is unverified. The
      +0x1fc/+0x200 contradiction is resolved: RectHandler transforms named
      rectangles before storage, rowPos is the transform's own mask, and the
      stored rowPos is the cell surface, so the fields reduce to zero-width
      horizontal masks rather than -1015/+1005. check-vgrid models first/last
      cells and a nonzero mask. The operation-0x79 UI mapping and stock capture
      remain open. APPS-GRID correction. STOCK-PARITY-2026-11-05. No TV access.
      AGENT-REVIEW subsequently fixes the missing second left inset in the
      production button right edge and two wrong nonzero-inset test oracles,
      compared with fresh10ca8ac instructions; zero-inset portal is unchanged.

- [x] Corrected-capture comparator and P1 blue-shift correction: the P1
      stock portal is another pre-correction BGRA capture; its corrected copy
      is recorded with raw/corrected samples and geometry. capture_compare.py
      supports explicit BGR correction, named regions and JSON output, and
      reproduces the earlier portal AE/RMSE values exactly. A fresh
      apps-grid comparison is prepared via apps-grid-regions.json and
      `make compare-stock-apps-grid`, which refuses until the stock capture
      exists. Host404 ok, verify45. STOCK-PARITY-2026-11-05. Limits: no stock
      apps-grid capture, pixel parity or TV access.

- [x] Plat-consumer socket lifecycle with a host-only plat fixture: audio
      (audio.enable=0), source (source.enable=0), ir (empty ir.device) and
      panel load/start and publish audio/source/input/panel services. Reload
      plat is refused while they are active; after dependency-ordered stops
      the provider reloads, all four consumers reload, and a fresh
      unload/load/start cycle passes. Five consecutive runs. Host403 ok,
      verify45. PLAT-CONSUMERS-SOCKET. Limits: fake plat; real HAL, devices,
      hardware-module reload and cold startup remain.

- [x] Graphics/scene_display socket lifecycle with the fixture renderer:
      graphics publishes display.gpu:v1 via oa-scene-broker; scene_display
      requires it and publishes display:v2/alpha/async. Reload and
      dependency-ordered reload pass, a graphics reload is refused while the
      GPU service is held, the broker child is replaced then reaped, and a
      fresh unload/load/start cycle works. Host402 ok, verify45.
      GRAPHICS-SOCKET. Limits: fixture renderer; real EGL/Mali, physical fb1
      and cold startup remain.

- [x] Fixture-backed module socket lifecycle: ui_theme (synthetic BinSVG),
      ui_profile (synthetic EFSL), audio_amp_profile (extracted hwprofile),
      audio_curve (synthetic 1668-byte AQ/profile0) and audio_mixer (real host
      broker) load/start, publish their services, reload, stop/start or
      unload/load, and exit cleanly. Host400 ok, verify45.
      FIXTURE-MODULES-IPC. Limits: host fixtures; hardware modules/stock/cold
      startup remain.

- [x] Hardware-free module matrix over the real socket: ticker -> ui_clock ->
      ui_anim, plus ui_navigation, ui_list, power and cec load/start and are
      reloaded in dependency-safe order; nav/list queries and CEC's async
      cleanup retry pass. No display/HAL. Host398 ok, verify45.
      MODULE-MATRIX-IPC. Limits: hardware modules and cold startup remain.

- [x] Real ui_omarchy/skin socket lifecycle: the existing `skin_socket.sh`
      test is now wired into `make check` with recorder display, ui_engine,
      ui_vendor and ui_omarchy. Missing/invalid configured skins fall back to
      vendor; `skin omarchy`/`skin vendor` switch to ready; enter failure keeps
      vendor with fallback1/error5; quit exits cleanly. Host399 ok, verify45.
      SKIN-SOCKET. Recorder-only; physical fb1 and cold startup remain.

- [x] Vendor decoder-class differential: an ARM LD_PRELOAD probe executes the
      shipped PngImageDecoderImpl and JpegImageDecoder under qemu before vendor
      main, using vendor MemoryBufferSource/MutableImage and requested RGBA
      ImageProperties (model3; model2 is BGRA). Nine corpus PNGs plus
      gray1/2/4/8, gray+alpha, palette, palette+tRNS, RGB16, RGBA16, interlaced
      and gAMA variants are byte-exact requested RGBA. DMR-small/large JPEG
      are byte-exact against TurboJPEG `TJFLAG_FASTDCT`, the mode now used by
      image.decode; ImageMagick's default accurate DCT differed by up to8.
      A progressive JPEG is byte-exact and a generated CMYK JPEG is rejected
      by both classes. Two available real foxxum JPEGs are byte-exact; the
      other two were not offline. The2026-11-05 review now runs the actual
      image.decode DSO on every same input under ARM/qemu, including rejection;
      the original harness compared vendor output with external libraries only.
      Available foxxum inputs are durable under workbench/decoder-corpus and
      included by default; explicitly requested missing inputs now fail.
      VENDOR-DECODER-DIFF / AGENT-REVIEW. This differential uses no TV/HAL/UI. Limits:
      remaining two foxxum files, YCCK, APNG, worker/frame lifetime and
      on-TV/stock parity remain.

- [ ] Complete image.decode acceptance: the classic-libpng PNG conversion
      sequence from Aurora's b30918 `setConversions`, the fast-DCT TurboJPEG
      path and the vendor-class differential above now pass the maintained
      corpus, format variants, malformed/truncated input, suspend/resume and
      reload tests, and the supervised on-TV grid. Provider pixels are copied
      then released. Still need the two uncopied foxxum files, YCCK/APNG
      policy, worker scheduling/cancellation and stock pixel parity.
      Reopened by TODO-REAUDIT-2026-11-04; narrowed, not closed.

- [x] Stock-capture parity analysis: the old `shot-launcher-right.png` is an
      R/B-swapped pre-correction capture; after channel correction it matches
      the current Prime/Canal color orientation. Same-geometry portal cards
      still differ in artwork/crop (Netflix RMSE0.376, Prime0.216, others
      0.066..0.104), background is13,14,14 vs36,38,40, and stock status chrome
      is absent. The old capture is not an apps-grid or pixel oracle.
      STOCK-PARITY. A fresh same-session corrected stock apps-grid capture is
      still required; no TV operation in this analysis.

- [x] Scrollbar skin minimum helper: moving caps20+16 yield36px; recovered
      standard-name lookups and supplier cache behavior match32 native cases.
      WORK-REVIEW corrects the old two-orientation interpretation: +6c is
      the four SVGWindow UFA/FA/UFI/FI states, with4 cache/name entries.
      SVG-SCROLLBAR-SKIN retains the dated16-case baseline.

- [x] Scrollbar renderBackground/renderBar call sequence and geometry match
      36 native cases across all4 widget states, including changing window
      getters. Geometry comes from the first query; placement uses the second.
      WORK-REVIEW supersedes SVG-SCROLLBAR-RENDER's8-case baseline. Vendor
      renderer callbacks remain controlled stubs here; paint_apps separately
      supplies an immediate raster adapter, not a general SVG renderer.

- [x] Apps-grid prototype and immediate scrollbar raster adapter exist:
      SCREEN_APPS5, reconstructed5x3 grid and1750,180,8x720 track. Current
      host tests cover20/70 rows, hidden non-overflow tracks, sorted64-card
      retention, asset-cache pressure and reload/restart state. UI-APPS-GRID's
      first GPU capture failed; OA-CANDIDATE later obtained a live-DB scanout
      image. Neither proves stock appearance or full native apps; downloaded
      PNG loading is covered separately by IMAGE-DECODE.

- [x] Historical supervised scanout test: OA-CANDIDATE captured18 live apps
      and the600px scrollbar on vendor-initialized fb1 with stock still owning
      the hardware. WORK-REVIEW hardens current display_fb to reject HAL init,
      avoid mdev and link no HAL libraries; host and TV loader checks pass. The
      revised binding later drew the live PNG/JPEG grid on the TV
      (VISIBLE-IMAGES). The earlier stock screenshot and the pixel/yoffset
      restore did not prove full backing/region/MAU/pan restoration.

- [x] Offline framebuffer pixel/region/pan snapshot helper: common/fb_snapshot saves
      pixels/var/fix, all8 region words (RD0x8020c86c/WR0x4020c86b), MAU
      (read-only, verified) and pan; display_fb.display.restore_snapshot=1
      snapshots before region change and restores on stop/suspend, with
      resume re-snapshot; fb-restore uses an FBR2 file. Fake-fb tests cover
      save/restore, MAU mismatch, region/pan failures, the tool file, and the
      actual display module. Host354 ok, focused ASan/UBSan clean,
      FB-SNAPSHOT-COLD-BUNDLE. This is offline implementation, not live-driver
      equality. FB-RESTORE-REVIEW corrects the broad "full" label: var/fix are
      saved metadata, not a restored mode. Changed backing/layout/mode/MAU
      now rejects before writes; mmap/sync errors still attempt region/pan;
      failed backup-file close reports failure. Regression/focused sanitizer/
      ARM-qemu tests pass. No new native restoration acceptance.

- [ ] Pair temporary display binding with owned full backing/region/MAU/pan
      restoration and failure cleanup on the real driver. The offline
      snapshot/restore and display_fb restore path exist; live equality,
      suspend/resume ordering and failure cleanup remain unverified. The module
      originally logged restore failure and freed the snapshot; DISPLAY-CLEANUP
      now retains failed restore/fd state, returns suspend errors, and moves
      stop/unload restore/unmap/close onto a worker with nonblocking actual join.
      Both variants pass host/sanitizer/ARM-qemu failure/blocked-driver tests;
      firmware release/PMI paths were decompiled and compared, with the retry
      layer explicitly identified as project policy. DISPLAY-DSO adds actual
      display_fb host/loader/control-dispatch hold, blocked/force reload,
      deferred resume, failed start and shutdown tests: five groups pass host/
      sanitizer/ARM-qemu. GDB confirmed a completed-worker shutdown poll(-1)
      hang after the last timer disappeared; shutdown poll now caps at20ms.
      Still need exclusive authority, native equality and external socket-client
      display integration. Start/draw/
      suspend remain synchronous; this is not kernel power parity or complete
      fallback. FBR2 native-struct ABI portability and file
      validation remain open. Current TV permission excludes active fb1.
- [ ] Cold independent startup without vendor Aurora: establish MCU authority,
      watchdog/fallback and full display ownership/restoration from firmware.
      A historical FreshWDT return1 did not prove rejection or impossibility.
      On-TV PNG/JPEG display is now covered by the supervised VISIBLE-IMAGES
      test; stock apps-grid pixel parity remains. An ARM cold-start bundle
      (openaurora,25 modules, oa-supervise, fb-restore, cold config) is built
      and manifest-verified offline by `make cold-bundle`
      (FB-SNAPSHOT-COLD-BUNDLE), but not executed. The owner's current permission
      allows non-display tests only: active fb1, cold takeover and stock
      interruption remain outside this test window.
- [x] Implement SettingItemsListBoxRow::renderItems type 0x9 (slider row):
      95624 native/C comparisons and prior groups pass; full host/Unix IPC,
      ASan/UBSan, ARM/qemu and focused real TV CPU pass. SETTINGS-ROW-SLIDER.
      slidebar/text2 order, the empty-char* textbox create and the formatted
      value, plus the slidebar range/value are asserted.

- [x] Implement SettingItemsListBoxRow::renderItems type 0x8 (plain button):
      43976 native/C comparisons and prior groups pass; full host/Unix IPC,
      ASan/UBSan, ARM/qemu and focused real TV CPU pass. SETTINGS-ROW-BUTTON8.
      Name order 1/0/3/2, icon-0 create and the created setText refresh are
      asserted; no new callbacks and no crash input.

- [x] Implement SettingItemsListBoxRow::renderItems type 0x7 (banner
      button):40904 native/C comparisons and prior groups pass; full host/Unix
      IPC, ASan/UBSan, ARM/qemu and focused real TV CPU pass.
      SETTINGS-ROW-BANNER. Direct-string key, no destroy/images, created refresh
      without setText and setDrawOnTop are asserted.

- [x] Implement SettingItemsListBoxRow::renderItems types 0x5/0x6 (launcher
      button + image1/image2):38216 native/C comparisons and prior groups pass;
      full host/Unix IPC, ASan/UBSan, ARM/qemu and focused real TV CPU pass.
      SETTINGS-ROW-BUTTON. Two ABI corrections (two-arg getImageId,
      icon-carrying create) and the null-image1 rebuild crash exclusion are
      asserted. LAUNCHER-IMAGEID now supplies the real mapping behind that callback.

- [x] Implement SettingItemsListBoxRow::renderItems text-entry family
      0xd/0xe/0xf/0x10:33608 native/C comparisons and prior groups pass; full
      host/Unix IPC, ASan/UBSan, ARM/qemu and focused real TV CPU pass.
      SETTINGS-ROW-TEXTENTRY. Mode/max pairs, the two created sources and the
      1/0/3/2 name order are asserted; an un-stubbed setText callee was caught
      and all twelve family callees are audited.

- [x] Implement SettingItemsListBoxRow::renderItems type 0xd (text-entry
      box):24392 native/C comparisons and prior groups pass; full host/Unix IPC,
      ASan/UBSan, ARM/qemu and focused real TV CPU pass. SETTINGS-ROW-13. Name
      order1/0/3/2, entry-mode0/max-chars4/cursor-auto and the created
      numeric-contents source are asserted.

- [x] Implement SettingItemsListBoxRow::renderItems types 0x2/0x3 (selection
      box):21320 native/C comparisons and prior groups pass; full host/Unix IPC,
      ASan/UBSan, ARM/qemu and focused real TV CPU pass. SETTINGS-ROW-SELECTION.
      Slot order 1/3/0/2 and the lookup-before-imageid ordering are asserted;
      SETTINGS-ROW-SELIMAGE now supplies that helper.

- [x] Implement SettingItemsListBoxRow::renderItems type 0x16 (checkbox):
      7592 native/C comparisons and prior groups pass; full host/Unix IPC,
      ASan/UBSan, ARM/qemu and focused real TV CPU pass. SETTINGS-ROW-16.
      UString row-slot deref and the seven-arg preserving trampoline are
      asserted; the presence grid is five keys.

- [x] Implement SettingItemsListBoxRow::renderItems type 0x1a (shared-tail
      0x2e8c name branch + button case body):3032 native/C comparisons and prior
      groups pass; full host/Unix IPC, ASan/UBSan, ARM/qemu and focused real TV
      CPU pass. SETTINGS-ROW-1A. The case body's own "button" literal needed an
      owned-string redirect; null key2rect still runs the case body with no
      tail.

- [x] Implement SettingItemsListBoxRow::renderItems for types 0/1/4 plus
      0x1b/default exits: shared 0x104 tail, type-selected children, created
      gates and always-create with 2264 native/C comparisons and prior groups
      pass; full host/Unix IPC, ASan/UBSan, ARM/qemu and focused real TV CPU
      pass. SETTINGS-ROW-RENDER. Jump-table dispatch, reconstruct fallthrough
      into case 4 and dead-temp button create verified against objdump/ELF.

- [x] SettingItemsListBoxRow label/progress/range setters: type-selected child,
      created gates, stores and wrapped percentage calculation. Current native
      group has276 comparisons, using the shipped signed divider. UI-REVIEW
      supersedes SETTINGS-ROW-SETTERS' earlier324 cases and saturated-zero
      assumption. Zero-range EDOM is an explicit replacement policy; native
      fault parity remains open. Real setters exist behind the renderer ops.

- [x] Implement SettingItemsListBoxRow::setType cache/rebuild/render/visibility
      controller and bounded owned-model type-event join:12480 shipped ARM/C
      cases and prior groups pass; full host/Unix IPC, ASan/UBSan, ARM/qemu and
      focused real TV CPU pass. SETTINGS-ROW-TYPE. Preserve exact qualified
      child order, omitted1ca8 for type24 and unknown-type label-only behavior.

- [x] Implement owned setting-list item/text/value/image snapshot and draw
      providers;2048 real-provider calls plus allocation-failure/deep-copy/
      reentry checks pass strict full host/Unix IPC, ASan/UBSan, ARM/qemu and
      real TV CPU. SETTINGS-MODEL. Staging hash/size verified separately and
      removed, no helper remains. Live item/resource acquisition, module/skin/
      row widget/rendering/scrollbar/display/startup/visible integration stays
      open; this is not a complete UString or hardware rendering milestone.
- [x] Implement parent SettingItemsListBox::drawRow dispatcher:18441 shipped
      ARM/C cases and all prior groups pass, plus strict full host/Unix IPC,
      focused ASan/UBSan and fresh ARM/qemu units. SETTINGS-DRAW. Type
      writeback, all setter/image priorities and owned temporary string/vector
      order are explicit.

- [x] Implement base/manager single-row refresh and row setChecked:2880+24
      native/C cases, all prior groups, strict host/Unix IPC, focused ASan/UBSan
      and fresh ARM/qemu units pass. SVG-LIST-REFRESH. Uses total+8 capped by
      slots and wrapping upper bound; checkbox setter precedes gated cache.
      Row-view fixture out-of-bounds pointer arithmetic fixed. Actual parent
      virtual+0x60 is SettingItemsListBox::drawRow (now SETTINGS-DRAW).

- [x] Review ARM animation harness/recent list work:153 native comparisons and
      prior groups, full host/Unix IPC, focused ASan/UBSan and ARM tests pass.
      ARM-HARNESS-REVIEW. Fix owned strings/context/order/character bounds/signed
      count, wrong-page stub/bounds, unpublished restore validation/atomicity and
      text alignment. AGENTS records first-fault/GDB/ABI workflow. Historical
      SIGSEGV cause remains unproven; actual providers/rendering/startup and
      full replacement/native acceptance remain open.
- [x] Implement select animation via startSelectAnimation (disabled/missing-
      row/unchanged-rect gates, rect snapshot, ordered name search, first- match
      atof*1000.0f start with float-bit compare); earlier14 native/C comparisons
      plus full host/focused sanitizer/ARM and prior groups pass. SVG-LIST-ANIM.
      ARM-HARNESS-REVIEW supersedes string/order assumptions with 153 cases.

- [x] Implement row editing via setSlideValue (progress vs slider/text routing,
      two flag bytes, type word, bar selection, empty-format text, stored word);
      72 native/C comparisons plus full host/focused sanitizer/ARM and prior
      groups pass. SVG-LIST-SLIDE.

- [x] Implement the widget-level getters (numeric forward, selected-id gates/
      table walk); 21 native/C comparisons plus full host/focused sanitizer/ ARM
      and prior groups pass. SVG-LIST-WIDGETS.

- [x] Implement the first consumer module (ui.list v1 over the verified key/
      body/move/row/impl chain, list-state/list-key over socket/CLI, OAL1 reload
      handoff); host, focused sanitizer, ARM/qemu and actual Unix IPC pass.
      SVG-LIST-MODULE.

- [x] Implement the row-view getters and full list refresh (total/last/item
      bounds, virtual total, stored count, refresh order); 100+20 native/C
      comparisons plus full host/focused sanitizer/ARM and prior groups pass.
      SVG-LIST-UPDATE.

- [x] Implement the list scrollbar bodies (create forward, signed setpos gate,
      conditional background, bar draw); 294 native/C comparisons plus full
      host/focused sanitizer/ARM and prior groups pass. SVG-LIST-SCROLLBAR.

- [x] Implement the list update core (updateImpl start/clamp/redraw/writeback/
      empty plus rotateRows pair-swap/position exchange); 6952 native/C
      comparisons with exact call order/args/state plus full host/focused
      sanitizer/ARM and prior groups pass. SVG-LIST-IMPL.

- [x] Implement the SVG list scrollbar (gating, literal rect, show, position);
      4320 native/C comparisons plus full host/focused sanitizer/ARM and prior
      groups pass. SVG-LIST-SCROLL.

- [x] Implement the RowManager internals (field layout, focusItem body,
      updatePartial forward); 324 native/C comparisons plus full host/focused
      sanitizer/ARM and prior groups pass. SVG-LIST-ROWS.

- [x] Implement the eight primitive list move/selection/window bodies; 5040
      native/C comparisons with exact call order/args plus full host/focused
      sanitizer/ARM and prior groups pass. SVG-LIST-MOVE.

- [x] Implement the seven lower SVGListBoxBase key bodies; 26048 native/C
      comparisons with exact call order/args/handled truth plus full host/
      focused sanitizer/ARM and prior groups pass. SVG-LIST-BODIES.

- [x] Implement SVGListBoxBase key/message routers and typed row-value dispatch;
      999216 native key cases,49344 message joins and68640 row-value cases plus
      full host/focused sanitizer/ARM and prior groups pass. SVG-LIST-INPUT.

- [x] Implement SettingItemsListBox map/Right-Left/Digit callback bodies;
      65600 shipped map/matcher and138240 native body cases plus full host,
      focused sanitizer/ARM and prior groups pass. SETTINGS-LIST.

- [x] Implement navigateInList active-ID cursor and signed viewport algorithm
      via owned callbacks.101384 shipped ARM state/action matches, full host,
      focused sanitizer/ARM and prior groups pass. SETTINGS-NAVIGATION.

- [x] Implement concrete qualified SettingsDialog map and Up/Down, Select, Digit,
      Left/Right and Mute callback bodies.65600 shipped map/matcher and86400
      native body cases pass; full host/sanitizer/ARM/metadata and prior groups.
      SETTINGS-INPUT.

- [x] Implement six dispatched leaf bodies (Tab volume/digit/removal, qualified
      Settings menu/quit/guide).238860 shipped ARM comparisons include native
      signed division and field-write/end ordering; host/sanitizer/ARM and prior
      groups pass. TAB-LEAF.

- [x] Implement the concrete TabDialog fallback message map and pre-first key
      join.65600 shipped ARM map/matcher routes and16384 caller joins pass, with
      host/sanitizer/ARM and prior groups. TAB-DISPATCH.

- [x] Implement the outer TabDialog pre-handler body behind callbacks, including
      layout/circular/edge/back/close/repost logic.65664 valid-tree shipped ARM
      action/message/state/handled-truth matches, host/sanitizer/ARM tests pass.
      TAB-PREHANDLE. Missing-child safety is explicit.

- [x] Implement ordered Up/Down and Left/Right controller bodies behind backend
      callbacks;1008 shipped ARM action/state comparisons and host/sanitizer/ARM
      checks pass. TAB-EVENTS. This is not the outer pre-handler or a wired UI
      consumer; query/action order must remain dynamic across callbacks.
- [x] Read persisted hotel inputs from firmware-mapped HMCSettings via companion
      settings.hotel + hotel-state CLI/actual Unix IPC; combine ui.profile facts
      without changing entry/media/app state.345 shipped row-copy cases and
      DSO/SQLite/host/sanitizer/ARM checks pass. HOTEL-CONTEXT. No cache/current
      TV acceptance or complete persistence/default initialization is claimed.
- [x] Resolve show+50: content removes its own five full-screen component pointers,
      with TabDialog's separate header untouched by that function. Source-only
      CONTENT-HEADER-OWNERSHIP; no new renderer or visual parity gate is closed.
- [x] Add ui.navigation v2 independent retained setters, CLI/actual Unix IPC
      and OAN2 raw-state snapshots with OAN1 migration.1792 shipped setter
      cases and1792 DSO round-trips pass host/sanitizer/ARM-qemu. TAB-WRITEBACK.
      +18 isShowing/+8 show/+c hide correction is firmware-verified; per-event
      content/focus/modal writeback and the queue/UI consumer remain open.
- [x] Recover menu-capability byte9 ->connectivity flag and software profile
      paths/defaults. Bounded scalar decoder and reloadable ui.profile v1
      supply immutable copied facts;65281 shipped hashes/27 lookups and DSO
      lifecycle host/sanitizer/ARM-qemu pass. TAB-CONTEXT. No TV run or mutable
      hotel/media/app state, content permissions or UI consumer is claimed.
- [x] Resolve initial unavailable/missing-tab fallback and raw-ID/created-index
      selection; empty group has no focused tab but retains content0. Common
      helper and9216 shipped group-code qemu comparisons pass; TAB-SELECTION.
      This is initial construction, not later modal/key/state writeback.
- [x] Recover TabDialog availability and initial content-focus request;
      common/tab_policy matches actual shipped ARM predicate in884736 offline
      qemu comparisons; host/sanitizer/ARM tests pass. TAB-NAVIGATION.
      This does not close native context acquisition or content/key/UI acceptance.
- [x] Reloadable ui.navigation provider implements route plans, retained
      selection and a deferred route-ID queue. Current version isv2/OAN2,
      with the v1 prefix and OAN1 migration tested (TAB-WRITEBACK); the initial
      NAVIGATION-SERVICE milestone usedv1/OAN1. Host/sanitizer/ARM and actual
      Unix IPC tests pass. No drawing consumer or full native modal pipeline.
- [x] Recover full62 launcher operation results and28 unconditional ordered
      settings plans, including retained fields and queue-front order; common/
      launcher_route and firmware differential pass host/sanitizers/ARM-qemu.
      LAUNCHER-ROUTING. Conditional modal-result->navigation13 link established;
      this is not a UI consumer, live key recipe or full OperationFinder.
- [ ] Integrate launcher routing into the modular focus/modal/navigation pipeline,
      replacing the captured SCREEN_SETTINGS Select->full-list shortcut with
      actual state dispatch. Handle4b portal prework without excluded tracking,
      a6 runtime/tuner gates, provider actions, retained TabDialog selection and
      initialization before real-display acceptance. Native plans are not wire
      or reload encodings; queue insertion must not trigger reentrant creation.
      ui_navigation supplies the queued state/IPC provider; actual TabDialog
      initialization/selection writeback, drawing consumer and pressure behavior
      beyond the project16-request bound still require firmware/native acceptance.
      TAB-NAVIGATION supplies availability/focus predicates; TAB-CONTEXT supplies
      immutable ui.profile facts; HOTEL-CONTEXT supplies the persisted hotel-row
      path through settings.hotel. Complete native hotel defaults/setters/cache
      behavior and acquire real app/media/entry
      inputs and description presence (initial unavailable-tab
      fallback is now resolved by TAB-SELECTION), then handle
      content validity and password/network gates before consuming/drawing.
- [x] Review ff89e27..e7e7333: scanner mapped/complete/escaped key reads,
      nonzero-base snapshots and retained UString metadata fixed; full strict
      host/focused sanitizers/ARM-qemu pass. AGENT-WORK-REVIEW. No TV access.
      Lookup-key pointers may be stale; raw metadata is not current text.
- [x] Independently verify154 navigation case-local call lists and SubLauncher
      setFocus/isShowing virtual slots. This is offline firmware evidence, not
      resolved goto/delegate control flow, key translation or full-list entry.
- [ ] Resolve remaining key/profile/focus gates and live full-list entry before
      repeating no-effect1043 tests. LAUNCHER-ROUTING closes conditional result
      conversion/dispatch offline. Later matched full-list
      identity/current UString characters/draw state must explain apparent
      slider-label+30 without a fitted constant. Old scanner did not inventory
      TabDialog; absence cannot identify tab ownership or prove no full list.
- [x] Recover conditional TextCommand character dispatch: nonnull GPU getter
      selects OpenGL drawText; null selects CPU fallback. UI-GLYPH-ROUTING;
      saved context/vtables verify nonnull configuration. Opaque-parent probe
      passes host/sanitizers/TV but still differs (1318 core/1072 mismatches).
      Not a captured-frame trace, title-alpha proof or pixel parity.
- [x] Lower GPU glyph primitive: shipped GL_ALPHA masks, native quad/UV/shaders,
      blend and RGBA FBO composition. UI-FONT-GPU-2026-11-03; full strict host,
      focused sanitizers, ARM/qemu geometry and authorized native pbuffer
      synthetic/title corpus pass. This is not service integration or pixel parity.
- [ ] Resolve saved title pixel discrepancy through actual command routing,
      background/alpha and active rendering/presentation state. One-pass GPU
      matches CPU opaque core1971 versus vendor1446 (525 differences); recovered
      two-pass primitive gives1228/1150 differences. Shader varying change does
      not fix it. Conditional dispatch is now recovered; opaque-parent1318/1072
      comparison also fails. Establish actual parent paint/active FBO/presentation
      state; do not repeat bare shader/precision/opaque-base comparisons.
      Do not refit established origin23,5 or assume corrupt fonts.
      Add owned glyph commands/font lifetimes to the worker/broker/UI pipeline
      only after that contract is established; visible acceptance stays open.
- [x] Observe original title render graph with owner-opened panel: Surface bounds
      108,5,691,95, retained LineObj23,5 and current ink136..388/31..85; text/
      zero margins/unit glyph ratio verified by local
      snapshot verifier. UI-TITLE-LIVE-GRAPH-2026-11-03. No hardware mutation;
      non-atomic observations, not title placement implementation or acceptance.
- [x] Recover the assignment rule for retained title X23: negative signed global
      minimum-left bearing (b42468), live font18 e9 ->23. FONT-X-OFFSET corrects
      the erroneous zero-getter claim and applies viewport/origin policy in
      common/font_box. Full host/focused sanitizers/ARM-qemu/native CPU tests.
      Keep title default-off; full raster differs. Flag+1b1 is
      CharClipping, not bold. Native complete drawing/style parity stays open.
- [x] Integrate boxed text alignment/clipping into the optional title diagnostic:
      font18 height80/viewport90 yields local Y5, accounting for the historical
      vertical delta without moving the box. Full host/focused sanitizers/ARM-
      qemu/native memory-only corpus pass; zero-height title-font fallback tested.
      UI-TITLE-BOX-2026-11-03. Horizontal23px is subsequently closed by
      FONT-X-OFFSET; title stays default-off. Draw controls/ratios/presentation
      and independent visual acceptance remain open.
- [x] Review subsequent UI work through6822fce: scanner extended-field span
      overflow reproduced under ASan/fixed; lab presets now honor ui.edit=1.
      Exact/truncated scanner buffers, invalid/disabled presets, default-off
      title and partial-subscription cleanup pass full host/focused sanitizer/
      ARM-qemu checks. UI-WORK-REVIEW-2026-11-02. No TV operation in this review;
      native FA font/widget integration and settings interaction remain open;
      title origin/viewport helpers are subsequently recovered but raster parity is not.
- [x] Theme/index/layout files and 964 PNGs extracted; initial OAI1 conversion and
      ui_vendor rendering pipeline exist. Source and provenance: UI-ASSETS report.
- [x] BGRA capture bug fixed; selected portal-frame comparison ~1.62/255 recorded.
      This is one frame, not full screen/navigation or startup parity.
- [x] Follow-up format audit: tvshot uses explicit BGRA/RGBA/RGB565 raw decoding
      with offline known-colour PNG tests; legacy bootsplash source now writes
      BGRA, checked in memory under strict C flags. Stale report claims corrected.
      No TV capture, deployment or legacy/recovery binary rebuild occurred.
- [x] String inventory extracted (56,715 lines); it is not an aligned localized table.
- [ ] Integrate original localization selection/fallback/shaping with live
      widgets. UI-ROW-LABELS already decodes the stored41-locale/2435-record
      dictionary, including Dutch slot10/dut; do not repeat that decoding or
      infer runtime selection policy from the flattened string inventory.
- [x] Project-local portal rail/card navigation in mod_ui_vendor accepts profile
      RC Up/Down/Left/Right/Select/Exit IDs and NEC OK0x8a in injected events.
      Current reload snapshot is5 bytes (band, rail, card, screen, grid selection),
      with legacy4-byte read compatibility;3-byte snapshots reject. Host, one
      ASan/UBSan run and one ARM/qemu run pass. No ttyS2, fbdev, CEC or amp open.
      The live UART code byte is still unconfirmed. UI-PARITY.
- [x] Portal hit testing uses the paint rectangles. Select changes the local
      screen/emits an action; it does not launch a real application or tune a
      source. Menu/Exit return to the portal, and Aux emits source with the
      index read from settings m_current_source. Hit still resolves tiles hidden
      by the open screen. Proven by tests/ui_nav.c on host make check, one
      ASan/UBSan run and one ARM/qemu run. The test database is not the live
      settings store. UI-PARITY.
- [x] Source-selection screen (DialogSourceSelection.svg 185): host test and a
      real fb1 run under the owner's permission while HDMI1 stayed on the video
      plane. Panel/bar geometry and the vendor ink (white rows, #7E000A focused
      row) match the swap-corrected p0/06 capture (RMSE 0.28%, AE 0.0073%);
      fb1 backed up/restored and staging removed. The p0/06 capture is R/B
      swapped; the screen uses the resource palette #BE1E2D/#FE9419. Localised
      source names and exact focused font metrics remain open.
- [x] Captured picture-settings (Beeld) state: `ui_vendor` SCREEN_SETTINGS
      paints the three R/B-corrected p0/03 bands (rail, top tiles, category row
      and the "Beeld" caption) over (13,14,14); `ui.screen=settings` and Select
      on rail 4 enter it, Exit/Menu return. Static state, explicitly not the
      interactive settings screen. tests/ui_settings.c passes; a real fb1 run
      is pixel-identical to the corrected p0/03 (0/2,073,600 differing pixels);
      fb1 backed up/restored, staging removed. Evidence:
      artifacts/analysis/settings-2026-11-01/.
- [x] Captured full picture-settings panel (shot-settings.png, vendor
      SettingsDialog picture tab): ui_vendor SCREEN_SETTINGS_LIST paints the
      corrected 780x1080 panel; Select on the quick tiles or
      ui.screen=settings_list enters, Exit returns to the tiles, Exit again to
      the portal. Static panel state, explicitly not live rows.
      tests/ui_settings_panel.c covers the flow; a real fb1 run is
      pixel-identical to the corrected capture (0/2,073,600 pixels). Evidence:
      artifacts/analysis/settings-panel-2026-11-01/.
- [x] Captured-panel row-selection prototype: Up/Down follows the186+70*N
      row positions, skips the capture's disabled row6 and clamps. With live
      text it paints750x66 focus backgrounds; UI-DISPLAY-REVIEW separately
      restores sliders/chevrons. The old183px fitted overlay and its bitwise
      reconstructed-reference result are historical, not current vendor parity.
      tests/ui_settings_rows.c and ui_settings_text.c cover this local flow.

- [x] Fix missing focused sliders and choice chevrons in the captured-panel/
      live-text preview. Firmware-derived track/PNG composition, shared shadow
      digit/knob values; seven real-TV GPU states captured, independent stock
      Contrast/Modus widget crops within2/255 and both buffers identical. Full
      display restore and staging cleanup verified. UI-DISPLAY-REVIEW-2026-11-04.
      Reconstruction only: native widgets/providers/focused-font parity remain open.
- [ ] Exact focused-row/panel evidence and the interactive remainder: value
      changes, Beeld/Geluid tab switching, scrolling/overflow, localization,
      the title/tab/value labels and focused-label pixels. Firmware decode so
      far (analysis/ui-text-setup-2026-11-01): SettingItemsListBox assigns
      state-indexed list/row resources exactly (FA=479 etc.); row ctor fixes
      widget slots and per-state FA/UFA/FI/UFI IDs (label 488-491, caption
      492-495, button 496-499, selbox 504-507); renderItems gates widgets by
      row type, translates resource rects by row position, and falls back to
      alexa resources + setFont 26/30 when the row marker is absent;
      renderForeground gives font=+0x21c from getFont(height,+0x1b4 lines,
      +0x1c4 type), line height +0x1c0, alignment +0x220/+0x22c with
      mirror/RTL remap. Slider rows are type 9 (slidebar + value textbox);
      PictureSettingsDialog builds ids 1-4 as type 9 (CONTRAST/BRIGHTNESS/…),
      5/6/9/0xd/0xe/0xf/0 as type-2 selection rows and 7/8/10/0x120/0x6d as
      type-4 button rows; Left/Right dispatch (0xe0ad0d, decompiled): ids 0-5
      call updateViaPictureElementSetting, which OPENS the per-setting
      sub-dialog (showPictureElementSettingDialog) — no in-place adjust on
      the main list; other ids remap vendor keys 0x2c8/0x2c9→0x20c/0x20d and
      forward to base list navigation. Select opens sub-dialogs, tabs switch
      on Left/Right
      by replacing the content dialog (tab 0 Beeld = PictureSettingsDialog,
      1 Geluid = SoundSettingsDialog, 2 Netwerk, 3 Installation, 4 System;
      showSubMenu 0xd6f141). The five tab labels are button texts from
      localized keys (table in .bss, not statically resolvable). TextBox
      branches are asm-verified: +0x1b4 <0 draws nothing, ==0 single-line
      full-height, >0 divided multi-line; +0x220 alignment is a bitmask
      (0x92/0x248/0x124); the +0x1b4 writer is SVGTextBox::create's 4th arg
      (rows pass literal 1, so row text is single-line). Value digits come
      from setSlideValue formatting into the +0x1ca8 value textbox; type-2
      values via the selection box. Row rect literals are resolved
      (text1/labelLong/button/image1/image2/selectionbox + alexa presence
      test); each widget rect = resource rect + row offset. Single-line text
      height scale is bottom'=top+trunc(float(surfaceH)/float(maskH)*float(rectH))
      (UI-TEXT-GEOMETRY-2026-11-02 corrects the earlier width/rounding claim).
      Single-line textPos goes to+0x1ec and foreground consumes it; +0x254
      holds addition. +0x1c4's writer is the font-marker scan (asm-verified
      10c62ac-10c63c6: first-present marker wins in type order — fontRegular→0,
      fontBold→1, fontLight→2, fontItalic→3, fontBoldItalic→4,
      fontLightItalic→5, else 0; sl holds the fontBold result after the textPos
      rescale consumes it). SVGListBoxRow::renderBackground's nine widget-name
      lookups are asm-verified (glyph/mRowL/gRowL/mRowR/gRowR/mRowM/gRowM/mRowM/
      mRowM; 5/10 are a swapped mirror pair like 17/22); the first name feeds
      the row's own rect lookup (miss destroys row children) and the g/m row
      names feed the two mask passes and the horizontal scale transform.
      The FA animation callback chain is closed (vtable slot +100/index 25 =
      repositionSelectionIndicator, which calls list renderBackground; the
      driving animation is the selectionIndicator-named one, interpolation is
      truncating, gated on animation-enabled with unchanged-rect skip).
      What list renderBackground draws from the moving rect is decoded
      (0x10bd305): it draws the moving
      +0x26c rect list-relative via SVGSurface::drawRectangle with the
      selectionIndicator record fill (record cached at +0x27c) — a
      firmware-filled rectangle, distinct from the row slice stretch. Its 22
      widget-name lookups resolve to m/g top/row/bottom edge sets plus rowPos,
      scrollPos and selectionIndicator; it also holds a list-level first-present
      font-marker mode selector at +0x244 (new slices gRowMRed, mRowMGreen,
      title_gRowL/R). A direct-offset scan found no SVGListBoxBase reader of
      +0x244/+0x27c (only ctor zeroing + renderBackground writes; other hits
      are different classes/stack slots). This is a negative result for
      immediate-offset accesses only — computed-offset/aggregate reads are not
      excluded, so do not invent consumers from it. Per-widget rect-to-draw consumption is closed for textbox
      widgets: renderItems rect → parent-origin offset in create → +0x284 →
      drawTextBox; +0x294 feeds animation decode; rect updates resize the
      surface via the setWindowRect override. The selection box follows the
      identical dual-copy convention (+0x368/+0x378, refreshed per draw),
      and the button too (+0x148/+0x158); the slidebar and image work off
      the base +0x28 rect only (verified, no copies). FA/UFA textPos geometry
      is now firmware-sourced (analysis/ui-row-geometry-2026-11-02): FA bar
      479 is a solid-#FE9419 3-slice (flat-bar reconstruction proven
      equivalent), label/caption/button/selbox FA/UFA pairs differ per-widget
      (label 42/40 px, caption x-shift, button/selbox 68/66 px, fontBold only
      in FA). Live focused text still needs the runtime composition wired to
      a live row instance.
- [x] Lab slider value editing (NOT parity): Left/Right on settings-list rows
      1..4 adjusts a per-process shadow value (seeded from the static digits),
      re-renders the digit blob and repaints, gated on ui.edit=1 (default 0).
      Ranges 0..100 are the remaining invented part (step 1 now matches the
      firmware ctor default; adjust math and %d formatting verified); the
      row→slidebar range path is fully mapped (item +0x20/+0x24/+0x28 through
      row +0x2df0/+0x2df4), but live per-setting bounds need provider queries
      (native ownership, out of scope for this lab path). UI-DISPLAY-REVIEW
      now moves the reconstructed knob with the same shadow value as the digits;
      type-2 rows/Select sub-dialogs untouched, no settings-DB writes.
      Firmware (0xe0ad0d/0xe0aca1, decoded after this shipped) shows the
      vendor OPENS a per-setting sub-dialog on Left/Right instead of adjusting
      in place — the lab path is a functional stand-in, not the vendor flow.
      ui.edit_init="r=v" presets values for captures. Host test drives real
      keys (95→100→clamp 0, untouched row keeps value), asserts the focused
      row is a solid #FE9419 fill and that the baked settings_focus_N asset is
      NOT blitted when live text is on (it bakes grey widget shapes + captured
      glyphs, which owner observation saw as artifacts and doubled digits).
      fb1, fixed build: edit_init 1=49 gives 283 ink px spanning x660..703
      (two glyphs), 1=7 gives 121 px spanning x660..679 (one glyph) at the same
      origin, proving the visible digits are the live blob.
      CORRECTION: the first capture (33c719a1) showed the overlay's captured
      glyphs because text was painted under it; that evidence is withdrawn.
- [x] Decode the row mask→surface mapping before changing bar geometry.
      CLOSED: horizontal composition verified; row resources verified
      (list 475-478, rows 479/481/480/482); renderer = BinSVGRenderer,
      vtable+0xc = renderGroup (clears per call, first match wins);
      transformRect translates y only (height preserved); row height 70
      verified three ways (raw rowPos in all four records, unscaled
      record-field computation, height-preserving transform). Implemented
      (20,186+70i,750,66) + unpainted strip; fb1-verified (e3fd5209...).
      Evidence: artifacts/analysis/ui-row-rect-2026-11-02/.
      Evidence: artifacts/analysis/settings-edit-2026-11-01/. Full make check
      261 ok.
      UI-MASK-LAYOUT
      now recovers mask positioning and row arithmetic; runtime field
      population and full native integration remain open. Slice/text-origin
      helpers and FA resource geometry were subsequently recovered; the
      fixed-panel slider/chevron preview is now UI-DISPLAY-REVIEW. General
      widget rendering must still use actual instance/style/ratio state.
      Do not guess FA/FI slider/button graphics.
- [x] Recover the single-line textPos draw source, EASIRect axes and manual
      height rescale, with bounded common/text_layout.c. Strict host, sanitizer,
      full suite and49,728 ARM VFP comparisons on qemu/TV pass. This is a lower
      geometry helper, not complete widget layout or replacement of fitted UI
      origins. See UI-TEXT-GEOMETRY-2026-11-02; integration remains open above.
- [x] Recover MaskPositioningTransform numeric anchors/relative translation
      and SVGListBoxBase vertical/horizontal/mirrored row arithmetic. Bounded
      common/text_layout.c helpers and contract cases pass strict host/full
      suite/sanitizer/ARM-qemu/TV. UI-MASK-LAYOUT-2026-11-02 records field offsets
      and limits; actual widget-state population and full UI integration remain
      open, and fitted settings-label positions have not changed.
- [x] Recover raw slice→list metrics and painted rowPos→cached insets/row size,
      including visible-count-before-paint ordering and missing-resource state.
      UI-LIST-METRICS-2026-11-02; strict full host/sanitizer/ARM-qemu/native TV
      arithmetic tests pass. Raw475/477 slices explain step70/top16/bottom10.
      Resource parsing/provider helpers now exist; native runtime field population,
      window allocation and text-origin integration remain open.
- [x] Recover double-ratio horizontal stretch and compose actual477 slices
      through raw measurement, middle destination, transformed rowPos and row
      placement.49,538 independent ARM VFP cases pass qemu/TV; strict full host
      and focused sanitizers pass. UI-SLICE-STRETCH-2026-11-02. New row creation
      adds parent client origin; concrete margin bindings and actual widget
      inputs/text baseline/runtime theme/UI integration remain open.
- [x] Confirm zero client margins for concrete list/setting-list/row/setting-row/
      textbox vtables; recover vertical line positioning and separate ratio-floor
      reference conversion. common/text_origin.c contract tests pass strict full
      host/sanitizer/ARM-qemu/native TV. UI-TEXT-ORIGIN-2026-11-02. Horizontal
      direction/truncation/alignment, actual widget styles/ratios/coordinate
      composition and runtime theme/UI integration remain open.
- [x] Add plain EF41 aligned line layout/drawing: physical-width corrections,
      start/end/center, symmetric center trimming and end reverse paint order.
      UI-HORIZONTAL-ORIGIN-2026-11-02; strict full host/sanitizer/ARM-qemu/native
      TV offscreen synthetic+30-line font7/16 corpus pass. End reads final right
      bearing, not advance. Word justification4, shaping/direction/markup, full
      truncation/scaling, actual widget/clip/vertical-origin integration remain
      open; current UI fitted origins are not replaced by this milestone.
- [x] Add bounded runtime BinSVG record view/iteration and raw named rectangle
      lookup, including first duplicate/first group and descendants. All702
      records/11888 elements/6733 rectangles/39 animations pass strict host,
      focused sanitizers/ARM-qemu/native TV; full host suite passes. Actual477
      resource slices compose through existing geometry helpers for an explicit
      test window. UI-BINSVG-VIEW-2026-11-02. Indexed loading/ownership and the reloadable provider are now implemented
      (UI-THEME-PROVIDER); native widget/text integration remains open.
- [x] Load indexed BinSVG into an owned immutable snapshot and expose reloadable
      ui.theme v1, with explicit paths, first-CRC/index lookup and held borrowed
      views. UI-THEME-PROVIDER-2026-11-02: strict full host/focused sanitizers/
      ARM-qemu/native TV real-DSO lifecycle and702-record corpus pass. Input
      edits, invalid later records, holds/collisions, standby, deferred activation
      and failed replacement/recovery covered. Actual UI consumers/resource
      selection/widget/text geometry/rendering remain open; captured panels and
      fitted origins unchanged. Host binary rollback remains a separate gap.
- [x] First ui_vendor theme consumer: actual DialogLauncher marker lookup with
      first numeric mode priority, service hold/removal/rebind and copied active
      mode. UI-THEME-SKIN-2026-11-02: full strict host/focused sanitizers/ARM-qemu/
      native TV policy tests and live resource60/type4 pass. Recorder/injected
      callbacks establish policy/lifetime only. Existing ui.launch_anim remains
      default0/lab-only; actual settings geometry/styles and visible parity open.
- [x] Recover ordered rowItems mask/container semantics and compose named child
      windows from actual460. UI-ROW-WINDOWS-2026-11-02: first mask raw/pre-mask
      identity/later cached transform, duplicate/nested/offset/error tests pass
      strict full host/focused sanitizers/ARM-qemu/native TV. Explicit parent
      749x70 is a conditional input; actual row/textbox/font/clip/UI integration
      remains open. Animation-group seek/depth handling explicitly unsupported.
- [x] Compose fresh normal single-line textbox surfaces and alignment/font
      markers from shipped styles: full-height horizontal slices, textPos
      height correction, actual460->489/488 and20 live startup metrics select
      fonts7/20 for an explicit parent. UI-TEXTBOX-COMPOSE-2026-11-02; strict
      full host/focused sanitizers/ARM-qemu/native TV CPU tests pass. Actual
      parent/glyph-origin/ratio/clip, overrides/retained state, pre-mask second
      pass and visible FA/UFA integration remain open; fitted UI unchanged.
- [x] Recover widget->HAL alignment bridge and join vertical placement to plain
      EF41 boxed rendering with local surface clipping. UI-TEXT-VIEWPORT-2026-11-02;
      strict full host/focused sanitizers/ARM-qemu/native TV exact9-alignment and
      54-line actual resource/font corpus pass. Unit-ratio/one-line scope only;
      actual parent/text/resource/ratio state, skin integration and visible
      acceptance remain open. Conditional X31 does not prove fitted49: no guessed
      padding from zero dialog margins/list ctor insets. Correction: font offset
      was misclassified as zero; FONT-X-OFFSET implements its per-font value.
- [x] Bounded read-only UI object-candidate scanner exists with chunk
      lookahead, map/executable checks, signed fields and current bounds fixes.
      ui_object_scan tests pass. UI-LIVE-OBJECTS' six uncreated candidates
      were an earlier sample; UI-TITLE-LIVE-GRAPH later observed a created
      title/render graph. Neither a failed search nor a non-atomic snapshot
      proves absence, current text ownership or complete native UI state.

- [ ] Repair/test historical tools/wspoll.c before reuse: 127-frame read overruns
      ext[4], short/header-coalesced reads and uninitialized/stale state need
      handling. UI-LIVE-OBJECTS records using host WebSocket API instead; old
      native helper was not executed. Keep state observation separate from
      key-dispatch and visible-screen proof.
- [x] Trace ordinary settings label content into the textbox and resolve original
      MODE localization: UI-ROW-LABELS-2026-11-02. Key constructor/drawRow add no
      prefix; hash-checked offline profile traversal validates2435x41 strings,
      including exact dut Modus with no whitespace/markup. This closes the stored
      label-padding hypothesis only; no production localization implementation.
- [x] Explain normal MODE label X31 versus fitted49: font7 global minimum-left
      is-18 and the vendor reserves its negative. FONT-X-OFFSET uses this metric,
      not a fitted18px adjustment. Stored Dutch MODE has no hidden prefix.
- [ ] Resolve indented labels and integrate actual instance/style/ratio state
      before replacing all UI fits. Independent visible FA/UFA acceptance,
      original runtime text, full localization/shaping remain open.
- [x] Decode the shipped EF41 bitmap-font records and packed UTF8 lookup:
      bounded borrowed-data reader, signed glyph metrics, raw grayscale
      coverage, blank wrapped extents and all38 theme records validated on
      host/sanitizers/ARM-qemu/TV. See UI-FONTS-2026-11-01. This closes data
      decoding only; per-widget font type/size, text raster integration,
      alignment, substitution and exact focused text remain open.
- [x] Recover startup font ID/type registration and generic font selection:
      UI-FONT-SELECTION-2026-11-01;20 pack-derived heights, stable typed floor,
      signed height/line-count division and global-smallest fallback tested on
      host/sanitizers/ARM-qemu/TV. Per-widget SVG text type/rectangle/line count, baseline/alignment,
      substitution and rendering integration remain open.
- [x] Render a single selected EF41 glyph into an offscreen RGBA canvas:
      signed bearings, clip intersection, firmware coverage/alpha arithmetic,
      safe bounds and blank wrapped extents. All52022 shipped glyphs plus
      synthetic alpha/bounds tests pass host/sanitizer/ARM-qemu/TV; see
      UI-GLYPH-DRAW-2026-11-01. SVG marker names/priority recovered; focused
      labels change type/geometry as well as color. String layout, actual row
      setup/baseline, font-data provider and UI-engine text integration remain
      open. Recoloring captured labels is not focused-text parity.
- [x] Bounded font pack provider and one-line LTR renderer are wired to the
     17-item live settings-label/value preview. This audit reran the local
      font7/16 corpus and current ui_settings_text tests. Fitted origins,
      captured tabs/panel and default-off title remain explicit prototype inputs.
      UI-FONT-LIVE's pixel metrics describe its dated capture only.
      UI-DISPLAY-REVIEW now draws focused sliders/chevrons separately; ui.text
      requires exported slider/selection assets and refuses an incomplete bundle.
      Native focused type/size, runtime settings/localization and full parity stay open.

- [ ] Complete the remaining vendor screens and labels. The injected portal
      above is not all-screen parity or a confirmed live remote capture. The
      source-selection panel (DialogSourceSelection.svg 185), the captured
      Beeld state and the captured full picture panel are implemented; none is
      the full interactive screen.
- [x] ui_engine owns display/alpha discovery, lazy OAI1 assets, receipts,
      readiness and rebinding; ui_vendor holds the engine and owns its skin
      callbacks/screens/OA_SVC_PORTAL. Current service is ui.engine v2 with
      owner-safe register_skin_dispatch; the unsafe v1 registration entry
      returns ENOTSUP. UI-ENGINE-REVIEW supersedes the initial v1 milestone.
      Fresh host lifetime/fallback tests pass; full vendor content is separate.

- [x] Skin seam: ui.engine now owns the key topic and the ui.fps repaint timer,
      posts skin enter/paint/key back to the skin's owner token, and selects the
      active skin by `ui.skin`. The vendor module is renamed ui_vendor; a
      ui_omarchy skeleton draws its own shell and registers with the same
      engine. tests/ui_skin.c switches vendor -> omarchy -> vendor by unload/
      load with no rebuild; make check 233 ok. The vendor skin remains the
      fallback.
- [x] Control-plane startup/runtime skin selection with vendor fallback for
      missing/start/enter/first-frame failures; drain busy failed candidates
      before fallback, including NULL-returning failed init. Host249/focused
      sanitizer/native contracts, Unix socket and actual GPU switching/fallback/
      restoration pass. SKIN-SELECTION. No process-crash recovery.
- [x] Harden ui_vendor against service-removal subscription failure: m_start now
      fails (like ui_omarchy) instead of warning and continuing, so a later
      forced ui_theme unload cannot leave a stale c->theme that dialog_fs_type
      would call after dlclose. The host calls stop() after a failed start,
      which releases all holds. Regression: tests/ui_vendor_subs.c exhausts the
      subscription table, asserts start fails with no leaked holds/subs, then
      retry starts. Fixed 2026-11-02; full make check 261 ok.
      (Found by independent adversarial review 2026-11-02; narrow path:
      exhausted subscription table + forced unload + ui.launch_anim Select.)
- [ ] Add an optional embedded Lua skin/scripting module after shared UI primitives
      stabilize and before implementing the full OmarchyTV skin. Expose a small,
      versioned API for layouts, navigation, themes, key bindings and animation
      configuration; keep rendering execution, blocking I/O and hardware ownership
      in C services. Support script reload through existing IPC/socket/CLI with
      candidate validation, explicit state migration, owned timer/subscription/
      service-hold cleanup and stale-callback rejection. Bound script memory and
      execution, report errors and recover to the vendor skin; test failed loads,
      callback errors, reload/suspend/unload and target runtime compatibility.
      Instruction hooks do not bound blocking C calls. Scripts must not bypass
      device ownership or reintroduce excluded telemetry/advertising functionality.
- [ ] Real OmarchyTV screens replacing the skeleton after vendor parity. Keep
      OA_SVC_PORTAL on vendor and preserve engine draw/asset/present contracts.
- [ ] Split module/test compile+link rules into .o + link so ccache can cache
      them (most are one-step -shared invocations, which ccache cannot cache).
      Preserve -Werror/-pedantic and the current link flags; measure a clean
      cold/warm build and report the hit rate.
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
- [x] Bounded ui.anim prototype exists: enablement defaults off, four slots,
      integer durations>=1, same-id replacement, cancel/finalise and recovered
      list/dialog arithmetic. Host tests pass. Initial scheduling uses an owned
      0ms kick and10ms timer; accepted starts while that timer runs retain its
      phase, unlike the vendor's restart-on-every-start behavior. Dynamic
      float-duration/controller and exact start/timer parity remain open below.

- [x] Text-slide math from AnimationHelper instructions: `oa_anim_slide_duration` (1600 +
      trunc(distance/50)×1000) and `oa_anim_slide_margin` (pause 800 ms, clamp above only; forward ends
      at distance, reverse at zero). Published as `ui.anim` v2 and asserted with exact vectors for
      distances 0/49/50/100/400 and elapsed 0/800/801/4800/9600/90000 plus reverse and short-slide
      cases. UI-ANIMATIONS.
- [x] Text-slide caller wiring in mod_ui_vendor: distance from isTextSlidingNeeded @0x10c76b0
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
      (recording display, CLOCK_MONOTONIC ui.clock) and the real ui_anim/ui_vendor measured a 10.08 ms
      mean sampler cadence, first nonzero margin at +822 ms, 163 px at +3808 ms, and the repeat
      restart at +6557 ms (= 4554 + 2000, the 0.99 arm), clean exit. No fb1, vendor module, key,
      settings or network access; staging removed and vendor PIDs unchanged. This is not visual
      scanout or pixel parity evidence. UI-ANIMATIONS.
- [x] Rail-highlight prototype:11 exported vendor frames, one-shot playhead
      and final-frame hold in the160x160 window, reset on focus change.
      The lower decoder's non-rearm behavior is recovered; complete higher
      widget/image lifetime semantics remain unverified. Current code uses a
      10ms poll, a30ms target interval and async completion backpressure.
      The dated29.98ms recorder measurement is not physical scanout cadence.
      Frame order/cadence/vendor appearance acceptance remains open.

- [x] Full-screen transform consumer: display v2 publishes `set_scene_transform`; all draws render
      to an offscreen RGBA scene and present() maps it through the recovered four-vertex clip-space
      quad (x-slide, y-slide, scale, type-1 keystone). mod_ui_vendor pushes `ui.fs_type` with the eased
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
      adds the coalescing canvas and async receipts. UI-ASYNC wires ui_vendor receipts,
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
      is in SCENE-HELPER; the provider/receipts/UI layers are subsequently
      implemented in GRAPHICS-MODULE, SCENE-DISPLAY and UI-ASYNC. Full native
      ownership/visual acceptance remains open.
      SCENE-BROKER-2026-11-01.
- [x] Separate graphics bootstrap/renderer: close inherited descriptors before
      Mali loads; keep renderer loaded until child exit. Two process groups pass
      strict host/sanitizers/TV. Actual native helper completes410 GPU frames
      (identity plus eight effects), child exits/reaps, full16588800-byte backing,
      region/MAU/pan restored and independently hashed. Full host209 checks pass.
      This milestone used a blocking diagnostic client; subsequent provider/
      readiness layers are implemented below. SCENE-HELPER-2026-11-01.
- [x] Lower nonblocking graphics parent: one pending/result slot, generated
      instance tickets, revision validation, MSG_DONTWAIT I/O, copied completion,
      EOF shutdown and WNOHANG reap/destroy gate. Five strict host/sanitizer/TV
      groups pass, including blocked child work/cleanup and target exec failure.
      Maintained client also completes410 actual GPU frames through the full
      snapshot diagnostic, with child exit0/restoration/independent hash match.
      Full host214 checks pass at that milestone. GRAPHICS-MODULE subsequently
      adds the provider/watch/timer; SCENE-DISPLAY and UI-ASYNC now supply the
      coalescing/readiness/UI layers. SCENE-CLIENT-2026-11-01.
- [x] Reloadable graphics provider: display.gpu v1 with owned completion watch,
      reap timer, retained failed-start cleanup and nonblocking quiescence.
      Public tickets use host module ids across reload. Four host/sanitizer/TV
      lifecycle groups pass (holds, suspend/resume, failures, blocked close and
      abnormal-peer fault latch). Actual service completes410 GPU frames; host
      reactor drains/unloads before full snapshot restoration/hash match.
      At that milestone, host218/source17 modules/preserved production16 passed.
      Coalescing/readiness and UI module-cycle coverage now exist below;
      actual TV standby and independent ownership remain open.
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
      checks passed at that milestone (source18/preserved production16).
      Subsequent UI-ASYNC supplies ui_vendor integration; full visual/cold acceptance
      remains separate.
      SCENE-DISPLAY-2026-11-01.
- [x] Async ui_vendor presentation/readiness: owned receipt sampler, both service
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
      The lower decoder is one-shot and the current prototype already holds
      its last frame. Higher-level image/focus lifetime remains unverified;
      do not reapply an endless-loop fix or equate prototype timing with parity.
      Vendor start also resets its immediate timer on every accepted start,
      unlike our retained phase; dynamic list/float duration versus four-slot/
      uint32 controller remains another gap. HANDOFF indexes raw ARM evidence.
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
- [x] Required firmware/toolchains/Ghidra/research inputs were migrated to
      artifacts/workbench and maintained build/extractor inputs use durable
      paths. Historical migration/empty-tmp checks are in its manifest.
      This does not mean every old helper is tmp-free: tvshot/tvsim defaults
      and rebuild_payload scratch output still use /tmp/opencode. New durable
      evidence must use artifacts/analysis; payload maintenance stays deferred.

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
