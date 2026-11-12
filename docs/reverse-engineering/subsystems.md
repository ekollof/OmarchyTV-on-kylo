# Subsystems: recovered contracts and evidence

Each section is a starting point: the recovered contract, where it lives in the
replacement, and the report(s) with the full evidence. Addresses are virtual
unless marked section-relative. Read the report before reusing a claim.

## Boot init and recovery

**Chain (verified read-only):**

```text
kernel init=/init  ->  /etc/inittab ::sysinit:/etc/rc.local
  rc.local: mount_ramdisk, bootlogo/PreInit.ko, mount_ubifs
            ap1 -> /vendor2 ro, ap0 -> /vendor ro, apdat0 -> /conf rw,
            buffer -> /buffer rw, setup_network, /usr/script/startup.sh
  startup.sh -> /vendor/run.sh
  run.sh -> appmgr --applist=<nvt.app.ini | nvt.app.debug.ini>
  appmgr [idtv] -> /mtd_apexe/board/run_aurora.sh
  run_aurora.sh -> /conf/remotelf/aurora.elf if /conf/remote.txt (CIFS),
                   else /vendor/com.nvt.idtv/aurora.elf
```

- `/mtd_apexe/board` is a second mount of ap0 (same read-only squashfs as
  `/vendor`); `/mtd_rwarea` is a second mount of `/buffer`. Rootfs, `/vendor`
  and `/vendor2` are read-only; `/conf` (ubi13) and `/buffer` (ubi14) are the
  writable volumes.
- The only writable program hook is the `/conf/remote.txt` CIFS path; the
  dwelf portal needs a missing `/vendor/data/nvt.app.dwelf.ini`.
- The installed local hook (`/conf/remotelf/aurora.elf`) gates native takeover
  on `/conf/oa_boot` + no `/conf/oa_safe`; its state files are `oa_boot`,
  `oa_safe`, `oa_fail`, `oa_try`, `oa_bootfail`, `oa_stage`, `oa_last_ok`.
  Boot logs are on `/buffer` (`tvhack-boot.log`, `openaurora-app.log`,
  `oa-boot-history.log`), so the apdat0 recovery does not erase them.
- MCU watchdog: `/proc/nvtstbcipc`, command `0xae` (`FIPC_set_watch_dog`); stock
  Aurora feeds it. `wdtctl set/disable` is the recovered lever; the replacement
  uses `oa-wdtfeed` + `oa-supervise --wdt-keep` for the staged cold path.
- Recovery: `/conf/coredumpnoreset` disables the crash-reset path; the tested
  USB pair rewrites only apdat0. Do not patch signed partitions or the
  bootloader.

**Cold hardware sequence** (instruction-verified; see
[COLD-START-SEQUENCE](../../artifacts/reports/COLD-START-SEQUENCE-2026-11-10.md)):

```text
ICE_HardwareInit @7551a0
  wait /tmp/ntkdriver_completed
  PLAT_DRV_INI_Init @89b6e4
    Hal_Audio_Protect_Init(0), Mixer_Ctrl_Init, Proxy_Init   (returns ignored)
    plat_common_init("DVIF")                                  (idempotent; byte src=1)
    Hal_Gpio_Init, Hal_Iic_Init, plat_vin_slot_init          (nonzero logged; continue)
    NDAL_VideoExt_OpenDevice(0,10)
  DEV_INI_Init @853a58                                       (each zero logged; continue)
    PCB layout 14, DEV_GPIO_Init, dev_src_init, DEV_PANEL_Init,
    DEV_NIM_Init, DEV_IIC_Init, DEV_AMP_Init, DEV_SPI_Init,
    DEV_SPI_Open(0), DEV_NIM_DTV_Init, DEV_MTD_Init
```

`mod_plat` implements the HAL half exactly (including the non-aborting
prelude). The `DEV_*` device half is not called: GPIO/IIC use the HAL,
`mod_panel` owns PWM/power, `mod_tuner` owns NIM/NIM-DTV, `mod_spi` owns the
SPI open, and source hotplug/`DEV_AMP_ID` remain gated or missing. Full vendor
cold-start parity is not claimed; no UI is cold-start evidence.

