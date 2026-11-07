# Panel backlight calibration and 50% hardware run — 2026-11-07

Owner authorization: TV read-write/full access, display/fb1 tests allowed,
recovery reboot allowed. This record covers the real `mod_panel` PWM path. It
does not claim an independent brightness measurement: the owner did not confirm
the 50% dimming during the run, and `panel-state` is the module's cached
request, not a hardware readback.

## Read-only stock calibration

The stock `aurora.elf` process was running (PID 1765). `/proc/1765/mem` was read
only, no writes:

```text
DAT_01931270 (GPIO descriptor table) = 0x026d6000
descriptor index 0x6c (DEV_PANEL backlight id):
  ffffffff ffffffff 0000006c 0000006c 00000000 00000001 00000002 00000002
```

`DEV_GPIO_GetDescription` returns `entry + 8`, so `DEV_PWM_Get_ChIdx` reads the
pin at `entry + 0xc` = `0x6c` (108). The `astPwmBasePin` table at
`0x0175f320` is:

```text
{base=0x6c (108), count=9, channel_base=0}
{base=0x77 (119), count=2, channel_base=9}
{base=0x7c (124), count=1, channel_base=11}
{base=0x7e (126), count=1, channel_base=12}
```

Pin 108 is therefore PWM **channel 0**.

Other stock globals:

| Address | Value | Meaning |
|---|---|---|
| `0x01761870` | `0` | `genPwmdimMode` (set-mode) |
| `0x01761878` | `30000` | `gu32PanelFreqInHz` |
| `0x019354c4` | `03e803e8 00000000` | `gstBacklightPara`: level 1000, minimum 0 |

`DEV_PANEL_SetPWMBacklightLevel` @ `0x008578d9` builds the `DEV_PWM_Set_ByChannel`
request as `{freq=gu32PanelFreqInHz, duty=level*4.095 (clamp 4095), shift=0,
mode=genPwmdimMode}`. `DEV_PANEL_SetBacklightLevel` @ `0x00858149` updates
`gstBacklightPara._2_2_` only when the call succeeds.

## Module correction

`mod_panel` previously defaulted to 20,000 Hz and hardcoded set-mode 1. The
stock values are 30,000 Hz and mode 0. The module now:

- defaults `panel.freq=30000`;
- reads `panel.mode` (0..8, default 0) and uses it for the set request;
- keeps the recovered open request at mode 1;
- keeps `panel.curve=1`, `panel.min_percent`, `panel.duty_max` and the vendor
  curve.

Tests: `check-panel-curve` and `check-panel-quiesce` pass, including
`last_open.freq=30000`, `last_set.freq=30000`, `last_set.mode=0`. ARM
`mod_panel.so` sha256
`caed827d184efdc59d0e8e02d57041629099ffbb88bb132c21feb5ea6e206ec0`.

## First run (stage error, superseded)

The first stage copied a stale `build-tv-v3/tests/mod_plat_stub.so` that was
actually the real `mod_plat` binary. The log showed `loaded plat v0.1` and the
full plat HAL sequence (`audio prelude`, `common init`, `NDAL_VideoExt_OpenDevice
(0,10)`). The panel module still started at 50%, but this was not a clean
panel-only test. The set was restored by `wdtctl set 10`; no persistent change.
The actual stub was then rebuilt from `tests/mod_plat_stub.c` (sha256
`37f8f320...`, descriptor `plat_stub`) and the test repeated.

That stub name later broke `check-vin-real`, which loads the same artifact as
`modules/mod_plat.so` and calls `load plat`. The stub source was changed to
publish the module name `plat` and a Makefile rule was added so the VIN harness
builds it reproducibly (sha256 `97ef7656...`). This does not change the panel
module evidence: the panel service only requires `OA_SVC_PLAT`, and the clean
stage above used the earlier `plat_stub` build. Full `make check` now passes.

## Clean run

Stage `/buffer/oa-panel-20261107b`, tar SHA256
`ab1171d17470252ecb86d2df887b81a2381002f38d85a39fd48fb95f9051a75a`;
`SHA256SUMS` and `--check` verified. Config:
`autoload=plat_stub, panel`, `panel.start=50`, `panel.curve=1`,
`panel.channel=0`, `panel.freq=30000`, `panel.mode=0`, `panel.duty_max=4095`.

Pre-state: appmgr 1569, `aurora.elf` 1771, stock `gstBacklightPara` level 1000.
Watchdog disabled, stock SIGTERM'd.

Start log:

```text
loaded plat_stub v1 (./modules/mod_plat_stub.so)
plat_stub: service up: plat v1
loaded panel v0.2 (./modules/mod_panel.so)
panel: vendor HAL linked: channel 0, 30000 Hz, start 50%
panel: service up: panel v1
Can not open /dev/nvt/ntkled , try path /dev/ntkled
Open /dev/ntkled successfully
```

