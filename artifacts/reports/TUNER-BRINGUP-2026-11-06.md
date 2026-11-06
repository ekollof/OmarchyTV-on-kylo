# Tuner bring-up sequence, DEV_IIC adapter and ownership — 2026-11-06

Scope: implement the remaining SAL/HAL tuner sequence and the TV IIC adapter,
and add an explicit hardware gate to `mod_tuner`. No tuner IIC, demod, dvif or
GPIO operation was performed. TV access was read-only (process/fd listing,
`md5sum`); stock Aurora stayed running.

## Device ownership (read-only TV check)

`/proc/*/fd` targets were listed with `ls -l`/`readlink` only (never read):

| Owner | Devices |
|---|---|
| `aurora.elf` PID 1774 | `/dev/i2c-0..5`, `/dev/ntkiic0`, `/dev/ntgpio`, `/dev/ntkdvif`, `/dev/dvb/adapter0/frontend0`, `/dev/ntkci`, `/dev/ntktspu`, `/dev/video21/24/28`, ... |
| `com.nvt.appmgr` PID 1574 | `/dev/ntkgpu` |
| PID 1945 (`[aurora.elf]`) | no device fds |

`/dev/gpiochip0` and exported `gpio110/111/112/117` exist. The boot GPIO table
(`au8GPIOFuncName` @ `0x0175f350`, 24-byte entries) names index 5 `LNB_EN`
(index 4 `LNB_FAULT`). The Novatek GPIO device `/dev/ntgpio` is currently open
by stock Aurora, so our process must not issue `DEV_GPIO_GetDescription(5)` or
`Hal_Gpio_Write` until a controlled ownership handover is authorized.

## Recovered HAL contracts

- `C_SalTunerAV2018::C_SalTunerAV2018` @ `0x0084db3c` only fills the class
  fields (0xc0 address, type 10, band/IF values); no I2C.
- `C_SalTunerAV2018::GetTunerResponse` @ `0x0084d8cc` is a literal
  `return 1` in this firmware.
- `HAL_Demod_Init` (libNDAL3 `0x4e7b2cd8`): index 0 opens/caches the dvif
  handle via `_HAL_dvif_open`, resets its 0x54-byte state block, creates a
  semaphore and returns 0; the error branch returns 1. Index 1/6 set other
  state bytes; any other index prints and still does the common state setup.
- `HAL_dvif_open` (libNDAL3 `0x4e7b3350`) returns the cached handle when >= 1,
  otherwise calls `_HAL_dvif_open` and caches the result.
- `HAL_DEMOD_EXT_DVIF_SetTunerType` (libNDAL3 `0x4e7b55dc`) calls
  `ioctl(handle, 0xc0044485, &type)`:
  `3,0xc -> 0xc`, `5,7 -> 5`, `6 -> 6`, default -> `8`. Returns 0/-1.
- `SAL_LNB_Initialize` @ `0x00877384` calls `RT5047_Device_Initialize` once.
  RT5047 @ `0x00877674` does `DEV_GPIO_GetDescription(5)`; if present it calls
  `DEV_GPIO_SetLevel(pin, level)` and marks the LNB initialized. No I2C.
  The GPIO binding is **not implemented** here (ownership above).

The sequence order from `HAL_Demod_User_Initialize`/`SAL_DEMOD_TUN_Initialize`:

```text
HAL_Demod_Init(0)
tuner class Init()              // MxL661 recovered; SI2151 backend not written
AV2018 GetTunerResponse()       // return 1
HAL_dvif_open()
HAL_DEMOD_EXT_DVIF_SetTunerType(handle, table[type])   // 0xd -> 0xc, 8 -> 8
SAL_LNB_Initialize()            // LNB_EN GPIO
```

SAL type 0xf skips the tuner/AV2018/dvif block but still runs the LNB call.

## Hal IIC context correction

The DEV_IIC wrappers build a 24-byte `Hal_Iic_Read/Write` context, not the
8-byte `{bus, addr}` used by the first NIM adapter:

```text
+0x00  bus/device index (byte)
+0x04  8-bit I2C address
+0x08  subaddress length 0..4
+0x0c  zero
+0x10  timeout (100 for DEV type 0, 400 for DEV type 4/HAL_TUNER)
+0x14  zero
```

`common/nim_dtv_hal.c` and `common/tuner_hal.c` now build this full context
(shared `include/openaurora/hal_iic.h`). The previous two-word context would
have made `_i2c_RdWr_Transfer` read beyond the object; the host tests cannot
detect that against the vendor library, so the fix is instruction-derived.