`openaurora-cold-hw.conf` + `stage-hw-profile.sh` provide the no-UI ladder
`hw0..hw4` (plat -> panel -> I2S MCLK -> SPI open -> NIM-DTV). The core's
`host.ready_after_autoload` writes the supervisor receipt only when every
autoload start succeeds. It is a host-tested sequence marker, not a hardware
acceptance.

Evidence: [COLD-START-STAGE-AUDIT](../../artifacts/reports/COLD-START-STAGE-AUDIT-2026-11-06.md),
[STAGED-COLD-BOOT-DESIGN](../../artifacts/reports/STAGED-COLD-BOOT-DESIGN-2026-11-09.md),
[BOOT-HOOK-COLD-ATTEMPT](../../artifacts/reports/BOOT-HOOK-COLD-ATTEMPT-2026-11-09.md),
[APP-MANAGER-ENTRY](../../artifacts/reports/APP-MANAGER-ENTRY-2026-11-07.md),
[APP-MANAGER-CONFIG](../../artifacts/reports/APP-MANAGER-CONFIG-2026-11-07.md),
[openaurora/docs/RECOVERY.md](../../openaurora/docs/RECOVERY.md).

## Display OSD DRM GPU

- OSD layers: `fb0` 1920×3240 (3 frames), `fb1` 1920×2160 (2×1080), stride
  7680, 32 bpp **BGRA**, pan 0,1080 on stock. Layers are OSD_NORMAL1/2/3 and
  OSD_CURSOR1/2; stock holds fb0–2, card0 and ntkgpu.
- OSD region ioctl: `0x8020c86c` reads 8×u32, `0x4020c86b` writes. Words
  `[2]/[3]` = source/window size, `[4]/[5]` = position, `[6]/[7]` =
  destination size. Stock: `[2,3]=1920,1080`, `[4,5]=0,0`,
  `[6,7]=3840,2160`. `NVTGA_OSD_SetRegion` (`libdirectfb_tdga.so` `0x152b8`)
  only writes `[2]/[3]`; a cold boot must program the destination itself (the
  2026-11-09 quadrant fix). `display.dst_width/dst_height` default 3840×2160.
- `FBIOPUT_VSCREENINFO` runs the vendor `SetPar`: `_ker_osd_fb_SetPar`
  `0x754ac`, `_ker_osd_fb_CheckVar` `0x754a8`, `gstOSDfbOps` `0x75494` in
  `ntkdriver_nondovi.ko`. `display.cold_init` repairs an invalid var;
  `display.osd_reinit` forces SetPar.
- MAU memory selection: `_IOR(0xc8,0xbd,uint32)`; primary vs alternate MAU.
- DRM/KMS: `Hal_Video_DrmInit(1)`; cold gate observed DRM card 13, fb 98,
  crtc 26, planes 40/54/55/56/59/60/61/62; VPLTIO command `0x302` (DVIF);
  `Hal_Video_Vpltio_Init/V4l2InfoInit/DrmShareMemCreate`.
- EGL: display1, retained 4-byte uint16 width/height native window,
  `FRAMEBUFFER_2=/dev/fb1`, `MALI_MAU_SWITCH=0,0,0,0,0`; pbuffer readback is
  bottom-up premultiplied RGBA, native storage is BGRA.

Evidence: [OSD-BUFFER-CONTRACT](../../artifacts/reports/OSD-BUFFER-CONTRACT-2026-10-31.md),
[OSD-KERNEL-INIT](../../artifacts/reports/OSD-KERNEL-INIT-2026-10-31.md),
[OSD-MAU-SELECTION](../../artifacts/reports/OSD-MAU-SELECTION-2026-10-31.md),
[COLD-DISPLAY-REPAIR](../../artifacts/reports/COLD-DISPLAY-REPAIR-2026-11-07.md),
[DISPLAY-RESUME-FRAME](../../artifacts/reports/DISPLAY-RESUME-FRAME-2026-11-06.md),
[EGL-WINDOW](../../artifacts/reports/EGL-WINDOW-2026-11-01.md).

