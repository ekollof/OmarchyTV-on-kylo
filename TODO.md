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

## Platform + stock-UI parity (current priority, 2026-11-08)

Platform ownership and the stock vendor UI are the gate; the native Omarchy
shell is deferred. [PLATFORM-PARITY-GAP-2026-11-08](artifacts/reports/PLATFORM-PARITY-GAP-2026-11-08.md)
maps the cold-start stages and the current stock-UI coverage.

- [ ] Cold-start `DEV_INI_Init` stage ownership: decide/implement the device
      layer for GPIO/IIC/src/panel/NIM/amp/SPI/NIM-DTV. `plat` currently calls
      the HAL `Hal_Gpio_Init`/`Hal_Iic_Init`, which are not `DEV_GPIO_Init`/
      `DEV_IIC_Init`; do not call vendor `DEV_*` code without their device-layer
      state. Grok's device-stage report is merged: panel copies GPIO110/111,
      timing block, PWM108 + curve 40/60/80/100 (init does not light the panel);
      NIM creates ATV/DTV factories; amp sends 0x3a/0x100 + DEV_AMP_ID task;
      SPI opens `/dev/ntkspi` when `/conf/spi.bin` is absent (stock holds fd27);
      NIM-DTV models0..3/default take demod HAL, model4 skips; MTD returns0.
      Next: implement the one-owner-per-stage order, GPIO/IIC first. The panel
      power sequence is already in `mod_panel` (`panel.standby` for suspend/
      resume; new `panel.power_on_start` for cold boot, default0), with the
      stock values 30/580/220/30 ms, GPIO110 on=1, GPIO111 on=0, PWM108
      channel0 and TCON gate0; `check-panel-curve`/`check-panel-quiesce` pass.
      Remaining: visible panel-on/off and a cold-boot acceptance.
      See `artifacts/analysis/panel-power-2026-11-08.md`.
- [x] HAL-only device-layer decision + cold-start profile candidate:
      no ARM module has an undefined vendor `DEV_*` symbol; the new
      `openaurora-cold.conf` sets real DRM init, no snapshot restore,
      panel power-on, I2S MCLK and SPI before tuner, and uses `ui_vendor`.
      `make cold-replacement-bundle` (33 modules, manifest verified) and
      `make check-cold-config` pass. Not a TV cold-boot acceptance; the
      TV-side `--check`/boot gate is still open.
- [ ] Real display/OSD cold enable: the opt-in `display.osd_reinit` path forces
      the kernel vendor `_ker_osd_fb_SetPar` via FBIOPUT_VSCREENINFO
      (BGRA/transp alpha/32-byte stride), checks the format, writes region and
      pans; resume reopens/repeats. A cold-profile gate on 2026-11-09 ran with
      `plat.drm_init=1`, `display.osd_reinit=1` and `panel.power_on_start=1`
      after stopping stock: DRM 13/fb 98/crtc 26, panel 100%, fb1
      1920x2160/pan 0,1080, `ui_vendor` painted the portal and the fb1 frame
      was captured non-blank. Stock was restored/rebooted. Still open: a true
      power-on boot without stock, boot-hook persistence, owner visual
      confirmation and full plane/stock-UI parity. COLD-OSD-PANEL-GATE.
- [ ] Source/VIN full path: the connect half is implemented behind
      `source.enable=0` with an injectable HAL seam (SOURCE-PATH-2026-11-09):
      type gates, per-type Open(0/1/3+2/4), VPLT `0x4c4`, V4L2Info/FdCommon,
      `plat_drm_get/connect_dpsource`, `NDAL_VideoExt_OpenDevice` and the
      `Hal_Audio_EasySelect` companion. `check-source-path` and
      `check-source-fake-ipc` pass; ARM/cold bundle builds. Remaining: the
      vendor disconnect body, `NdVideoSetting`/VinParam/UpdateTDLayer, HDMI
      PHY and the high-level `AudioIF` transport. A 2026-11-09 controlled
      takeover run selected HDMI1 and TV live (`ok`, dual/single VIN fds,
      V4L2Info/FdCommon words, OSD apps grid captured) and showed the switch
      needs `plat.drm_init=1` (takeover leaves the DRM device id -1). The HDMI
      picture, audio and cold boot remain unconfirmed. The module fails fast
      before any VIN/VPLT call when the DRM id is -1; keep `source.enable=0`.
- [x] Audio source service behind a gate: `mod_audio_source` implements the
      recovered `ICE_AudioStart` external branch with the 56-entry source
      table, `Hal_Audio_Ctrl(0x2a)`, the five `plat_adec_connect(0,2)`
      EasySelect calls, `plat_audio_connect`, speaker mutes and output mute.
      `check-audio-source` asserts the exact 13-call HDMI sequence, ATV,
      source-10 skip, cache and gates; ARM links the five HAL symbols.
      `audio.source_enable=0` is enforced in all profiles. The
      ExternalAVSourceManager table (13×0x38) and HP policy are recovered:
      video types 15..22 map to their audio devices, and stock sound output1 /
      EP-off selects SwitchHP mode3 (5x16-byte table, 18 Ctrl 0x2c/0x2d +
      2 Ctrl 0x2b). Not TV-accepted; the mixer state is verified (main out
      select/connect 2:HDMI ES after `audio-source-video 17`). A known-playing
      HDMI1 PCM run kept `hdmi in codec info=PCM`, main out HDMI ES and mute 0
      for 90 s, then stock was restored and rebooted. The recovered MM-path
      `SetSoftMute(0,1)` release is opt-in (`audio.source.unmute=1`); the host
      config cap was raised 48→128 because it silently dropped that key.
      Owner-audible confirmation and foreground/scart/main-gain remain. The
      missing AIN init (`Ctrl 0x11/0x40/0x1e`, handle 0x12345678) and
      `MuxInput {source,port}` path config (0x801→0, 0x301→1, 0x101→8) are now
      implemented behind the gate; the final run reached the same mixer state
      as stock (HDMI ES, mute 0, speaker mute 0, codec PCM) and held 90 s.
      VAUD init (`Ctrl 0x37/0x1e`, `DualCmd 0x4a`) is also implemented, as is
      the 101-entry AQ volume table (`Ctrl 0x2e/0x3e` via
      `oa_audio_curve_from_aq`). The AQuality post-params
      (`InitPPParams` -> `Hal_Audio_Send("AQuality",{block,0x18a0,...})`) are
      supported via opt-in `audio.source.pp_blob` using a stock-captured block;
      the full derivation remains unported. The HP switch branch was corrected
      (stock map 0!=1: Ctrl 0x2b{1} -> 0x15{0} -> SpeakerConnect -> gains, no
      trailing 0x2b) and the ICE_AudioInit speaker mix gain (Ctrl 0x3c/0x3d,
      0x8000) is included.
- [ ] Audio amp/route: native AD82587 backend, reset/mute/standby, speaker
      gain/SPDIF and audible acceptance.
- [ ] Tuner/NIM-DTV, CEC ownership/TX, panel/power/watchdog, IR repeat.
      CEC now has a module-level injectable transport and a host fake TX/RX
      test (`check-cec-module-fake`: auto-open, RX `50 7a a8`, TX `04 04`,
      close); the real ARM module and cold bundle verify. Hardware/bus
      acceptance, the authority write path and enabling `cec.enable` remain.
- [ ] Stock vendor-UI screens: live launcher DB/icons, settings tabs, channel/
      EPG/PVR, app launch profile. `ui_omarchy` is not stock-UI parity.

## Scheduled takeover test queue (single session)

One owner-authorized session on 192.168.178.87. Nothing here is accepted until
observed on the TV and stock operation is restored and verified. Offline tests
and prior dated captures do not close these items. No persistent install,
boot-hook or firmware changes; the watchdog must never be left disabled.

### Preflight and rollback

- [x] Capture stock identity: uptime, appmgr/Aurora PIDs, `/proc/<pid>/fd`
      targets (`ntgpio`, `ntkiic0`, `ntkdvif`, `frontend0`, display), display
      state and `/conf/mb230_{hw,sw}profile.bin` MD5s. 2026-11-06: aurora 1774,
      appmgr 1574; hwprofile `1b063b2c...`, swprofile `a1d9e5e8...`;
      TUNER-TAKEOVER.
- [x] Stage `build-tv-v3` `openaurora` and the required modules under `/tmp`;
      verify size/SHA256 before execution; no writes to `/vendor`, `/conf` or
      boot hooks. Used `/buffer/oa-tuner-20261106` (rw, 191 MB free); tar
      SHA256 `785652c7...`, manifest verified, `--check` passed.
- [ ] Confirm the USB recovery media/script and `/conf/coredumpnoreset`; agree
      the recovery/reboot path with the owner before starting. `/conf/
      coredumpnoreset` is present; the USB media was not re-verified. Recovery
      used `wdtctl set 10` and returned stock cleanly.
- [x] Use the verified `wdtctl disable` / `wdtctl set 10` procedure only for
      the test window; record every transition and restore stock feeding.
      2026-11-06: disable sent, stock SIGTERM'd, `set 10` rebooted; no disabled
      watchdog left.
- [x] After each group: stop openaurora/modules, remove staging, restart stock
      Aurora/appmgr (or reboot), verify PIDs/fds/display/profile and confirm
      no helper or disabled-watchdog state is left behind. Tuner group restored
      by `wdtctl set 10` on 2026-11-06 (appmgr 1573, aurora 1779) and again on
      2026-11-07 (appmgr 1570, aurora 1770); staging/socket absent, profile
      unchanged.

### Tuner / NIM / LNB (main goal)

- [x] Claim `/dev/ntkiic0`, `/dev/ntgpio`, `/dev/ntkdvif` and
      `/dev/dvb/adapter0/frontend0`; verify stock no longer holds them.
      2026-11-06: Aurora 1774 stopped; appmgr kept fb1/ntkgpu only.
- [x] `tuner.hw_init=1`, `tuner.backend=hal`, `/conf/mb230_hwprofile.bin`,
      `tuner.pcb_layout=14`; verify SAL type 0xd (MxL661). 2026-11-06: the
      sequence reached SAL 0xd and `SetTunerType` returned PASS; module start
      was still rejected later at NIM IR code 21.
- [x] Observe the MxL661 init: reset, defaults table, `0x31` RMW, xtal,
      IF-out, AGC and power-up; check the 41-write/5-read order and errors.
      2026-11-06: module init returned success (`tuner=1`, `seq_rc=0`,
      `hal=1`, `lnb=1`); exact write-count instrumentation was not added.
- [ ] DVB-T tune on a known free-to-air multiplex (no CI+ card needed): case 9
      IF, case 0xd start tune and case 0xb lock; verify PLL/spur writes, lock,
      strength, quality and frequency.
- [ ] DVB-C tail if a cable signal is available; otherwise record "not tested"
      and do not mark it accepted.
- [ ] Analog ATV only if an analog source exists: verify `tuner.atv_tables`
      loads the on-TV aurora.elf CRC and run a PAL/NTSC tune; otherwise record
      "not tested". The analog tuning backend exists (`tuner.analog=1`,
      `tuner.analog_standard=0..5`); verify the tune, the recovered ATV lock
      bit and `tuner.analog_scan` stepping, and record the strength/quality
      limits.
- [x] NIM `InitNimFunc`/`Connect2HAL`: 0x29-word state clear, table install,
      IIC codes 0x15/0x11/0x40, checked/ignored returns and the no-IIC-after-
      failed-MxL661-init guard. 2026-11-06: state/table/guard ran; the
      NIM-DTV IIC code-21 read NACKed (`failed_step=5`). 2026-11-07: recovered
      that the vendor tolerates every IIC failure and still signals
      init-complete (`0x00860252`), removed the stricter start gate, and
      re-ran on hardware. Service started; `ir_code=21`, `ir_write=0`,
      `errno=1`, kernel `ki2c: [i2c-0] Dev NACK`. The NACK itself remains a
      hardware fact (no populated/responding device at slave `0x80`), not a
      replacement failure. RF acceptance is separate.
- [x] RT5047 LNB_EN descriptor + `Hal_Gpio_Init/Write`; if no LNB hardware or
      signal is available, verify the descriptor/no-op path and record the
      limit. 2026-11-06: LNB init returned success (`lnb=1`); no LNB signal or
      independent voltage check.
- [ ] Typed/raw 0x5c channel and the verified 0x180 status fields; record the
      unimplemented per-case maps as still open. 2026-11-07: implemented the
      recovered 0x5c DVB-T record builder/parser and the linkable body of the
      Aurora-only `HAL_Demod_User_SetChannel` (MxL661 program + `HAL_Demod_Set`)
      behind `tuner.vendor_channel=1`; host tests and a no-RF TV scan pass.
      Remaining: FTA RF tune/lock and the 0x180 lock/strength/quality
      acceptance; the 6/7 MHz and platform IF-mode-2 variants are not
      hardware-validated. The Aurora-only symbol itself is not executed.

2026-11-06 tuner subgroup: staging/preflight/claim and the MxL661 init/sequence
passed; the NIM task saw a NIM-DTV IIC code-21 NACK, so no tune/lock (also no
RF source). Stock was restored by watchdog reboot. TUNER-TAKEOVER.
Display/audio/CEC/input/PVR groups were not started in this attempt.

Follow-up correction: the helpers at `0x0085fd04`/`0x0085fc80` are
`_dev_nim_dtv_IIC_Read/Write` (their debug strings name them), not
`DEV_UART_IR`. They call `DEV_IIC` on i2c-0 with slave `0x80`, clock index 0
(79 kHz) and the code as subaddress. Every NIM-DTV IIC failure branch jumps to
the same finish path at `0x00860252`, which sets active-module `0xff`, sets
`DAT_01936fb0=1` and signals; `DEV_NIM_DTV_WaitInitComplete` only waits for
that signal. The module's `failed_step >= 0` start gate was stricter than the
vendor and has been removed; `ir_code`/`ir_write`/`errno` are logged for
diagnosis. `+0x48` is `DEV_NIM_DTV_PowerControl2` -> `PowerControl` -> HAL
table entry `0xf` (`HAL_Demod_User_PowerControl`) -> `SAL_DEMOD_TUN_PowerDown`,
a power-down path, not a power-up precondition.

2026-11-07 second run: staged `cf361243...` and verified the manifest/preflight;
stock 1779 stopped, watchdog disabled. The service started (`status started`,
`services tuner:v1`) with `failed_step=5` log-only, and the kernel confirmed
`ki2c: [i2c-0] Dev NACK` for slave `0x80`. `tuner-scan 1` (DVB-T) and
`tuner-scan 4` (analog) returned ok; `tuner-state` advanced 25/50/75/100 and
0/50/50/100/100. No RF source: the progress/services are synthetic and
`lock 1` is not independent acceptance. Restored by `wdtctl set 10` (appmgr
1570, aurora 1770, stage/socket absent, profile unchanged). An FTA RF source
is still required for tune/lock/scan acceptance.

2026-11-07 vendor-channel run: added the recovered 0x5c DVB-T record and the
linkable `HAL_Demod_User_SetChannel` body (MxL661 program + `HAL_Demod_Set`)
behind `tuner.vendor_channel=1` (host tests pass; ARM module `7828b42f...`).
Staged `/buffer/oa-tuner-20261107b` (tar `b9b9dc30...`, manifest/preflight
verified); service started, `tuner-scan 1` returned ok and advanced
25/50/75. A first `tuner-scan 4` correctly returned `-EBUSY` while the DVB-T
scan was SCANNING; the retry after state DONE returned ok and advanced
0/50/50. No RF, so lock is not acceptance. Restored by `wdtctl set 10`
(appmgr 1562, aurora 1765, stage/socket absent, profile/fb unchanged). Next:
FTA RF compare of `vendor_channel=0/1`; 6/7 MHz and IF-mode-2 variants remain
unvalidated.

### Display / panel

- [x] `display.verify_scanout` and full-screen bars; first frame on
      vendor-initialized fb1; restore stock display. CONTROLLED-SCANOUT
      2026-11-06: stock stopped, magenta band held byte-identical, driver
      yoffset/region/MAU verified; second run's eight full-screen bars were
      owner-confirmed on the panel; stop restored fb1 and watchdog reboot
      returned stock. This is not cold startup or MCU/standby authority.
