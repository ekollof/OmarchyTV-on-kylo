# Cold-start sequence audit — 2026-11-10

Status: **current offline instruction audit, host fix and no-UI ladder. TV was
not contacted and no UI was loaded.** The HAL half of the vendor cold start is
instruction-exact. A hardware-only `hw0..hw4` ladder is implemented and
host-tested. The device half (`DEV_INI_Init`) is still not called by the
replacement; the gap disposition below closes the stages with no HAL-path
consumer and leaves the real remaining effects open. Full vendor cold-start
parity is therefore **not** claimed yet.

Last verified: 2026-11-10 (firmware instructions, data-reference queries and
host fake-HAL tests; TV not contacted).
Supersedes: the HAL return/sequence claims in COLD-START-STAGE-AUDIT-2026-11-06
and the panel off-path/PWM claim in PANEL-BACKLIGHT-2026-11-07; those reports
are marked historical/corrected at source.
Evidence class: firmware instructions plus host tests. No on-TV or qemu
hardware-effect acceptance.

Sources: `aurora.elf` MD5 `64c252340d3af6c5d66072611b7bad7a` (Ghidra project
`artifacts/workbench/ghidra_proj`, program `aurora.elf`, `-readOnly
-noanalysis`); `libNDAL3.so` MD5 `c842bad804302ec822076c3d9d3ce31d`
(`ghidra_ndal3`); `artifacts/analysis/platform-dev-ini-grok-2026-11-08.md`;
`artifacts/reports/PLATFORM-PARITY-GAP-2026-11-08.md`.

## Vendor order

`ICE_HardwareInit` `@0x007551a0` waits for `/tmp/ntkdriver_completed`, then
calls `PLAT_DRV_INI_Init` `@0x0089b6e4` (`0x0075564c`) and `DEV_INI_Init`
`@0x00853a58` (`0x00755650`). It does not test either return.

### `PLAT_DRV_INI_Init` (HAL/library half)

Instruction order from `0x0089b6e4`:

| # | call | result handling |
|---:|---|---|
| 1 | `Hal_Audio_Protect_Init(0)` | return ignored; the body stores r0 but never reads it |
| 2 | `Hal_Audio_Mixer_Ctrl_Init()` | return ignored |
| 3 | `Hal_Audio_Proxy_Init()` | return ignored |
| 4 | `plat_common_init("DVIF")` `@0x009247f8` | `void`/always 0; idempotent through `DAT_019e59a8`; first call runs Vpltio/V4L2/sharemem/`Hal_Video_DrmInit(1)`/`SendCmd(0x302,0,&byte=1,...)` |
| 5 | `Hal_Gpio_Init()` | nonzero prints `fail to init Hal_Gpio_Init.` and continues |
| 6 | `Hal_Iic_Init()` | nonzero prints `fail to init Hal_Iic_Init.` and continues |
| 7 | `plat_vin_slot_init()` `@0x0092e9c4` | always returns 0; fills 7×9 slot words, 7×3 side words and zeroes 0x150 state bytes |
| 8 | `NDAL_VideoExt_OpenDevice(0,10)` | return ignored |
| 9 | `PLAT_SP_DebugRegisterMenu(2,0x100000,"Hal_Layer",...)` | debug menu only |

The epilogue sets `r0=1`, but the function is used as `void`; `ICE_HardwareInit`
ignores it. `Hal_Audio_Protect_Init` returns 0 on first use and -1 if its
0x854-byte block was already initialised. `Hal_Gpio_Init` returns 0 on a
successful `/dev/ntgpio` open and -1 on failure. `Hal_Iic_Init` opens eight
`/dev/ntkiicN` buses plus the misc node and returns 0/-1.

### `DEV_INI_Init` (device half)

From `0x00853a58` and the device analysis; every stage failure only prints and
continues. `DEV_COMMON_SET_PCBLAYOUTTYPE(14)` returns 1 and its return is
ignored. `DEV_INI_Init` returns the low byte of `DEV_MTD_Init`, which returns 0.