## Panel TCON

- Stock power sequence values (read-only): panel-extra 30/580/220/30/100 ms,
  `+0x4a=0`; GPIO110 on=1, GPIO111 on=0; PWM108 channel0, high period 1000;
  TCON gate 0. `panel.power_on_start` replays the recovered sequence;
  `panel.standby`, `panel.start`, `panel.freq`, `panel.curve`, `panel.duty_max`
  are the exposed knobs. The stock Init mode byte is 1, so the off path sets
  PWM to `panel.off_pwm` (default 1000) **before** LVDS off, matching
  `DEV_PANEL_PowerSequence`; the timing halfwords at `0x019354b8..c0` are
  write-only. The GPIO 110/111 description copies are replaced by direct HAL
  pins for layout 14; other layout/TCON branches remain open.
- Backlight/TCON and the HAL error paths are separate from OSD/DRM ownership;
  a successful panel write is not visible-scanout proof.

Evidence: [PANEL-BACKLIGHT](../../artifacts/reports/PANEL-BACKLIGHT-2026-11-07.md),
[PANEL-TAKEOVER](../../artifacts/reports/PANEL-TAKEOVER-2026-10-31.md),
[PANEL-HAL-ERRORS](../../artifacts/reports/PANEL-HAL-ERRORS-2026-11-07.md),
[COLD-OSD-PANEL-GATE](../../artifacts/reports/COLD-OSD-PANEL-GATE-2026-11-09.md).

## Audio

- Vendor HAL lives in `libNDAL3.so` (`Hal_Audio_*`) and `libNDAL_EX.so`
  (`Hal_Audio_DualCmd`, PQ/EX). Recovered control ops used by the replacement:
  `0x11/0x40/0x1e` (AIN init), `0x37/0x4a` (VAUD init), `0x2e/0x3e` (101-entry
  AQ volume table), `0x2a` (PIP payload), `0x33` (decoder main volume),
  `0x3c/0x3d` (speaker mix gain 0x8000), `0x15/0x2b/0x2c/0x2d` (HP switch/DAC
  gains), plus `Hal_Audio_EasySelect/EasySet/KItem/SetControl/Send`.
- The main amplifier is **ESMT AD82587_D**, not AD82586C: I2C bus 3, address
  `0x34`, reset GPIO114, MCU mute GPIO208/STBC8. Speaker gain, decoder volume
  and amp register volume are distinct scales.
- AIN source table (56 entries), `AOUT` cache (612 B), `ICE_AudioStart`
  external branch, `audio.source` AIN/VAUD/AQ/AQuality/HP sequence, and the
  opt-in `audio.source.unmute` MM-path release are implemented in
  `mod_audio_source`.
- `SetOPUGain` channel operands are `0..0x8000`; `GetOPUGain` remaps cached
  100 to 0x8000, so it cannot snapshot an exact rollback. AQ tables come from
  `/conf/AQ.bin` (101 entries, 1668-byte layout); the EQ/AVL post-params are
  partially recovered (`AQuality` send).

Evidence: [AUDIO-ROUTING](../../artifacts/reports/AUDIO-ROUTING-2026-11-01.md),
[AUDIO-VOLUME](../../artifacts/reports/AUDIO-VOLUME-2026-11-01.md),
[AUDIO-CURVE-MODULE](../../artifacts/reports/AUDIO-CURVE-MODULE-2026-11-01.md),
[AUDIO-GAIN-TRANSPORT](../../artifacts/reports/AUDIO-GAIN-TRANSPORT-2026-11-01.md),
[AUDIO-AMP-NATIVE](../../artifacts/reports/AUDIO-AMP-NATIVE-2026-11-04.md),
[AUDIO-AMP-PROFILE](../../artifacts/reports/AUDIO-AMP-PROFILE-2026-11-04.md),
[AUDIO-SOURCE-SERVICE](../../artifacts/reports/AUDIO-SOURCE-SERVICE-2026-11-09.md),
[AUDIO-BROKER](../../artifacts/reports/AUDIO-BROKER-2026-11-01.md),
[AUDIO-NETLINK](../../artifacts/reports/AUDIO-NETLINK-2026-11-01.md).