- [x] Offline cold-fb1 repair: `display.cold_init=1` detects an invalid var and
      writes the shipped 1920x1080/2-frame/32bpp template via
      `FBIOPUT_VSCREENINFO`; `ntkdriver_nondovi.ko`'s `gstOSDfbOps` proves
      `fb_check_var`→`_ker_osd_fb_CheckVar` and `fb_set_par`→
      `_ker_osd_fb_SetPar`. `check-display` covers zero-var repair,
      region-before-pan and first buffer 0, and fails if the repair is skipped.
      Both bundles set it; no effect on a valid var. Not cold-boot or visible
      scanout acceptance. COLD-DISPLAY-REPAIR-2026-11-07.
- [ ] Panel backlight/PWM curve: stock calibration recovered read-only
      (channel 0, 30000 Hz, set-mode 0, level 1000) and `mod_panel` defaults
      corrected; clean 50% hardware run opened `/dev/ntkled` and reported
      `panel-state 50`. 2026-11-07 owner-confirmed run: `panel 50` and
      `panel 0` both returned `ok` and updated `panel-state`, but the owner saw
      **no visible dimming at either value**. The cause was a units mismatch,
      not a wrong device path: the firmware converts the 12-bit duty to the
      HAL's `0..1000` domain in `plat_set_dimming` before
      `Hal_Pwm_Set_ByChannel`, while the module sent `0..4095` directly and
      saturated 50%/100%. The module now applies that exact conversion and
      `check-panel-curve` covers 1000/600/200 for 100/50/0%. Do not claim
      brightness acceptance until visible dimming is independently confirmed;
      panel power and resume remain separate. PANEL-BACKLIGHT-2026-11-07.
- [ ] Panel power/backlight resume after real kernel PM suspend and standby
      ownership; separate from the host hook-only `suspend`/`resume` tests.
      `mod_panel` now implements the recovered
      panel-power/LVDS/PWM/backlight order behind `panel.standby=1` (default
      0), with pins 110/111, levels 1/0, 30/580/220/30 ms delays and a 70 ms
      pre-backlight delay; `check-panel-standby` asserts the order and fails a
      wrong order. It has not been run on the TV (suspend blocked), and the
      real standby/power-owner path remains unaccepted. PANEL-BACKLIGHT.
- [ ] Replacement takeover system: `make replacement-bundle` builds the
      reproducible `plat_stub` + display/panel/ir/settings/service_db/
      image_decode/ui_engine/ui_vendor/ticker/ui_clock/ui_anim composition
      with client and menus. Verified portal paint, physical BLAST navigation,
      picture-settings entry and `reload ui_vendor`; `display.mirror=0` +
      `ui.fps=0` gives ~46–51 ms scene + 6–12 ms pan. Remaining: exact
      rail-highlight cadence/geometry, full menu/source/channel UI assets,
      real-plat OSD unification (real plat leaves the vendor OSD black) and
      boot persistence/boot-hook (separate gate). `reload-suite` (exit 75) +
      `oa-supervise` relaunches the whole suite without rebooting or re-arming
      the watchdog; per-module reload remains. `full-replacement-bundle` adds
      the real platform with `plat.drm_init=0`, all feature modules, broker,
      and AQ/theme/profile/menu configs; preflight and CPU-only asset starts
      pass, unified real-plat display/hardware awaits a takeover test.
      2026-11-08 `native-replacement-bundle`: 26 modules autoloaded,
      `ui_omarchy` started and painted 1920x1080 (40-44 ms scene, 4-8 ms pan),
      `ui_vendor` absent; stock restore then reboot (hidden audio DSP state).
      2026-11-08b adds `mod_app_launch`, `oa-app-launch` and `oa_apps.map`
      (33 bundle modules, 27 autoloaded); the consumer is host-tested but
      `app_launch.enable=0`. Two owner-approved runtime retail-appmgr attempts
      failed: the first could not create SaWMan; the second (`run.sh` paused,
      watchdog disabled) created SaWMan but not the NAM manager socket, so
      `oa-app-launch` registration was refused. The helper now links libpthread
      for libnpc's `sem_init`. Stock restored after reboot: appmgr 1578/aurora
      1780, controls healthy. The native shell handles volume up/down/mute
      (codes 16/17/13) with a 1.5 s OSD. TV test: volume up/down OSD + soundbar
      volume accepted; mute set_mute -1, but stock
      `DemoApp::toggleAndDisplayMuteState` skips external output types 1/2 and
      the settingsDB has `m_sound_output_mode=1`, `m_digital_audio_out=1`,
      `m_spdif_out_on=1`; optical mute is likely stock no-op, do not force
      volume 0. Source OK blocked by the incomplete source path; up/down
      reversed and Exit2 unhandled. Firmware IR-KEYMAP/
      SETTINGS-INPUT/TAB-LEAF confirm RC 20=Up/0x20a, 19=Down/0x20b,
      37=KeyExit2/0x25b menu/back; the shell now maps these (`check-ui-skin`
      passes). Source/mute firmware recovery is in progress. App-launch TV
      acceptance needs a boot-time profile/list gate;
      native shell parity, app-launch TV acceptance and cold boot remain open.
      REPLACEMENT-SYSTEM, NATIVE-REPLACEMENT.
      TV/audio state, 2026-11-08: the bundle-e key/source test confirmed
      up/down/Exit/volume OSD; source did not change and the test left stock
      `m_current_source=5`/`m_volume=0`. The owner force-rebooted to restore
      audio; the TV is stock/audio back. The takeover restore does not restore
      stock volume/source; no further TV tests until fixed. The native UI
      target is the browser OmarchyTV app reproduced natively.

- [ ] Native OmarchyUI port: the workspace shell and Lua panes are
      host-tested (NATIVE-OMARCHY-UI-2026-11-08): five-workspace bar/clock,
      six-slot TV/app dock, 4-column inputs, 7-column apps, themes, OAI1
      wallpaper and `ui.lua` v2 `oa.pane` widgets. Remaining: TV acceptance,
      launcher icon decode, focus animation/transitions, screensaver,
      theme/wallpaper persistence, EF41→reference font conversion and the
      larger plugin/arcade/Cinemarchy scope. No browser/webview/Opera.
- [ ] Takeover stock-settings restore: `settings-get`/`settings-set` are
      host-tested, and `takeover-full.sh` snapshots
      `m_volume`/`m_current_source`/`m_cec_enabled` and restores them before
      `quit`, plus a settingsDB copy. Not TV-validated; hidden DSP/route state
      may still require a reboot. No further TV tests until validated.
- [ ] Source switching full path: implement `plat_vin_slot_connect`/VPLT/
      `Hal_Video_Set_V4l2Info` and the audio companion; VIN select/start alone
      does not change the display. `source.enable` remains disabled.

### Audio / CEC

- [ ] Amp profile/init/reset/mute/standby (AD82587), speaker gain/volume and
      mixer; verify audible output and no stuck mute/standby. 2026-11-07
      read-only group: audio_amp_profile/audio_curve/audio modules start,
      profile query 12Watt/27/5/5/128, curve 8137, `audio.enable=0`; stock
      mixer values0 and amp registers 1b/05/05 unchanged. No audible output,
      volume/mute write or amp init/reset/standby. Native backend remains open.
      AUDIO-CEC-TAKEOVER. 2026-11-07 offline: added `common/amp_board.c`, the
      explicit adapter between the recovered errno-returning transports
      (normal reset114, MCU/STBC mute208, one-byte I2C writes) and
      `amp_init`'s vendor boolean callbacks; `check-amp-board` covers the
      default records, pin routing, errno->false mapping, write-failure finish
      and cache/subwoofer policy. Injected I/O only; no device opened or
      written. A second layer, `common/amp_vendor.c`, composes the linkable
      vendor wrappers: `Hal_Gpio_Write/Config` for pin<200 and
      `NDAL_VideoExt_STBC_SetStbcGpio((pin+0x38)&0xff, level?2:3)` for
      pin>=200, plus the I2C session. `check-amp-vendor` covers routing,
      vendor booleans, 38 writes and the full sequencer; ARM links only those
      three vendor symbols. The GPIO wrappers issue the `670a` claim and the
      STBC call needs the plat video-open global fd. No module consumer,
      hardware run, paired shutdown or audible acceptance yet.
      AMP-TRANSPORT. 2026-11-07 module: `audio_amp` v0.1 publishes
      `audio.amp:v1`; `amp-init` submits one Init to the existing owner worker
      and `amp-state` polls the completion. `audio_amp.enable=0` by default;
      enable=1 requires plat ready, loads the profile, opens `/dev/i2c-3` and
      creates the owner with the recovered cold-boot image (`aurora.elf`
      vaddr `0x17640e8`, FNV `0xab635b86`). Quiesce unpublishes, then joins
      the owner and closes I2C on a worker. Host lifecycle/fail-closed tests,
      ARM build and TV `--check` pass. 2026-11-07 hardware: with the real
      `mod_plat` + `audio_amp`, `amp-init` completed (`result 1`, `reset 0`,
      `mute 1`); the init-muted amp NACKed the read-only probe, and the new
      `amp-mute 0` restored I2C and readback `1b/05/05`. The stock runtime
      image matched the embedded baseline plus configure patches. Restored by
      `wdtctl set 10` (appmgr 1575/aurora 1777, source 5, profile/fb/backlight
      unchanged). 2026-11-07 owner-confirmed amp A/B: `amp-init` result 1
      (mute 1) and `amp-mute 0` (`live 0`), but the same 440 Hz default-PCM
      tone was **audible in both states**. The Init/mute state does not gate
      that PCM path; audible-amp acceptance is retracted. In-module readback,
      mute gating, volume/balance hardware and standby remain open.
      AMP-MODULE, HW-ACCEPT. Follow-up: the AD82587 object has NULL
      Term/PowerDown slots and an empty Close, so there is no vendor paired
      shutdown; do not invent a reset/mute-on-stop. Standby/amp-rail policy is
      an MCU/panel/power-owner concern. AMP-SHUTDOWN. 2026-11-07 balance:
      recovered the SetBalance regs4/5 formula and added `amp-balance
      <0..100>`/`set_balance` with host tests. Follow-up AMP-JOBS moved both
      mute and balance to typed owner worker jobs; `amp-state` reports the
      applied value, and a pending job still rejects with `-EBUSY`. SetVolume
      (reg3 prescale + readback), SoftMute (reg2 bits 0x0e) and HP_Mute
      (GPIO0xc3) remain unwired; no hardware/audible balance run.
      AMP-BALANCE.
- [ ] Agent-review follow-ups (AGENT-REVIEW-2026-11-07): UART repeat
      semantics. The CEC authority mapping lifetime
      (`check-cec-authority-lifetime`), CEC suspend/resume
      (`check-cec-suspend-resume`), IR helper pipe/8-byte
      record/CLOEXEC/bounded reap (`check-ir-helper`, IR-HELPER), amp
      mute/balance owner jobs (`check-amp-owner-jobs`, `check-audio-amp-jobs`,
      AMP-JOBS), tuner p4 (`check-tuner-backend`, TUNER-P4), panel HAL errors
      (`check-panel-curve`, PANEL-HAL-ERRORS), the CEC reply queue
      (`check-cec-reply-queue`, CEC-REPLY-QUEUE) and amp Init
      retry/suspend (`check-audio-amp-jobs`, AMP-RETRY-SUSPEND) are fixed.
- [ ] Panel `panel <0-100>` still runs `Hal_Pwm_*` on the host loop; submit it
      to a worker and report completion (AGENT-REVIEW-2-2026-11-07).
- [ ] CEC ownership/arbitration, logical address, one-touch play, standby,
      ARC/audio and TX/RX; shared-owner queries only where read-only.
      2026-11-07: `mod_cec` starts and publishes owner/audio policy, but
      `cec-state` reports `supported 0`; no raw open, LA claim, authority or
      bus traffic. 2026-11-07 native backend: added the read-only authority
      mirror map, `cec.enable` gate, async owner acquire/receive/send, 100 ms
      poll, policy reply queue and `cec-open`/`cec-close`/`cec-send`; host
      tests pass. Hardware: authority bit1 was set (`0x000001ab`), but stock
      SIGTERM left kernel thread `DRV_CEC_CALLBAC` holding the exclusive
      KER_CEC lock, so raw open returned EPERM (`CEC Try lock fail`) and no
      frame was sent/received. Follow-up close-path recovery: `ICE_CECClose`
      is a stub, `PLAT_CEC_Close` has no internal caller, and
      `NDAL_VideoExt_CEC_Init(NULL)` from a new process reports success without
      stopping the orphaned thread. No tested userspace stock-close trigger was
      found (indirect callers were not exhaustively checked), so the viable
      path remains an early boot-order start (boot-hook gate) or shared
      stock-owned monitoring. Do not patch driver globals or unload the
      driver. CEC-RAW-BACKEND.

### Input / source / PVR

- [ ] Source/VIN switching: CVBS/HDMI1-3/YPbPr/VGA/TV IDs and picture slots.
      2026-11-07: `Hal_Vin_SelectInput`/`SetStreamStart` selected type17
      (HDMI1, vin id3) then type0 (TV, vin id1); `source-state` reflected both.
      Visible display/audio routing and DB/UI resync were not validated; the
      settings `m_current_source` stayed5. INPUT-PVR-TAKEOVER.
- [ ] IR via `/dev/video28` BLAST RX: key map, repeat and release. 2026-11-07:
      `mod_ir` now runs BLAST_RX on a dedicated worker, queues keys and posts
      them to the loop; the TV link also needed `-ldl`. `check-ir-blast-worker`
      uses a blocking fake libNDAL_EX and proves loop responsiveness, posted
      press/release keys and clean fake quit. Hardware `ir.blast=1`: main
      thread `poll_schedule_timeout`, worker `blast_io_get_key`, source/PVR
      commands responsive. **Superseded by the helper follow-up:** BLAST_RX
      now runs in killable `oa-ir-blast`; real quit and `reload ir` pass with
      no helper left, and `pipe2` replaced a GLIBC_2.28 `fcntl` binding. Still
      unverified: a physical remote press/repeat/release with the helper
      active. The UART parser marks a repeated `P` frame as repeat; the BLAST
      path forwards `BLAST_RX` unchanged, so the window must determine whether
      held keys produce repeated `flag=0` records or a repeat flag. If only
      edges arrive, repeat synthesis is still missing. Packaging verified:
      `all`/`all-core`/`all-stage` build the helper,
      cold/stage bundles copy and hash it, `install*` put it in `BINDIR`, and
      `ir.blast_helper=oa-ir-blast` resolves against `/proc/self/exe`.
      IR-BLAST-HELPER. 2026-11-07 physical run: owner key presses produced
      real BLAST records for Right (0x3056), Down (0x3853), Up (0x3054) and
      Left (0x3855) with press and release records. Held keys arrived as
      `aux=01 flag=00` between `aux=00 flag=00` press and `flag=01` release;
      the module emits every flag=0 as PRESS. Firmware `_dev_uart_parse_rc_key`
      @0x85d7dc passes the aux pointer but selects 0x50/0x5e from flag only
      and never reads aux, so the module matches vendor synthesis; an
      `OA_KEY_REPEAT` emitter is still unimplemented. No OK press was
      captured. HW-ACCEPT.
- [ ] PVR: frontend/demux/dvr open, record/playback and TS output; record what
      is gated by storage. 2026-11-07: only the offline persistent stub ran;
      `pvr-add`/`pvr-list`/`pvr-cancel` worked (cancel state3, record retained).
      No frontend/demux/dvr fd, recording, playback or TS output.

### Separate owner gates (not this session unless explicitly extended)

- [ ] Cold independent startup / boot without stock Aurora. 2026-11-09: the
      installed `/conf` hook was exercised on the real boot path with the
      34-module cold bundle; the first attempt hung the set and required the
      USB `/conf` recovery (BOOT-HOOK-COLD-ATTEMPT-2026-11-09). The staged
      safety path was then implemented and armed three times
      (STAGED-COLD-BOOT-DESIGN-2026-11-09): A blocked on a gate-argument bug
      and B on a tuner init/NULL-ctx cleanup (both fell back to stock safely,
      fixes committed); C reached **our UI** on a true boot but showed only
      the top-left quadrant with icons missing — the set was alive, the
      network miss was misread (only `.87` probed), and the forced
      `display.osd_reinit` is the prime suspect. Not accepted. Next: stage 0
      is now the takeover display path (`plat.drm_init=0`,
      `display.osd_reinit=0`); the forced DRM/OSD reinit is stage 2; add the
      remaining `plat` pieces one at a time (audio prelude, VPLTIO/V4L2/
      sharemem, GPIO/IIC, video device). Logs now live on `/buffer` and
      survive the apdat0 recovery. The hook template, deploy/restore script
      and runbook are in the repo; `/buffer/openaurora` is the prior 12-module
      payload and the native path is disabled.
