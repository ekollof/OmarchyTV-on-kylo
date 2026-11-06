# Panel backlight curve and NIM-DTV task contract — 2026-11-06

Offline recovery/implementation of `DEV_PANEL_Init`/`SetPWMBacklightLv` and the
`DEV_NIM_DTV_Init` gate/task constants, with fake-HAL/unit tests. No TV or
hardware access.

## Panel backlight curve

Firmware (aurora MD5 `64c252340d3af6c5d66072611b7bad7a`, read-only Ghidra):

- `DEV_PANEL_Init` @0x008583fc stores `{enable=1, 10, 40, 800, 220, 55, 10}`,
  copies GPIO descriptions 110/111, gets PWM channel for id 108 and calls
  `DEV_PANEL_SetBacklightPWMCurve(40,60,80,100)`, then sets the level scale to
  1000.
- `DEV_PANEL_SetPWMBacklightLimit` @0x008581dc writes the runtime minimum
  `M = (1000*percent+50)/100` at `gstBacklightPara+4`.
- `DEV_PANEL_SetBacklightPWMCurve` @0x00858320 builds
  `v(p) = M + ((1000-M)*p + 50)/100` for p=40/60/80/100 and the mid entry
  `(v1+v2)/2`, `(v2*p4+50)/100` or `(v3+v2)/2` per the recovered branches.
- `DEV_PANEL_SetPWMBacklightLv` @0x0085801c clamps level to 1000, maps
  `x = M + (level*(1000-M)+500)/1000`, then piecewise-linearly interpolates
  `(v1,v1) -> (v2,mid) -> (v3,v3)` using the vendor's integer divisions.

Implementation:

- `include/openaurora/panel_curve.h` + `common/panel_curve.c` reproduce the
  table builder, level mapping and `percent -> level*10 -> 12-bit duty` path
  with all integer rounding. A degenerate table returns the pre-curve value
  instead of taking the vendor divider fault (documented safety deviation).
- `mod_panel` gains opt-in `panel.curve=1` and `panel.min_percent` (default 0).
  Default behavior remains the historical linear percent mapping; the curve
  path uses `oa_panel_percent_to_duty(..., panel.duty_max)`. `panel.duty_max`
  is the final 12-bit calibration knob (default 4095, clamped 1..4095); the
  0..1000 -> 0..duty_max scale is explicit test input, not hardware-accepted.

Tests:

```text
ok panel curve: vendor breakpoints, runtime minimum, piecewise midpoint, percent-duty and bounds
ok panel hal: curve min/breakpoints and duty_max reach 12-bit PWM; default linear path unchanged
```

Vectors cover the identity default, min=200 (`level 250 -> 400`, `500 -> 600`,
`1000 -> 1000`), p4=50 bending (`500 -> 450`, `700 -> 650`), percent->duty
(50% -> 2047 at 4095, 50% -> 500 at duty_max 1000, min20% 0% -> 819), clamping
and a degenerate table.

## NIM-DTV task body

`DEV_NIM_DTV_Init` @0x00861678 is idempotent on a global initialized byte,
accepts model types 0..3, marks type 4 with active module 1 and others 3,
initializes `g_InitMutex`, and creates one task at
`CreateTask(entry 0x008601ad, arg 0, stack 0x1000, priority 14, params, 1)`.
Implemented `common/nim_dtv.c` for that gate/active code and the constants:

```text
ok nim dtv: recovered model gate/active module and task constants
```

The task entry at `0x008601ac` (created by `DEV_NIM_DTV_Init`) is a **one-shot
initializer**, not a monitor loop. Read-only Ghidra decompilation of the raw
Thumb entry (disposable project copy; it was not a defined function) plus the
exported helpers:

1. lock `g_InitMutex`;
2. `PLAT_DRV_GetModuleBufInfo(9, ...)`, logging the success/error branch;
3. `_dev_nim_dtv_InitNimFunc(&state)`;
4. if `DAT_0176201c << 0x1e < 0` (bit1): `HAL_Demod_User_Initialize()` then
   `_dev_nim_dtv_Connect2HAL(&state)`;
5. the unnamed UART helpers call
   `DEV_IIC_Read/Write(0, 0, 0x80, 1, code, 1, &byte)`, then update bits:
   - code `0x15` (21): `byte &= 0xf7` (clear bit3);
   - code `0x11` (17): `byte |= 8` (set bit3);
   - code `0x40` (64): `byte |= 2` (set bit1); its write result is not checked;
6. write active `0xff`, mark the stage initialized, signal the condition and
   unlock.

The IIC helpers return 1 only when `DEV_IIC_Read/Write` returns 0. The three
bytes are separate stack slots, zeroed once before code 21. `DEV_NIM_DTV_Term`
clears the guard and writes `0xff`; the recovered body does not join.

Implemented as `oa_nim_dtv_task_run` with read/write callbacks and steps:

```text
LOCK, MODULE_BUF(9), INIT_NIM, [HAL_DEMOD, CONNECT_HAL],
IR21 read/clear8/write, IR17 read/set8/write, IR64 read/set2/write,
ACTIVE 0xff, SIGNAL, UNLOCK
```