## CEC

- Normal traffic is the private V4L2/kernel CEC path, not UART. The raw/VPL
  packet ABIs are 16-byte structs; the kernel copies 16 bytes to the VPL receive
  buffer even on rejected frames. Read CEC-TRANSPORT-CONTRACT before touching
  receive/filter/close.
- Authority: mirror word at physical `0xfc040200`; bit 1 clear means the
  replacement must fail closed. PreInit registers the cdev; the authority write
  path is not implemented (fail-closed).
- Owner model: blocking open/send/receive/close on a worker; 100 ms poll;
  policy/reply queue with a user TX ring (commit `b9269fc`); CEC 1.3a base +
  1.4 ARC/System Audio. The 2026-11-01 cache “LA8” was an array slot, not a
  proved wire LA: first-opener TX on 2026-11-12 NACKed LA8 and ACKed LA4 plus
  broadcast. LA4 accepted an LA0/PA 0.0.0.0 announcement, a poll and 83/46/8f
  but sent no reply. CEC-ASYNC/REPLY-QUEUE/QUIESCE cover the ownership and
  reload gates; CEC-MODULE-TRANSPORT adds the injectable raw-device/authority
  seam and a host fake; CEC-FIRST-OPENER records the boot-order/TX path.
- Routing/power status: 0x82 Active Source is implemented behind
  `cec.active_source` by PA top nibble (HDMI1..4; `cec_active.cjs`); the stock
  path `selectSource` uses CEC logical addresses. The
  `NovoCECController::handleCECMessage` 0x9D path returns
  unless the TV is in the standby transition, but the separate
  `SvgAppCECEventsListener` handles 0x9D by posting `UIKeyPressMessage(597)`;
  0x36 Standby is accepted only from the active device. The owner's 2026-11-12
  Deck suspend/resume test left
  the TV on at a tuner input on sleep; this is CEC `0x9D Inactive Source`:
  `SvgAppCECEventsListener` posts `UIKeyPressMessage(597)`, and the UI selects
  DTV/tuner (`ICE_VideoSelectSource` source 8 →
  `TunerManager::selectServiceForDisplay(1)`). The VIU lost-signal/ICE_VideoHide
  lines follow the source change. The external-interface disconnect policy is
  the cable-unplug fallback in SOURCE-LOSS-FALLBACK. Resume-to-HDMI1 is
  consistent with 0x82/video ready. `mod_cec` routes broadcast 0x9D behind
  `cec.inactive_fallback` and 0x82 behind `cec.active_source`; 0x36 is behind
  `cec.standby` and accepted only from the tracked active source
  (`cec_standby.cjs`). Identity queries are behind `cec.identity`
  (0x83 -> 0x84, 0x85 -> 0x82 while tuner-owned; `cec_identity.cjs`).
  Power-status queries are behind `cec.power_status` (0x8F -> 0x90 "on",
  0x90 logged; `cec_power_status.cjs`). 0x80 Routing Change follows only a
  matching old active PA and selects the new PA's HDMI input behind
  `cec.active_source` (`cec_routing.cjs`). OSD names are behind
  `cec.identity` (0x46 -> 0x47 configured name, 0x47 logged;
  `cec_osd_name.cjs`). One-touch 0x04/0x0D selects the 0x84 device PA's HDMI
  input behind `cec.active_source` (`cec_image_view.cjs`).

Evidence: [CEC-TRANSPORT-CONTRACT](../../artifacts/reports/CEC-TRANSPORT-CONTRACT-2026-10-31.md),
[CEC-OWNERSHIP](../../artifacts/reports/CEC-OWNERSHIP-2026-11-01.md),
[CEC-LIVE](../../artifacts/reports/CEC-LIVE-2026-11-01.md),
[CEC-SPEC](../../artifacts/reports/CEC-SPEC-2026-10-30.md),
[CEC-SPEC-VS-AURORA](../../artifacts/reports/CEC-SPEC-VS-AURORA-2026-10-30.md),
[CEC-AUDIO-CONTRACT](../../artifacts/reports/CEC-AUDIO-CONTRACT-2026-10-31.md),
[CEC-MODULE-TRANSPORT](../../artifacts/reports/CEC-MODULE-TRANSPORT-2026-11-09.md),
[CEC-FIRST-OPENER](../../artifacts/reports/CEC-FIRST-OPENER-2026-11-12.md).