- [ ] MCU/watchdog authority and standby/resume ownership.
- [ ] Any persistent replacement or boot-hook change. The `/conf`-only hook
      (`/conf/remote.txt` -> `/conf/remotelf/aurora.elf`, gated by
      `/conf/oa_boot` + `/conf/oa_safe`) is installed but disabled (oa_safe
      present, oa_boot absent). Enabling the native path remains this gate.

## Current acceptance gates

- [ ] **Takeover audio restore:** the first real-platform takeover produced a
      visible UI but left the optical-S/PDIF soundbar silent after process-only
      restore; a stock Aurora restart did not recover it, `reboot -f` did (owner
      confirmed optical soundbar audio back). TV internal speakers are
      intentionally muted. 2026-11-08 ALSA-CONTROL: `nvt opu hw volume
      0 12000 12000` is one reproduced cause and `0 0 0` restored audio without
      reboot; a later run also left sticky route controls (`spdif region area`,
      `hp/line out nvt enable`, `hp/line out hp gain enable`) whose writes
      return success but do not change. Recover/prevent the sticky writer or a
      safe driver reset, or explicitly require a reboot after every takeover.
      `oa-alsactl` and filtered OPU/route restore are integrated but not
      sufficient for the sticky state. TAKEOVER-ACCEPT-2026-11-07.

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

- [x] Quick picture-settings real-asset offline capture: added
      `tests/ui_settings_quick.c` and `make capture-settings-quick-real`; the
      shipped `paint_settings` path paints the workbench `settings_rail/top/cat`
      at 0,180/0,240/0,440 over (13,14,14). The recorder snapshot is
      byte-identical to the on-TV fb1 quick-settings capture
      (`capture_compare.py`: AE=0, max=0, 0/2,073,600 changed).
      SETTINGS-QUICK-CAPTURE.

- [x] Startpagina portal real-asset offline capture: added
      `tests/ui_portal_quick.c` and `make capture-portal-real`; the workbench
      `title`/`rail1..5`/`card_*` assets, focus rail2 and the default nine-card
      row are painted by the shipped portal path. The recorder snapshot,
      composited over black (`rawrgba-to-png.py --over-black`), is
      byte-identical to the on-TV fb1 portal capture (`capture_compare.py`:
      AE=0, max=0, 0/2,073,600 changed). `capture-portal-card-real` adds the
      Prime Video focused-card state via two Right keys and the accent fill at
      394,473; it is also AE=0 against `ui-card-focus.png`. PORTAL-CAPTURE.
      Limits: default card row, not live extra cards; rail-highlight/title-
      slide animation states and GPU/scanout remain open.

- [x] Bronlijst source-dialog real-asset offline capture: added
      `tests/ui_sources_quick.c` and `make capture-sources-real`; the
      workbench `src_title`/`src_0..8`/`src_*_f` assets and the 780x1080
      `#BE1E2D` panel with focused row 0 at 18,152 are painted by the shipped
      `paint_sources` path. The composited recorder frame is byte-identical to
      the on-TV `ui-sources.png` (`capture_compare.py`: AE=0, max=0,
      0/2,073,600 changed). SOURCES-CAPTURE. Limits: row 0 only; other rows,
      animation/interaction and GPU/scanout remain open.

- [x] Full settings_list 780x1080 real-asset offline capture: added
      `OA_SETTINGS_REAL=1` to `tests/ui_settings_panel.c` and
      `make capture-settings-panel-real`. The shipped path clears transparent,
      blits the OAI1 panel at 0,0 and presents; the recorder crop matches the
      captured `settings_panel.rgba` source exactly (`panel-source-compare.py`:
      AE=0, max=0, 0/842,400 changed). Navigation panel -> quick tiles ->
      panel -> portal still passes. `OA_SETTINGS_SEL=0` plus
      `make capture-settings-focus-real` compares the focused row 0 region
      (20,186 750x66) to `settings_focus_0.rgba`: AE=0, max=0, 0/49,500
      changed. SETTINGS-PANEL-CAPTURE.

- [x] Live settings full-panel text: `OA_SETTINGS_TEXT=1` enables
      `ui.text=1` with the workbench font pack, and
      `make capture-settings-text-real` compares the complete 780x1080 live
      labels/values/title/slider/selection composition to the captured
      `settings_panel.rgba`: AE=0, max=0, 0/842,400 changed. This closes the
      captured static-value text path; settings acquisition/editing, other
      focused rows and animation remain.

- [ ] Offline full settings preview: complete live settings-value acquisition,
      remaining focused rows/slider/selection and Mesa renderer integration,
      including transformed frames. Recorder alpha composition is corrected;
      its set_scene_transform only records arguments. The quick
      picture-settings state, base `settings_list` 780x1080 panel, focused row
      0 and the live-text composition are exact offline; acquisition, other
      focused rows, animation and Mesa/transformed-frame paths remain open.
      The settings_panel OAI1 and slider/selection exporter already exist.
      Vendor Mali rasterization/scanout comparison still requires the set.


- [x] Temporary staged fb1/display binding and on-TV ABI 3 modules: with the
      owner's 2026-11-05 "full control" authorization, staged the current ABI 3
      core/modules in `/buffer`, loaded/started/reloaded epg/tuner/pvr/lua,
      and ran `display_fb` against the vendor-initialized fb1. It saved a
      16,588,800-byte snapshot, painted the magenta self-test band
      (`display-active-buffer.png`), then stop/unload restored fb1
      (`display-restored-buffer.png`, blank like the pre-test state). PVR
      add/list/reload and Lua load/start/reload passed; NDAL tuning returned
      EBUSY against stock-owned frontend0, and the direct DVB EIT demux opened
      but saw no sections while `frequency=0`. The full staged
      display_fb/ui_engine/settings/service_db/image_decode/ui_vendor stack
      painted "Startpagina" with 5 rail items and 12 cards; `reload ui_vendor`
      restored state and repainted. The new `key` control command fed real NEC
      frames through `mod_ir` (UART empty), moving card focus, opening the
      Bronlijst source dialog and the Beeld settings screen after staging the
      full asset set; Exit returned home. The stack then stopped and fb1
      restored. Staging was removed; stock
      Aurora PIDs 1967/1785 and `/vendor/com.nvt.idtv/aurora.elf` survived.
      LIVE-TV-ACCEPTANCE. Limits: temporary vendor-initialized binding, blank
      stock UI, no cold startup/native scanout, no live EIT signal, PVR DVR
      sink not wired.

- [x] Full portal takeover with the physical remote: staged
      plat_stub/display_fb/ui_engine/settings/service_db/image_decode/ir/
      ui_vendor under `/buffer`, stopped stock and painted Startpagina on
      fb1. The physical remote produced 29 BLAST presses
      (Right22/Left21/Down19/Up20) and moved card/rail focus; `reload ir`
      reopened `/dev/video28` without hang; `reload ui_vendor` restored state,
      republished `ui.portal`, repainted and kept decoding keys. Display stop
      needed the known retry; quit was clean and fb1 restored blank. Staging
      removed and watchdog reboot restored stock (aurora 1769, appmgr 1569,
      pan0,0, source5 unchanged). TAKEOVER-FULL. Limits: portal only, no
      settings/apps/source physical run, no cold startup/scanout/animation
      parity.

- [x] Control client timeout and fire-and-forget: `oa-client` defaults to a
      bounded 5 s wait, accepts `--timeout SECONDS`, reports a clear timeout
      error, and supports `--no-wait` for a reload the host may finish later.
      The silent-server test covers 1.02 s timeout and 0.00 s no-wait.
      `mod_ir` now keeps `/dev/ttyS2` closed when BLAST succeeds unless
      `ir.device` is explicit, removing the common dual-open reload stall
      (observed host stall ~64 s). OA-CLIENT-ASYNC. `mod_ir` also has the async
      cleanup gate below. Later gates are in the following entries; the open
      generic item lists what is still synchronous.

- [x] Non-blocking control batch loader: additive `load-batch`/`start-batch`/
      `stop-batch`/`unload-batch`/`reload-batch` queue one lifecycle operation
      per host-loop turn (64-entry FIFO, immediate `queued N`/`queued N
      dropped M` reply), and the loop skips the poll wait while entries
      remain. `tests/batch_load.c` asserts one module per pump, nothing before
      the pump, stop/reload/unload state, failure continuation, overflow and
      oversized-name handling; `tests/batch_ipc.cjs` saves `queued 3` + `no
      such module: ui_anim` from one pipelined write, then loads, starts,
      stops, reloads, unloads and reloads the stack without further commands.
      Teardown/reload order is consumers first because a loaded consumer keeps
      its provider hold. Host, focused ASan/UBSan, full suite and ARM build
      pass. The module scheduler is not used because its contract forbids
      lifecycle calls. Limit: one slow lifecycle call still blocks its own
      turn. OA-CLIENT-ASYNC.

- [x] Module inventory and packaging consolidation: 36 installed modules
      categorised as hardware 12, middleware 11, utilities 2 and UI/UX 11;
      `module-categories` prints the taxonomy and every stage. Core is the
      base platform (10: hardware access + settings/image_decode, no TV/UI),
      not a functional TV; distribution profiles add `tv` (15), `av` (19) and
      `ui` (31, the functional TV), then `dev` (34) and `full` (36). Added
      `all-stage`/`install-stage`/`stage-bundle` with generated per-stage
      `autoload=` lists; the base is `cold-bundle-core` (10, 1.4 MB) and the
      full `install`/`cold-bundle` is unchanged. The startup parser now
      accepts 64 autoload names and 511-byte config values (was 16/191);
      `tests/autoload.cjs` proves a 40-name list is not truncated. No module
      was merged or deleted; leaf merges remain a product decision. The old
      take job hang was a config truncation plus a test that did not kill its
      child on assertion failure; both are fixed. MODULE-CONSOLIDATION.

- [x] Skin hot-switch from inside any skin: `api->control("skin <name>")`
      from a skin callback is queued in `skin_pending` and executed by
      `oa_skin_pump()` after posts/scheduler/batch/timers unwind, so the
      calling skin is not unloaded on its own stack. `ui_omarchy` and
      `ui_diag` demonstrate it on project key 240; vendor remains available
      via `skin vendor`. `tests/skin_switch_ipc.cjs` switches omarchy → diag
      → omarchy with the real engine and fake display; full suite passes.
      SKIN-SWITCH. Hardware/display acceptance remains open.

- [x] First production GPU scene-path probe on fb1: staged
      graphics/scene_display + oa-scene-broker/scene-renderer.so, painted the
      5-rail/12-card portal and handled 14 physical BLAST presses. Non-reload
      paint intervals median260 ms/min129 ms, 11/23 <=200 ms versus 617 ms for
      display_fb; scene_display now uses the vendor mipmap sampler. The
      dual-open reload stalled ~64 s; killed, fb1 snapshot restored and
      watchdog reboot returned stock (1777/1572, pan0,0). GPU-SCENE-PROBE.
      Limits: SIGKILL rather than graceful GPU teardown, no timed animation,
      AE, cold startup or scanout.

- [x] Asynchronous cleanup gate for `mod_ir`: `m_quiesce` cancels the timer/
      watch, transfers the UART/BLAST fds and libdl handles to a worker, and
      returns EAGAIN while it runs. The first reload/stop fails fast with
      `cleanup pending`; the loop stays responsive and a retry completes.
      `tests/ir_quiesce.cjs` uses a pty plus a 400 ms diagnostic close delay
      and asserts fail-fast, responsive status and successful retry; host
      `check-ir-ipc`, full-stack IPC and ARM/libpthread builds pass.
      OA-CLIENT-ASYNC. `mod_i18n` now has the same gate (LIVE-SUBLAUNCHER-I18N):
      unpublish, detach the5 MB profile, free it on a worker; first reload
      fails fast, status stays responsive, retry completes. `mod_settings`
      now has the gate too (SETTINGS-QUIESCE): detach the SQLite handle and
      clear publication flags before unpublishing, close on a worker; the
      control test asserts fast-fail reload, responsive status, retry and the
      republished `picture-state 1` values. `mod_ui_vendor` now has the gate
      too (UI-VENDOR-QUIESCE): synchronous timer/subscription/provider/portal/
      skin teardown, then the font pack and text caches are detached and freed
      on a worker; idempotent for reload/unload. The control test asserts
      fast-fail reload, responsive status, retry, republished portal screen7
      and clean stop/unload. It also fixed restore rejecting screen7. `mod_ui_engine` has the gate too
      (UI-ENGINE-QUIESCE): refuse while a skin dispatch is on the stack,
      otherwise unpublish/drop display/alpha/async, unsubscribe, delete
      timers, then free the OAI1 icon cache on a worker; idempotent. The
      control test loads settings_top1920x180, fails fast, retries, reloads
      the asset and stops/unloads cleanly. `mod_display` has the gate too
      (DISPLAY-QUIESCE): closing rejects draw/present/info, the existing worker
      restores the snapshot and closes the layer with
      `display.cleanup_delay_ms`, and m_stop unpublishes after join. The
      fake-fb socket test asserts fast-fail reload, responsive status, retry,
      `display-info` 1920x1080 32bpp2, then the snapshot equality checks.

- [x] Asynchronous gate for `mod_source`: unpublish `source` and detach the
      VIN fd synchronously (stale iface sees fd<0/unavailable), then
      `Hal_Vin_Close` on a worker after `source.cleanup_delay_ms`; tryjoin and
      idempotent for stop+unload, `m_stop`/`m_destroy` join. Fake-VIN control
      test: reload fails fast, `source-state` already ENODEV, status
      responsive, retry reopens VIN with the worker close before the new
      open; stop gate closes the second fd; `source.enable=0` still refuses.
      ARM mod_source links libpthread+vendor libs. Full suite/QEMU pass.
      SOURCE-QUIESCE.

- [x] Real VIN HAL device-boundary simulation: run the shipped ARM
      `libNDAL3.so` `Hal_Vin_*` wrappers and the real `mod_source.so` under
      qemu against an `LD_PRELOAD` fake `/dev/video20`. `Hal_Vin_Open(0)`
      opens `/dev/video20` (not `/dev/ntkdvif`), `SelectInput` is ioctl
      `0xc0045627` (id3/id66), `SetStreamStart` is `0x40045612` (on1), `Close`
      is `close`. The real module's reload/stop gate closes before reopening;
      `libNDAL_EX` supplies `gs32Ndp_PQ_fd`. `check-vin-real`.
      VIN-DEVICE-SIM.

- [x] Asynchronous gate for `ui_menu`: set publication false, unpublish
      `ui.menu` (document/dialog/branch return NULL), detach the parsed JSON
      document, then free it on a worker after `ui_menu.cleanup_delay_ms`;
      tryjoin/idempotent for stop+unload, `m_stop`/`m_destroy` join. New
      control `menu-state <dialog>` returns `menu sub_launcher dialogs 1
      branches 5`. Test: reload fails fast, `menu-state` ENODEV while the
      worker runs, status responsive, retry republishes the same tree; stop
      uses the gate. ARM module links libpthread. Full suite/QEMU pass.
      UI-MENU-QUIESCE.

- [x] Asynchronous gate for `mod_audio`: unpublish `audio` and release `plat`
      synchronously, detach the `oa_audio_owner`, then quiesce/poll-join/
      destroy it on a worker after `audio.cleanup_delay_ms` (the owner may
      block in a client RPC). `m_stop`/`m_destroy` join; `m_start` refuses if
      live; `settled` is suppressed while quiescing. New `audio-state`/
      `audio-volume` control probes. Stub-HAL test: volume 42 set, reload fails
      fast, `audio-state` ENODEV, retry republishes with volume 42 restored
      and reapplied; stop uses the gate. Existing audio/CEC tests pass; ARM
      links vendor libs + libpthread. Full/QEMU pass. AUDIO-QUIESCE.