Tests cover the full order with/without HAL, module-buffer and NIM-init
failures continuing, every read/write rejection (including the ignored code-64
write), the masks, the fixed IIC arguments, and bad arguments:

```text
ok nim dtv: model gate/constants and one-shot read/modify/write order, masks and failure finish
ok nim dtv hal: DEV_IIC bus/reg/address packing and Hal success inversion
```

Only the ordered state machine and gate/constants are implemented; the real
NDAL/HAL/UART callbacks and a cold-start execution remain open.

### HAL callback wiring

`include/openaurora/nim_dtv_hal.h` + `common/nim_dtv_hal.c` implement the next
layer: pack `{bus=0, reg=0x80}`, the per-code address and one byte, call an
`oa_nim_dtv_iic_ops` read/write callback, and invert the HAL convention
(`Hal_Iic_Read/Write` return 0 on success) to the DEV helper convention
(1 = success). `oa_nim_dtv_hal_read/write` plug directly into
`oa_nim_dtv_ops.ir_read/ir_write`.

The end-to-end test runs the one-shot state machine with those adapters over a
fake `Hal_Iic_*`, asserting the fixed bus/register/address packing, read/write
success inversion, failure paths and the resulting bytes (`0x67/0x6b/0x9a` for
the 0x15/0x11/0x40 sequence). A TV module can pass the real
`Hal_Iic_Read/Write`; `mod_tuner` does not call the chain yet because the
module-buffer and HAL demod callbacks still need real implementations.

### Module-buffer and HAL-demod boundary

`PLAT_DRV_GetModuleBufInfo` @0x0089b79c is a literal `return 0` stub
(`movs r0,#0; pop`). `oa_nim_dtv_module_buf_info()` reproduces that and is
covered by the state-machine test (the task logs the not-found branch and
continues).

`HAL_Demod_User_Initialize` @0x0086ade4 is not a small callback:

```text
memset 0x68-byte demod state
_PLAT_OS_CreateMutex(...)
TD_DBG_Register(3, 0x100000, "DEV_HAL_DEMOD", ...)
HAL_Demod_Init(state+4)
SAL_DEMOD_TUN_GetInitTunerType()
SAL_DEMOD_TUN_Initialize(type, 1)
SAL_LNB_Initialize()
```

Decompiling `SAL_DEMOD_TUN_Initialize` shows `HAL_TUNER_Init`, SI2151/MxL661
tuner-class construction, AV2018 response handling, `HAL_dvif_open` and
`HAL_DEMOD_EXT_DVIF_SetTunerType`; `SAL_LNB_Initialize` calls
`RT5047_Device_Initialize`. Those SAL classes are aurora-internal, not exported
by the linked vendor libraries.

Further recovery of the dependency graph:

- `HAL_TUNER_Init` @0x0084c164 does not touch hardware; it resolves four IIC
  bus IDs by calling `DEV_IIC_GetDescription(0..3)` and reading byte +0x19
  (tuner, sat tuner, ext-demod, LNB).
- `SAL_DEMOD_TUN_GetInitTunerType` @0x0084adb8 checks PCB layout: layouts
  3/4/5/6 force type 0xf; the default path reads `Profile_GetTunerType` and
  selects 8 for `"Si2151"`, otherwise 0xd (MxL661). The profile keys are
  `TunerTypes`/`TunerType`; the live EFSL `mb230_swprofile.bin` is a hashed
  binary blob without the plaintext value.
- The live TV DT `tztv_tuner` exposes `name=tuner` at I2C device `0-a000`
  (`modalias=i2c:tuner`), with no chip ID or bound driver. It does not identify
  Si2151 vs MxL661.
- `HAL_Demod_Init(0)` and `HAL_dvif_open`/`HAL_DEMOD_EXT_DVIF_SetTunerType`
  are exported by `libNDAL3.so`, but they are only part of the vendor sequence;
  skipping the tuner-class and LNB initialization would not be a functional
  tuner backend.

So `mod_tuner` is deliberately **not wired** yet. The state machine, module
buffer stub and IIC HAL adapter are ready; the next prerequisite is the real
SAL/HAL demod-tuner backend (including the profile-selected tuner class), not a
fake callback. No hardware was accessed to identify the tuner; that would need
an explicit read-only probe decision.

## Verification

All three tests pass under focused ASan+UBSan (`detect_leaks=1`,
`halt_on_error=1`). Full suite ran visibly and ended `all tests passed`.
ARM `mod_panel.so` builds with `panel_curve.c`; it retains the vendor HAL
`NEEDED` set plus libpthread/libc.

## Limits

- `panel.curve` is opt-in and not tested on the panel; the 12-bit duty scale and
  runtime minimum are uncalibrated. The default linear path is unchanged.
- Only the NIM-DTV gate/constants are implemented; the monitor task, model-
  specific branches and real tuner hardware remain open.
- Real audio/CEC/tuner/PVR/VIN, the live vendor PQ object, broad UI parity and
  cold startup remain unaccepted.