| # | stage | address | recovered effect | current replacement |
|---:|---|---|---|---|
| 1 | `DEV_COMMON_SET_PCBLAYOUTTYPE(14)` | `0x00850288` | stores 14 in `DAT_0175f028`, returns 1 | tuner config only; global not set |
| 2 | `DEV_GPIO_Init` | `0x00851fcc` | opens `/dev/ntgpio` O_RDWR, two semaphores, 0x1fec state, GPIO/PWM description tables | HAL `Hal_Gpio_Init` only |
| 3 | `dev_src_init` | `0x008fc654` | mutex + `DEV_SRC_HDMIHotPlug` task (stack 0x1000, prio 0x10) + `plat_vin_slot_set_auto_color_system(2)` | `mod_source` VIN select, gated off |
| 4 | `DEV_PANEL_Init` | `0x008583fc` | copies GPIO 110/111 descriptions, timing block `{1,10,40,800,220,55,10}`, PWM ch108, curve 40/60/80/100; returns channel index | `mod_panel` PWM/curve/power; PTF off order now matched; descriptions replaced by direct HAL pins |
| 5 | `DEV_NIM_Init` | `0x00854fd0` | one-shot `DEV NIM ATV`/`DEV NIM DTV` factories + debug menus; always returns 1; no device node opened | `mod_tuner` direct HAL/NIM-DTV; factories not created |
| 6 | `DEV_IIC_Init` | `0x008531e8` | opens `/dev/ntkiic0` O_RDWR, stores device fd, returns 1/0 | HAL `Hal_Iic_Init` only; no DEV consumer |
| 7 | `DEV_AMP_Init` | `0x0084f4ac` | `Hal_Audio_Ctrl(0x3a,{0x100})` ignored; creates `DEV_AMP_ID` task (stack 0x1000, prio 3); returns 1/0 | `audio.i2s_mclk=1` sends 0x3a; task missing; `audio_amp.enable=0` |
| 8 | `DEV_SPI_Init` | `0x0085946c` | creates/replaces semaphore at `0x019354f0`, returns 1/0 | `mod_spi` sole owner; host loop serializes access |
| 9 | `DEV_SPI_Open(0)` | `0x00858d0c` | `/conf/spi.bin` `rb+`, else `/dev/ntkspi` O_RDWR; returns 1/0 | `mod_spi` file/device open; equivalent boundary |
| 10 | `DEV_NIM_DTV_Init` | `0x00861678` | one-shot task; models 0..3/default take demod+Connect2HAL, model4 skips; IR failure still marks done | `mod_tuner` `tuner.hw_init=1` NIM-DTV task |
| 11 | `DEV_MTD_Init` | `0x0085419c` | registers a debug menu only, returns 0 | no owner needed |

## Gap disposition (instruction/data-reference evidence)

