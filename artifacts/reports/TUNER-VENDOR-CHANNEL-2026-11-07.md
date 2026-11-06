# Vendor DVB-T SetChannel path — 2026-11-07

Owner authorization: TV read-write/full access, no FTA RF source connected.
This record covers the offline recovery and one hardware run of the vendor
0x5c/HAL_Demod_Set start path. It is not tune/lock acceptance: the lock bit
without RF is not independent evidence.

## Sources

| Asset | Identity / address |
|---|---|
| `aurora.elf` | MD5 `64c252340d3af6c5d66072611b7bad7a` |
| `DEV_NIM_DTV_StartTune` | `0x008619e0` |
| `_dev_nim_dtv_TerrTuneTo` | `0x00860498` |
| `FUN_0086aab0` (channel consumer) | `0x0086aab0` |
| `HAL_Demod_User_SetChannel` | `0x0086b0e8` |
| `HAL_Demod_User_Initialize` | `0x0086ade4` |
| `SAL_DEMOD_TUN_ProgramTuner` | `0x0084b0c8` |
| `SAL_TUNER_ProgramTuner` | `0x0084af68` |
| `HAL_Demod_Set` (libNDAL3) | `0x4e7b38dc` |

Ghidra projects: `artifacts/workbench/ghidra_proj` (Aurora/aurora.elf) and
`artifacts/workbench/ghidra_ndal3` (ndal3/libNDAL3.so), read-only.

## Recovered 0x5c record

`DEV_NIM_DTV_StartTune` builds the record at `r7+0x48` and passes it to
`_dev_nim_dtv_TerrTuneTo`; `FUN_0086aab0` copies the 0x5c bytes and consumes
the fields below.

