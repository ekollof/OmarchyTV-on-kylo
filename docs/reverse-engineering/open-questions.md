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
  panel/scanout confirmation, and stages 1–5 of the ladder.
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
  and module-level fake are implemented/tested, but **hardware/bus/TX
  acceptance** and the authority write path are not. `cec.enable=0`.
- ARC/System Audio and the CEC 1.3a/1.4 clauses still need a live acceptance
  run; the earlier Steam Deck was LA8/PA1000. A raw receive consumes messages
  and is not a view-only query.

Reports: [CEC-TRANSPORT-CONTRACT](../../artifacts/reports/CEC-TRANSPORT-CONTRACT-2026-10-31.md),
[CEC-OWNERSHIP](../../artifacts/reports/CEC-OWNERSHIP-2026-11-01.md),
[CEC-LIVE](../../artifacts/reports/CEC-LIVE-2026-11-01.md),
[CEC-MODULE-TRANSPORT](../../artifacts/reports/CEC-MODULE-TRANSPORT-2026-11-09.md).

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
