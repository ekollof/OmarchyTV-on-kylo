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

- [x] Implement base/manager single-row refresh and row setChecked:2880+24
      native/C cases, all prior groups, strict host/Unix IPC, focused ASan/UBSan
      and fresh ARM/qemu units pass. SVG-LIST-REFRESH. Uses total+8 capped by
      slots and wrapping upper bound; checkbox setter precedes gated cache.
      Row-view fixture out-of-bounds pointer arithmetic fixed. Actual parent
      virtual+0x60 is SettingItemsListBox::drawRow (source saved), not yet
      implemented; scrollbar construction/rendering, owned backend/display/
      startup/independent visible acceptance remain open.
- [x] Review ARM animation harness/recent list work:153 native comparisons and
      prior groups, full host/Unix IPC, focused ASan/UBSan and ARM tests pass.
      ARM-HARNESS-REVIEW. Fix owned strings/context/order/character bounds/signed
      count, wrong-page stub/bounds, unpublished restore validation/atomicity and
      text alignment. AGENTS records first-fault/GDB/ABI workflow. Historical
      SIGSEGV cause remains unproven; actual providers/rendering/startup and
      full replacement/native acceptance remain open.
- [x] Implement select animation via startSelectAnimation (disabled/missing-
      row/unchanged-rect gates, rect snapshot, ordered name search, first-
      match atof*1000.0f start with float-bit compare); earlier14 native/C
      comparisons plus full host/focused sanitizer/ARM and prior groups pass.
      SVG-LIST-ANIM. ARM-HARNESS-REVIEW supersedes string/order assumptions with
      153 cases. Parent refresh, scrollbar construction/rendering
      internals, owned backend/context beyond words/counters, display/scanout/
      startup and independent native visible acceptance remain open.
- [x] Implement row editing via setSlideValue (progress vs slider/text routing,
      two flag bytes, type word, bar selection, empty-format text, stored
      word); 72 native/C comparisons plus full host/focused sanitizer/ARM and
      prior groups pass. SVG-LIST-SLIDE. Parent refresh, scrollbar
      construction/rendering internals, startSelectAnimation, owned backend/
      context beyond words/counters, display/scanout/startup and independent
      native visible acceptance remain open.
- [x] Implement the widget-level getters (numeric forward, selected-id gates/
      table walk); 21 native/C comparisons plus full host/focused sanitizer/
      ARM and prior groups pass. SVG-LIST-WIDGETS. Row editing, parent
      refresh, scrollbar construction/rendering internals, startSelectAnimation,
      owned backend/context beyond words/counters, display/scanout/startup and
      independent native visible acceptance remain open.
- [x] Implement the first consumer module (ui.list v1 over the verified key/
      body/move/row/impl chain, list-state/list-key over socket/CLI, OAL1
      reload handoff); host, focused sanitizer, ARM/qemu and actual Unix IPC
      pass. SVG-LIST-MODULE. Widget-level row getters/editing, parent refresh,
      scrollbar construction/rendering internals, startSelectAnimation, owned
      backend/context beyond words/counters, display/scanout/startup and
      independent native visible acceptance remain open.
- [x] Implement the row-view getters and full list refresh (total/last/item
      bounds, virtual total, stored count, refresh order); 100+20 native/C
      comparisons plus full host/focused sanitizer/ARM and prior groups pass.
      SVG-LIST-UPDATE. Widget-level row getters/editing, parent refresh,
      scrollbar construction/rendering internals, startSelectAnimation, owned
      backend/context and independent native visible acceptance remain open.
- [x] Implement the list scrollbar bodies (create forward, signed setpos gate,
      conditional background, bar draw); 294 native/C comparisons plus full
      host/focused sanitizer/ARM and prior groups pass. SVG-LIST-SCROLLBAR.
      Row getters/editing, parent refresh, scrollbar construction/rendering
      internals, startSelectAnimation, owned backend/context and independent
      native visible acceptance remain open.
- [x] Implement the list update core (updateImpl start/clamp/redraw/writeback/
      empty plus rotateRows pair-swap/position exchange); 6952 native/C
      comparisons with exact call order/args/state plus full host/focused
      sanitizer/ARM and prior groups pass. SVG-LIST-IMPL. Row getters/editing,
      parent refresh, scrollbar construction/position bodies,
      startSelectAnimation, owned backend/context and independent native
      visible acceptance remain open.
- [x] Implement the SVG list scrollbar (gating, literal rect, show, position);
      4320 native/C comparisons plus full host/focused sanitizer/ARM and prior
      groups pass. SVG-LIST-SCROLL. SVG-LIST-IMPL implements updateImpl and
      rotateRows next; row getters/editing, parent refresh, scrollbar
      construction/position bodies, startSelectAnimation, owned backend/context
      and independent native visible acceptance remain open.