`HAL_TUNER_Read/Write` take the 8-bit I2C address, not a register: the SI2151
caller uses `HAL_TUNER_Write(0xc0, {reg, value}, 2)` then
`HAL_TUNER_Read(0xc0, buf, 4)`. The base context has `subaddr_len = 0`.

## MxL661 DEV adapter

`common/mxl661_dev.c` reproduces `Ctrl_WriteRegister`/`Ctrl_ReadRegister`
without aurora's unexported `DEV_IIC_*`:

```text
write: Hal_Iic_Write(ctx{0, addr, 1, 0, 400, 0}, &reg, 1, &value)
read:  Hal_Iic_Write(ctx{..., 1, ...}, &0xfb, 1, &reg)
       Hal_Iic_Read (ctx{0, addr, 0, 0, 400, 0}, &zero, 1, value)
```

`DEV_IIC` returns 1 on success; the adapter returns 0 on success for the
MxL661 ops. The fake test models the 0xfb pointer protocol and runs the full
41-write/5-read init through the adapter (46 DEV writes counting the five
pointer writes, 5 reads).

## Module gate

`mod_tuner` now accepts `tuner.hw_init=1` (default off):

- host/non-vendor build: `init` returns `-ENOTSUP`; the gate test asserts
  `-95` and that default init still succeeds.
- TV build: `start` reads `tuner.hwprofile` (default
  `/conf/mb230_hwprofile.bin`), computes SAL type with
  `tuner.pcb_layout` (default 14), requires type 0xd, then runs the NIM-DTV
  task with `hal_requested=1`:
  - `hal_demod_initialize` = the recovered sequence (MxL661 init + AV2018 +
    dvif + SetTunerType),
  - `module_buf_info` = the verified return-0 stub,
  - IIC callbacks = `oa_nim_dtv_hal_*` over `Hal_Iic_Read/Write`,
  - `init_nim_func` and `connect_hal` are NULL because
    `_dev_nim_dtv_InitNimFunc` / `Connect2HAL` are aurora-internal and not
    available to a replacement process,
  - `lnb_init` is NULL because the LNB_EN GPIO is unowned.
  Safety deviation: if the MxL661 init fails, the IIC callbacks fail before
  touching the bus, so the NIM-DTV task stops instead of continuing the
  vendor's I2C codes. The vendor task cannot observe the init failure; this
  guard keeps "no IIC after a failed tuner init" true for the gate.
  The module returns `-EIO` if the sequence or an IIC step failed. The gate is
  not exercised on hardware in this milestone.

## Evidence

```text
ok mxl661 dev: DEV_IIC context, 0xfb pointer read, failure propagation and
               full init over Hal_Iic
ok tuner sequence: demod->tuner->AV2018->dvif->SetTunerType->LNB order,
                   0xd->0xc/8->8, type-0xf skip and failure continuation
ok tuner hal: four bus IDs from description+0x19, 0xff-only resolution, ...
ok mxl661: default/custom init order, RMW masks, defaults table and failure behavior
ok nim dtv hal: DEV_IIC bus/reg/address packing and Hal success inversion
ok tuner gate: host build refuses tuner.hw_init=1 and default init still works
```

Focused ASan/UBSan clean; full suite ended `all tests passed`; ARM musl
static tests pass under `qemu-arm-static`. TV module build:

```text
build-tv-v3/modules/mod_tuner.so
sha256 50b27429d14530a490d6f64f78b6bb55eef8a8be190cc95d7f60730374e3a882
NEEDED libNDAL3.so, libNDAL_EX.so, ...; RPATH /vendor/lib:/vendor/lib/libs
undefined-but-runtime: Hal_Iic_Read/Write, HAL_Demod_Init, HAL_dvif_open,
                       HAL_DEMOD_EXT_DVIF_SetTunerType
```

## Remaining

1. LNB_EN GPIO: implement the Novatek GPIO descriptor/`Hal_Gpio_Write` path
   only under a controlled ownership handover (stock Aurora currently owns
   `/dev/ntgpio`).
2. `_dev_nim_dtv_InitNimFunc` and `_dev_nim_dtv_Connect2HAL` equivalents
   (aurora-internal; the gate currently skips them).
3. SI2151 tuner backend (only the MxL661 path is recovered).
4. Real hardware acceptance: stop stock Aurora, verify exclusive ownership of
   `/dev/ntkiic0`, `/dev/ntgpio`, `/dev/ntkdvif` and `frontend0`, run the gate,
   observe tuner lock, then restore stock and verify.

No tuner hardware has been accessed.