- [x] Asynchronous gate for `mod_cec`: unpublish `cec.owner`/`cec.audio`
      synchronously, detach the `oa_cec_owner`, then quiesce/poll/join/destroy
      on a worker after `cec.cleanup_delay_ms`. A failed hardware close
      retains the owner (`-EAGAIN`) for a new generation; normal stop waits,
      force unload with a retained owner is outside the safe contract.
      `m_start` refuses if live; `settled` suppressed while quiescing. New
      `cec-state` probe. Test: reload fails fast, both services ENODEV, status
      responsive, retry republishes; stop uses the gate. Existing
      retained-close/owner tests pass; ARM links libpthread. Full/QEMU pass.
      CEC-QUIESCE.

- [x] Asynchronous gate for `mod_audio_mixer`: stop/reload unpublishes
      `audio.mixer` synchronously, removes the watch and detaches the broker
      socketpair, then closes it on a worker after
      `audio_mixer.cleanup_delay_ms`; the 50 ms reap timer stays active to
      retire the child. Suspend keeps the service/holds and timer; stop
      joins/deletes the timer; start refuses if live. Echo-broker control
      test: query/collect left123/right456, reload fails fast, `mixer-query`
      ENODEV, status responsive, retry republishes with a fresh broker, child
      reaped before quit, stop gate. Existing watch/timer/suspend/reload tests
      pass; ARM links libpthread. Full/QEMU pass. AUDIO-MIXER-QUIESCE.

- [x] Asynchronous gate for `mod_tuner`: stop an active scan and unpublish
      `tuner` synchronously, then close the TS replay `FILE*` synchronously or
      on a worker after `tuner.cleanup_delay_ms`. The `ts_open` flag prevents
      the stop+unload double-quiesce from spawning a second worker after the
      close. `m_stop`/`m_destroy` join and close leftovers; `m_start` refuses
      if published or a worker is live. Control-socket test with the generated
      EIT TS fixture: reload fails fast, `tuner-state` ENODEV, status
      responsive, the old `/proc/<pid>/fd` TS entry disappears, the reload
      reopens it, scan still works, stop closes the second fd. Existing
      stub/TS tuner IPC tests pass; ARM links libpthread. Full/QEMU pass.
      TUNER-QUIESCE.

- [x] Asynchronous gate for `mod_epg`: clear publication and unpublish
      `epg.ingest`/`epg` synchronously, detach the `oa_eit_cache`, then persist
      (when `epg.path` is set) and destroy it on a worker after
      `epg.cleanup_delay_ms`. `m_stop`/`m_destroy` join/free leftovers;
      `m_start` refuses if live and recreates the cache for a stop/start cycle.
      Control-socket test: ingest two EIT events, reload fails fast,
      `epg-count` ENODEV, status responsive, the worker writes the two-record
      `OAEPG1` file, the retry completes and the replacement reloads both
      events; clear and stop use the gate. Existing EPG persistence and tuner
      handoff tests pass; ARM links libpthread. Full/QEMU pass. EPG-QUIESCE.

- [x] Asynchronous gate for `mod_pvr`: clear publication and unpublish `pvr`
      synchronously, then close the file-backed backend synchronously or on a
      worker after `pvr.cleanup_delay_ms`. An `opened` flag prevents the
      stop+unload double quiesce from spawning a second worker.
      `m_stop`/`m_destroy` join/close leftovers; `m_start` refuses if
      published, a worker is live or the backend is still open. Control-socket
      test: add two reservations, reload fails fast, `pvr-list` ENODEV, status
      responsive, the two-record `OAPVR1` file is present, the retry completes
      and lists both, a new add continues the persisted id sequence,
      record/stop still work, and stop uses the gate. Existing PVR
      reload/cross-process test passes; ARM links libpthread. Full/QEMU pass.
      PVR-QUIESCE.

- [x] Asynchronous gate for `mod_channels`: move the runtime active list from
      process-global arrays to a heap-owned `oa_channel_list` per module
      instance (the shared `oa_channel_runtime_iface` stays for direct tests).
      Quiesce clears publication, unpublishes `channels` synchronously, detaches
      the list, then destroys it synchronously or on a worker after
      `channels.cleanup_delay_ms`. A stale iface fails closed;
      `m_stop`/`m_destroy` join/free; `m_start` refuses if live and starts with
      an empty list. Control-socket test: ingest two rows, reload fails fast,
      `channel-state` ENODEV, status responsive, retry starts empty and ingests
      again, stop uses the gate. Direct/IPC/UI channel tests pass; ARM links
      libpthread. Full/QEMU pass. CHANNELS-QUIESCE.

- [x] Asynchronous gate for `mod_service_db`: clear publication and unpublish
      `service.list` synchronously, detach the heap `oa_service_list` row
      array, then free it synchronously or on a worker after
      `service_db.cleanup_delay_ms`. `m_stop`/`m_destroy` join/free leftovers;
      `m_start` refuses if live. Control-socket test with the 525-row dump:
      reload fails fast, `service-list` ENODEV, status responsive, retry
      re-reads 525 rows, sorted/name/channel probes work, stop uses the gate.
      DB unit/module/IPC tests pass; ARM links libsqlite3 + libpthread.
      Full/QEMU pass. SERVICE-DB-QUIESCE.

- [x] Asynchronous gate for `mod_image_decode`: clear publication and
      unpublish `image.decode` synchronously, detach the libpng/zlib/TurboJPEG
      handles and clear the TurboJPEG pointers, then `dlclose` them
      synchronously or on a worker after `image_decode.cleanup_delay_ms`.
      `m_stop`/`m_destroy` join/close leftovers; `m_start` refuses if live and
      reopens the codecs. Control-socket test: decode PNG, reload fails fast,
      `image-decode` ENODEV, status responsive, retry reopens and decodes the
      same bytes, stop uses the gate. Existing PNG/JPEG unit and IPC tests
      pass; ARM links libdl + libpthread. Full/QEMU pass.
      IMAGE-DECODE-QUIESCE.

- [x] Asynchronous gate for `mod_ui_theme`: clear publication and unpublish
      `ui.theme` synchronously, detach the parsed `oa_svg_pack`, then free it
      synchronously or on a worker after `ui_theme.cleanup_delay_ms`.
      `m_stop`/`m_destroy` join/free leftovers; `m_start` refuses if published,
      a worker is live or a pack is still attached. New `theme-state` count
      probe. Control-socket test: count 1, reload fails fast, `theme-state`
      ENODEV, status responsive, the fixture is changed to 2 records while the
      old pack is detached, the retry reports count 2; stop uses the gate.
      Existing 702-record corpus, skin, hold, suspended-reload, fixture-IPC and
      UI-anim bootstrap tests pass; ARM links libpthread. Full/QEMU pass.
      UI-THEME-QUIESCE.

- [x] Asynchronous gate for `mod_ui_list`: move owned state to the heap. Quiesce
      clears publication, unpublishes `ui.list` synchronously, snapshots the
      OAL1 handoff blob, detaches the state, then frees it synchronously or on
      a worker after `ui_list.cleanup_delay_ms`. Retry `save()` uses the
      snapshot after detach; `restore()` allocates fresh state on a stopped
      instance after validating the blob. `m_stop`/`m_destroy` join/free;
      `m_start` refuses if live. Control-socket test: key selection, reload
      fails fast, `list-state` ENODEV, status responsive, retry preserves the
      selected/animation state and still routes keys; stop uses the gate.
      Direct/IPC list tests pass; ARM links libpthread. Full/QEMU pass.
      UI-LIST-QUIESCE.

- [x] Asynchronous gate for `mod_ui_navigation`: move selection/queue state to
      the heap. Quiesce clears publication, unpublishes `ui.navigation`
      synchronously, snapshots the OAN2 handoff blob, detaches state, then
      frees it synchronously or on a worker after
      `ui_navigation.cleanup_delay_ms`. Retry `save()` uses the snapshot after
      detach; a failed start/resume that triggers quiesce restores from that
      snapshot on the next start. `m_stop`/`m_destroy` join/free; `m_start`
      refuses if live. Control-socket test: queue two requests, reload fails
      fast, `nav-state` ENODEV, status responsive, retry preserves the
      selection/queue and still delivers a request; stop uses the gate.
      Existing unit/IPC/matrix tests pass, including 1792 OAN roundtrips and
      failed-resume retry; ARM links libpthread. Full/QEMU pass.
      UI-NAVIGATION-QUIESCE.

- [x] Asynchronous gate for `mod_ui_profile`: move the copied profile to the
      heap. Quiesce clears publication, unpublishes `ui.profile`
      synchronously, detaches the profile, then frees it synchronously or on a
      worker after `ui_profile.cleanup_delay_ms`. New `profile-state` probe.
      Control-socket test: connectivity1/hotel0, reload fails fast,
      `profile-state` ENODEV, status responsive, fixture changed to
      connectivity2/hotel1, retry reports the new facts; stop uses the gate.
      Audit/fix `ui_list` failed-start: start rebuilds from the OAL1 snapshot
      after a publish-collision quiesce, and fresh-state restore
      reinitializes fixture handles; new suspend/reload/peer/resume regression.
      Unit/fixture tests pass; ARM links libpthread. Full/QEMU pass.
      UI-PROFILE-QUIESCE.

- [x] Asynchronous gate for `mod_audio_curve`: move the decoded 101-point
      table to the heap. Quiesce clears ready/published, unpublishes
      `audio.curve` synchronously, detaches the table, then frees it
      synchronously or on a worker after `audio_curve.cleanup_delay_ms`.
      `m_stop`/`m_destroy` join/free; `m_start` refuses if live and rereads the
      AQ file/profile, so no handoff snapshot is needed. Control-socket test:
      base100 curve110, reload fails fast, `audio-curve` ENODEV, status
      responsive, AQ changed to base200, retry curve210; stop uses the gate.
      Direct/module/fixture tests pass; ARM links libpthread. Full/QEMU pass.
      AUDIO-CURVE-QUIESCE.

- [x] Asynchronous gate for `mod_audio_amp_profile`: move profile+32-byte
      power into one heap object. Quiesce clears published, unpublishes
      `audio.amp_profile` synchronously, detaches the data, then frees it
      synchronously or on a worker after
      `audio_amp_profile.cleanup_delay_ms`. `m_stop`/`m_destroy` join/free;
      `m_start` refuses if live and rereads the hwprofile, so no handoff
      snapshot is needed. Control-socket test: prescale17, reload fails fast,
      `amp-profile` ENODEV, status responsive, hwprofile changed to31, retry
      reports31; stop uses the gate. Existing provider/module/IPC/fixture
      tests pass; ARM links libpthread. Full/QEMU pass.
      AUDIO-AMP-PROFILE-QUIESCE. On-TV ARM follow-up: the TV
      `/conf/mb230_hwprofile.bin` matches the local artifact (md5
      1b063b2cc7faa8b92c0f843921e561c7), and the hash-verified ARM host+module
      ran data-only against it: prescale27, fail-fast reload/ENODEV, delayed
      retry/stop, unload clean, staging removed, stock Aurora PID unchanged.
      Amplifier hardware/audible acceptance still open.

- [x] Asynchronous gate for `mod_panel`: unpublish `panel` synchronously, arm
      one `needs_cleanup` cycle per start, then run `Hal_Pwm_Close` on a worker
      after `panel.cleanup_delay_ms`. Host stubs still honor the diagnostic
      delay. New `panel-state` probe. Control-socket test: percent40, reload
      fails fast, `panel-state` ENODEV, status responsive, retry republishes40;
      stop uses the gate. `needs_cleanup` prevents the stop+unload double
      quiesce from arming a second worker. Existing plat-consumer IPC passes;
      ARM links vendor libs + libpthread. Full/QEMU pass. PANEL-QUIESCE. No
      PWM/backlight hardware run; real close remains unaccepted.

- [x] First non-Lua host `sched` use: migrate `ui_theme`’s delayed pack free
      from a pthread to one nonblocking scheduler slice. Quiesce still
      unpublishes synchronously, fails fast while pending, frees at delay0,
      and cancels/frees if the task is dropped. ARM `ui_theme` no longer links
      libpthread. Existing 400 ms gate, fixture IPC, skin and UI-anim tests
      pass; full suite ends `all tests passed`; QEMU passes. Only pure-free
      cleanup migrated; blocking close/join paths remain workers. TV not used.
      SCHED-UI-THEME.

- [x] Second host `sched` use: migrate `channels`’ delayed list destroy from a
      pthread to one nonblocking scheduler slice. Quiesce still unpublishes
      synchronously, fails fast while pending, destroys at delay0, and
      cancels/frees a dropped task. ARM `channels` no longer links libpthread.
      400 ms gate, XML/IPC tests, full suite `all tests passed`, and QEMU pass.
      Pure-free cleanup only; blocking close/join paths remain workers. TV not
      used. SCHED-CHANNELS.

- [x] Asynchronous gate for `mod_ui_diag`: synchronously set suspended,
      unsubscribe, unregister the `diag` skin/release `ui.engine`, detach the
      font pack/pixels, then free them from one `sched` slice after
      `ui_diag.cleanup_delay_ms`. `m_start` refuses while pending; stop/destroy
      cancel/free. Control-socket test: engine `skin` shows no active diag
      while the 400 ms task is pending, first reload fails fast, status
      responsive, retry re-registers diag; stop uses the gate. Existing
      diag/skin-socket tests, full suite `all tests passed`, and QEMU pass. ARM
      links libm/libc only. Recorder fixture, not scanout; no TV run.
      UI-DIAG-QUIESCE.

- [x] Asynchronous gate for `mod_ui_anim`: unpublish `ui.anim`, remove the
      cadence timer, unbind callbacks and release `ui.clock` synchronously,
      detach the heap controller, then destroy it from one `sched` slice after
      `ui_anim.cleanup_delay_ms`. Stale calls fail closed; start recreates the
      controller; stop/destroy cancel/free. New `anim-state` probe. Control
      test: reload fails fast, `anim-state` ENODEV, status responsive, retry
      republishes; stop uses the gate. Animation review/bootstrap tests, full
      suite `all tests passed`, and QEMU pass. ARM links libm/libc only. No
      live animation across reload; no TV run. UI-ANIM-QUIESCE.

- [x] Asynchronous gate for `mod_ui_clock`: unpublish `ui.clock`, remove the
      timer and release the raw clock synchronously, snapshot/detach the FROST
      time state, then free it from one `sched` slice after
      `ui_clock.cleanup_delay_ms`. Retry save uses the snapshot; failed
      activation start restores it; stop/destroy cancel/free. New
      `clock-state` probe. Control test: reload fails fast, `clock-state`
      ENODEV, status responsive, retry republishes; stop uses the gate. Clock
      unit tests, full suite `all tests passed`, and QEMU pass. ARM links libc
      only. No live continuity across reload; no TV run. UI-CLOCK-QUIESCE.

- [x] Asynchronous gate for `mod_ticker`: remove the timer and unpublish
      `clock` synchronously, snapshot/detach the tick counter, then free it
      from one `sched` slice after `ticker.cleanup_delay_ms`. Retry save uses
      the snapshot; start restores the counter; stop/destroy cancel/free. New
      `ticker-state` probe (published clock + saved ticks). Control test waits
      1.1 s for tick1, reload fails fast, `ticker-state` ENODEV, status
      responsive, retry preserves tick1; stop uses the gate. Clock/animation
      dependencies, full suite `all tests passed`, and QEMU pass. ARM links
      libc only. Project diagnostic probe. On-TV CPU-only follow-up staged
      under `/buffer/oa-sched-verify` with hash-verified ARM host+ticker/clock/
      anim: tick1 observed, each stop/reload failed fast with ENODEV then
      completed after 0.7 s, tick2 restored, unload clean, staging removed,
      stock Aurora PID1781 unchanged; no fb/DVB/alsa/GPIO paths. Not
      display/HAL/scanout/audio/tuner acceptance. TICKER-QUIESCE.