| Offset | Meaning | DVB-T value |
|---|---|---|
| `+0x00` | frequency, kHz | from the tune request |
| `+0x04` | bandwidth, MHz | `6`, `7`, or `8` (StartTune's default path is `8`) |
| `+0x08` | ProgramTuner param[2] | `0` for DVB-T; used by the type-6 analog path |
| `+0x10` | IF/frequency mode | StartTune initializes `1`, sets `2` when the platform flag is set |
| `+0x14` | p4 | `0..2` from the tune request; nonzero becomes the `HAL_Demod_Set` spectrum flag |
| `+0x1c` | signal type | `2` = DVB-T |
| `+0x24..0x27` | PLP/stream bytes | `ff ff ff 00` on the default path |
| `+0x58` | override/analog p6 | `0` for DVB-T |

`SAL_DEMOD_TUN_ProgramTuner` maps signal type `2` to its SAL standard `6`,
which the MxL661 layer maps to MxLWare standard `8` (DVB-T). The record's
bandwidth is in MHz and `SAL_TUNER_MxL661_SetRF` maps `6/7/8` to
`0x20/0x21/0x22`.

## Linkability correction

`nm -D` shows `HAL_Demod_User_Initialize` and
`HAL_Demod_User_SetChannel` are exported by **aurora.elf only**, not by
`libNDAL3.so`/`libNDAL_EX.so`. A replacement process cannot link them. The
linkable pieces are:

- `HAL_Demod_Set` in `libNDAL3.so`;
- our recovered MxL661 DVB-T program sequence (the
  `SAL_DEMOD_TUN_ProgramTuner` equivalent);
- `HAL_fe_*`, `Hal_Iic_*` and `HAL_Demod_Init` in `libNDAL3.so`.

The replacement therefore reproduces the body of `HAL_Demod_User_SetChannel`
for DVB-T rather than claiming to execute the Aurora-only symbol.

## Implementation

- `common/nim_tune.c`
  - `oa_nim_raw_channel_build_dvbt()` writes the recovered record with
    `ff ff ff 00` at `+0x24`.
  - `oa_nim_raw_channel_setup_dvbt()` produces the host-testable plan:
    `freq_hz = kHz*1000`, MxL661 standard `8`, bandwidth
    `0x20/0x21/0x22`, IF `5000`, and the 0x54-byte `HAL_Demod_Set` block:
    `demod[0]=0x12`, `[1]=bandwidth_MHz`, `[4]=p4`, `[5]=1`,
    `[0x14]=freq_kHz`.
  - `oa_nim_raw_channel_parse()` now reads the corrected offsets and maps
    signal type `2` to DVB-T standard `8`, type `3` to DVB-C standard `6`;
    bandwidth `6/7/8` maps to `0x20/0x21/0x22`.
- `common/nim_hal.c`
  - New optional `oa_nim_tune_ops.set_channel(ud, signal_type, record)`.
  - `tuner.vendor_channel=1` (`oa_nim_hal_set_vendor_channel(1)`) selects the
    record path and does **not** call `program_tuner` as well.
  - DVB-T default record: 8 MHz, IF mode `1`; the typed fallback now uses
    8 MHz (`0x22`) to match `DEV_NIM_DTV_StartTune`.
- `common/tuner_vendor.c`
  - `tune_set_channel()` runs `oa_mxl661_tune_dvbt()` first, then
    `HAL_Demod_Set(0, setup.demod)`; 0 = success.
  - This is the recovered `FUN_0086aab0` order: SAL tuner program, then
    demod set.
- `modules/tuner/tuner.c`
  - `tuner.vendor_channel` is parsed only on the HAL backend and defaults to
    off; the manual SAL/demod/LNB init remains unchanged.

## Tests

`make check-tuner-backend` passes. The new tests cover:

- exact record offsets/bytes and builder rejection of zero/oversized
  frequency, 5/9 MHz bandwidth and IF mode 0/3;
- parser correction, 6/7/8 MHz mapping, override, unsupported signal type;
- `oa_nim_raw_channel_setup_dvbt` exact demod block and p4 spectrum flag;
- HAL backend fake `set_channel`: one record, signal type 2, no duplicate
  `program_tuner`, failure -> `-EIO`, and fallback when the op is absent.

Focused `check-nim-dtv`, `check-tuner-gate` and `check-tuner-analog-scan`
pass. ARM `mod_tuner.so` sha256
`7828b42f03a814539c72b8c1c156a1a8d0b75a8963a68a75e76056c2e5ceebd8`.

## Hardware run

Stage `/buffer/oa-tuner-20261107b`, tar SHA256
`b9b9dc303c67ed8ac90fd5d5fd35ea3547c29837458619619e3e84920b964a75`;
`SHA256SUMS` and `openaurora --check --modules ./modules` verified. Config
included `tuner.hw_init=1`, `tuner.backend=hal`, `tuner.vendor_channel=1`.
Pre-state: appmgr 1570, `aurora.elf` 1770, watchdog disabled, stock stopped.

The service started:

```text
tuner hw: rc=0 failed_step=5 nim=1 hal=1 tuner=1 seq=1 seq_rc=0 lnb=1 ir_code=21 ir_write=0 errno=1
service up: tuner v1
status tuner: state started, services tuner:v1
```

`tuner-scan 1` returned `ok`; `tuner-state` advanced 25/50/75 with `lock 1`.
If `tune_set_channel` had failed, the scan command would have returned
`failed errno 5`; the host tests pin that branch and the no-duplicate-program
ordering. `lock 1` is **not** RF acceptance.

The first `tuner-scan 4` (analog) returned `failed errno 16` because the
DVB-T scan was still in state `1` (SCANNING), so the module correctly returned
`-EBUSY`. After the DVB-T scan reached state `2` (DONE), the retry returned
`ok` and analog progressed 0/50/50 with `services` 1/1/2. This is the expected
single-scan serialization, not an analog-path failure.

## Restore

`quit`, stage/socket/heartbeat removed, `sync`, then `wdtctl set 10` rebooted
the set. After boot: appmgr 1562, `aurora.elf` 1765, zombie 1949, no
openaurora; stage/socket absent; hwprofile `1b063b2c...` unchanged; fb1
`1920,2160`, pan `0,0`; `wdtctl get` still returns failed as on this MCU.

## Limits and next steps

- No FTA RF source: tune/lock/scan are not accepted. The lock bit from the
  vendored status call remains unverified.
- `HAL_Demod_User_SetChannel` itself is not executed; its recovered body is
  reimplemented because the symbol is Aurora-only. `HAL_Demod_Set` return and
  the scan acceptance are the hardware evidence, not a per-write I2C trace.
- The normal 8 MHz/IF-mode-1 record is the recovered StartTune default. The
  platform flag that selects IF mode 2 and the 6/7 MHz input path are not
  hardware-validated.
- Next: connect an FTA RF source, compare `tuner.vendor_channel=0` and `1`
  for a real DVB-T multiplex, then verify lock/strength/quality against the
  vendor stock path. Analog/Cable remain separate.
