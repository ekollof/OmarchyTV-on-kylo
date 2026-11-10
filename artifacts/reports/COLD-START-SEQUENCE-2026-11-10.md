# Cold-start sequence audit — 2026-11-10

Status: **current offline instruction audit and host fix. TV was not contacted
and no UI was loaded.** The HAL half of the vendor cold start is now
instruction-exact. The device half (`DEV_INI_Init`) is still not called by the
replacement: its effects are partly implemented in modules, partly gated, and
partly missing. Full vendor cold-start parity is therefore **not** claimed yet.

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
| 4 | `DEV_PANEL_Init` | `0x008583fc` | copies GPIO 110/111 descriptions, timing block `{1,10,40,800,220,55,10}`, PWM ch108, curve 40/60/80/100; returns channel index | `mod_panel` PWM/curve/power; timing/descriptions not copied |
| 5 | `DEV_NIM_Init` | `0x00854fd0` | one-shot `DEV NIM ATV`/`DEV NIM DTV` factories + debug menus; always returns 1 | `mod_tuner` owns HAL/NIM-DTV; factories not created |
| 6 | `DEV_IIC_Init` | `0x008531e8` | opens `/dev/ntkiic0` O_RDWR, stores device fd, returns 1/0 | HAL `Hal_Iic_Init` only |
| 7 | `DEV_AMP_Init` | `0x0084f4ac` | `Hal_Audio_Ctrl(0x3a,{0x100})` ignored; creates `DEV_AMP_ID` task (stack 0x1000, prio 3); returns 1/0 | `audio.i2s_mclk=1` sends 0x3a; task missing; `audio_amp.enable=0` |
| 8 | `DEV_SPI_Init` | `0x0085946c` | creates/replaces semaphore at `0x019354f0`, returns 1/0 | `mod_spi` transport; semaphore not reproduced |
| 9 | `DEV_SPI_Open(0)` | `0x00858d0c` | `/conf/spi.bin` `rb+`, else `/dev/ntkspi` O_RDWR; returns 1/0 | `mod_spi` file/device open; equivalent boundary |
| 10 | `DEV_NIM_DTV_Init` | `0x00861678` | one-shot task; models 0..3/default take demod+Connect2HAL, model4 skips; IR failure still marks done | `mod_tuner` `tuner.hw_init=1` NIM-DTV task |
| 11 | `DEV_MTD_Init` | `0x0085419c` | registers a debug menu only, returns 0 | no owner needed |

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
```

These are fake-HAL tests; they prove order and state, not hardware effects.

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

1. Decide the target for the device half: formally accept the HAL-only design
   (and prove each enabled effect is covered) or implement the device-layer
   equivalents. The current cold config comment claims HAL-only; that decision
   is not yet accepted by hardware evidence.
2. Build a **hardware-only** cold ladder with no display/UI modules:
   `plat` alone first, then panel, audio/I2S MCLK, SPI, NIM-DTV. Each stage
   gets one owner and one pass/fail observation.
3. Keep `source.enable=0`, `audio.source_enable=0`, `audio_amp.enable=0` and
   `cec.enable=0` until their own cold stage is accepted.
4. Only after the hardware ladder is stable should a display/UI stage be
   considered.

## Limits

- No TV access, no firmware write, no boot, no UI.
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
