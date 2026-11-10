# Cold-start stage audit and `plat-state` — 2026-11-06

Status: historical, partly superseded by
[COLD-START-SEQUENCE-2026-11-10](COLD-START-SEQUENCE-2026-11-10.md).
Superseded-by: COLD-START-SEQUENCE-2026-11-10 for the current cold-start
contract and gap disposition.
The stage order and `plat-state` evidence remain useful; the “full suite
passes” and “panel/SPI/MTD/NIM-DTV still unimplemented” statements are dated and
corrected there.
Last verified: 2026-11-10 (documentation correction; original run 2026-11-06).
Evidence class: offline firmware instructions plus host tests.

Offline firmware audit of the vendor hardware-init stage order after our
`PLAT_DRV_INI_Init` work, plus a probe for the stages `plat` owns. No TV
operation, HAL call or stock-Aurora stop.

## Firmware evidence

`ICE_HardwareInit` @ `0x007551a0` runs `PLAT_DRV_INI_Init` @ `0x0089b6e4` at
`0x0075564c`, then `DEV_INI_Init` @ `0x00853a58` at `0x00755650`, before creating
its next mutex:

```text
75564c: bl 89b6e4 <PLAT_DRV_INI_Init>
755650: bl 853a58 <DEV_INI_Init>
```

`DEV_INI_Init` order (each return is checked; a zero logs and continues):

| order | call | address |
|---:|---|---|
| 1 | `DEV_COMMON_SET_PCBLAYOUTTYPE(14)` | `0x00850288` |
| 2 | `DEV_GPIO_Init` | `0x00851fcc` |
| 3 | `dev_src_init` | `0x008fc654` |
| 4 | `DEV_PANEL_Init` | `0x008583fc` |
| 5 | `DEV_NIM_Init` | `0x00854fd0` |
| 6 | `DEV_IIC_Init` | `0x008531e8` |
| 7 | `DEV_AMP_Init` | `0x0084f4ac` |
| 8 | `DEV_SPI_Init` | `0x0085946c` |
| 9 | `DEV_SPI_Open(0)` | `0x00858d0c` |
| 10 | `DEV_NIM_DTV_Init` | `0x00861678` |
| 11 | `DEV_MTD_Init` | `0x0085419c` |

This is the device half of vendor bring-up; it is separate from the
audio/DRM/GPIO/I2C/VIN/video sequence `plat` already reproduces.

## Why `plat` must not call `DEV_INI_Init`

The modular design assigns device ownership outside `plat`:

| vendor stage | current OpenAurora owner / status |
|---|---|
| GPIO/I2C init | `plat` (`Hal_Gpio_Init`, `Hal_Iic_Init`) |
| source init | `mod_source` VIN path; not `dev_src_init` |
| panel init | `mod_panel` PWM/backlight; not `DEV_PANEL_Init` |
| NIM / NIM DTV | `mod_tuner` stub / NDAL frontend; not the vendor device init |
| amp init | `mod_audio` / `mod_audio_mixer` / `audio_amp_profile` data; amp board lifecycle still open |
| SPI open/init | `mod_spi` opt-in read-only transport; not autoloaded (SPI-MTD-NIM-STAGE) |
| MTD init | no owner needed: `DEV_MTD_Init` only registers a debug menu |

Calling `DEV_INI_Init` from `plat` would duplicate panel/source/tuner/audio
ownership and run unrecovered device code under stock Aurora. The correct cold
start needs one owner per stage, not a second monolithic call. The real
NIM-DTV task and amp/panel power stages remain unimplemented; SPI is available
behind `spi.enable=1`. This is part of why independent cold startup is still
unaccepted.

Correction 2026-11-10: `mod_panel` now implements the recovered panel power/PTF
off order; `mod_tuner` owns the NIM-DTV task; `mod_spi` is the SPI open owner;
MTD needs no hardware owner. The current stage-by-stage disposition is in
[COLD-START-SEQUENCE-2026-11-10](COLD-START-SEQUENCE-2026-11-10.md). The
“full suite: all tests passed” line below is the dated 2026-11-06 host result,
not current evidence; the full suite currently stops in the unrelated
`check-vin-real` fixture.

## Implementation: `plat-state`

A `plat-state` control probe reads the published `plat` v1 interface:

```text
plat ready <0|1> errors 0x<stage mask>
```

The mask bits are the existing stages: audio prelude, common init, GPIO, IIC,
VIN, video device and the driver-marker wait. It is diagnostic access, not a new
service contract or a hardware-acceptance claim.

`check-plat-startup-wait` now also asserts:

```text
after start: plat-state -> plat ready 1 errors 0x0
after stop:  plat-state -> failed errno 19
```

```text
ok plat startup wait: pre-existing 1ms, arrived after 201ms, timeout refused at 402ms then retried
Full suite: all tests passed
```

## Limits

- Host build uses `OA_NO_VENDOR`; no real HAL stage, device node or TV was
  exercised. `plat-state` reports what the module did, not hardware success.
- The vendor `DEV_INI_Init` stages are recovered but not implemented as owned
  modules; SPI/MTD/NIM-DTV and amp/panel power remain open.
- Cold boot, MCU/watchdog authority, independent scanout and real
  audio/CEC/tuner/PVR/VIN hardware remain unaccepted.
- A stable controlled-ownership scanout test still needs explicit owner
  authorization to stop stock Aurora and independent observation.