- [x] Asynchronous gate for `mod_ui_console`: clear `active` and unsubscribe
      KEY/TICK synchronously, snapshot/detach the heap selection state, then
      free it from one `sched` slice after `ui_console.cleanup_delay_ms`.
      `save()` uses the snapshot after detach; start refuses while pending and
      restores the selection; stop/destroy cancel/free. New `console-state`
      probe and diagnostic `emit-key`. Control test: selection 0→1, reload
      fails fast, status responsive, a key during the gate is ignored, retry
      preserves selection 1 and a new key gives 2; stop uses the gate. Focused
      ASan/UBSan clean, full suite `all tests passed`, and QEMU pass. ARM links
      libc only. On-TV CPU-only `/buffer/oa-console-verify` run with
      hash-verified host+module reproduced the same sequence (`restored 1
      byte`), staging removed, stock Aurora PID1781 unchanged; no
      fb/DVB/alsa/GPIO paths. Text UI only; not scanout or real input.
      UI-CONSOLE-QUIESCE.

- [x] Asynchronous gate for `mod_ui_omarchy`: clear `active`, remove the rebind
      timer/subscriptions, unregister the engine skin and release engine/Lua
      synchronously; snapshot/detach the heap colors/layout/screen state, then
      free it from one `sched` slice after `ui_omarchy.cleanup_delay_ms`.
      `save()` uses the snapshot; the 72-byte magic/versioned blob is restored
      on reload; start refuses while pending. New `omarchy-state` probe.
      Control test with nav_display/lua/ui_engine: script theme 9/8/7,
      layout 0,0,40,100, screen 1920x1080; reload fails fast, engine reports
      `active -`, status responsive, retry restores 72 bytes and re-registers,
      stop uses the gate. Focused ASan/UBSan clean, skin/Lua suites pass, full
      suite `all tests passed`, QEMU pass. ARM base glibc only. On-TV CPU-only
      `/buffer/oa-omarchy-verify` run with hash-verified host+modules
      reproduced it (`restored 72 bytes`), staging removed, stock Aurora
      PID1781 unchanged; no fb/DVB/alsa/GPIO paths. Recorder display only; not
      scanout or real input. UI-OMARCHY-QUIESCE.

- [x] Audit the last synchronous modules, `mod_plat` and `mod_power`: neither
      owns a worker, queued callback, heap cleanup or reversible device close,
      so neither gets a fake async gate. `plat` stop unpublishes `plat`; its
      vendor HAL bring-up is process-global with no recovered de-init and
      `NDAL_VideoExt_OpenDevice(0,10)` installs a global fd, so real-HAL reload
      is not a reset. `power` stop unpublishes `power` and drops the
      `host.sleep` refcount; its sysfs fd exists only inside the final entry
      callback. New `check-plat-power-sync-stop` drives the real host modules
      (OA_NO_VENDOR plat, `power.enable=0`): reload 1 ms / stop 0 ms with the
      service unpublished; focused ASan/UBSan clean, full suite
      `all tests passed`, QEMU pass. No HAL/standby hardware and no TV run.
      33/35 deferred/scheduler gates + 2 audited synchronous stops = all 35
      descriptors decided. PLAT-POWER-QUIESCE.

- [x] Vendor cold-start order for `mod_plat`: firmware `ICE_HardwareInit`
      @0x007551a0 polls `access("/tmp/ntkdriver_completed", R_OK)` every 50 ms
      (`0x00755498..0x007554ae`, literal `0x00755540`→`0x012930c5`), logs the
      elapsed time and only then calls `PLAT_DRV_INI_Init` @0x0089b6e4
      (`0x0075564c`). `plat` now waits before the audio prelude:
      `plat.driver_wait` (unset auto-detects `/dev/ntkled`), `driver_marker`,
      `driver_timeout_ms` (60 s, 0 = forever); timeout sets `STAGE_DRIVER`,
      refuses HAL and allows retry. New `check-plat-startup-wait`: pre-existing
      marker 1 ms, arriving marker 151 ms, timeout refusal 401 ms then retry;
      focused ASan/UBSan clean, full suite `all tests passed`, QEMU pass. Cold
      config opts in. View-only TV confirmed marker readable `1`, `/dev/ntkled`,
      five `/proc/fb` OSD planes and fb1 OSD_NORMAL2 1920×2160 stride 7680 with
      stock PIDs unchanged. No cold boot/HAL/display write; not scanout
      acceptance. PLAT-STARTUP-WAIT.

- [x] Firmware-derived display resume ordering: vendor `idtvResume` @0x00752b88
      re-enables the OSD plane and restores the STR boot-logo frame
      (`0x752ca6/aa/b0`, repeated `0x752cfc/d00/d06`); kernel PM replays OSD
      state and first-pan recovery shows the plane/region. `mod_display` now
      keeps the last RGBA scene across suspend, reuses it on reopen, saves the
      resume snapshot before re-applying the region, and recommits the scene
      through `FBIOPAN_DISPLAY` (`resume: first frame committed`). Failure
      restores/closes but keeps the scene for retry; a suspended instance frees
      only heap synchronously so reload stays immediate. New fake-driver test
      clears its map on every open and proves re-render, region, commit and
      retry; existing display/lifecycle/fake-fb socket suites pass, ASan/UBSan
      clean, full suite `all tests passed`, QEMU pass. On-TV under stock: magenta
      first frame read back at start and on 4 consecutive samples across resume
      in the panned frame (yoffset 1080); stock repaints over it, so panel
      scanout/cold acceptance remains open. Staging removed, stock PIDs
      1711/1781 unchanged. DISPLAY-RESUME-FRAME. Follow-up: opt-in
      `display.verify_scanout` re-reads yoffset/region/MAU after start/resume
      first frames and rejects a mismatch; the fake one-shot wrong-yoffset test
      proves restore/close and retry. On-TV `/buffer/oa-display-verify` logged
      `scanout verified` at start and resume (yoffset 0, region 1920x1080 at
      0,0, mau 0). This is driver state, not panel scanout; stable acceptance
      needs authorized controlled ownership plus independent observation.

- [x] Cold-start stage audit: firmware `ICE_HardwareInit` calls
      `PLAT_DRV_INI_Init` then `DEV_INI_Init` @0x00853a58 (`0x75564c/50`);
      recovered its 11-stage order (PCB layout type, GPIO, source, panel, NIM,
      IIC, amp, SPI init/open, NIM-DTV, MTD) and mapped each to the modular
      owner. `plat` must not call it wholesale: that would duplicate
      panel/source/tuner/audio ownership and run unrecovered device code under
      stock. SPI/MTD/NIM-DTV and amp/panel power stages remain unimplemented.
      New `plat-state` control probe reports `plat ready N errors 0xM`;
      `check-plat-startup-wait` asserts ready/0x0 after start and ENODEV after
      stop; full suite `all tests passed`. Offline host stubs only; no cold
      boot or hardware acceptance. COLD-START-STAGE-AUDIT.

- [x] SPI/MTD/NIM-DTV stage recovery: `DEV_MTD_Init` @0x0085419c only registers
      a debug menu, so no cold-start MTD owner is needed. `DEV_SPI_Open`
      @0x00858d0c prefers `/conf/spi.bin` (`rb+`/`rb`) and otherwise
      `/dev/ntkspi` `O_RDWR`; reads use `fseek/fread` or
      `ioctl(0x8018531e)` with a 24-byte `{offset,size,buf,0,0,0}` request and
      return 1/0. Added `common/spi_dev.c` and an opt-in read-only `spi` v1
      module (`spi.enable=0`, not autoloaded). Fake file/device tests wrap
      open/close/ioctl and cover bounds, NULL/zero-length and ioctl failure;
      focused ASan/UBSan clean, full suite `all tests passed`, QEMU pass; ARM
      module ELF32 and libc-only. `DEV_NIM_DTV_Init` @0x00861678 creates a DTV
      task and remains hardware/tuner work; SPI writes/control and real
      `/dev/ntkspi` are untested. 33/36 descriptors have deferred/scheduler
      gates; `plat`/`power`/`spi` are audited synchronous. SPI-MTD-NIM-STAGE.

- [x] Amp-init I2S MCLK request: `DEV_AMP_Init` @0x0084f4ac zeroes a 56-byte
      parameter block, stores `0x100` and calls `Hal_Audio_Ctrl(0x3a)` before
      creating the amp monitor task; the scalar return is ignored. `mod_audio`
      now optionally sends the same `0x3a/0x100` once before volume/mute
      (`audio.i2s_mclk=1`, default off; failure logs and continues).
      Fake-HAL test asserts exactly one call, zeroed remaining params and a
      following `0x33/50`; focused ASan/UBSan clean, full suite
      `all tests passed`, QEMU pass. No vendor HAL, I2S clock, amp task or
      audible acceptance; default off. AUDIO-I2S-MCLK.

