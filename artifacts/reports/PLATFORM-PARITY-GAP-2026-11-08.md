# Platform + stock-UI parity gap — 2026-11-08

Status: **current priority audit; firmware facts recovered offline, plus a
read-only TV state check. No code or hardware change.**

Owner direction (2026-11-08): platform and hardware support, then stock-UI
parity, come first. The native Omarchy shell is only a skin on top of the
platform; it is not evidence of platform parity. This report maps the gap and
records the cold-start facts recovered while the Herdr `grok-firmware` worker
recovers the device-stage half (`platform-dev-ini-grok-2026-11-08.md`).

## Cold-start stage map

Vendor order from `ICE_HardwareInit` `0x007551a0`:
`PLAT_DRV_INI_Init` `0x0089b6e4` then `DEV_INI_Init` `0x00853a58`; the caller
does not check `DEV_INI_Init`'s return (`0x00755650` is followed by mutex
creation at `0x00755654`).

`DEV_INI_Init` calls, in order, and ignores every stage result except logging:

| # | stage | address | current owner | gap |
|---:|---|---|---|---|
| 0 | `DEV_COMMON_SET_PCBLAYOUTTYPE(14)` | `0x00850288` | none; `plat` passes `tuner.pcb_layout=14` to the tuner backend | vendor global `DAT_0175f028` not set; any `DEV_COMMON_GET_PCBLAYOUTTYPE` consumer sees its default |
| 1 | `DEV_GPIO_Init` | `0x00851fcc` | `plat` calls `Hal_Gpio_Init` (NDAL library) | **different layer**: device opens `/dev/ntgpio`, creates semaphores and the 0x1fec shared state; `Hal_Gpio_Init` only opens a HAL fd |
| 2 | `dev_src_init` | `0x008fc654` | `mod_source` VIN select only | no `DEV_SRC_HDMIHotPlug` task, no source mutex, no `plat_vin_slot_set_auto_color_system(2)` |
| 3 | `DEV_PANEL_Init` | `0x008583fc` | `mod_panel` PWM/backlight HAL | copies GPIO 110/111 descriptions, writes the timing block at `0x019354b4`, gets PWM channel 108, sets curve 40/60/80/100; returns the channel index. Init does **not** light the panel: `DEV_PANEL_PowerSequence`/`PanelPowerSet`/`PanelBackLightPowerSet` and the panel-extra delays are still missing (panel-power-2026-11-08) |
| 4 | `DEV_NIM_Init` | `0x00854fd0` | `mod_tuner` HAL/NDAL/stub backend | creates `DEV NIM ATV`/`DEV NIM DTV` factories and the SI2151/MxL661 debug menus; it opens no tuner device itself |
| 5 | `DEV_IIC_Init` | `0x008531e8` | `plat` calls `Hal_Iic_Init` (8 buses) | **different layer**: device opens `/dev/ntkiic0` and stores a separate fd used by `DEV_IIC_*` |
| 6 | `DEV_AMP_Init` | `0x0084f4ac` | `mod_audio`/`mod_audio_amp` transport | `Hal_Audio_Ctrl(0x3a,{0x100})` is implemented opt-in; the `DEV_AMP_ID` profile/factory/GPIO task is not |
| 7 | `DEV_SPI_Init` | `0x0085946c` | `mod_spi` opt-in read-only transport, not autoloaded | creates/destroys the vendor semaphore; returns 0 if missing |
| 8 | `DEV_SPI_Open(0)` | `0x00858d0c` | same | `/conf/spi.bin` absent on this set, so the path is `open("/dev/ntkspi",2)`; do not open while stock holds fd 27 |
| 9 | `DEV_NIM_DTV_Init` | `0x00861678` | `mod_tuner` NDAL frontend | creates the DTV init task; models 0..3/default take `HAL_Demod_User_Initialize` + `Connect2HAL`, model4 skips; IR failure still marks init done |
| 10 | `DEV_MTD_Init` | `0x0085419c` | none needed | only registers a debug menu and returns 0; flash ioctls are not on this path |

