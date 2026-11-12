# Open questions and unverified contracts

This is a navigation aid, not a replacement for [TODO](../../TODO.md) and the
correction map in [FINDINGS](../../FINDINGS.md). A checkmark only closes the
stated, evidenced milestone; an unverified helper or a dated observation does
not close it.

## Cold boot, scanout and persistence

- Stage 0 reached a **full-screen vendor portal** on a real cold boot (2026-11-09);
  the OSD destination fix is proven by the log (`was dst 1920x1080 → 3840x2160`).
  Still unverified: the settle repaint for late icons/fonts and the 60 s `eth0`
  watcher (both committed in `bff29c5`, not yet boot-tested), the physical
  panel/scanout confirmation, the UI stages 1–5 and the no-UI `hw0..hw4`
  hardware ladder. The hw ladder and ready receipt are host-tested only.
- The vendor cold-start HAL order is now instruction-exact; the device-gap
  disposition is in [COLD-START-SEQUENCE](../../artifacts/reports/COLD-START-SEQUENCE-2026-11-10.md).
  Remaining real device effects: `dev_src_init` hotplug, `DEV_AMP_ID`, the GPIO
  earphone handler and non-layout-14 panel/TCON branches. PCB layout, DEV
  GPIO/IIC description state, NIM factories, SPI semaphore and MTD are not
  required for the HAL-only path.
- A **true power-on/persistent boot hook** remains a separate owner gate.
  The installed hook is disabled (`oa_safe` present, `oa_boot` absent); the
  last armed attempt required the USB `/conf` recovery. Do not re-arm without
  a guaranteed out-of-band recovery.
- The forced DRM/OSD reinit is isolated at stage 2 because it produced the
  top-left quadrant on a cold boot. The `plat` software/audio prelude,
  VPLTIO/V4L2/sharemem, GPIO/IIC and video-device calls still run in every
  stage; they are the next candidates to gate if a stage fails.

Reports: [STAGED-COLD-BOOT-DESIGN](../../artifacts/reports/STAGED-COLD-BOOT-DESIGN-2026-11-09.md),
[BOOT-HOOK-COLD-ATTEMPT](../../artifacts/reports/BOOT-HOOK-COLD-ATTEMPT-2026-11-09.md),
[COLD-OSD-PANEL-GATE](../../artifacts/reports/COLD-OSD-PANEL-GATE-2026-11-09.md),
[COLD-START-STAGE-AUDIT](../../artifacts/reports/COLD-START-STAGE-AUDIT-2026-11-06.md).

## Audio

- The HDMI external route is implemented and mixer-verified (`main out 2:HDMI ES`,
  mute 0, codec PCM) but **owner-audible confirmation is still pending**.
  `source.enable` and `audio.source_enable` remain 0.
- The DSP command surface (AIN, VAUD, AQ, AQuality, MuxInput, speaker release,
  corrected SwitchHP, mix gain) is covered behind the gate. The EQ/AVL
  post-param derivation and the ICE AudioIF/decoder transport are not fully
  ported; foreground/scart routing and speaker main gain remain.

Reports: [AUDIO-SOURCE-SERVICE](../../artifacts/reports/AUDIO-SOURCE-SERVICE-2026-11-09.md),
[AUDIO-ROUTING](../../artifacts/reports/AUDIO-ROUTING-2026-11-01.md),
[AUDIO-VOLUME](../../artifacts/reports/AUDIO-VOLUME-2026-11-01.md),
[AUDIO-AMP-NATIVE](../../artifacts/reports/AUDIO-AMP-NATIVE-2026-11-04.md).

## CEC

- The raw/VPL transport, authority mirror, owner worker, reply queue, policy
  and module-level fake are implemented/tested. Raw first-opener TX is now
  hardware-ACKed (LA4 and broadcast), but the authority write path, RX replies
  and stock integration are not; hardware NACK/error behavior is only partly
  mapped. `cec.enable=0`.
