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
  Default behavior remains the historical linear `pct*4095/100`; the curve path
  uses `oa_panel_percent_to_duty(..., 4095)`. The 0..1000 -> 0..4095 duty scale
  is the existing HAL assumption, not hardware-accepted.

Tests:

```text
ok panel curve: vendor breakpoints, runtime minimum, piecewise midpoint, percent-duty and bounds
ok panel hal: curve min/breakpoints reach 12-bit PWM; default linear path unchanged
```

Vectors cover the identity default, min=200 (`level 250 -> 400`, `500 -> 600`,
`1000 -> 1000`), p4=50 bending (`500 -> 450`, `700 -> 650`), percent->duty
(50% -> 2047, min20% 0% -> 819), clamping and a degenerate table.

## NIM-DTV task contract

`DEV_NIM_DTV_Init` @0x00861678 is idempotent on a global initialized byte. It
accepts model types 0..3, marks type 4 unsupported with
`gu32NimActiveModule=1`, and leaves 0..3/default at 3. It initializes
`g_InitMutex` and calls:

```text
PLAT_OS_CreateTask(entry 0x008601ad, arg 0, stack 0x1000, priority 14,
                   params, 1)
```

`DEV_NIM_DTV_Term` @0x008617d4 clears the guard and writes active `0xff`; the
recovered body does not join the task.

Implemented `common/nim_dtv.c` + header for the verified model gate, active
module code and task constants, with:

```text
ok nim dtv: recovered model gate/active module and task constants
```

The task entry/body performs NIM/hardware work and is **not** implemented; no
tuner access or fake-device task execution is claimed.

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