| stage | finding | disposition |
|---|---|---|
| PCB layout | `DAT_0175f028` is read only by `DEV_COMMON_GET_PCBLAYOUTTYPE` (`0x00850272`); no replacement module calls it. `DEV_GPIO_Init` reads it into state+0x7fa. | **Not applicable to the HAL-only path.** The tuner receives layout 14 explicitly. A device-layer implementation must set this global before its GPIO init. |
| DEV GPIO | The 0x1fec state pointer `0x01931270` is read only by `DEV_GPIO_GetDescription`; the `/dev/ntgpio` fd `0x0175f0a4` is read only there. `DEV_GPIO_Init` also starts an earphone-handler task and initializes PWM/descriptions. | **Not applicable for HAL GPIO.** HAL modules use their own device paths. The earphone hotplug task is still open if audio-jack events are required; do not fake it. |
| DEV IIC | The `/dev/ntkiic0` fd `0x017608c8` is read only by `DEV_IIC_GetDescription` (`0x008532d4`); no replacement module calls it. | **Not applicable for HAL IIC.** `Hal_Iic_Init` owns the eight HAL buses. |
| source init | `dev_src_init` creates the `DEV_SRC_HDMIHotPlug` task and calls `plat_vin_slot_set_auto_color_system(2)`; it returns 1. The task body at `0x008fc3b0` resolves GPIO 204..207 (`HDMI_HOTPLUG_1..4`) and 193 (`HDMI_HOTPLUG_5`), drives inactive, reads MHL CD sense 192, waits 500 ms, then drives active. | **Implemented and gated**: `common/src_hotplug.c` reproduces the one-shot pulse (including the MHL-high skip of GPIO 204 and the post-delay MHL re-read); `common/dev_gpio.c`/`dev_gpio_vendor.c` implement the runtime descriptor/LED level provider from `DEV_GPIO_GetDescription`/`GetLevel`/`SetLevel`; `mod_source` runs it on a worker only with `source.enable=1` and `source.hotplug_pulse=1` (both default 0). Live TV read-only query: 204..207 -> pins 274..277, output, normal/active low; 192/193 absent (errno 524). The pulse was run on the TV on 2026-11-11 (STBC 74..77 values 2/3; the connected Deck re-ran HDCP22); see HOTPLUG-HW-2026-11-11. `plat_vin_slot_set_auto_color_system(2)` is implemented behind default-off `source.auto_color_system`; the VIN(0) ioctl `0x40085618` with `{0x00ff40ff,0}` was traced on the TV on 2026-11-11; see AUTO-COLOR-SYSTEM-2026-11-11. |
| panel init | The timing halfwords at `0x019354b8..c0` have only writes (`DEV_PANEL_Init`, `DEV_PANEL_SetPowerSequenceParams`); the only read of the block is the mode byte at `0x019354b4` by `DEV_PANEL_PowerSequence` (`0x00857bf8`). GPIO 110/111 copies are read only by the two DEV panel-power helpers. | **Closed for the stock layout 14**: `mod_panel` uses direct HAL pins/levels and now follows the PTF off order (PWM at `panel.off_pwm=1000` before LVDS off). Other panel layouts/TCON BIN branches remain unverified. |
| NIM init | `DEV_NIM_Init` opens no device node; its factory result only selects a debug string and the factory objects are vendor-internal dispatch tables. | **Not applicable to the direct `mod_tuner` NIM-DTV task.** |
| amp init | `DEV_AMP_Init` sends the recovered MCLK request and creates `DEV_AMP_ID`; the task body at `0x0084ebf0` reads `Profile_GetMainSpeakerPower`/`Profile_GetDRC` (`SpeakerAmp`, `6Watt`, `6Watt_12V`, `8Watt`), selects `AMP_AD82587`, copies its default resource row (row 1: reset GPIO 114, mute GPIO 208), and resolves the GPIO descriptions. | **Effect covered by existing modules**: `audio_amp_profile` decodes `DefaultSpeaker`/DRC, `amp_board` uses reset 114 / mute 208, and `audio_amp` drives the direct AD82587 register sequence. The vendor `DEV_AMP_ID` factory/monitor task is **not reproduced**; amp remains gated (`audio_amp.enable=0`) with no audible acceptance. |
| SPI init | The semaphore at `0x019354f0` is used by `DEV_SPI_Wait/Read/Write/Close`; `mod_spi` is the sole SPI owner and host dispatch is single-threaded. | **Not applicable to the current single-owner module.** Do not add a fake semaphore; the boundary is already serialized. |
| NIM-DTV | Task model/IR failure behavior matches `mod_tuner` `tuner.hw_init=1`. | **Closed** for the recovered model 0 path. |
| MTD | `DEV_MTD_Init` only registers a debug menu and returns 0. | **Closed**: no hardware owner. |

**Panel correction:** `mod_panel` now writes `panel.off_pwm` (default 1000)
before LVDS off, matching the stock PTF branch; the earlier “PWM 0 after LVDS
off” claim is corrected at source in PANEL-BACKLIGHT-2026-11-07. The fake-HAL
`check-panel-curve`/`panel_standby` test asserts the new order.

### DEV_SRC_HDMIHotPlug recovery

The task body at `0x008fc3b0` (created by `dev_src_init` at `0x008fc654` with
stack `0x1000`, priority `0x10`) is one-shot, not a continuous watcher:

1. At index 0, resolve MHL 192 and read its level. If it is high, log
   `#DEV-SRC#MHL CD_SENSE Is High` and skip GPIO 204 for this loop; a read
   failure logs `%s,Get CDSense level of pin %d failed` and continues.
2. Resolve GPIO descriptors 204, 205, 206, 207 (`HDMI_HOTPLUG_1..4`) and 193
   (`HDMI_HOTPLUG_5`). A missing descriptor logs
   `#DEV-SRC#Not found HDMI%d HotPlug Definition`.
3. Drive each present pin to its inactive level (`active_level ? 0 : 1`).
4. Delay 500 ms.
5. Re-read MHL at index 0 with the same high-skip rule.
6. Drive each present pin to its active level, logging
   `End toggle PIN=%d, Active level=%d` and
   `#DEV-SRC#Toogle HDMI%d HotPlug Successfully`.

The MHL skip and re-read are firmware behavior, not an optimization: a level
that changes during the 500 ms delay changes whether GPIO 204 is driven
inactive, active, or neither.

The runtime descriptor layer is now implemented. `DEV_GPIO_GetDescription`
(`0x00850c38`) sends ioctl `0xc088671f` on `/dev/ntgpio` with a zeroed 0x88
buffer and the 24-byte DTS name at `+0x80`; the kernel returns the pin at
`+0` and `active` at `+0x38`. `DEV_GPIO_GetLevel`/`SetLevel` use
`Hal_Gpio_Read/Write` below pin 200 and
`NDAL_VideoExt_STBC_Get/SetStbcGpio((pin+0x38)&0xff, ...)` at or above 200
(value 2 for level 1, 3 for level 0).

