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
| 3 | `DEV_PANEL_Init` | `0x008583fc` | `mod_panel` PWM/backlight HAL | vendor panel power/timing/GPIO description and backlight curve are not run |
| 4 | `DEV_NIM_Init` | `0x00854fd0` | `mod_tuner` HAL/NDAL/stub backend | vendor NIM device creation and debug registration are not run |
| 5 | `DEV_IIC_Init` | `0x008531e8` | `plat` calls `Hal_Iic_Init` (8 buses) | **different layer**: device opens `/dev/ntkiic0` and stores a separate fd used by `DEV_IIC_*` |
| 6 | `DEV_AMP_Init` | `0x0084f4ac` | `mod_audio`/`mod_audio_amp` transport | `Hal_Audio_Ctrl(0x3a,{0x100})` is implemented opt-in; the `DEV_AMP_ID` task is not |
| 7 | `DEV_SPI_Init` | `0x0085946c` | `mod_spi` opt-in read-only transport, not autoloaded | vendor semaphore/init state is not run |
| 8 | `DEV_SPI_Open(0)` | `0x00858d0c` | same | `/conf/spi.bin`, `/dev/ntkspi`, and the vendor open path are not run |
| 9 | `DEV_NIM_DTV_Init` | `0x00861678` | `mod_tuner` NDAL frontend | vendor NIM-DTV task/initialization is not run |
| 10 | `DEV_MTD_Init` | `0x0085419c` | none needed | only registers a debug menu |

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

1. **Cold-start stage ownership** (this report + the parallel worker). Decide
   device-layer ownership per stage, then implement a `DEV_INI`-equivalent
   module or extend `plat` without duplicating stock ownership.
2. **Real display/OSD cold enable** (OSD-BUFFER-CONTRACT, DRM-PLANE-CONTRACT,
   COLD-DISPLAY-REPAIR): complete format/stride/region/enable/alpha/update and
   resume, then a cold scanout acceptance. `plat.drm_init=0` is a takeover
   workaround, not parity.
3. **Source/VIN full path**: finish `PLAT_VIU_ChangeInput`, `plat_drm_connect_
   dpsource`, `NDAL_VideoExt_OpenDevice`, V4L2Info/FdCommon and the audio
   companion. Keep `source.enable=0` until then.
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