## Source VIN V4L2

- `ICE_DeviceSource` table (56 entries) maps source → AIN device. External
  video types 15..22 map to audio devices (`15→0x23`, `16/20→0x29`,
  `17→0x25`, `18→0x26`, `19→0x27`, `22→0x28`).
- Connect path: per-type `Open(0/1/3+2/4)`, VPLT `0x4c4`, V4L2Info/FdCommon,
  `plat_drm_get/connect_dpsource`, `NDAL_VideoExt_OpenDevice`, and the
  `Hal_Audio_EasySelect` companion. `source.enable=0` gates the whole path;
  the replace-source probe fails fast when vendor DRM is not initialized.
- `plat_vin_slot_init` is pure data (7 slots, defaults `{0,7,7,1,1,0,4,0,0}`
  plus `{7,0,25}`); `mod_source` drives slot 0 through `Hal_Vin_*`.
- External-interface disconnect fallback: navigator
  `handleExternalInterfaceDisconnected` @ `0xfb32f0` →
  `InputManager::handleExternalSourceDisconnected` @ `0xfb1fbc`. It matches the
  disconnected video group and selects the saved tuner service `this+0x90`
  (`requestSelectServiceForDisplay` @ `0xfb19f0`) or the previous external
  source `this+0x94` (`requestSelectExtSource` @ `0xfb1c50`). A pure policy
  helper (`common/source_loss.c`, `check-source-loss`) is tested but not wired.
  This is the **cable-unplug** policy. The Deck-sleep switch is CEC `0x9D
  Inactive Source`: `SvgAppCECEventsListener` posts `UIKeyPressMessage(597)`;
  the UI selects DTV/tuner (`ICE_VideoSelectSource` source 8 →
  `TunerManager::selectServiceForDisplay(1)`). The VIU lost-signal/ICE_VideoHide
  lines follow the source change. Stock provider entry: `AitHub` → `NEBULA_Register...` → `NEBULA_Handle...` →
  Nebula navigator; kernel status carrier `ndp_SRC_ConnectInfo` /
  `NDP_ExtSrc_Get/SetConnectInfo`. V4L2 event `0xc000001` on `/dev/video21` is
  a scaler update (source type `[0..3]`, `UpdateSCLR arg==6` at `[4]`,
  `VidSetting+0x0c` at `[5]`, `VidSetting+0x86` at `[6..7]`), not source loss;
  the decoder/adapter and the HPD model (`common/source_hpd.c`,
  HDMI1=pin203/byte0, level1=connected) are tested. HPD stayed high through Deck
  suspend. `Hal_Vin_GetHdmiStatus`/`0xa00967` works on `video21` (ignore the
  kernel `error_idx` sentinel) and captured `w0=1` active, `w0=0/5` during
  suspend, back to `1`; `source.video_status()` v2 behind `source.vin_status`
  exposes ready/lost (`check-source-vin`, fake-module `source_path`) and does
  not select a tuner. It is a no-signal detector, not the sleep trigger.
  `common/tuner_view.c` + `check-tuner-view` cover the recovered
  `TunerManager` view position→handle mapping
  (`(this+0x1c)->vtable+0x54`). The source service's gated
  `inactive_source(position)` v3 (`source.inactive_fallback`) resolves
  `OA_SVC_CHANNELS` or the stored `OA_SVC_SERVICE_LIST` fallback, connects DTV
  and calls `OA_SVC_TUNER.select(handle)`; `source_path` fake-service tests
  cover the runtime/stored order/gates. `mod_cec` routes broadcast 0x9D behind
  `cec.inactive_fallback` (`cec_inactive.cjs` covers both paths). The full
  select/mute/decoder/UI body remains open. Do not substitute an
  always-switch-to-TV or HPD shortcut.