### Exact orchestration facts

- `DEV_INI_Init` `0x00853a58` is best-effort for all ten stages. A zero result
  logs (`TD_DBG_Print(0x100,2,...)`), then the next stage runs. There is no
  abort and no return-value aggregation.
- `DEV_INI_Init` returns the `uxtb` of `DEV_MTD_Init`'s r0 (`0x00853ada` /
  `0x00853ade`). `DEV_MTD_Init` `0x0085419c` returns 0 on its normal path
  (`0x008541dc`), so `DEV_INI_Init` normally returns 0. `ICE_HardwareInit`
  ignores it. Do not use this return as a success signal.
- `DEV_COMMON_SET_PCBLAYOUTTYPE` `0x00850288` writes its argument to
  `DAT_0175f028` and returns 1 (`0x008502b8`).
- `DEV_GPIO_Init` `0x00851fcc` opens `/dev/ntgpio` with `open64(path,2)` (O_RDWR),
  creates two semaphores, allocates a 0x1fec-byte shared parameter block,
  reads the PCB layout, and initializes the GPIO/PWM description tables. It
  returns 1 on its success path (`0x008520b0`) and 0 on failure. It does not
  call `Hal_Gpio_Init`; `Hal_Gpio_Init` `0x4e7be3ec` in `libNDAL3.so` only
  opens its own HAL fd and returns void.
- `DEV_IIC_Init` `0x008531e8` opens `/dev/ntkiic0` O_RDWR if its global fd is
  -1, stores it in `DAT_017608c8`, and returns 1/0. It does not call
  `Hal_Iic_Init`; `Hal_Iic_Init` `0x4e7bee98` opens eight `/dev/ntkiicN` buses
  and a separate misc node.
- `dev_src_init` `0x008fc654` creates a mutex, creates the
  `DEV_SRC_HDMIHotPlug` task (stack 0x1000, priority 0x10), calls
  `plat_vin_slot_set_auto_color_system(2)`, and returns 1.
- `DEV_PANEL_Init` `0x008583fc` returns the nonzero PWM channel index from
  `DEV_PWM_Get_ChIdx(0x6c,...)` on success and 0 on failure. It reads
  `DEV_GPIO_GetDescription(0x6e)` and `(0x6f)`, sets panel timing globals, and
  calls `DEV_PANEL_SetBacklightPWMCurve(40,60,80,100)` before returning. The
  decompiler's `void` prototype is wrong; the epilogue moves r4 to r0
  (`0x008584ce`).
- `DEV_NIM_Init` `0x00854fd0` returns 1 explicitly (`0x00855008`), runs
  `DEV_NIM_Dbg_Init` once and `_initNimDevice`, gated by `DAT_019353fc`.
- `DEV_AMP_Init` `0x0084f4ac` returns 1 on success and 0 if
  `TD_OS_TaskCreate("DEV_AMP_ID", stack 0x1000, priority 3)` fails; the
  `Hal_Audio_Ctrl(0x3a,{0x100})` I2S-MCLK request is already implemented
  opt-in in `mod_audio`.
- `DEV_SPI_Init` `0x0085946c` creates a semaphore and returns 1/0.
  `DEV_SPI_Open(0)` `0x00858d0c` reads `/conf/spi.bin`; if it is absent and
  `DAT_019354ec` is 0, it opens `/dev/ntkspi` O_RDWR and returns 1/0.
- `DEV_NIM_DTV_Init` `0x00861678` and `DEV_MTD_Init` `0x0085419c` are being
  recovered in detail by the parallel worker; `DEV_MTD_Init` is confirmed to
  return 0.

### `PLAT_DRV_INI_Init` half (verified)

`PLAT_DRV_INI_Init` `0x0089b6e4` is the HAL/library half and returns 1
(`0x0089b760`); `ICE_HardwareInit` ignores it. Exact order:

