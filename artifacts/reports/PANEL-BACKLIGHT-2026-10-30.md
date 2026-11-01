# Backlight (PWM) — authoritative contract vs what we measured — 2026-10-30

> **Status: Mixed historical measurements and hypotheses. PWM-CONTRACT-RECHECK owns declarations; brightness/override cause and independent control remain unverified.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

**Return-path correction, 2026-10-31:** ARM instructions contradict the old
Ghidra void prototypes for Init/Open/Set/Terminate; see
[PWM-CONTRACT-RECHECK-2026-10-31.md](PWM-CONTRACT-RECHECK-2026-10-31.md).
The six-/four-word layouts remain valid. Current declarations are maintained in
`openaurora/include/openaurora/vendor_pwm.h`.

Investigating why `mod_panel` can call the vendor PWM HAL without any visible effect.
Everything below is decompiled from the firmware (`aurora.elf`, `libNDAL3.so`), not guessed.

## The call chain (aurora, verified)

```
DEV_PANEL_SetBacklightLevel(0..1000)          dev_panel.cpp
  └─ PLAT_VIU_GetPanelExtraStruct()[10]       invert flag (1000 - level)
  └─ DEV_PANEL_SetPWMBacklightLv(level)
       └─ curve map through gstBacklightPara / gstVoutPwmParams
       └─ DEV_PANEL_SetPWMBacklightLevel(duty)
            ├─ ch   = DEV_PWM_Get_ChIdx(0x6c)
            ├─ freq = gu32PanelFreqInHz
            ├─ shift = 0
            ├─ mode  = genPwmdimMode
            ├─ duty = clamp(level * 4.095, 0xfff)
            └─ DEV_PWM_Set_ByChannel(ch, &{freq, duty, shift, mode})
                 └─ _dev_config_pwm → plat_set_dimming → Hal_Pwm_*
```

`DEV_PANEL_GetPWMBackLightLevel()` is an **empty function** — the firmware cannot read the level
back either. `DEV_PANEL_SetOSDPWMBackLightLv()` only caches the value in `gstBacklightPara`.

## The HAL contract (decompiled from libNDAL3.so, ghidra_ndal3 project)

```c
int Hal_Pwm_Init(void);
int Hal_Pwm_Open(uint desc[6]);                      /* { ch, freq, duty, shift, mode, extra } */
int Hal_Pwm_Set_ByChannel(uint ch, uint desc[4]);    /* { freq, duty, shift, mode }             */
void Hal_Pwm_Close(void);                            /* no arguments!                           */
```

**Corrected:** Init/Open/Set deliberately return scalar values; Close takes no
argument and has no deliberate scalar result. Ghidra's old void prototypes were
inaccurate. A nonzero result can skip the ioctl and is not panel-level readback.
The earlier three-word request omitted the required mode word, explaining the
driver's `Not Support Mode %d` message (it read `desc[3]` from whatever followed our struct).
`mode` must be `< 9`.

Internally `Set_ByChannel` copies our words into a 60-byte request and issues
`ioctl(0xc03c4c53)` on the LED device; `Open` issues `ioctl(0x403c4c4c)`.

## Board constants (read from .data)

| symbol | value |
|---|---|
| `gu32PanelFreqInHz` | **15000** |
| `genPwmdimMode` | **1** |
| `astPwmBasePin[4]` | `{108,9,0}`, `{119,2,9}`, `{124,1,11}`, `{126,1,12}` → pins 108–116 → channels 0–8 |

## The GPIO side

`DEV_PWM_Get_ChIdx(0x6c)` asks the GPIO driver for the pin of the interface named
`au8GPIOFuncName[0x6c]` and maps it through `astPwmBasePin`. The names are static:

```
0x6c BACKLIGHT_LEVEL   0x6e PANELPWR   0x6f BKLTPWR   0x71 Tcon0 …
```

`DEV_GPIO_GetDescription()` is a 0x88-byte struct, the interface **name pointer sits at +0x80**, and
the driver is queried with `ioctl(0xc088671f)` on the GPIO device. Implemented as
`/buffer/gpioprobe` on the set; results:

```
BACKLIGHT_LEVEL  id=108 pin=1 dir=none
BKLTPWR          id=111 pin=0 dir=output
PANELPWR         id=110 pin=0 dir=output
```

`pin=1` does **not** land in any `astPwmBasePin` range (108–116), so `DEV_PWM_Get_ChIdx` would fail
and `DEV_PANEL_SetPWMBacklightLevel` would bail out with an error — i.e. even the firmware's own
lookup would not resolve a channel from this field. That strongly suggests the "pin" word at +4 is
not the same quantity the PWM table indexes, or the driver answers differently for a caller that
has already configured the pin.

## What was tried on the set

| attempt | result |
|---|---|
| channel 0, 20000 Hz, 100% | HAL opened `/dev/ntkled`, no error |
| channel 0, 20% | no visible change |
| channel 1, 15000 Hz, 20% | no visible change |
| `panel.start` sweep 100/50/30/10 | all accepted; only the void-return misread made them look like failures |
| all channels 0–12, 10 % for 3 s then 100 % | **brief flicker, then back to normal** |

Historical interpretation (not a proven cause): the panel *did* react, but only momentarily. A correct channel with
a working path gives a steady dim for the whole 3 s; a flicker at the transition followed by an
immediate return to normal means the value was applied and then **overridden**.

That fits the architecture: aurora owns the panel and keeps `gstBacklightPara` as its cached level, so
anything that re-applies the cached value (OSD wake, picture-mode update, panel power sequence) undoes
our write within a frame or two. So the PWM path is real and reachable — it just cannot hold a value
while aurora is running.

Conclusion: the HAL device path is reachable, but the sweep did not isolate the
correct channel, prove who restored the level, or verify independent panel control.
The override explanation is a hypothesis consistent with concurrent vendor ownership.

## Historical follow-up ideas (not an active experiment list)

The flicker did not establish a cause. Trace board/profile/channel selection and
cached-level callers offline first. The ideas below were proposed historically;
debug-flag writes, menu actions and takeover are not authorized while in use.

1. **Watch the vendor's own debug print.** `DEV_PANEL_SetPWMBacklightLevel` logs
   `DEV_PWM_Set_ByChannel(CH=%d, Freq=%d, Level=[%d,%d])` when `DAT_01935458 != 0`. Change the
   brightness in the TV's own menu with that flag set: the log then gives the exact channel, freq and
   level the firmware uses, and confirms whether it re-applies periodically.
2. **Find the re-applier.** `gstBacklightPara` is the cached level; grep who writes it
   (`DEV_PANEL_SetOSDPWMBackLightLv`, `DEV_PANEL_SetBacklightLevel`) and what calls those on a timer or
   on source/picture-mode changes.
3. **Test without aurora.** The only clean steady-state test is a process that owns the panel. That is
   the native-shell scenario (`openaurora` with aurora stopped), which is the same session that tests
   `mod_display` ownership and the 4K→1080p region write.

Panel behavior remains required parity work. Its request ABI is corrected; init/close
ownership, brightness/power curves and resume are not complete. The steps above
are historical possibilities, not authorization to change an in-use TV.