- [ ] Generic asynchronous module lifecycle: extend the queued/nonblocking
      stop/reload model beyond
      `ir`/`i18n`/`settings`/`ui_vendor`/`ui_engine`/`display`/`source`/`ui_menu`/`audio`/`cec`/`audio_mixer`/`tuner`/`epg`/`pvr`/`channels`/`service_db`/`image_decode`/`ui_theme`/`ui_list`/`ui_navigation`/`ui_profile`/`audio_curve`/`audio_amp_profile`/`panel`/`ui_diag`/`ui_anim`/`ui_clock`/`ticker`/`ui_console`/`ui_omarchy`,
      with a completion query and bounded client behavior. Do not call
      `dlclose`/module callbacks from an unowned worker. `display_fb`, `ir`,
      `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
      `ui_menu`, `audio`, `cec`, `audio_mixer`, `tuner`, `epg`, `pvr`,
      `channels`, `service_db`, `image_decode`, `ui_theme`, `ui_list`,
      `ui_navigation`, `ui_profile`, `audio_curve`, `audio_amp_profile`,
      `panel`, `ui_diag`, `ui_anim`, `ui_clock`, `ticker`, `ui_console` and
      `ui_omarchy` are the gate models. `ui_theme`, `channels`, `ui_diag`,
      `ui_anim`, `ui_clock`, `ticker`, `ui_console` and `ui_omarchy` use the
      host `sched`; the others use workers. `lua` unpublishes
      `ui.lua` and rejects hook/reload while a fetch child is unreaped.
      `graphics` and `scene_display` have their own non-pthread gates. `plat`,
      `power` and `spi` have no deferable cleanup and keep synchronous stop by
      design; the plat/power control-socket test asserts immediate stop/reload
      and service invalidation (PLAT-POWER-QUIESCE), and `spi.close` is a
      synchronous fclose/close (SPI-MTD-NIM-STAGE). Host-level
      load/start/stop/unload/reload batches now bound a stack, but each call is
      still synchronous; a completion query and truly per-call asynchronous
      lifecycle remain.

- [x] Firmware-shaped partial rail animation: added `display.patch` v1
      replacement RGBA fill, dirty-rect conversion and page-rounded sync in
      display_fb, and a `paint_rail_hl` path that patches the portal rail
      strip `0,211 209x670` and redraws only the five rail slots/highlight.
      Host patch + rail tests pass. TV: 10 highlight frames at scene3-5 ms/
      pan5-8 ms, ~50 ms cadence (~450 ms total) versus 617 ms per full
      repaint; key full paint scene52 ms/pan11 ms. RAIL-PARTIAL. Static and
      full-paint pixels are unchanged.

- [x] Partial GPU scene uploads: OGS1 FRAME carries x/y/w/h, sealed images
      may be w*h*4, renderer ABI2 uploads a subrect, display.gpu v2 exposes
      `frame_rect`, and scene_display tracks dirty rects. Host broker/patch/UI
      tests pass. TV: initial full upload 73 ms; ten rail frames uploaded only
      `0,211 209x670`, ~20 ms request-to-done, ~50 ms cadence. GPU-PARTIAL.
      Exact 30 ms cadence, CPU canvas reduction, graceful teardown and cold
      startup remain separate.

- [x] Vendor animation bootstrap: recovered `DemoApp::checkUIDesign` →
      `SVGDesignChecker::checkUsabilityStyle` → `DesignStyle.svg` (resource79)
      rectangle `animation` → `UIFramework/UIAnimationController::
      setAnimationEnable`. This TV contains `animation -480 -270 1920 1080`,
      so stock animations are enabled. `ui_vendor` applies the policy from
      `ui.theme` with `ui.anim_enable` override; synthetic marker/no-marker
      host tests pass. ANIM-BOOTSTRAP.

- [x] Sub-launcher grid and 300 ms animation geometry: decoded
      `SVGGrid::renderBackground` counts `(area-offset-(button-pitch))/pitch`,
      the +0x1e4/+0x1e8 pitches and +0x1f4/+0x1fc offsets, and
      `SVGButton::animRectDecode`: elapsed_ms/(duration*1000), linear scale/
      glyph from→to, top-left button-window anchor, named #scalerect size.
      Added pure `launcher_anim` helpers and tests for sublauncher 240x140/
      1→1.35 and glyph 0→-42/-24. SUBLAUNCHER-GRID-ANIM.

- [x] Stock Apps-grid pulse capture: read-only fb1 sampler over
      Markt/Netflix (700x280) captured one redraw at t=53 ms and 16 identical
      post frames through 621 ms. The full-screen Apps grid focus is an
      instant redraw, not a 300 ms scale/glyph pulse. Corrected BGRA→RGB PNGs
      and hashes in APPS-PULSE; do not apply the sublauncher records to this
      screen.

- [x] Firmware sub-launcher menu tree and JSON model: decoded
      `LauncherDialog` → `SubLauncherDialog` (750 ms timer/Down), the
      launcher-type dispatch, picture/sound/system/channel/AI item and value
      lists, the `DialogSubLauncher.svg` listBox 189,255 1774x222, 240x140
      items and the 300 ms 1→1.35 + glyph −42/−24 animation. Added the
      bounded JSON loader, action strings, `menus/sub_launcher.json` and the
      reloadable `ui.menu` v1 service; host tree/action/service tests pass,
      ARM GLIBC 2.4. SUBLAUNCHER-MENU-TREE.

- [x] JSON menu action dispatch to vendor screens: `oa_portal_iface` v2
      appends `menu_action`; `open:portal|sources|settings|settings_list|
      apps|channels` switch the vendor skin. `tests/ui_menu_action.c` loads a
      JSON entry with `open:settings`, dispatches it through the portal
      service, asserts screen 3, and verifies v1 lookup/v3 refusal. Other
      verbs remain `ENOTSUP`.

- [x] QEMU offline UI oracle and JSON sub-launcher row: `sim-display` (ARM
      GLIBC 2.4) writes 1920x1080 frames; `qemu-ui-layout.sh` runs the real
      ARM ui_engine/ui_menu/ui_vendor stack. P0/03 `SCREEN_SETTINGS` band is
      AE=0 vs `settings_top.rgba`. `SCREEN_SUBLAUNCHER` maps the JSON picture
      branch to captured cards at checked layout 201/451/701,276 235x130,
      pitch 250, item cell 199,274 240x140; unfocused row AE=0 and 300 ms
      card2 bounds 235x130 → 317x175 at p=0/0.5/1.
      `qemu-sub-launcher-check.sh` repeats it. QEMU-UI-LAYOUT.

- [x] Sub-launcher live text/runtime: font3 labels + font12 values on clean
      captured backgrounds; Dutch live text within4 px of p0/03 (most≤2 px).
      `mod_i18n` resolves all41 langprofile locales with current→eng→key
      fallback; U+202C-style controls are stripped. Runtime Up/Down uses
      `ui.clock` + a10 ms timer for300 ms; QEMU `key 20` through the real IR
      parser drives card0 bottom405→450. `qemu-sub-live-check.sh` repeats it.
      LIVE-SUBLAUNCHER-I18N.

- [ ] All-language shaping and fit: the vendor row `text1` is 240x54; the row
      sets `setElipses(false)` for labels (clip) and `setElipses(true)` for
      values (elide), with no shrink-to-fit. At 240 px only Bulgarian
      ENERGY_SAVING (253) and Ukrainian PICTURE_ZOOM (264) clip; Lithuanian
      (239) fits. Profile Hebrew is stored in visual order and is drawn as
      stored; Arabic/Persian are converted from stored logical order by
      `common/rtl_order.c` (QEMU ara/per verified); RTL-resolved paired
      punctuation is mirrored per UAX #9 L4 (RTL-BRACKET-MIRROR). Mixed
      Hebrew/Latin BiDi and elision/clipping at the 240 px row remain.
      Capture focused vendor cards and multilingual oracle frames before
      claiming full visual parity.

- [x] RTL paired-punctuation mirroring: the Arabic/Persian visual-order helper
      now applies UAX #9 L4 mirrored glyphs to RTL-resolved paired punctuation
      (`() [] {} <> «» ‹› ⁅⁆ ⁽⁾ ₍₎ ≤≥ ≦≧`); LTR/no-Arabic fallbacks and
      stored-visual Hebrew are unchanged. New cases `(ب)→(ب)`,
      `ABC (ب)→(ب) ABC`, `[(ب)]`, `(abc)` plus the real 41-locale corpus pass;
      focused ASan/UBSan clean, full suite `all tests passed`, QEMU live
      ara/per pass. Standard BiDi rule, not instruction-level vendor text
      recovery; mixed Hebrew/Latin BiDi and visible capture remain.
      RTL-BRACKET-MIRROR.

- [x] `setting:`/`set:` dispatch, MODE and the picture store:
      `DemoApp+0x318` is `NovoPictureSettings*` (vptr
      `_ZTV19NovoPictureSettings`+8); the per-source config is 0x240 bytes
      (mode+0x04, zoom +0x08 normally / +0x0c when format-0 full mode applies,
      energy+0x44+mode*0x70+device3d*0x38). Fields are vendor SettingValue
      ids, not JSON positions: MODE0..4; zoom AUTO1/16:9=5/SUBTITLE6/14:9=3/
      14:9zoom4/4:3=2/21:9=11/CINEMA8/FULL9; energy CUSTOM5/SCREEN_OFF6.
      Dump source1 is NATURAL/AUTO/MAXIMUM; configs3/4/14 (zoom5) match p0/03.
      The JSON carries the ids; mod_settings publishes `picture.settings`
      (20×0x240, whole-blob update, EROFS read-only, picture.full_mode);
      ui_vendor stores/reads ids and derives the index from m_current_source
      (external index+1, 0=Tuner); the ui.state_file shadow is gone. Host test
      checks IDs, reload and the +0x08/+0x0c rule; PICTURE-OBJECT executes the
      shipped accessors offline (20 configs/80 zoom/200 energy/242 setters).
      QEMU: source3 gives NATURAL/MAXIMUM/16:9 (132/129/54);
      Ondertitels155/DB id6, survives restart; focus450; Down,Down,Right
      AUTO176/energy4; set:MODE=CINEMA writes mode2 and applies energy0
      (Cinema/Uit); m_current_source5 selects config5 and renders
      DYNAMIC/OFF151/36. SUBLAUNCHER-VALUES and PICTURE-OBJECT.

- [x] Bronlijst source selection writes the picture index, calls the VIN path
      and re-syncs: the ICE descriptor table at0x0175de8c gives CVBS2,
      HDMI1-3 4-6, YPbPr8, PC9, WiDi11, APM12; m_current_source=index+1 (TV0).
      ui_vendor's `SRC_PICTURE_INDEX={0,3,5,6,7,9,10,12,13}` and
      `SRC_SOURCE_TYPE` (CVBS15, HDMI1/2/3 17/18/19, YPbPr20, VGA16, TV0).
      Select calls source.select_type first; only success writes
      m_current_source and calls picture_sync_index.
      Host check-source-fake-ipc asserts VIN ids3/3/66/3/2/1, state and
      source.enable; QEMU HDMI1 row2 logs VIN id3, writes5 and shows
      DYNAMIC/OFF151/36 in-process. SOURCE-VIN.

- [ ] Live picture/VIN backend details: the service uses the serialized
      settingsDB blob; the shipped accessors are executed offline
      (PICTURE-OBJECT), not inside a running Aurora `UserPreferences`/PQ
      process. HDR energy variants (+0x446c/+0xa10/+0xa48), the twin-source
      `setPictureSettingConfiguration` path (index != 0) and
      `NovoPictureSettings::setZoomMode` need the live EASI/InputManager graph.
      `picture.full_mode` is an explicit replacement input, not derived from
      the panel/decode state. The source screen calls
      `mod_source`/`Hal_Vin_SelectInput` behind `source.enable`; the real
      wrappers are executed against a fake `/dev/video20`, but the real kernel
      driver/V4L2/HDMI path and TV hardware switching are still unaccepted.
      Writing the TV's DB requires `settings.readonly=0`. Do not apply the row to the
      category rail or an invented portal card row.

- [ ] Exact animation cadence: the rail is ~50 ms/frame (CPU and GPU) versus
      the vendor 30 ms APNG/10 ms controller cadence. Tighten the compositor
      pipeline and measure frame-to-frame timing on the TV. GPU-PARTIAL,
      RAIL-PARTIAL and UI-ANIMATIONS are the starting evidence.

- [x] Portal repaint/animation cost: the old full 1920x1080 repaint measured a
      median 617 ms on the TV, stretching the 11-frame/30 ms highlight to
      ~6.8 s. The firmware-shaped rail patch now measures 3-5 ms scene +
      5-8 ms pan per highlight frame (~50 ms cadence, ~450 ms total); a
      key-driven full paint is 52 ms scene + 11 ms pan. TAKEOVER-FULL and
      RAIL-PARTIAL record before/after. Exact 30 ms and GPU partial upload
      remain separate items.

- [ ] Runtime EASI/testtool: testtool TCP1986 remains disabled. The 2026-11-05
      full-control authorization covered temporary staged modules/display, not
      enabling testtool or persistent replacement. Reopen with an explicit
      watchdog-safe instruction.

- [ ] Stock apps-grid pixel parity: needs a pixel-correct full-screen apps-grid
      oracle. A 2026-11-06 view-only wake still showed stock Aurora's
      "Geen signaal" banner (PIDs 2204/1781, appmgr 1579, no openaurora),
      not the apps grid. No HDMI capture device
      is attached and no keys were sent. Owner must leave the TV on the stock
      apps grid for a read-only fb1 capture, authorize navigation keys, or
      provide an HDMI capture. Then run `tools/tvshot.py` and
      `make -C openaurora compare-stock-apps-grid` against the firmware-grounded
      region manifest.

- [ ] Independent startup: boot into openaurora without starting vendor Aurora.
      Temporary takeovers showed scanout, input, fallback and resume, then stock
      was restored. Cold boot, MCU authority and a clean handback are not verified.
      The 2026-11-06 controlled run held a stable first frame with stock stopped
      and restored by reboot (CONTROLLED-SCANOUT), but fb1 was already
      vendor-initialized; it is not a cold independent startup.
      Owner authorized an idle hardware/CEC/audio test window on November 1; see AGENTS.md.
- [ ] Reloadable modules: state, service holds, callback ownership and dependencies
      behave correctly through start/stop/reload/suspend/resume and failures.
- [ ] Module consolidation audit: 36 descriptors is a large reload surface, but
      do not merge blindly. First build a service/dependency + reload-blast-radius
      matrix (who publishes what, who holds it, what reload resets, what hardware
      or worker cleanup is involved). Preserve ABI 3, published service
      names/versions, hold/owner semantics, quiesce, save/restore, independent
      reload and config autoload.
      Candidate tiers:
      * data-only: `audio_curve` + `audio_amp_profile` -> one `audio_data`
        module (separate services, no hardware close); fold `spi` into
        `tuner`/`plat` once it has a real consumer; replace `ui_console` with a
        test fixture or an `ui_omarchy` dev mode.
      * UI state: `ui_list` + `ui_navigation` + `ui_profile` -> one `ui_state`
        module if the service names/versions and independent reload tests can be
        preserved; keep `ui_menu` separate unless its document reload is proven
        equivalent.
      * time: evaluate `ticker` + `ui_clock` -> one `time` descriptor. They are
        provider/consumer today, so only merge if start/suspend/reload ordering,
        clock publication and failure isolation remain equivalent.
      * do not merge without new evidence: `plat`, `panel`, `power`, `audio`,
        `audio_mixer`, `cec`, `tuner`, `epg`, `pvr`, `channels`, `service_db`,
        `display`, `scene_display`, `graphics`, `ir`, `source`, `settings`,
        `ui_engine`, `ui_vendor`. Their hardware, process, device or skin
        ownership boundaries and reload blast radius are intentional.
      Acceptance: same host/fake/ARM tests, module matrix IPC, quiesce/reload
      suites, cold-bundle manifest; measure DSO count, build time, bundle size
      and reload impact. Any service or ABI change must be explicit, not a
      silent merge.
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
- [x] Make queued animation timer-allocation failure observable to the initiating
      consumer. `ui.anim` v3 `fault()` stays 0 until the owned kick runs, then
      returns EAGAIN when the timer table is full and clears after a later kick
      arms the 10 ms timer. The log line remains. `check-ui-anim-review` covers
      the full table and the retry. No visual acceptance claim.
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
      The userspace resume path now retains/recommits the scene and re-applies
      the window (DISPLAY-RESUME-FRAME, offline + on-TV device readback); the
      controlled first-frame scanout/full-screen geometry is owner-confirmed
      (CONTROLLED-SCANOUT). Kernel PM interaction, suspend/resume on hardware
      and cold ordering remain unverified.
- [ ] Finish alternate-MAU/separate-buffer/zero-buffer and later resize/surface
      switching behavior. Do not infer actual mapping size from framebuffer metadata.
- [x] Verify independent scanout and full-screen geometry on panel at a future
      authorized takeover. Present success, pixel readback and oa.ready are insufficient.
      Controlled first frame on 2026-11-06: `display.test=2` painted eight
      full-screen bars, two captures 5 s apart matched, and the owner confirmed
      "Yes, I see the bars" in the expected left-to-right order
      (CONTROLLED-SCANOUT). Vendor UI pixel parity and cold startup remain open.

- [x] Controlled-ownership first-frame stability: under the owner's 2026-11-06
      full-access authorization, the watchdog was disabled, stock Aurora 1781
      stopped, and a hash-verified staged `display_fb` started on the
      vendor-initialized fb1 with snapshot/self-test/`verify_scanout`. The driver
      reported `yoffset 0 region 1920x1080 at 0,0 mau 0`; three captures 2 s
      apart and three raw active-frame reads 1 s apart were byte-identical
      (`656b0caf…`), with the magenta band in the active frame and no stock
      repaint. Stop restored fb1 (band gone, region/pan/MAU unchanged), staging
      was removed, and `wdtctl set 10` rebooted; stock returned (appmgr 1582,
      aurora 1784, fb1 1920x2160, no openaurora). A second run with
      `display.test=2` (module `5422a86c…`) painted eight full-screen bars;
      two captures 5 s apart matched (`a29ccecc…`) and the owner confirmed
      "Yes, I see the bars" in the expected order. This is confirmed controlled
      panel scanout/full-screen geometry, not cold startup, vendor UI pixel
      parity or MCU authority. Captures under
      `artifacts/analysis/controlled-scanout-2026-11-06/`. CONTROLLED-SCANOUT.

- [x] Offline panel backlight curve and NIM-DTV gate: recovered
      `DEV_PANEL_Init` breakpoints 40/60/80/100 and scale 1000,
      `SetPWMBacklightLimit` minimum `(1000*pct+50)/100`, and
      `SetPWMBacklightLv` mapping/table interpolation. Added
      `common/panel_curve.c` plus opt-in `panel.curve`/`panel.min_percent`/
      `panel.duty_max` (default linear/4095 path unchanged); unit/fake-HAL
      vectors cover identity, min20%, p4=50 bend, percent→duty at 4095 and a
      1000 calibration, clamping and degenerate tables.
      Recovered `DEV_NIM_DTV_Init` idempotent guard, model 0..3/type-4 active
      codes, mutex and `CreateTask(entry 0x008601ad, stack 0x1000, priority
      14)`, Term `0xff`. Ghidra decompile of the raw Thumb task entry gives a
      one-shot init: module buffer 9, NIM init, conditional HAL demod/connect,
      then `DEV_IIC_Read/Write(0,0,0x80,1,code,1,&byte)` where 0x15 clears
      bit3, 0x11 sets bit3 and 0x40 sets bit1 (write result unchecked), then
      active `0xff` + signal + unlock. Implemented as an ordered fakeable
      state machine with read/write callbacks and failure tests. Added
      `common/nim_dtv_hal.c`: packs `{bus 0, reg 0x80}`, the code address and
      one byte, calls `Hal_Iic_Read/Write` ops and inverts 0=success to the
      DEV 1=success; the state machine runs end-to-end over fake `Hal_Iic_*`
      (result bytes 0x67/0x6b/0x9a). `PLAT_DRV_GetModuleBufInfo` is a literal
      `return 0` stub and is reproduced/tested. `HAL_Demod_User_Initialize`
      requires the SAL tuner/LNB subsystem: HAL_TUNER_Init resolves four IIC
      bus IDs via DEV_IIC_GetDescription, SAL_DEMOD_TUN_Initialize builds
      SI2151/MxL661 + AV2018, tuner type comes from profile
      TunerTypes/TunerType (8/0xd; EFSL value hashed, live DT only an unbound
      tuner I2C node), and SAL_LNB_Initialize calls RT5047; so
      `mod_tuner` is intentionally not wired
      rather than faking tuner init or touching IIC early.
      Focused ASan/UBSan clean, full suite `all tests passed`, ARM mod_panel
      builds. No panel/tuner hardware; curve default off.
      PANEL-CURVE-NIM-DTV.
- [ ] EFSL/SAL demod-tuner backend: the EFSL container and selection are decoded
      (EFSL-TUNER-IDENTITY): ROMProfile `initialize/getUniqueID/findValue/
      readValue` layout, 20-byte hashed entries, `0x0128570c` =
      `__aeabi_uidivmod`; hwprofile `TunerTypes/TunerType = "Si2157"` and the
      vendor compares only `"Si2151"`, so `DEV_INI_Init` layout14 selects
      SAL0xd/NdNim0xc. `common/efsl_profile.c` + `common/tuner_identity.c` pass
      host, MD5-pinned corpus, ASan/UBSan, full suite and ARM/qemu. Remaining
      hardware backend: Done so far: `SAL_TUNER_MxL661_Init`'s six MxLWare601
      commands (41-write/5-read order, `MxL_OverwriteDefaults`, RMW masks,
      stale-`cfg[3]` IF-out branch) in `common/mxl661.c`, and
      `HAL_TUNER_Init` bus IDs plus `HAL_TUNER_ReadBase/WriteBase` in
      `common/tuner_hal.c`; host, focused ASan/UBSan, full suite and ARM/qemu
      pass (TUNER-MXL661). Also done (TUNER-BRINGUP): AV2018 `GetTunerResponse`
      = return1, `HAL_Demod_Init(0)`, `HAL_dvif_open`,
      `HAL_DEMOD_EXT_DVIF_SetTunerType` (`0xd->0xc`), the 24-byte Hal IIC
      context, the MxL661 DEV adapter (0xfb pointer), the ordered sequence
      (`common/tuner_sequence.c`) and the default-off `tuner.hw_init=1` gate in
      `mod_tuner` (host refuses with -ENOTSUP; ARM `mod_tuner.so` builds with
      libNDAL3). LNB_EN/RT5047 (descriptor ioctl + `Hal_Gpio_Init/Write`) and
      `_dev_nim_dtv_InitNimFunc`/`Connect2HAL` equivalents (0x29-word state
      clear, typed HAL table) are implemented behind the gate. The NIM tuning
      subset (path map, open/close, monitor/disconnect, check_lock, strength,
      quality) is implemented and installed in the module's NIM vtable.
      `set_channel`/`get_channel_status` need `SAL_DEMOD_TUN_ProgramTuner`
      (MxL661 RF/IF) and the 0x180-byte status mapping and return -ENOTSUP.
      The MxL661 DVB-T/DVB-C case 9/0xd/0xb RF/IF sequence and the typed
      SetChannel/GetChannelStatus mapping are implemented
      (`common/mxl661_rf.c`, `common/nim_tune.c`); a real `tuner.backend=hal`
      NIM backend (`common/nim_hal.c`, requires `tuner.hw_init=1`, terrestrial
      only) wires start/monitor/signal/stop to the ops. The raw 0x5c channel
      record is parsed into the typed channel (offsets +0/+4/+8/+0x10/+0x1c/
      +0x58, SAL standard map, path-1 override/40000 rule); the verified 0x180
      lock/strength/quality fields are parsed, and the 0x180 `tbb` dispatch
      (types 1/2,7/3/4,5/6/8) with case targets is recovered; every output
      map is implemented (types 1/2/3/6/7/8, types 4/5 return 0 with no
      output), with deterministic zeroing of vendor-stack slots and the
      type 2/7 out[14] preservation. The earlier type 6/8 mapping was
      corrected. The analog NTSC/PAL/SECAM case 9/0xd/0xb tails are
      implemented (`common/mxl661_atv.c`) using the vendor tables loaded at
      runtime from the TV's own aurora.elf with a CRC32 check; no vendor
      table is embedded or distributed. Synthetic `check-mxl661-atv` and an
      explicit private `check-mxl661-atv-corpus` pass, including ASan and
      ARM/qemu. The analog tuning path is wired into the NIM HAL backend
      (`oa_nim_hal_set_atv` -> `program_atv` -> `oa_mxl661_tune_analog`), and
      monitor/signal use the recovered SAL_TUNER_MxL661_GetATVStatus
      0x2b/0x2c lock decode (`oa_mxl661_atv_status_lock`, fake-tested).
      Strength/quality are not part of that SAL contract and report 0.
      Analog scan stepping is wired (`tuner.analog_scan` with start/stop/step/
      dwell; `check-tuner-analog-scan` over the stub). Still open: the case-9
      per-address calibration and real-HAL scan validation.
      SI2151 is the full Silicon Labs L1/L2/L3 `PowerUpWithPatch` + property
      driver, not this profile's path (hwprofile selects SAL 0xd/MxL661).
      Hardware acceptance awaits the scheduled owner takeover window and has
      not run. Do not touch IIC/GPIO while stock owns the devices.

- [x] Analog MxL661 NTSC/PAL/SECAM RF/IF: case 9 property/register tables,
      case 0xd pre-tune/LUT/divider tails and the shared 0xb lock, with the
      vendor tables read at runtime from the TV's own aurora.elf (CRC32
      `0x93a05fff`) instead of embedded. Synthetic `check-mxl661-atv` and the
      explicit private `check-mxl661-atv-corpus` pass, as do focused
      ASan/UBSan and ARM/qemu. Limits: analog strength/quality availability,
      per-address case-9 calibration and hardware acceptance remain; scan
      stepping is tested offline over the stub.

- [x] Analog scan stepping: network 4 maps to the terrestrial NIM path;
      `tuner.analog_scan` steps `analog_start_khz..stop_khz` by `step_khz`
      with `dwell` status polls, counts a locked channel once per frequency,
      reports range progress and supports cancel. `tests/tuner_analog_scan.cjs`
      over the stub visits five 1 MHz channels with progress 25/50/75/100 and
      a clean cancel. Limits: the HAL backend repeats start per frequency and
      no real hardware has run.
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
      TAKEOVER-IR-2026-11-05 resolved the physical path: stock's BLAST common fd
      is **222 = `/dev/video28`**, RC type **1** means protocol **4**, and
      `NDAL_VideoExt_BLAST_RX` returns **0 when a key is present** (nonzero is
      no-key) and needs a `v4l2_open` fd. A clean IR-only takeover decoded the
      physical Right/Down/Up/Left keys as profile RC **22/19/20/21**; `0x800` is
      the RC5 toggle and `flag` is press/release. `mod_ir` now defaults to
      BLAST/video28 and emits the RC code directly, with ttyS2 as fallback.
      IR-IPC-2026-11-05 adds `tests/ir_ipc.cjs`: real Unix-socket
      load/start, `input:v1`, key navigation, stop→ENODEV, start, `reload ir`,
      live `reload ui_vendor`, stop/start ui_vendor, unload/load ir and clean
      exit with plat/recorder fixtures. `make -j24 check-ir-ipc`,
      `check-ui-vendor-ipc` and `check-ui-nav` pass. TAKEOVER-FULL-2026-11-05
      accepted the full portal UI takeover with the physical remote and both
      TV-side reloads. Remaining: repeat/long-press and complete key mapping,
      and clean vendor-library reload on the TV. OA-CLIENT-ASYNC keeps the
      UART closed when BLAST succeeds unless `ir.device` is explicit, avoiding
      the observed dual-open reload stall in the normal path. The MCU
      `_HW_MCU_RcInit`/STBC RWDB path is not required for this remote; do not
      call STBC cold without the ownership/recovery plan.
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
      proof that an ioctl or brightness change happened. The curve/minimum
      mapping and opt-in `panel.curve` are recovered and fake-HAL tested
      (PANEL-CURVE-NIM-DTV); exclusive hardware, the final duty scale and resume
      remain open.
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
- [x] Audio curve control queries: `audio-curve <volume>` returns the
      published 16-bit AQ entry and `audio-curve-dump` returns all 101 entries
      over the real socket. Module/real-socket tests cover values 110/200,
      reload 210, unsigned clamp and the dump; missing provider/argument errors
      are bounded. AUDIO-CURVE-IPC. Still read-only data: no speaker/amp/ALSA
      write, mute/routing/balance, profile/default fallback or audible test.
- [x] Speaker level/balance clamp mapping: recovered
      `PLAT_VAUD_SetSpeakerVolumeBalance` 0x8d1c3c and added
      `oa_audio_speaker_map`/`audio-speaker <level> <balance>`: level clamps
      to100, balance to±50, HAL balance balance+50, equal left/right. Unit and
      real-socket cases pass; ARM core links. AUDIO-SPEAKER-MAP. No speaker-id
      validation, HAL/ALSA/amp write, cached-volume short-circuit or audible
      test.
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

- [x] Read-only vendor inventory: 389 ELF files in the extracted `/vendor` tree
      plus 72 outside it (461 total, 15 platform kernel modules; 41 .ko total) are hashed and dumped for SONAME/NEEDED, sections, symbol tables,
      dynamic symbols and strings; Ghidra functions/callgraphs/symbols/data
      cover aurora (89,220 functions, 485,969 resolved call edges) and 14
      library/kernel projects. Recovered the 0..207 GPIO function-name
      table, VPLTIO 45/45 call-site map and 58 immediate audio call sites (37 unique opcodes), with
      call-site maps for GPIO/IIC/DRM/NDAL/PWM/panel. This is static evidence,
      not hardware acceptance; per-subsystem table decoding remains. An
      independent pass adds 132/36 NDAL3/NDAL_EX ioctl immediates and module
      init/exit details; 9 of the 16 platform modules still lack Ghidra projects; grok-findings-2 now covers codec/tuner/demod/TSD/PQ/PreTcon/PCI/kteec/HDMI by objdump/relocations.
      VENDOR-INVENTORY-2026-11-07, grok-findings.md.
- [x] Shipped app-manager configs parsed: `nvt.app.ini`/`.debug.ini`/`.nfs.ini`
      (519 keys, 30 app rows). The stock appmgr uses the debug profile
      (dial/streaming apps disabled, `run_aurora.sh` idtv); retail enables them.
      App layers/states/properties, hotkeys f1/f2/f3 and YouTube repeat
      19000us x4 are in APP-MANAGER-CONFIG. Config only; no launch/IPC test.
- [x] Unstripped `libappmgr.so` inventory: 2,476 symbols, 693 exports, new
      read-only Ghidra project. Recovered the full 127-slot/119-key US keyboard
      layout at `keyboard_layout` 0x52f50 (Linux keycode + modifier flags + two
      label/codepoint subrecords, including DirectFB nav/F-key codes) as
      `keyboard-layout-us.tsv`, plus the HID/IR path (`HID_CommandIrInput`,
      `COM_SendKey`, `HID_NamKeyToDfbKey`, `HID_HandleWindowEvent`,
      `MGR_SendCommand`). Static only; `HID_LoadKeycodeExtMap` reads `NVT_KEYCODE_EXT_MAP`
      JSON, but the runtime `.ini` is not in the rootfs, and the table is not
      yet used by our UI. APP-MANAGER-KEYBOARD.
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
- [x] Runtime channel-order ingestion: added a `channels` module and parser
      for the recovered ACTIVECHANNELLIST XML (order-preserving, bounded,
      entity decoding, transactional replacement) with
      channel-ingest/clear/state/list CLI. `ui_vendor` now prefers a runtime
      snapshot with an amber "RUNTIME ACTIVE LIST - LCN/VISIBILITY
      UNAVAILABLE" marker and keeps the stored red provisional fallback.
      Unit, socket and recorder-UI tests pass. Host423 ok, verify45; ARM/qemu
      passes. CHANNEL-RUNTIME-ORDER. Limits: no live EASI feed (testtool still
      disabled by owner), no LCN/visibility fields in ACTIVECHANNELLIST and no
      stock channel-list geometry capture.

- [x] Vendor-grounded PVR module: added an `oa_pvr_backend` boundary shaped
      after `DvrController::startRecording/recordingQuery/stopRecording` and
      the SDK scheduler getters (integer recording id, index enumeration).
      `mod_pvr` publishes `pvr`; the offline `pvr-stub` persists up to 256 jobs
      atomically in `pvr.path`, validates requests, assigns monotonic ids,
      cancels only scheduled jobs and fails closed on corrupt files. Unit and
      real Unix-socket tests cover add/list/cancel/reload and cross-process
      persistence. Host421 ok, verify45; ARM mod_pvr and qemu test pass.
      PVR-MODULE. Limits: no active recording/timeshift/playback/storage/
      conflict/series engine, project file store (not vendor SQLite metadata),
      repeat days stored not expanded. No TV access.

- [x] PVR fixture conflict and recording states: publish `pvr` v2 with
      `record`/`finish` and `pvr-record`/`pvr-stop`. One scheduled/recording
      padded window per service; overlapping add returns EBUSY; state changes
      persist through the OAPVR1 store and reject repeats/unknown ids.
      Real-socket tests cover conflict add, record/stop, completed cancel,
      reload and cross-process states; ARM/qemu pvr_stub passes.
      PVR-CONFLICT-STATE. Limits: fixture policy only; no tuner-count/vendor
      conflict callback, media capture, timeshift, repeat-day expansion or TV.

- [ ] Vendor-backed PVR engine: replace the persistent stub with real
      stream/PMT capture, vendor/SQLite recording metadata, timeshift/
      playback, storage management and vendor conflict semantics once a live
      tuner/storage integration window is owner-approved.

- [x] Live CPU-only module acceptance on the TV: staged the ABI 3 ARM core,
      `mod_epg` and `mod_tuner` in /tmp, verified all hashes on-device before
      execution, and drove load/start/scan/reload/stop/unload over the real
      Unix socket. Tuner scan reached 12 services with lock/signal transitions
      and fed 12 EIT events into mod_epg; query metadata matched. Exact-PID
      SIGTERM, socket/staging removal and unchanged stock PIDs (1967 1785)
      were verified. LIVE-MODULE-ACCEPTANCE. Limits: CPU-only; no fb1/display/
      HAL, no device paths opened, no testtool, no vendor backend or cold
      startup. LIVE-MODULE-ACCEPTANCE.

- [x] Vendor-grounded tuner module: added an `oa_nim_ops` boundary mirroring
      the recovered `DEV_NIM_DTV_StartTune`/`MonitorTune`/signal calls, with an
      offline stub backend (path enum, 3/4 lock words, frequency, float
      BER/SNR, UNC sentinel). `mod_tuner` publishes `tuner`, maps DVB-T/C/S and
      IP (analog `ENOTSUP`), advances a 25% scan state machine with live
      signal/lock metrics, and pushes synthetic EIT sections into
      `epg.ingest` when present. Unit NIM test and real Unix-socket test cover
      scan/signal/cancel/reload plus EPG query results. Host419 ok, verify45;
      ARM mod_tuner/mod_epg build and qemu NIM run pass. TUNER-NIM-MODULE.
      Limits: stub only, no tuner hardware/transponder scan or demux/PID
      assembly, synthetic EIT/service data, project handle state machine, no
      vendor execution or live backend. No TV access.

- [x] EIT section parser + real EPG module: added a DVB EIT parser (table ids
      0x4e..0x6f, section/CRC validation, MJD/BCD time gate, short-event
      descriptor, actual/running/version metadata, invalid-time skips) feeding
      `eit_cache.c`. `mod_epg` publishes `epg`/`epg.ingest`, clears on
      start/reload and replaces matching event keys. Control adds
      `epg-ingest <hex>`, `epg-clear`, `epg-count`; a real Unix-socket test
      covers ingest, replacement, bad CRC/stale sections, clear, reload and
      stop/unload. Host417 ok, verify45; ARM build and qemu parser run pass.
      EPG-EIT-MODULE. Limits: complete pre-assembled sections only; no PID/EIT
      filters, section merging, CRID/first-showing, persistence or live tuner
      source. No TV access.

- [x] Persistent EIT schedule cache: opt-in `epg.path` loads/saves an atomic
      `OAEPG1` store on start/ingest/clear/stop; malformed records, missing
      header, directory path and read errors refuse module start. Added the
      project `oa_eit_cache_get_at` sorted accessor. Real-socket tests cover
      reload, fresh-process restart, clear and corrupt-file fail-closed;
      `eit_cache` passes host and qemu. EPG-PERSISTENCE. Limits: project text
      store, not vendor persistence; no EIT filtering/CRID/retention or
      incremental merge, save per section, 512-record cap, no TV/hardware.

- [x] EPG now/next control query: `epg-nownext <svc> <now_ms>` returns the
      earliest current (`start<=now<end`) and earliest next (`start>now`),
      with `-` for missing, over the existing epg v1 query/cache. Real-socket
      tests cover boundaries (exact start/end), before/after, clear, stop and
      invalid args; ARM core links. EPG-NOWNEXT. Project query over the
      512-event cache; no new vendor ABI, CRID/full schedule or TV.

- [x] EIT TS demux and replay backend: added `common/eit_ts.c` (PID 0x12,
      188-byte validation, transport-error/scrambled drops, adaptation fields,
      continuity duplicates/discontinuities, PUSI pointer, split/back-to-back
      sections, 0xff stuffing, 4096-byte cap) and `common/nim_ts.c`
      (`nim-ts-replay`, 8-section queue). `mod_tuner` reads `tuner.ts_path`,
      opens on start, closes on stop/destroy and feeds assembled sections into
      `epg.ingest`. Tests cover the demux and replay backend; a real Unix-socket
      replay produced `epg-count 2` with both events and metadata. Host423 ok,
      verify45; ARM/qemu passes. EIT-TS-DEMUX. Limits: replay file only, NIM
      tune/signal remains the stub, fixed PID, no live demux/hardware, CRID or
      persistence. No TV access.

- [x] Live-capable NDAL tuner backend: added `oa_ndal_nim_ops` and the
      selectable `nim-ndal` frontend adapter. Default ops open the configured
      `/dev/dvb/adapterN/frontendN`, tune DVB-T with `FE_SET_FRONTEND`, read
      lock/strength/SNR/BER/UNC with `FE_READ_*`, and resolve
      `Nd_NIM_SetScanMode/GetFrequency/GetDeliverySystem/GetSymbolRate/
      SetTsOutput/SetStandy` from `libNDAL_EX.so`. `tuner.backend=ndal` selects
      it; mock ops validate open/tune/scan/frequency/status/metrics/close and
      failure cleanup. Host423 ok, verify45; ARM/qemu passes.
      NDAL-TUNER-BACKEND. Limits: mock only, DVB-T tune path only, no live
      EIT/demux or PVR stream, no hardware lock/signal/scan acceptance; cold
      start/display owner-gated. No TV access.

- [x] DVB section-filter EIT source and DVR sink: added `oa_eit_dvb_ops`
      (open demux, PID section filter, read/close) with default
      `Hal_Demux_Dvb_*` resolution and mock tests; `nim-ndal` attaches it via
      `tuner.eit_demux=adapter:index[:pid]`, opens lazily and closes on stop.
      Added `oa_dvr_ops`/`oa_dvr_sink_open/write/flush/close` with partial-write
      handling and mock tests; the PVR scheduling store remains separate.
      Added a real Unix-socket `mod_tuner` reload/re-scan test over the TS
      fixture. Host424 ok, verify45; ARM/qemu nim_stub and pvr_stub pass.
      EIT-DVB-DVR. A later review against the Ghidra listing of `libNDAL3.so`
      (MD5 c842bad804302ec822076c3d9d3ce31d) fixed three offline gaps: demux
      close now calls `Hal_Demux_Dvb_Close` (`DMX_STOP` 0x6f2a, then `close`),
      `next` accepts a read only when it is one section whose length field
      matches the TS demux bounds, and `nim->stop` closes a lazy EIT filter
      when the frontend was never opened. A `-1` HAL open/write with errno 0
      is `-EIO`. DVR open mode remains 0 (`O_WRONLY`). Re-ran `nim_stub` and
      `pvr_stub` on the host, under ASan/UBSan, and on ARM/qemu. The earlier
      full host424/verify45 run was not repeated. Limits: mock only, PID-only
      filter, DVB-T tune only, DVR sink not scheduler-driven, no hardware
      acceptance; cold start/display owner-gated. No TV access.

- [x] Vendor broadcast contract fields + EIT schedule cache: extended
      `oa_tuner_status` with lock/strength/quality/BER/SNR/UNC,
      `oa_epg_event` with CRID/first-showing/running-status/state/version/
      actual, and PVR request/job with repeat days and start/end padding.
      Fixture, control-socket and CLI tests cover the new fields and the
      optional `pvr-add` arguments. Added an in-memory, firmware-grounded EIT
      schedule cache (`eit_cache.c`: keyed put/get, seconds start+duration
      query, now/next) on `common/eit_time.c`. Host415 ok, verify45; ARM/qemu
      passes. BROADCAST-VENDOR-FIELDS. Limits: fixture/project code only, no
      vendor execution or hardware; CRID string resolution, EIT filters,
      persistence, series/conflict handling and `EITEventRecordList`
      find/remove semantics remain absent. No TV access.

- [x] Vendor broadcast ABI grounding: reversed `EITEventTimeRange` (MJD/BCD
      -> Unix seconds, duration, sentinels, merge/grow/shift/clip/moveInto),
      `EITEventKey`, `ScheduleEvent` schema, `EASIControlTask::querySchedule`/
      `ScheduleCache` index+duration queries, `DEV_NIM_DTV_StartTune`/signal/
      lock paths, and `DvrController`/`NebulaPvrRecordingScheduler` recording
      APIs. Added a firmware-faithful EIT codec/range module
      (`eit_time.c` + tests) and documented contract deltas. Host414 ok,
      verify45; ARM build/run also passes under qemu. VENDOR-BROADCAST-ABI.
      Limits: static RE only, no vendor execution; tuner paths 6/8 unnamed;
      contracts not yet extended with signal/CRID/PVR scheduler fields; no
      tuner/recording hardware or TV access.

- [x] Broadcast CLI/socket coverage: `control.c` now serves `tuner-state`,
      `tuner-scan`, `tuner-cancel`, `tuner-select`, `epg-query`, `pvr-list`,
      `pvr-add` and `pvr-cancel`, with pre-lookup argument validation, bounded
      multi-line replies and service-handle release. `tests/broadcast_ipc.cjs`
      drives the real binary/fixture over the Unix socket: scan progress and
      completion, select bounds, cancel, EPG windows/caps, PVR ID/cancel/repeat,
      reload reset, stop->ENODEV and shutdown. Host413 ok, verify45.
      BROADCAST-IPC. Fixture backend only; no tuner/EIT/PVR hardware, vendor
      backend, UI or persistence. No TV access.

- [x] Broadcast service foundations: project-internal tuner/epg/pvr v1
      contracts and a host-only fixture. Tuner scan state machine, EPG overlap
      queries with bounds/caps, PVR add/list/cancel, reload and teardown are
      host-tested. Host412 ok, verify45. BROADCAST-FOUNDATION. Limits: fixture
      only; no tuner hardware/scan, recording engine, production module, UI or
      persistence (CLI/socket added later in BROADCAST-IPC; static EIT/tuner/
      PVR semantics recovered later in VENDOR-BROADCAST-ABI). No TV access.

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

- [x] First functional native OmarchyTV shell in `mod_ui_omarchy`: Home grid
      (Sources/Apps/Settings/About), 2D focus/accent frame, Sources picker,
      launcher.db Apps list, CEC/settings panel, About, live clock and optional
      EF41 text. `check-ui-skin` covers home/focus and all four screens;
      `check-ui-omarchy-quiesce` still passes. Included in the 32-module bundle
      but not autoloaded. Apps Select emits OA_ACTION_SELECT (index+name), Sources
      Select emits OA_ACTION_SOURCE after a successful switch, Exit emits
      OA_ACTION_BACK, and launcher.db changes reload the app list; fake service
      tests cover these. There is no in-process app launcher, so actual process
      launch, hardware input and scanout acceptance remain open.
      NATIVE-UI-2026-11-07.

- [x] Host-tested app-manager idtv entry wrapper (`run-aurora.sh`): bundle/
      manifest checks, stock fallback, `oa-supervise` argv with watchdog and
      child, dry-run; four host cases pass. Bundled but not installed; appmgr
      launch/suspend and persistent boot integration remain unauthorized.
      APP-MANAGER-ENTRY.

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

- [x] Diagnostic skin `ui_diag`: a reloadable ui.engine skin probes 12
      project services, paints green/red status rows with details where
      exposed, supports Up/Down/Select refresh and module-change repaint, and
      loads optional EF41 text with bar fallback. Recorder test covers rows,
      selection, reload and teardown; the real socket switches
      omarchy→diag→vendor; the ARM module builds. UI-DIAG. Fixed probe list,
      recorder-only; no TV/HAL/cold/visual parity.

- [x] Offline full-stack takeover rehearsal: `tests/full_stack_ipc.cjs` runs
      the deferred TV module order over the real socket with the live dump DB
      and workbench UI: plat, recorder display, settings, service_db,
      image_decode, ir, ui_engine, ui_vendor; `key 22`/`21`, `reload ir`,
      `reload ui_vendor`, reverse stop/unload, clean exit. `make -j24
      check-full-stack-ipc` passes. FULL-STACK-REHEARSAL. Recorder display,
      host plat and injected keys only; no UART/BLAST/fb1/HAL/TV.

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

- [x] Stock apps-grid oracle and renderer parity: captured the real stock
      full-screen Apps page via DIAL virtual-remote navigation (MENU 1048, Apps
      tile, OKs) and read-only fb1, saved it as
      `artifacts/protocol/stock-apps-grid.png`. Added a stock-layout renderer
      mode using `background_fullscreen`, solid `#481730` top strip, "Apps"
      title, 294x165 tiles at cell+7,+7 on a 310/230 pitch, focus painting only
      the orange cell backdrop, font-rendered Markt label and
      white-on-#811924 scrollbar. Added `stage-apps-grid.py`,
      `apps_grid_stock` and live-DB capture; all 23 firmware-dump parent-1 catalog rows and the original downloaded
      thumbnails are used, plus Markt (24 total), with no oracle tile crops.
      Manifest comparison improved from AE 29.56% / MAE8 75.38 to
      **AE 0.0629% / MAE8 0.160** (top 0.0035%, grid 0.1125%, scrollbar 0.0168%),
      with exact vendor bitmap font18/4 title/label rasters and the recovered
      GL_LINEAR_MIPMAP_LINEAR tile sampler. The focused-tile bbox oracle is
      294x165; the earlier full-cell model took Markt MAE8 3.343 to 0.185.
      JPEG staging now uses TurboJPEG FASTDCT to match hal::JpegImageDecoder,
      taking the 384x216 DAZN cell from 1.647 to 0.776.
      Host423 ok, verify45. STOCK-APPS-GRID-PARITY. Limits: fb1 capture, not
      HDMI/cold-start scanout; residual per-cell sampling/source differences.