`status panel` reported `state started`, `services panel:v1`; `panel-state`
reported `panel percent 50`. The host process held `/dev/ntkled` fd 6, so the
PWM open path reached the vendor device. `Hal_Pwm_Set_ByChannel`'s scalar result
is ignored by the module, and the actual brightness was not measured.

## Restore

`quit` unloaded both modules and removed the socket; stage/socket/heartbeat were
removed and `sync` completed. `wdtctl set 10` rebooted the set. After boot:

```text
appmgr 1577, aurora.elf 1774 (zombie 2211), no openaurora
gstBacklightPara level 1000, genPwmdimMode 0, gu32PanelFreqInHz 30000
stage/socket absent; hwprofile 1b063b2c... unchanged; fb1 1920x2160 pan 0,0
```

## Limits and next steps

- The 50% request reached the PWM HAL/device, but **visible brightness change
  is not independently confirmed**. A repeat with the owner watching, or a
  measured duty/photo, is needed for panel-brightness acceptance.
- The first run accidentally executed the real plat HAL; it was restored and is
  not part of the clean panel result.
- Panel power, backlight resume after real kernel PM suspend, and standby
  ownership remain unverified.
- Next: owner-confirmed 50% vs 100% observation, then panel power/resume; then
  audio/CEC and input/PVR groups.

## Correction, later on 2026-11-07: the HAL expects 0..1000, not 0..4095

The negative "no visible dimming" result above has a concrete firmware cause.
`mod_panel` called `Hal_Pwm_Set_ByChannel` directly with the 12-bit duty
(`0..4095`), but the vendor chain converts that word before the HAL:

```text
DEV_PANEL_SetPWMBacklightLevel @0x008578d9
  duty12 = level(0..1000) * 4.095, clamp 4095
DEV_PWM_Set_ByChannel @0x008523dc
  _dev_config_pwm @0x00850a84
    plat_set_dimming @0x0092e668
      duty12 < 0x1000 ? hal = duty12*1000/4095 : hal = 1000
      Hal_Pwm_Open/Set_ByChannel(ch, {freq, hal, shift, mode})
```

Checked against instructions: `plat_set_dimming` calls `FUN_01285200(duty*1000,
0xfff)` (an unsigned divide; a zero divisor raises, as elsewhere in this
firmware), and both its `Hal_Pwm_Open` initial duty and `Hal_Pwm_Set_ByChannel`
word 1 receive that 0..1000 value. Sending 2047 at 50% therefore saturates at
the device, exactly as sending 4095 does; 20% sent 819 (an effective ~82%),
which explains why the sweep showed almost no change.

Fix: `mod_panel` keeps its public `panel.curve`/`panel.duty_max`/`panel.raw`
values in the documented 12-bit domain and converts once at the HAL boundary
(`0 -> 0`, `>=0x1000 -> 1000`, otherwise `duty*1000/4095`), matching
`plat_set_dimming`. `check-panel-curve` now asserts the HAL receives `1000` at
100%, `600` at 50% and `200` at 0% for the default min-20% curve, and the old
direct 12-bit path fails the test. This is still not a brightness measurement;
visible dimming remains the acceptance gate.

`DEV_PANEL_PowerSequence` @0x00857ac0 is not a boot initializer. Its callers are
`ICE_HardwareActiveStandby` @0x0074c8ac/0x0074ca46 and the
`PLAT_SYS_PowerOffNow`/`Reset`/`ResetByHWWDT`/`PowerSavingNow` paths, so the
boot panel power comes from xboot/kernel/PreInit.

Read-only stock globals (`/proc/1778/mem`) resolve the panel-power polarity
that `DEV_PANEL_Init` copies from descriptors 0x6e/0x6f:

```text
GPIO table 0x01931270 -> 0x02923000
id 110 (PANELPWR) pin=110 dir=output; init copies input-level code = 1
id 111 (BKLTPWR)  pin=111 dir=output; init copies input-level code = 0
DAT_01935470 = 1   DAT_019354a8 = 0
PCB layout 14:
  PanelPowerSet(1)          -> SetLevel(110, 1);  (0) -> 0
  PanelBackLightPowerSet(1) -> SetLevel(111, 0);  (0) -> 1
```

The "input level" word is descriptor[5], not data beyond the 0x20-byte entry;
an earlier indexing pass was wrong. Standby panel power-down/power-up is still
unimplemented: the full sequence also needs LVDS mute/unmute, TCON PMIC GPIO and
the panel-extra delays, and partial GPIO-only power switching must not be used.
The polarity evidence is recorded for that work; it has not been exercised.