1. `Hal_Audio_Protect_Init(0)` — the vendor passes an explicit 0; the NDAL
   body `0x4e7aeeb0` stores but does not read r0, returns 0 on first use and
   -1 if the 0x854-byte block was already initialised. `PLAT_DRV_INI_Init`
   ignores the return.
2. `Hal_Audio_Mixer_Ctrl_Init()`, `Hal_Audio_Proxy_Init()`.
3. `plat_common_init("DVIF")` `0x009247f8`, idempotent via
   `DAT_019e59a8`: `Hal_Video_Vpltio_Init`, `Hal_Video_V4l2InfoInit`,
   `Hal_Video_DrmShareMemCreate`, `Hal_Video_DrmInit(1)`, then
   `Hal_Video_Vpltio_SendCmd(0x302,0,&src,0,0,0,0,0,0)` with `src=1` for
   DVIF (or 0 for SIF). A nonzero return logs "fail to init TD_Common_Init".
4. `Hal_Gpio_Init()`; nonzero logs failure.
5. `Hal_Iic_Init()`; nonzero logs failure.
6. `plat_vin_slot_init()`; zero is success (the vendor logs only on nonzero).
7. `NDAL_VideoExt_OpenDevice(0,10)`.
8. `PLAT_SP_DebugRegisterMenu(2,0x100000,"Hal_Layer",...)`.

Our `mod_plat` reproduces steps 1-7 in the same order, but it calls the HAL
`Hal_Gpio_Init`/`Hal_Iic_Init`, not the `DEV_*` device layer. The explicit
`Hal_Audio_Protect_Init(0)` argument is not currently passed (the function
ignores it); this is a cosmetic ABI mismatch, not a proven effect.

### Device-stage contracts (parallel worker)

Full instruction-level detail is in
`artifacts/analysis/platform-dev-ini-grok-2026-11-08.md`. The merged facts:

- `DEV_PANEL_Init` copies GPIO descriptions 110/111 into `0x0193545c`/
  `0x01935494`, writes the timing block `{1,10,40,800,220,55,10}` at
  `0x019354b4`, gets PWM channel 108, and calls
  `DEV_PANEL_SetBacklightPWMCurve(40,60,80,100)`. It returns the PWM channel
  index (1 on success), not a generic boolean. It does not call
  `DEV_PANEL_SetPWMBacklightLv` or any panel-power function; those remain a
  separate owner.
- `DEV_NIM_Init` is idempotent through `0x019353fc`, runs
  `DEV_NIM_Dbg_Init` (SI2151/MxL661 menus), then creates the `DEV NIM ATV`
  and `DEV NIM DTV` factory objects. It opens no tuner/demod device itself.
- `DEV_AMP_Init` sends `Hal_Audio_Ctrl(0x3a,{0x100})` (return ignored) and
  creates the `DEV_AMP_ID` task (stack 0x1000, priority 3). The task reads
  `Profile_GetMainSpeakerPower`/`Profile_GetDRC`, creates the amp factory and
  gets its GPIO descriptions; the board AD82587 sequencer remains the later
  AMP-INIT/AMP-MODULE contract.
- `DEV_SPI_Init` creates/replaces the vendor semaphore. `DEV_SPI_Open` reads
  `/conf/spi.bin` if present; on this set it is absent, so the vendor path is
  `open("/dev/ntkspi",2)`. Aurora already holds `/dev/ntkspi` at fd 27; do not
  open a second owner while stock runs.
- `DEV_NIM_DTV_Init` creates the DTV init task. Models 0..3 and the default
  take `HAL_Demod_User_Initialize` + `_dev_nim_dtv_Connect2HAL`; model 4
  skips them. The task's IR IIC chain (codes 21/17/64, bit 8/bit 2) marks
  init complete even when the IR helper fails.
- `DEV_MTD_Init` only registers a debug menu and returns 0. No cold-start
  owner is required; flash ioctls are a separate mutating contract.

