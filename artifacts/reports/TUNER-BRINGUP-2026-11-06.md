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

## NIM init/Connect2HAL and RT5047

`_dev_nim_dtv_InitNimFunc` @ `0x00860114` zeroes a 0x29-word NIM state block
and returns 1; it performs no I2C. `_dev_nim_dtv_Connect2HAL` @ `0x0086fea8`
fills a 0x29-entry table with `HAL_Demod_User_*` pointers (Initialize, Open,
SetChannel, Monitor, Close, GetChannelStatus, CheckLock, Strength, Quality,
SNR, ...; entry 0x12 is zero) and returns 1. Both are state/table functions.

`common/nim_dtv_hw.c` reproduces the state clear and a typed HAL table
(initialize/open/set_channel/status/lock/strength/quality/monitor/close/
disconnect). Only `initialize` is wired to the recovered sequence; the tuning
entries remain NULL rather than invented.

## NIM HAL tuning subset

`common/nim_tune.c` implements the verified part of the `HAL_Demod_User_*`
table:

- `Open`: switch tuner type (path 1 -> 1, else 2), read the SAL type, then
  deliver key `0xa000` with the type mapping (`5..8/10/11` unchanged,
  `0xd -> 0xc`); other types skip delivery.
- `Close`: `HAL_fe_open(0,state)` + `HAL_DEMOD_EXT_SetStandy(fe,1)` +
  `HAL_fe_close(fe)`.