- A successful `change_input` emits `source/state` with the source type.
  `ui_vendor` maps it to the Bronlijst row, persists `m_current_source`, syncs
  the picture cards and repaints; `ui_omarchy` repaints. `common/source_map.c`
  + `check-source-map` cover the nine-row map; `source_path` asserts the event.

Evidence: [SOURCE-PATH](../../artifacts/reports/SOURCE-PATH-2026-11-09.md),
[SOURCE-LOSS-FALLBACK](../../artifacts/reports/SOURCE-LOSS-FALLBACK-2026-11-12.md),
[SOURCE-VIN](../../artifacts/reports/SOURCE-VIN-2026-11-06.md),
[SOURCES-CAPTURE](../../artifacts/reports/SOURCES-CAPTURE-2026-11-05.md),
[SOURCES-CEC](../../artifacts/reports/SOURCES-CEC-2026-10-30.md).

## Tuner NIM DTV

- ATV tables live in the TV's own `aurora.elf`; the option header records the
  region `0x017681c8`, length `0xab8`, CRC32 `0x93a05fff`
  (`include/openaurora/mxl661_atv.h`). NIM-DTV init requires the SPI stage and
  `tuner.hw_init=1`; `tuner.backend=hal` intentionally rejects `hw_init=0`
  with `-EACCES`.
- `tuner.pcb_layout=14`, `/conf/mb230_hwprofile.bin`, ATV analog scan,
  vendor-channel mode and `tuner.ts_path`/NDAL backend are the exposed knobs.
  SPI/NIM-DTV runs before tuner in the cold profile.

Evidence: [TUNER-BRINGUP](../../artifacts/reports/TUNER-BRINGUP-2026-11-06.md),
[TUNER-MXL661](../../artifacts/reports/TUNER-MXL661-2026-11-06.md),
[TUNER-NIM-MODULE](../../artifacts/reports/TUNER-NIM-MODULE-2026-11-05.md),
[TUNER-VENDOR-CHANNEL](../../artifacts/reports/TUNER-VENDOR-CHANNEL-2026-11-07.md),
[SPI-MTD-NIM-STAGE](../../artifacts/reports/SPI-MTD-NIM-STAGE-2026-11-06.md).

## GPIO I2C SPI

- GPIO function-name table: **208 entries, IDs 0..207**, `au8GPIOFuncName`
  `0x175f350`, `astPwmBasePin` `0x175f320`. The ioctl word 0 is the pin and
  word 1 the function id; `DEV_GPIO_GetDescription` returns `entry+8`, so
  descriptor[1] is the pin used by `DEV_PANEL_*`.
- Read-only queries: `AMP-GPIO-QUERY` uses direct GET670d/670f on an
  already-open GPIO114. `Hal_Gpio_GetDirection` first issues 670a (marks the pin
  open) and ordinary `Hal_Gpio_Read` exports the pin first — neither is
  view-only. Do not initialize GPIO/claim pins under stock Aurora.
- I2C: `/dev/i2c-6` and `-7` were absent on this set; the amp is bus 3/0x34.
  SPI: `DEV_SPI_Init/Open` before NIM-DTV; `/conf/spi.bin`, `/dev/ntkspi`.

Evidence: [AMP-GPIO-QUERY](../../artifacts/reports/AMP-GPIO-QUERY-2026-11-04.md),
[AMP-I2C](../../artifacts/reports/AMP-I2C-2026-11-04.md),
[AMP-STBC](../../artifacts/reports/AMP-STBC-2026-11-04.md),
[VENDOR-INVENTORY](../../artifacts/reports/VENDOR-INVENTORY-2026-11-07.md).

## IR

- Separate code spaces: ICE, DIAL, NEC and DFB; the keymap CSV maps ICE
  IDs/KeyString to profile RC codes but does not settle the active UART
  encoding. The UART reader (`irkey`) consumes input and can disrupt the stock
  UI; it is not a view-only observation.