### Panel power sequence (new)

`artifacts/analysis/panel-power-2026-11-08.md` recovers the missing panel
power owner:

- `DEV_PANEL_PanelPowerSet(on)` writes the GPIO 110 (PANELPWR) description
  from `DEV_PANEL_Init`; `PanelBackLightPowerSet(on)` writes GPIO 111
  (BKLTPWR). Both have layout/TCON branches and are not a plain `SetLevel(on)`.
- `DEV_PANEL_PowerSequence(on)` is the real on/off sequence: backlight off,
  70 ms, backlight-power off, `extra+4` delay, PWM/LVDS mute, `extra+6`
  delay, TCON PMIC GPIO and panel power off; on is panel power, TCON PMIC,
  `extra+0` delay, LVDS unmute, PWM level, `extra+2` delay, backlight power.
  The delays come from `PLAT_VIU_GetPanelExtraStruct`, not the Init block.
- `DEV_PANEL_SetPowerSequenceParams(NULL)` restores the Init block
  `{1,10,40,800,220,55,10}`; that block is distinct from the panel-extra
  timing fields.
- The panel-extra struct is `0x019d4d64` (0x5e bytes), filled by
  `PLAT_VIU_init_panelextrastruct` -> `FUN_008fec68`, a parser for a panel text
  file whose path is supplied by `FUN_007800ac` and is not yet decoded. The
  timing values must be read from that file/vendor state, not guessed.
- LVDS mute is VPLT command `0x1cc` with a one-byte value
  (`plat_vpltio_mute_lvds`). TCON PMIC is GPIO 209, gated by panel-extra+0x4a
  and `0x01935539` (`DEV_TCON_CHIPSET_Set_PMIC_GPIO`).
- Runtime stock panel-extra (read-only): `+0=30`, `+2=580`, `+4=220`,
  `+6=30`, `+8=100` (PTF PWM 1000), `+0x4a=0`. GPIO 110 pin/flag = 110/1;
  GPIO 111 = 111/0; PWM 108 = channel 0; high period 1000; TCON gate 0.
  Effective on: panel power (gated), 30 ms, LVDS unmute, PWM 1000, 580 ms,
  backlight on. Off: 70 ms, backlight off, 220 ms, PWM/LVDS mute, 30 ms,
  panel off.

`mod_panel` already implements `panel_power_on/off` and the LVDS VPLT command
behind `panel.standby`, with the runtime defaults above. It now also has
`panel.power_on_start` (default 0) to run the power-on sequence at `m_start`
for a cold boot. `tests/panel_standby.c` covers both orders with fake
PWM/GPIO/VPLTIO. The TCON PMIC gate is 0 on this set, so that step is
disabled; the missing acceptance is visible panel-on/off and a cold boot, not
the sequence body.

### Read-only TV state observed

The worker checked stock without stopping it. `/proc/fb` names: fb0
`OSD_NORMAL1`, fb1 `OSD_NORMAL2`, fb2 `OSD_NORMAL3`, fb3 `OSD_CURSOR1`,
fb4 `OSD_CURSOR2`; `/dev/dri/card0`, `card1`, `controlD64`, `controlD65`
exist. Aurora 1770 and appmgr 1567 hold fb0-fb2, card0 and `/dev/ntkgpu`;
Aurora also holds `/dev/ntgpio` (fds 16/24), `/dev/ntkiic0` (23/26),
`/dev/ntkled` (25), `/dev/ntkspi` (27) and the vplt/dvif/sys nodes.
`/tmp/ntkdriver_completed` exists; `/conf/spi.bin` does not. Loaded modules
include `ntkdriver`, `ntkndpalsa`, `ntkndptuner`, `ntkndpdemod`,
`ntkndpextin`, `ntkndpdrm`, `ntkndphdmi`, `mali`, `PreInit`. This confirms
that stock owns the OSD/DRM nodes; a replacement must not take a second
scanout owner while it is running.