- `Monitor` = return 1; `Disconnect` = no-op.
- `CheckLock`: path index `1->0x14`, `2/7->0x12`, `5->0x11`, `6->0x0e`,
  `8->0x10` (path 3's dynamic resolver is not recovered); then
  `HAL_Demod_GetStatus(handle,index,2,buf)` and `locked = buf[0]`.
- `GetStrength`: path 1 uses mask `0x40` and the u32 at `buf+0x18`; otherwise
  the tuner RSSI via `SAL_DEMOD_TUN_GetRSSI`.
- `GetQuality`: mask `0x80` and the u32 at `buf+0x24`.
- `SetChannel`/`GetChannelStatus` are large NIM-channel <-> NDAL struct
  mappings and return `-ENOTSUP` rather than guessed conversions.

The fake tests cover the path/index map, type delivery, open/close order,
lock/strength/quality masks and buffer offsets, RSSI fallback, and the
explicitly unresolved channel calls. `oa_nim_tune_hal_*` adapts the same state
to the NIM-DTV typed table; `mod_tuner` installs it in `connect_hal` with
vendor adapters (`HAL_fe_open/close`, `HAL_DEMOD_EXT_DeliverConfig/SetStandy`,
`HAL_Demod_GetStatus`). The table is installed but no tuning backend consumes
it yet; `set_channel`/`get_channel_status` remain `-ENOTSUP`.

### Why SetChannel/GetChannelStatus are not just struct mappings

`HAL_Demod_User_SetChannel` copies a 0x5c-byte channel record, extracts the
tuning parameters at +0xc/+0x10/+0x14/+0x1c/+0x28/+0x64, then calls
`SAL_DEMOD_TUN_ProgramTuner` (MxL661 RF/IF programming, not recovered yet)
before a path-specific NDAL/demux tail (`HAL_DEMOD_EXT_SetUnicableIF`,
`DeliverConfig`, spectral inversion, ...). `GetChannelStatus` calls the lock
helper, `HAL_Demod_GetStatus(...,0x600,...)` and maps a 0x180-byte status
struct per signal type. Implementing these faithfully needs the RF/IF tuning
sequence and the status layout, not only a field copy; both stay `-ENOTSUP`
rather than guessed.

## MxL661 RF/IF recovery (partial)

`SAL_DEMOD_TUN_ProgramTuner` @ `0x0084b0c8` maps standard index `0..7` to
`7,8,6,10,9,0xc,0xd,0xb`, applies the 17-entry IF table at `0x01367d38`
(`3650,4000,4100,4150,4500,4570,5000,5380,6000,6280,7200,8250,35250,36000,
36150,36650,44000` kHz), then calls `SAL_TUNER_ProgramTuner` @ `0x0084af68`
(SetIF via vtable+0x10, SetRF via +0xc, mutex and cached parameters).

`C_SalTunerMxL661::SetIF` @ `0x0084d299` wraps MxLWare `ConfigTuner` command 9
(standard/IF mode), and `SetRF` @ `0x0084d13d` wraps command 0xd
(start-tune) followed by 0xb (channel tune and lock sequence). The 3-byte
standard property tables are extracted: `DigitalDvbt` 21,
`DigitalIsdbtAtsc` 23, `DigitalDvbc` 26, `AnalogNtsc` 26, `AnalogPal` 28,
`AnalogSecam` 36 triples.

`common/mxl661_tune.c` carries the verified standard-index and IF table
mapping with tests. The `ConfigTuner` case 9/0xd/0xb register port and the
SetChannel/GetChannelStatus tails are not guessed and remain open.

The start-tune path (case 0xd) also depends on per-standard RF frequency LUTs
and `Ctrl_SetRfFreqLutReg` @ `0x008823f8`: it selects a LUT entry within
±500 kHz and writes the spur registers from `MxL601_SPUR_REGISTER` @
`0x01768578` (count byte 4; registers `0x49/0xdb/0xea/0xeb`). The LUTs are
large, frequency-dependent data tables. Porting them and the case 9/0xd/0xb
register sequences is the remaining RF/IF work; no values were guessed.

## SI2151 blocker

`SI2151_L3_Init` @ `0x0086f130` calls `L0_Init`, `Si2151_L1_API_Init`, then
`Si2151_Init` @ `0x0087f720`, which calls `Si2151_PowerUpWithPatch`,
`Si2151_storeUserProperties` and `Si2151_downloadAllProperties`. That is the
Silicon Labs L1/L2/L3 driver with a firmware patch and large property tables:
a separate large port, not a callback. The shipped hwprofile selects SAL 0xd
(MxL661), so this TV does not use the SI2151 path. No fake SI2151 backend was
added; it remains unimplemented.

RT5047 (`SAL_LNB_Initialize` @ `0x00877384` -> `RT5047_Device_Initialize`
@ `0x00877674`): look up function index 5 from `au8GPIOFuncName` (`LNB_EN`),
then `DEV_GPIO_SetLevel(pin, level)`. `DEV_GPIO_GetDescription` @ `0x00850c38`
uses the 0x88-byte ioctl `0xc088671f` on `/dev/ntgpio`: function name at
+0x80, pin at +0x00, level strings ("low"/"high") at +0x20. `DEV_GPIO_SetLevel`
takes the GPIO mutex and calls `Hal_Gpio_Write`; `Hal_Gpio_Init` opens the
device and the write issues ioctls `0x670a`, `0x670e`, `0x670c`. The TV adapter
`oa_tuner_vendor_lnb_init` implements the descriptor ioctl and
`Hal_Gpio_Init/Write`; `common/lnb_rt5047.c` handles present/absent/failure.
This is a real GPIO write and only runs behind the gate.

## Module gate

`mod_tuner` now accepts `tuner.hw_init=1` (default off):

- host/non-vendor build: `init` returns `-ENOTSUP`; the gate test asserts
  `-95` and that default init still succeeds.
- TV build: `start` reads `tuner.hwprofile` (default
  `/conf/mb230_hwprofile.bin`), computes SAL type with
  `tuner.pcb_layout` (default 14), requires type 0xd, then runs the NIM-DTV
  task with `hal_requested=1`:
  - `hal_demod_initialize` = the recovered sequence (MxL661 init + AV2018 +
    dvif + SetTunerType + RT5047),
  - `module_buf_info` = the verified return-0 stub,
  - IIC callbacks = `oa_nim_dtv_hal_*` over `Hal_Iic_Read/Write`,
  - `init_nim_func` = `oa_nim_dtv_init_state` (zero the 0x29-word block),
  - `connect_hal` = `oa_nim_dtv_connect_hal` with `initialize` wired and the
    tuning entries NULL,
  - `lnb_init` = `oa_tuner_vendor_lnb_init` (LNB_EN descriptor + level).
  Safety deviation: if the MxL661 init fails, the IIC callbacks fail before
  touching the bus, so the NIM-DTV task stops instead of continuing the
  vendor's I2C codes. The vendor task cannot observe the init failure; this
  guard keeps "no IIC after a failed tuner init" true for the gate.
  The gate returns `-EIO` if the NIM state/vtable, sequence or IIC step
  failed, or if an existing LNB_EN write failed (absent LNB is accepted).
  It has not been exercised on hardware.

## Evidence

```text
ok mxl661 dev: DEV_IIC context, 0xfb pointer read, failure propagation and
               full init over Hal_Iic
ok tuner sequence: demod->tuner->AV2018->dvif->SetTunerType->LNB order,
                   0xd->0xc/8->8, type-0xf skip and failure continuation
ok nim dtv hw: 0x29-word state clear and HAL table install
ok rt5047: LNB_EN present/absent/error flow and level write
ok nim tune: path/index map, open/close, lock/strength/quality masks and
             unresolved channel mappings
ok mxl661 tune: standard index map and 17-entry IF table
ok tuner hal: four bus IDs from description+0x19, 0xff-only resolution, ...
ok mxl661: default/custom init order, RMW masks, defaults table and failure behavior
ok nim dtv hal: DEV_IIC bus/reg/address packing and Hal success inversion
ok tuner gate: host build refuses tuner.hw_init=1 and default init still works
```

Focused ASan/UBSan clean; full suite ended `all tests passed`; ARM musl
static tests pass under `qemu-arm-static`. TV module build:

```text
build-tv-v3/modules/mod_tuner.so
sha256 519053e68854419e985c0448bd3fd7f359f816e6fb49aa04d0b22f1b1a2fc827
NEEDED libNDAL3.so, libNDAL_EX.so, ...; RPATH /vendor/lib:/vendor/lib/libs
undefined-but-runtime: Hal_Iic_Read/Write, Hal_Gpio_Init/Write,
                       HAL_Demod_Init, HAL_dvif_open,
                       HAL_DEMOD_EXT_DVIF_SetTunerType
```

## Remaining

1. SI2151 tuner backend: the Silicon Labs L1/L2/L3 driver with
   `PowerUpWithPatch` and property download is a separate large port; this
   TV's profile selects MxL661 and does not use it.
2. MxL661 RF/IF register port: `ConfigTuner` cases 9/0xd/0xb, the
   SetIF/SetRF packing, per-standard RF frequency LUTs and the
   `MxL601_SPUR_REGISTER` table (count 4; regs 0x49/0xdb/0xea/0xeb). The
   standard/IF mapping is recovered in `common/mxl661_tune.c`.
   SetChannel/GetChannelStatus depend on this.
3. The module's NIM vtable now carries the implemented tuning ops, but no
   tuning backend consumes the table yet.
4. Hardware acceptance is **not done**. The owner's standing instruction is
   read-only, no takeovers; running the gate requires explicit authorization
   to stop stock Aurora, claim `/dev/ntkiic0`, `/dev/ntgpio`, `/dev/ntkdvif`
   and `frontend0`, observe tuner lock, then restore stock and verify.


No tuner hardware has been accessed.