- BLAST uses `/dev/video28` protocol 4; `oa-ir-blast` is the helper. Repeat and
  helper argv are covered by IR-IPC/IR-HELPER/IR-BLAST-HELPER.

Evidence: [IR-KEYMAP](../../artifacts/reports/IR-KEYMAP-2026-10-30.md),
[IR-UART](../../artifacts/reports/IR-UART-2026-10-30.md),
[IR-IPC](../../artifacts/reports/IR-IPC-2026-11-05.md),
[IR-BLAST-HELPER](../../artifacts/reports/IR-BLAST-HELPER-2026-11-07.md).

## Settings DB appmgr launcher

- `/conf/sqlite_db/settingsDB.db` is a single-row `Settings` table with one
  INTEGER column per setting, plus `HMCSettings` and `LauncherItem`. The
  replacement reads/writes via `mod_settings`; `settings.readonly=0` is the
  cold profile, and the live DB is writable.
- `service_db` loaded 134 rows (54 dvb, 67 easi, 13 analog) from the DB.
- `libappmgr.so` is unstripped: keyboard layout `0x52f50` (0x180c bytes,
  119 keys, 127 slots × 0x30), `KBL_findCode`, `KBL_mapExtKeycode`,
  `HID_CommandIrInput` `0x30f74`, `COM_SendKey` `0x3678c`,
  `MGR_SendCommand` `0x2a550`. Stock appmgr uses `nvt.app.debug.ini` (dial/
  Netflix/Amazon/YouTube disabled, `run_aurora.sh` as idtv); retail
  `nvt.app.ini` enables them. Hotkeys f1=idtv/f2=netflix/f3=demo1.

Evidence: [APP-MANAGER-KEYBOARD](../../artifacts/reports/APP-MANAGER-KEYBOARD-2026-11-07.md),
[APP-MANAGER-CONFIG](../../artifacts/reports/APP-MANAGER-CONFIG-2026-11-07.md),
[APP-MANAGER-ENTRY](../../artifacts/reports/APP-MANAGER-ENTRY-2026-11-07.md),
[SETTINGS-PANEL-CAPTURE](../../artifacts/reports/SETTINGS-PANEL-CAPTURE-2026-11-05.md).

## UI vendor and native

- Vendor portal: rail (5×121×126 tiles from y 211), cards 12, title
  “Startpagina” at 28,80, palette orange→dark red; capture is
  `artifacts/protocol/shot-launcher.png` (corrected BGRA).
- Theme assets: `/vendor2/dynamic_ui/telefunken18_svg.{txt,bin}` (702 resource
  IDs/names, CRC32 verified), font pack
  `/vendor2/dynamic_ui/telefunken18_font_byte_data.{txt,bin}` (30 MB bin),
  images `telefunken18_img.*`. Card thumbnails:
  `/buffer/downloaded_images/launcher_app_thumb<N>`.
- Animations: 39 named descriptors from the BinSVG pack; `svg-animations.c`
  walks the pack and `svg-names.c` resolves the 702 names. Theme geometry and
  localized string records are partly undecoded; do not treat category styles
  as generic tiles.
- Native shell: `ui_omarchy` over `ui.engine` (home grid, sources, apps,
  settings, clock); `ui_vendor` reproduces the stock portal. The production UI
  is native `ui_engine` drawing; do not embed or launch the browser app.

Evidence: [UI-PARITY](../../artifacts/reports/UI-PARITY-2026-10-31.md),
[UI-ANIMATIONS](../../artifacts/reports/UI-ANIMATIONS-2026-10-31.md),
[UI-REVIEW](../../artifacts/reports/UI-REVIEW-2026-11-04.md),
[STOCK-APPS-GRID-PARITY](../../artifacts/reports/STOCK-APPS-GRID-PARITY-2026-11-05.md),
[NATIVE-OMARCHY-UI](../../artifacts/reports/NATIVE-OMARCHY-UI-2026-11-08.md),
[NATIVE-UI](../../artifacts/reports/NATIVE-UI-2026-11-07.md).