### Ownership consequence

Our `plat` reproduces the **HAL/library** half (`Hal_Gpio_Init`,
`Hal_Iic_Init`, VIN slot init, video-device open) but not the vendor
**device-layer** stages. Calling a vendor `DEV_*` function without the matching
`DEV_GPIO_Init`/`DEV_IIC_Init` is unsafe: the device-layer globals
(`DAT_0175f0a4`, `DAT_017608c8`, the 0x1fec GPIO state) are unset. The correct
cold-start design is one owner per stage: either implement the device-layer
equivalent in OpenAurora, or keep our modules strictly on the HAL layer and
never call vendor `DEV_*` code. Do not call `DEV_INI_Init` wholesale from
`plat` under stock Aurora.

### OSD cold reinit (vendor SetPar path)

`display.osd_reinit=1` forces the vendor kernel `_ker_osd_fb_SetPar` path even
when `fb_var` already looks valid: `FBIOPUT_VSCREENINFO` with the recovered
fb1 template (BGRA 32bpp, `transp` offset24/length8, 1920x1080, two frames),
then `FBIOGET_*` verification. The module rejects a stride below
`xres*4` or not 32-byte aligned, and rejects a missing alpha field. The
kernel SetPar performs alpha-blend enable, alpha255, color mode, aligned
stride and display-window setup; the module then writes the region and the
first `FBIOPAN_DISPLAY` commits/binds the buffer. Resume reopens the layer and
repeats the reinit before recommitting the retained scene.

This is the vendor fbdev/SetPar path, not a duplicate DRM/private-GEM or MMIO
implementation. Host tests: `check-display` covers forced reinit on a valid
var (format/BGRA/alpha/stride assertions, region-before-pan, resume reinit),
and `check-cold-config`; the ARM `mod_display.so` and cold bundle rebuild and
verify. It is not TV cold-scanout acceptance.

### Cold-start profile candidate

`openaurora/tools/boot/openaurora-cold.conf` is a separate cold-boot profile,
not the takeover config:

- `plat.drm_init=1`, `display.restore_snapshot=0`, `display.cold_init=1`,
  `display.osd_reinit=1`;
- `panel.standby=1`, `panel.power_on_start=1`;
- `audio.i2s_mclk=1`; `spi.enable=1` autoloaded before `tuner`;
- stock `ui_vendor` stack (no `ui_omarchy`);
- `source.enable=0` and `cec.enable=0` remain gated.

`make cold-replacement-bundle` builds the 33-module ARM candidate and its
manifest verifies. `make check-cold-config` checks the cold/takeover flags,
the `ui_vendor` selection and `spi` before `tuner`; it passes. The host/qemu
`--check` cannot run because its child loader needs the TV's ARM interpreter;
the real preflight belongs to a later TV/cold-boot gate. This is a profile
artifact, not cold-boot acceptance.

**HAL-only ownership evidence:** every ARM module in `build-tv-v3/modules`
was checked with `nm -D -u | grep ' DEV_'`; there are no undefined vendor
`DEV_*` symbols. The modules use the NDAL HAL (`Hal_Gpio_*`, `Hal_Iic_*`,
`Hal_Pwm_*`) or implement the device behavior themselves. The device-layer
`DEV_GPIO_Init`/`DEV_IIC_Init` are therefore not part of this replacement's
cold path; do not add them merely for parity with the vendor call graph.

## Stock-UI parity map

The stock UI is the vendor skin (`ui_vendor`), not the native Omarchy shell.