- [x] Implement the RowManager internals (field layout, focusItem body,
      updatePartial forward); 324 native/C comparisons plus full host/focused
      sanitizer/ARM and prior groups pass. SVG-LIST-ROWS. updateImpl, row
      widget internals, row getters/editing, parent refresh, scrollbar/
      animation bodies, owned backend/context and independent native visible
      acceptance remain open.
- [x] Implement the eight primitive list move/selection/window bodies; 5040
      native/C comparisons with exact call order/args plus full host/focused
      sanitizer/ARM and prior groups pass. SVG-LIST-MOVE. Primitive move
      targets, RowManager internals, row getters/editing, parent refresh,
      owned backend/context and independent native visible acceptance remain open.
- [x] Implement the seven lower SVGListBoxBase key bodies; 26048 native/C
      comparisons with exact call order/args/handled truth plus full host/
      focused sanitizer/ARM and prior groups pass. SVG-LIST-BODIES. Primitive
      SVG-LIST-MOVE implements the eight primitive move/selection/window bodies.
      Move targets, RowManager internals, row getters/editing implementations,
      parent refresh, owned backend/context/consumer and independent native
      visible acceptance remain open.
- [x] Implement SVGListBoxBase key/message routers and typed row-value dispatch;
      999216 native key cases,49344 message joins and68640 row-value cases plus
      full host/focused sanitizer/ARM and prior groups pass. SVG-LIST-INPUT.
      SVG-LIST-BODIES implements the seven lower bodies; primitive move/
      getter/editing implementations, parent refresh, owned backend/context/
      consumer and independent native visible acceptance remain open.
- [x] Implement SettingItemsListBox map/Right-Left/Digit callback bodies;
      65600 shipped map/matcher and138240 native body cases plus full host,
      focused sanitizer/ARM and prior groups pass. SETTINGS-LIST. SVG-LIST-INPUT
      later adds lower routing/value dispatch; actual lower actions/getters/
      editing/parent refresh, owned backend/context/consumer and independent
      native visible acceptance remain open.
- [x] Implement navigateInList active-ID cursor and signed viewport algorithm
      via owned callbacks.101384 shipped ARM state/action matches, full host,
      focused sanitizer/ARM and prior groups pass. SETTINGS-NAVIGATION.
      SETTINGS-LIST later adds list map/two bodies. Lower SVG/row, parent
      refresh, owned backend/context/consumer
      and independent native visible acceptance remain open.
- [x] Implement concrete qualified SettingsDialog map and Up/Down, Select, Digit,
      Left/Right and Mute callback bodies.65600 shipped map/matcher and86400
      native body cases pass; full host/sanitizer/ARM/metadata and prior groups.
      SETTINGS-INPUT. SETTINGS-NAVIGATION later adds navigateInList. List widget,
      parent refresh, owned
      context/backend/consumer and native visible acceptance remain open.
- [x] Implement six dispatched leaf bodies (Tab volume/digit/removal, qualified
      Settings menu/quit/guide).238860 shipped ARM comparisons include native
      signed division and field-write/end ordering; host/sanitizer/ARM and prior
      groups pass. TAB-LEAF. SETTINGS-INPUT subsequently adds the base map and
      five input bodies; real list/backend/context/consumer and visible
      acceptance remain open.
- [x] Implement the concrete TabDialog fallback message map and pre-first key
      join.65600 shipped ARM map/matcher routes and16384 caller joins pass,
      with host/sanitizer/ARM and prior groups. TAB-DISPATCH. Remaining base/leaf
      methods, owned renderer/content/context, queue consumer and native visible
      acceptance remain open; no module consumer was silently attached.
- [x] Implement the outer TabDialog pre-handler body behind callbacks, including
      layout/circular/edge/back/close/repost logic.65664 valid-tree shipped ARM
      action/message/state/handled-truth matches, host/sanitizer/ARM tests pass.
      TAB-PREHANDLE. Missing-child safety is explicit; TAB-DISPATCH later adds
      the fallback map. Actual adapter/queue consumer and native visible
      acceptance remain open.
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
- [x] Reloadable ui.navigation v1 uses recovered route plans, retained selection
      and deferred route-ID queue; nav-* CLI/actual Unix IPC, OAN1 pending-state
      reload, lifecycle/failure/standby tests pass host/sanitizers/ARM-qemu.
      NAVIGATION-SERVICE. No consumer drawing, modal handling or native TV run.
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
      actual FA font/widgets, title layout and settings interaction remain open.
- [x] Theme/index/layout files and 964 PNGs extracted; initial OAI1 conversion and
      ui_vendor rendering pipeline exist. Source and provenance: UI-ASSETS report.
- [x] BGRA capture bug fixed; selected portal-frame comparison ~1.62/255 recorded.
      This is one frame, not full screen/navigation or startup parity.