- [x] Apps-grid residual font/scaling parity: recovered the exact EF41 bitmap
      fonts from the firmware pack (title font18, label font4 in `(38,23,25)`),
      replaced the TTF approximation and switched the recorder to bilinear
      tile scaling. The 24-entry order and empty final cell are verified from
      the dump and scrolled capture. Manifest AE 0.0629%, MAE8 0.160; top
      0.0035%, grid 0.1125%, scrollbar 0.0168%. STOCK-APPS-GRID-PARITY.

- [x] Apps-grid live-DB focus/label and one-row scroll: captured the stock
      Twitter-focused page on BACK AV and matched it with the live database
      and original thumbnails. `SVGGrid::onDownKey` 0x10cb938 and
      `FullScreenGrid::onNextKey` 0xd20530 move the window by one row
      (`columns=5`), so selection 22 has `first=10`, not the old page-15
      `first=15`. `ui_vendor` inserts Markt ahead of the 23 parent-1 rows when
      absent and renders focused non-Markt labels with EF41 font 4 in
      `#261719`. Full-image AE0.0619%/MAE8 0.158; focused `make -j24
      check-ui-apps-grid` passes. Limits: recorder/offline render, not a
      post-fix fb1 capture; both focused labels remain 2px left of stock.

- [x] Vendor image-scaler parity: recovered the stock UI texture path from
      aurora.elf. hal::OpenGLGraphicsContext::createTexture 0xb32eb0 sets
      MAG_FILTER=GL_LINEAR and, for its mipmap bool, MIN_FILTER=
      GL_LINEAR_MIPMAP_LINEAR (0x2703) plus glGenerateMipmap;
      UISurfaceImpl::drawImage 0x1161ef8 enables it for UIImageScalingType
      1..3 and FullScreenGridButton::setApplicationImage 0xd21000 uses type 1.
      Added common/canvas.c oa_canvas_blit_mipmap (box mip chain, normalized
      bilinear per level, trilinear blend in sRGB) and wired mod_display and
      the recorder to it. Manifest AE 0.1185% -> 0.0629%, MAE8 0.302 -> 0.160,
      grid 0.2156% -> 0.1125%, per-cell MAE8 0.083..0.445. Host423 ok,
      verify45. STOCK-APPS-GRID-PARITY. Limits: Mali's exact mip-generation
      kernel is implementation-defined (box assumed); CPU emulation, not GPU
      execution or live scanout.