- ARC/System Audio and the CEC 1.3a/1.4 clauses still need a live acceptance
  run. The 2026-11-01 cache “LA8” was an array slot, not a wire LA; on
  2026-11-12 LA8 NACKed while LA4 and broadcast ACKed, but LA4 answered no
  83/46/8f query. A raw receive consumes messages and is not a view-only query.
- Routing/power status: 0x82 Active Source is implemented behind
  `cec.active_source` by PA top nibble (HDMI1..4; `cec_active.cjs`); the vendor
  path `selectSource` uses logical addresses. The
  `NovoCECController::handleCECMessage` 0x9D path returns unless the TV is in
  the standby transition, but the separate `SvgAppCECEventsListener` handles
  0x9D by posting `UIKeyPressMessage(597)`; 0x36 is implemented behind
  `cec.standby` and accepted only from the tracked active source
  (`cec_standby.cjs`). Identity queries are behind `cec.identity`
  (0x83 -> 0x84, 0x85 -> 0x82 while tuner-owned; `cec_identity.cjs`).
  Power-status queries are behind `cec.power_status` (0x8F -> 0x90 "on",
  0x90 logged; `cec_power_status.cjs`). 0x80 Routing Change follows only a
  matching old active PA and selects the new PA's HDMI input behind
  `cec.active_source` (`cec_routing.cjs`). OSD names are behind
  `cec.identity` (0x46 -> 0x47 configured name, 0x47 logged;
  `cec_osd_name.cjs`). One-touch 0x04/0x0D selects the 0x84 device PA's HDMI
  input behind `cec.active_source` (`cec_image_view.cjs`). Active-source 0x44
  volume/mute is behind `cec.rcp` through `OA_SVC_AUDIO`; navigation
  0x00..0x04/0x0D maps to RC ids 53/20/19/21/22/10, and 0x45 releases are
  logged. Other key forwarding is open (`cec_rcp.cjs`, `cec_rcp_keys.cjs`). Stock observation
  2026-11-12: Deck
  sleep left the TV on at a tuner input; this is CEC `0x9D Inactive Source`:
  `SvgAppCECEventsListener` posts `UIKeyPressMessage(597)`, and the UI selects
  DTV/tuner (`ICE_VideoSelectSource` source 8 →
  `TunerManager::selectServiceForDisplay(1)`). The VIU lost-signal lines follow
  the source change. The external-interface disconnect policy (saved tuner
  service or previous external source) is the cable-unplug fallback. Resume
  switched to HDMI1. A pure policy helper is tested but not wired; the stock
  provider entry (`AitHub` → `NEBULA_Handle...` → navigator;
  `ndp_SRC_ConnectInfo`/`NDP_ExtSrc_Get/SetConnectInfo`) is traced.
  The V4L2 event `0xc000001` on `/dev/video21` is a scaler update, not source
  loss; its decoder/adapter and the HPD plug/unplug model are tested. HPD stayed
  high through Deck suspend. The HDMI RX status getter on `video21` captured
  `w0=1` active and `w0=0/5` during suspend; the full DTV/tuner select/mute/
  decoder/UI body remains open. The source service
  now exposes the read-only `video_status()` v2 behind `source.vin_status`; it
  is host-tested but not wired to a tuner and is a no-signal detector, not the
  sleep trigger. `common/tuner_view.c` + `check-tuner-view` cover the recovered
  `TunerManager` view position→handle mapping. The source service's gated
  `inactive_source(position)` v3 resolves `OA_SVC_CHANNELS` with the stored
  `OA_SVC_SERVICE_LIST` fallback, connects DTV and calls
  `OA_SVC_TUNER.select(handle)`; `source_path` fake-service tests cover the
  runtime/stored order/gates. `mod_cec` routes broadcast 0x9D behind
  `cec.inactive_fallback`; `cec_inactive.cjs` covers both paths and gates.
  The source service now emits `source/state` after a switch; the vendor UI
  updates the Bronlijst row, `m_current_source` and picture cards, and both
  UIs repaint. The full stock CEC UI transition (dialog close/DTV screen)
  remains open.