| stock screen/function | current evidence | gap |
|---|---|---|
| portal/home (5 rail slots, 12 cards, weather/clock) | portal paint, rail assets and TV takeover accepted (REPLACEMENT-SYSTEM, TAKEOVER-ACCEPT) | modal/menu hierarchy, focus animation and all localized text not complete |
| picture/sound/system settings bands | settings panel/bands, sub-launcher tree/values and picture store recovered; QEMU AE 0 (QEMU-UI-LAYOUT, SETTINGS-*) | live vendor settings store writes, all tabs/values, sound/CEC/system screens |
| Bronlijst/source list | VIN ids and UI rows decoded; `mod_source` gated | full `PLAT_VIU_ChangeInput`/V4L2/DRM/audio connect path; see SOURCE-VIN and NATIVE-REPLACEMENT |
| apps grid/launcher | renderer parity and catalog recovered (STOCK-APPS-GRID-PARITY, LAUNCHER-*) | live launcher DB/icon path, focus/scroll animation, app launch profile |
| channel list / EPG / PVR | provisional channel screen, EPG/PVR services and fixtures (CHANNEL-LIST-UI, EPG-*, PVR-*) | vendor screen geometry, live EASI/EIT feed, tuner lock, recording |
| volume/mute OSD | native OSD tested; stock external-output mute is a no-op (NATIVE-REPLACEMENT) | vendor OSD geometry/behavior and CEC volume policy |
| CEC/standby/screensaver | CEC service/audio policy, panel/standby stubs | native CEC TX/RX, standby/power and screensaver |
| app launch (Netflix/Amazon/YouTube) | `oa-app-launch` host-tested; `app_launch.enable=0` | debug profile disables them; boot/profile gate and NAM supervision |

## Prioritized next work

1. **Cold-start stage ownership** (this report + Grok's merged device-stage
   report). Implement one owner per stage, in order: GPIO, IIC, src, panel,
   NIM, amp, SPI, NIM-DTV, then MTD (no owner needed). `DEV_INI_Init`'s return
   is not a gate. Panel init does not light the panel, so keep a separate
   power/backlight owner; enable SPI and do not open `/dev/ntkspi` while stock
   holds it; run NIM-DTV only after IIC/NIM. Do not call vendor `DEV_*` code
   without its device-layer state. The first panel-power slice is the recovered
   `PanelPowerSet`/`PanelBackLightPowerSet`/`PowerSequence` order and delays.
2. **Real display/OSD cold enable** (OSD-BUFFER-CONTRACT, DRM-PLANE-CONTRACT,
   COLD-DISPLAY-REPAIR): complete format/stride/region/enable/alpha/update and
   resume, then a cold scanout acceptance. `plat.drm_init=0` is a takeover
   workaround, not parity.
3. **Source/VIN path**: the connect half is implemented behind
   `source.enable=0` (type gates, per-type opens, VPLT `0x4c4`, V4L2Info/
   FdCommon, DRM dpsource, NDAL VideoExt, Audio EasySelect) and fake-HAL
   tested in SOURCE-PATH-2026-11-09. A 2026-11-09 controlled takeover run
   proved the connect/disconnect HAL path live and painted the OSD, but also
   showed that `plat.drm_init=0` leaves the DRM device id at -1: a source
   switch requires `plat.drm_init=1`. The HDMI picture, audio and cold boot
   are still unconfirmed. Finish `NdVideoSetting`/VinParam/TDLayer, HDMI PHY
   and the high-level AudioIF transport; keep the gate 0 until then.
4. **Audio amp/route**: native AD82587 backend, reset/mute/standby, speaker
   gain/SPDIF, audible acceptance.
5. **Tuner/NIM-DTV**, **CEC ownership/TX**, **panel/power/watchdog**, **IR
   repeat**, then the remaining stock-UI screens.

Do not use the native Omarchy shell to claim platform or stock-UI parity.

## Evidence

Offline: aurora.elf MD5 `64c252340d3af6c5d66072611b7bad7a`; addresses above
were read with `DumpFirmware.java --asm` (read-only) and the
`artifacts/workbench/aurora.dis` cache. libNDAL3 MD5
`c842bad804302ec822076c3d9d3ce31d`. TV state was checked read-only (stock
appmgr 1567 / aurora 1770, no OpenAurora process); no device, display, audio
or settings write was made.