- [x] Follow-up format audit: tvshot uses explicit BGRA/RGBA/RGB565 raw decoding
      with offline known-colour PNG tests; legacy bootsplash source now writes
      BGRA, checked in memory under strict C flags. Stale report claims corrected.
      No TV capture, deployment or legacy/recovery binary rebuild occurred.
- [x] String inventory extracted (56,715 lines); it is not an aligned localized table.
- [ ] Decode layout property IDs/records and original language IDs/grouping, including
      Dutch selection; do not guess slot numbers from the flattened string inventory.
- [x] Portal rail/card navigation on mod_ui_vendor from mod_ir frames. Profile RC
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
- [x] Picture-settings row selection: SCREEN_SETTINGS_LIST tracks a row and
      Up/Down moves the FA overlay (750x66 at 20,183+70*N) over the captured
      panel, skipping the disabled row 6 and clamping at both ends. FA
      resources 479 (stretched #FE9419 row), 488/496/504 (#7E000A textPos)
      back the reconstruction; sliders stay as captured. Host test
      tests/ui_settings_rows.c passes and an fb1 run matches the reconstructed
      reference exactly (0/2,073,600, sha256 86b66c00...). Evidence:
      artifacts/analysis/settings-focus-2026-11-01/.
      Historical183px fit superseded by186+70*N in subsequent implementation;
      UI-WORK-REVIEW records live row geometry and remaining FA parity limits.
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
      (native ownership, out of scope), slider bars stay static,
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
      population, slice composition, text origin and FA animation geometry are still
      undecoded, so title/tab/slider-graphics/focused text must not
      be built from the capture fits. Do not guess FA/FI slider/button
      graphics.
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
      Runtime resource parsing/transforms/window allocation and text-origin
      integration remain open; these helpers do not replace fitted UI positions.
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
      test window. UI-BINSVG-VIEW-2026-11-02. Indexed resource loading/ownership,
      reloadable immutable theme provider and actual widget/text integration
      remain open; current fitted UI positions/captured panels are unchanged.
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
- [x] Add bounded read-only UI vptr-candidate inventory with chunk lookahead,
      executable/map checks and signed field decoding. UI-LIVE-OBJECTS-2026-11-02:
      strict full host/focused sanitizers/ARM-qemu tests and native heap scan pass.
      Only6 uncreated textbox candidates in selected heaps; actual settings
      instance/window/text/ratio evidence is still pending, not closed by absence.
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
- [x] Font-data provider and one-line renderer: bounded indexed pack reader
      (font7 and font16 records from the live pack), packed-UTF8 LTR advances,
      spacing, clipping and coverage compositing, whole-line validation;
      ui_vendor ui.text=1 draws 17 settings text items: row labels font7 at
      (49|79,184+70*row), text values font7 (centered near x=577), slider
      digits font16 at (659,250+70*(row-1)), and the disabled grey
      "Beeld zoomen"/"Auto". Erases only the captured text strips and falls
      back to the capture on any font failure. Host tests incl. fallback pass;
      real fb1 vs the corrected panel: 1.184% any-channel, 22 px >8 (max 11),
      RMSE0.106%, rows0-4/6 no >8 diff. Title/tabs/slider graphics and focus
      overlays stay captured. Evidence:
      artifacts/analysis/ui-font-live-2026-11-01/.
- [ ] Complete the remaining vendor screens and labels. The injected portal
      above is not all-screen parity or a confirmed live remote capture. The
      source-selection panel (DialogSourceSelection.svg 185), the captured
      Beeld state and the captured full picture panel are implemented; none is
      the full interactive screen.
- [x] UI engine first slice: mod_ui_engine owns display/alpha discovery and
      rebinding, lazy OAI1 assets, draw/present/async receipts and the readiness
      receipt; it publishes ui.engine v1 and the ui/ready event. mod_ui_vendor (the
      vendor skin) holds only the engine, keeps the portal/source screens and
      OA_SVC_PORTAL, and repaints on ui/ready in its own owner context. Every UI
      test and tests/recovery.py load engine+vendor; make check 232 ok. The
      vendor skin is the working fallback.
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
- [x] Rail focus highlight: placement recovered (DialogMainMenu.svg button windows (6,y,160,160);
      capture-derived offsets); implemented in mod_ui_vendor as 11 vendor frames at 30 ms played once per
      focus (the last frame is held; decodeNextFrame does not rearm) in the 160x160 window, playhead reset on focus change. tests/ui_rail_hl asserts order/wrap/timer/window;
      host 198 ok, ASan 198 ok; live probe 166 blits at 29.98 ms mean, x=6 y=461. Visible-panel
      appearance still unverified. UI-ANIMATIONS.
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
      Fresh decoder audit contradicts our inferred endless rail loop; finish
      higher-level image/focus lifetime verification before changing playback.
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