Reports: [CEC-TRANSPORT-CONTRACT](../../artifacts/reports/CEC-TRANSPORT-CONTRACT-2026-10-31.md),
[CEC-OWNERSHIP](../../artifacts/reports/CEC-OWNERSHIP-2026-11-01.md),
[CEC-LIVE](../../artifacts/reports/CEC-LIVE-2026-11-01.md),
[CEC-MODULE-TRANSPORT](../../artifacts/reports/CEC-MODULE-TRANSPORT-2026-11-09.md),
[SOURCE-LOSS-FALLBACK](../../artifacts/reports/SOURCE-LOSS-FALLBACK-2026-11-12.md).

## Tuner, source and broadcast

- `tuner`/NIM-DTV, ATV tables and the SPI stage are recovered but the hardware
  bring-up and channel tuning are not TV-accepted.
- The source connect/disconnect path is implemented behind `source.enable=0`
  with a HAL seam and probe guard; full switching and its audio companion on
  the TV remain open. `source.enable=0`.
- Broadcast/EPG/PVR: persistence, now/next, conflict state and vendor fields
  are partly implemented/tested; recording/playback and full broadcast UI
  remain.

Reports: [TUNER-BRINGUP](../../artifacts/reports/TUNER-BRINGUP-2026-11-06.md),
[SOURCE-PATH](../../artifacts/reports/SOURCE-PATH-2026-11-09.md),
[EPG-PERSISTENCE](../../artifacts/reports/EPG-PERSISTENCE-2026-11-05.md),
[PVR-CONFLICT-STATE](../../artifacts/reports/PVR-CONFLICT-STATE-2026-11-05.md),
[VENDOR-BROADCAST-ABI](../../artifacts/reports/VENDOR-BROADCAST-ABI-2026-11-05.md).

## Stock-UI parity and native shell

- The vendor portal geometry, palette, rail/cards, SVG pack (702 resources) and
  39 animation descriptors are recovered, but timing/easing, interruption
  behavior, localized string records and some theme geometry remain partly
  undecoded. A single-frame similarity is not animation parity.
- The native Omarchy shell (`ui_omarchy`) paints screens and handles logical
  actions; actual process launch, hardware input and physical scanout are not
  accepted. Do not use the native skin as evidence of platform/stock-UI parity.

Reports: [UI-PARITY](../../artifacts/reports/UI-PARITY-2026-10-31.md),
[UI-ANIMATIONS](../../artifacts/reports/UI-ANIMATIONS-2026-10-31.md),
[STOCK-APPS-GRID-PARITY](../../artifacts/reports/STOCK-APPS-GRID-PARITY-2026-11-05.md),
[NATIVE-OMARCHY-UI](../../artifacts/reports/NATIVE-OMARCHY-UI-2026-11-08.md).

## Power, standby and PQ/network

- Physical standby/wake, MCU watchdog authority and suspend/resume ownership
  are not accepted; the sleep-cycle/power service have host tests only.
- The ~15 load average is a D-state `msleep` artifact, not CPU load. Do not use
  the load average as a health metric. The cold-boot `eth0` watcher is committed
  but not yet verified on a boot.

Reports: [POWER-STANDBY-CONTRACT](../../artifacts/reports/POWER-STANDBY-CONTRACT-2026-10-31.md),
[PQ-MPFW-TASK](../../artifacts/reports/PQ-MPFW-TASK-2026-11-09.md).

## Explicitly excluded

Telemetry, advertising, ACR, analytics, profiling, advertising IDs, sponsored
placements and ad delivery are out of scope, including as optional modules.
Local diagnostics and ordinary TV/network functions remain in scope.