Live TV read-only descriptor query (2026-11-11): `HDMI_HOTPLUG_1..4` are
pins **274..277**, mode output, normal low, active low; `MHL_CDSENSE` and
`HDMI_HOTPLUG_5` are absent (`errno 524`). The four present pins therefore
use STBC channels **74..77**. The pulse is wired in `mod_source` behind
`source.enable=1` and the new default-off `source.hotplug_pulse=1`; the worker
is joined by quiesce/stop. The actual pulse has **not** been run on the TV
(STBC hardware effect), and the separate
`plat_vin_slot_set_auto_color_system(2)` call from `dev_src_init` is still
open. Full evidence: [DEV-GPIO-PROVIDER-2026-11-11](DEV-GPIO-PROVIDER-2026-11-11.md).

### DEV_AMP_ID recovery

The task body at `0x0084ebf0` is a profile/resource selector, not the AD82587
register writer:

- `Profile_GetMainSpeakerPower` returns `SpeakerAmp`/`6Watt`/`6Watt_12V`/
  `8Watt`; `Profile_GetDRC` selects the DRC section.
- The task allocates the amp type string, parses the resource table with
  `TD_DBG_StrToNum`, calls `DEV_AMP_FACT_Create` (`AMP_AD82587`), copies the
  default resource row from `0x01763ef0` (row 1: reset GPIO 114, mute GPIO
  208), and resolves the GPIO descriptions.
- It then creates the vendor amp task (stack `0x1000`) for the factory.

Our `audio_amp_profile` already decodes `DefaultSpeaker`/DRC from the same
hwprofile, `amp_board` uses reset 114 / mute 208, and `audio_amp` owns the
direct AD82587 register sequence/owner worker. The vendor factory/monitor task
is still not reproduced; `audio_amp.enable=0` and no audible/on-TV acceptance
is claimed.


## What changed in the replacement

`openaurora/modules/plat/plat.c` previously did three things differently:

1. It called `Hal_Audio_Protect_Init()` with no argument. The vendor passes an
   explicit 0. The prototype is now `int Hal_Audio_Protect_Init(int)` and the
   call is `Hal_Audio_Protect_Init(0)`.
2. It aborted the platform start if any of Protect/Mixer/Proxy returned
   nonzero. The vendor ignores all three and continues. A nonzero return is now
   logged and recorded in the diagnostic stage mask, but later stages run.
3. It reran the whole common-init block on every start. The vendor
   `plat_common_init` is idempotent; the first call now sets a process-global
   flag and later starts skip the block. The DVIF source value is now a
   one-byte `unsigned char`, matching the vendor pointer.

The test `tests/plat_audio.c` previously asserted the opposite (prelude
failures gate later stages). It now asserts the vendor order and that a full
prelude failure still reaches GPIO/IIC/VIN/video-device. `tests/plat_drm_gate.c`
resets the process-global flag so its two gate cases remain independent.

Host tests that pass:

```text
make check-cec-audio          # plat_audio: exact order, prelude continuation, idempotence
make check-plat-drm-gate      # drm_init 0/1 gate
make check-plat-startup-wait  # driver-marker wait/refusal/retry
make check-plat-power-sync-stop
make check-panel-curve        # includes the corrected panel_standby PTF off order
make check-hw-stage-profiles  # hw0..hw4, no display/UI
make check-hw-ready           # receipt only after all autoload starts succeed
```

These are fake-HAL tests; they prove order and state, not hardware effects.

## Hardware-only ladder (offline, no UI)

Added `openaurora/tools/boot/openaurora-cold-hw.conf` and
`openaurora/tools/boot/stage-hw-profile.sh` for `hw0..hw4`. The order is the
documented device-stage order, restricted to effects with a current owner:

| stage | autoload | added stage | vendor evidence |
|---|---|---|---|
| hw0 | `plat` | `PLAT_DRV_INI_Init` HAL half | `0x0089b6e4` |
| hw1 | `plat,panel` | `DEV_PANEL_Init` / panel power | `0x008583fc`, panel-power analysis |
| hw2 | `plat,panel,audio` | `DEV_AMP_Init`'s `0x3a/0x100` MCLK request | `0x0084f4ac`; task **not** included |
| hw3 | `plat,panel,audio,spi` | `DEV_SPI_Init`/`DEV_SPI_Open(0)` boundary | `0x0085946c`, `0x00858d0c` |
| hw4 | `plat,panel,audio,spi,tuner` | `DEV_NIM_DTV_Init` task | `0x00861678` |