- [x] Stock-capture parity analysis: the old `shot-launcher-right.png` is an
      R/B-swapped pre-correction capture; after channel correction it matches
      the current Prime/Canal color orientation. Same-geometry portal cards
      still differ in artwork/crop (Netflix RMSE0.376, Prime0.216, others
      0.066..0.104), background is13,14,14 vs36,38,40, and stock status chrome
      is absent. The old capture is not an apps-grid or pixel oracle.
      STOCK-PARITY. A fresh same-session corrected stock apps-grid capture is
      still required; no TV operation in this analysis. **Superseded
      2026-11-05:** the stock Apps page was captured and compared in
      STOCK-APPS-GRID-PARITY (live DB AE 1.02%; old fixed-corpus 0.99% superseded).

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
      validation remain open. Temporary fb1 use was authorized on 2026-11-05;
      stock Aurora is running again. This is not cold startup or a persistent binding.
- [ ] Cold independent startup without vendor Aurora: establish MCU authority,
      watchdog/fallback and full display ownership/restoration from firmware.
      A historical FreshWDT return1 did not prove rejection or impossibility.
      On-TV PNG/JPEG display is now covered by the supervised VISIBLE-IMAGES
      test; stock apps-grid layout/chrome comparison now exists at AE 1.02% on the live DB
      (STOCK-APPS-GRID-PARITY) but content-independent/live-DB parity remains.
      An ARM cold-start bundle
      (openaurora,25 modules, oa-supervise, fb-restore, cold config) is built
      and manifest-verified offline by `make cold-bundle`
      (FB-SNAPSHOT-COLD-BUNDLE), but not executed. `plat` now enforces the
      vendor `/tmp/ntkdriver_completed` wait before its HAL prelude, with tests
      and a view-only live-marker check (PLAT-STARTUP-WAIT); that is ordering,
      not a cold boot. The `DEV_INI_Init` device stages are audited and mapped
      to owners (COLD-START-STAGE-AUDIT); the SPI transport is now an opt-in
      module with fake tests, MTD needs no owner, and NIM-DTV/amp/panel power
      remain unimplemented (SPI-MTD-NIM-STAGE). A temporary fb1 takeover on
      2026-11-05 showed scanout, injected IR, vendor fallback and resume, then
      a failed direct relaunch; the owner had the watchdog reboot stock Aurora
      (TAKEOVER-2026-11-05). Cold boot without the vendor process, MCU authority
      and a clean handback are still open.
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
- [x] Optional embedded Lua service `ui.lua` (module `lua`, not a skin) for every
      skin, before the full OmarchyTV skin. Versioned API for layouts, navigation,
      themes, key bindings and animation configuration; rendering, blocking
      transfers and hardware ownership stay in C. Script reload through the
      existing IPC/socket/CLI validates the candidate, migrates state, and drops
      timers and fetches. Memory and instruction limits are reported; a required
      script that is not loaded falls back to the vendor skin. Failed loads,
      callback errors, reload/suspend/unload and the target ABI are tested.
      Instruction hooks do not bound blocking C calls such as `file:read("*a")`.
      Scripts cannot take device ownership or add telemetry/advertising.
      Evidence 2026-11-05: `make check-ui-lua` and `make check-ui-skin` exit 0;
      full host `make check` 434 ok, verify45; `BUILD=build-lua-san` ASan/UBSan
      `check-ui-lua` exit 0; ARM glibc
      `build-lua-arm/tests/ui_lua` under qemu-arm-static and bwrap exit 0.
      The http success path is the host run. qemu-user cannot exec host python
      or curl, so the ARM run takes the failed-transfer callback. Vendor pixels
      are unchanged. Quiesce unpublishes `ui.lua` and rejects hook/reload while
      a fetch child is unreaped (`check-ui-lua`). Start and resume fail, and
      resume stays suspended, when the 50 ms fetch pump cannot be allocated
      (`check-ui-lua`); that path was not re-run under ASan/UBSan or ARM.
      This is not television startup or scanout; criterion 4 stays unmet.
      See openaurora/docs/LUA.md.
- [x] Host `sched` service for cooperative background work, shared by Lua and
      other modules. At most 32 tasks. Each due task runs one nonblocking slice
      per loop turn (at most eight slices a turn). Delay 0 continues on the
      next turn; longer delays are capped at 60 s. `wake` makes a task due.
      `cancel`, stop, and unload drop the owner's tasks without calling the
      step. Standby defers slicing. `oa.spawn` / `oa.yield` use it; the
      `coroutine` library stays closed, and a task does not run during a hook.
      Suspend, reload, stop, and quiesce drop Lua tasks. `check-sched` and the
      spawn case in `check-ui-lua` pass on the host. No ASan/UBSan, ARM, or
      television run. See openaurora/docs/LUA.md.
- [x] Gated `oa.aurora` Lua control bridge: `ui.lua.control` defaults off;
      1 allows synchronous read-only control queries, 2 allows deferred
      control commands through the host post queue. `aurora.query`,
      `aurora.request` and `aurora.result` are bounded; lifecycle commands
      targeting `lua`, plus exit/quit/standby, are refused. The existing
      control socket is the MCP/automation transport; no JSON-RPC listener or
      raw shell/device API was added. The ui_lua group covers query, deferred
      request/result, and forbidden-command rejection; host434 ok, verify45,
      sanitizer and ARM/qemu pass. LUA-SERVICE. No TV access.
- [ ] Real OmarchyTV screens replacing the skeleton after vendor parity. Keep
      OA_SVC_PORTAL on vendor and preserve engine draw/asset/present contracts.
- [x] Split module/test compile+link rules into .o + link so ccache can cache
      them. Each shared library compiles PIC objects (`-c -fPIC -MMD -MP`) and
      links those objects with the previous `-shared` flags. `-Werror` and
      `-pedantic` stay in `CFLAGS`. Host measurement with a private `CCACHE_DIR`,
      `rm -rf openaurora/build` between runs, `make -j all` plus the 75 concrete
      `.so` targets: cold 202 cacheable calls, 185 misses and 17 hits (identical
      sources compiled again inside that build), 79 uncacheable link steps;
      warm 202/202 cacheable hits and 0 misses, plus 79 uncacheable links.
      `check-sched`, `check-ui-lua`, `check-power`, `check-safety`,
      `check-phases`, `check-quiesce` and `check-reload` exit 0 on that build.
      No ASan/UBSan, ARM, or television run.
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