## EPG PVR channels

- `epg`: EIT/EASI persistence, now/next and the persistent cache; `pvr`:
  conflict state and playback; `channels`: 134-row service list, LCN and
  visibility are not fully exposed by `ACTIVECHANNELLIST` (see the report).
  `CHANNEL-RUNTIME-ORDER` records the stock order.

Evidence: [EPG-EIT-MODULE](../../artifacts/reports/EPG-EIT-MODULE-2026-11-05.md),
[EPG-NOWNEXT](../../artifacts/reports/EPG-NOWNEXT-2026-11-05.md),
[EPG-PERSISTENCE](../../artifacts/reports/EPG-PERSISTENCE-2026-11-05.md),
[PVR-MODULE](../../artifacts/reports/PVR-MODULE-2026-11-05.md),
[PVR-CONFLICT-STATE](../../artifacts/reports/PVR-CONFLICT-STATE-2026-11-05.md),
[CHANNEL-RUNTIME-ORDER](../../artifacts/reports/CHANNEL-RUNTIME-ORDER-2026-11-05.md).

## Power standby

- `POWER-STANDBY-CONTRACT` recovers standby/wake/network/UART and the target
  `ntkethmac` PHY/PM policy. Standby closes services; the sleep-cycle engine
  and power service have host tests but no physical standby acceptance.
  Do not treat a write/return code as a hardware effect.

Evidence: [POWER-STANDBY-CONTRACT](../../artifacts/reports/POWER-STANDBY-CONTRACT-2026-10-31.md).

## PQ MPFW and kernel tasks

- `PQ_MPFW_TASK` is a vendor kthread from `ntkdriver_nondovi.ko`, function
  `PLAT_PQ_Mpfw_CtrlTask` at section-relative `0x253d84`. It polls the PQ/MPFW
  firmware every **20 ms** (`PQ_OS_GetClockDiff`/`PQ_OS_TaskDelayInMS(20-diff)`)
  and `PQ_OS_TaskDelayInMS` calls `msleep`.
- On Linux 4.9 `msleep` is uninterruptible, so ~15 vendor polling kthreads
  (`PQ_MPFW_TASK`, `PQ_SHELL_DBG_TA`, VPL/TCON/SRC/DRV/PLAT_WDT …) sit in D
  state and the load average reads ~15 while the CPU is ~89% idle. It is a
  load-average accounting artifact, not CPU saturation. Do not kill the
  driver or use load average as a health metric.

Evidence: [PQ-MPFW-TASK](../../artifacts/reports/PQ-MPFW-TASK-2026-11-09.md),
[VENDOR-INVENTORY](../../artifacts/reports/VENDOR-INVENTORY-2026-11-07.md).

## Vendor inventory

VENDOR-INVENTORY-2026-11-07 hashed/dumped 461 ELF files (389 in `/vendor`,
72 outside), 41 kernel modules, 15 platform modules. Recovered call-site
inventories: 208 GPIO names, 45/45 VPLTIO immediates (39 unique), 58
`Hal_Audio_Ctrl` immediates (37 unique), 132 NDAL3 + 36 NDAL_EX distinct ioctl
immediates. Nine platform modules lacked a Ghidra project but have
objdump/relocation inventories.

Evidence: [VENDOR-INVENTORY](../../artifacts/reports/VENDOR-INVENTORY-2026-11-07.md),
[VENDOR-BROADCAST-ABI](../../artifacts/reports/VENDOR-BROADCAST-ABI-2026-11-05.md),
[VENDOR-DECODER-DIFF](../../artifacts/reports/VENDOR-DECODER-DIFF-2026-11-04.md).

## Privacy and telemetry

The replacement must not port telemetry, advertising, ACR, analytics,
profiling, advertising IDs, sponsored placements or ad delivery. The local
hook/keepalive blocks telemetry/portal/search/ACR endpoints (hosts bind,
iptables for the CID range), keeps internet consent at 1 and preserves local
SSH/telnet access. Evidence lives in the hook and `artifacts/privacy/`; do not
weaken these blocks for convenience.