`hw1` runs `mod_panel`'s recovered standby/power sequence behind
`panel.standby=1`/`panel.power_on_start=1`. `DEV_PANEL_Init` itself does not
light the panel; the stock cold-boot panel power comes from
xboot/kernel/PreInit. `hw1` is therefore a project test of the recovered power
sequence, not a claim that the vendor `DEV_INI_Init` stage powers the panel.

No stage autoloads `display`, `ir`, `settings`, or any `ui_*`. PCB layout,
DEV GPIO/IIC, `dev_src_init`, NIM factories, `DEV_AMP_ID` and MTD are **not**
listed as passing stages because they have no documented replacement effect.

The base profile sets `host.ready_after_autoload=1`; `core/main.c` writes the
supervisor receipt only if every autoloaded module loaded and `start()` returned
0. That is a sequence-completion marker, not a hardware-effect claim. `hw0..hw4`
use the normal `oa_stage` selector (`openaurora-stagehwN.conf`), boot gate and
`oa-supervise`; no UI fallback exists in these profiles.

Host tests:

```text
check-hw-stage-profiles  # no UI/display modules; order and flags
check-hw-ready           # receipt only after all autoload starts succeed
check-boot-gate          # hw2 recorded/cleared
check-appmgr-entry       # hw2 selects openaurora-stagehw2.conf
```

## Verdict

- **HAL half:** exact for the implemented `mod_plat` path, including the
  vendor's non-aborting return handling and `plat_common_init` idempotence.
- **Device half:** not implemented as a single owner. Some effects are covered
  by HAL/module equivalents (SPI open, NIM-DTV, panel PWM/power, amp MCLK),
  some are gated off (source hotplug, amp task), and some vendor globals/tables
  are absent (PCB layout, DEV GPIO/IIC descriptions, NIM factories, panel
  timing).
- **Therefore the vendor cold-start sequence is not 100% reproduced yet.**
  Do not load UI as evidence of cold-start correctness.

## Required next order

1. In the next authorized TV window, run `hw0..hw4` one stage per boot, with no
   display/UI modules, and record the stage-specific log/control state. The
   ladder and ready receipt are host-tested only; no hardware effect is proven.
2. Decide the target for the device half. The gap disposition above closes
   PCB layout, DEV GPIO/IIC description state, panel timing/GPIO copies, NIM
   factories, SPI semaphore and MTD for the HAL-only path. `dev_src_init`
   hotplug is now implemented and wired behind `source.enable=1` +
   `source.hotplug_pulse=1` (default off), with the live pin/active values
   recovered read-only. The real remaining device effects are the vendor
   `DEV_AMP_ID` factory/monitor task, the GPIO earphone handler and
   non-layout-14 panel/TCON branches. The `dev_src_init` auto-color-system
   call is implemented behind `source.auto_color_system` and hardware-traced
   (AUTO-COLOR-SYSTEM-2026-11-11). Implement the remaining effects only from
   firmware evidence.
3. Keep `source.enable=0`, `audio.source_enable=0`, `audio_amp.enable=0` and
   `cec.enable=0` until their own cold stage is accepted.
4. Only after the hardware ladder is stable should a display/UI stage be
   considered.

## Limits

- No TV boot, display/UI or GPIO level effect in this pass. On 2026-11-11 the
  TV was woken for a **read-only** `DEV_GPIO_GetDescription` query; no pin
  claim, level write, export, init ioctl, firmware write or boot was issued.
  The hotplug pulse itself was not run on hardware.
- `PLAT_DRV_INI_Init` uses a 60 s driver-marker timeout in `mod_plat`; the
  vendor loop has no timeout. This is a fail-closed safety deviation, not
  vendor parity.
- `plat.drm_init=0` is a takeover-only gate; the cold profile keeps 1. A
  process that starts with 0 and later requests 1 does not rerun the vendor
  common init (the vendor never reloads).
- The vendor `PLAT_SP_DebugRegisterMenu` call is not reproduced; it is a debug
  menu, not a hardware stage.
- The full `make check` was not green at this commit: the unrelated
  `check-vin-real` fixture fails at `source-select 17` (EIO) with the stub
  platform. The cold-start tests above pass. The `ui_skin.c` initializer was
  also fixed so the full build can reach that point.
