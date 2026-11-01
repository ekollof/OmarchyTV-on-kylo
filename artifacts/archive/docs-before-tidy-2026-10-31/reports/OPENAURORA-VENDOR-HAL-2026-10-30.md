# Linking against the vendor HAL instead of rewriting it — 2026-10-30

Design decision for the T1 hardware modules of openaurora. Short version: **the parts that would
take months to re-implement (backlight, GPIO, amp/DSP, video) are exported by unstripped vendor
libraries in the firmware image. We call those, and only write the glue.**

## What is actually in the image

`ap0/lib` (mounted at `/vendor/lib` on the set) contains ~180 shared objects. The vendor ones are
**not stripped**, so we get names, sizes and (via Ghidra) signatures:

| library | size | symbols | what it is |
|---|---:|---:|---|
| `libNDAL3.so` | 388 KB | 4,132 | the audio/video HAL — `Hal_Audio_*`, `Hal_Pwm_*`, `Hal_Gpio_*`, `Hal_Video_*` (121 `Hal_*` exports) |
| `libNDAL_EX.so` | 198 KB | 2,332 | extended NDAL helpers |
| `libWasabi.so` | 5.8 MB | 74,033 | the UI framework behind the stock menus |
| `libdirectfb_tdga.so` | 56 KB | 373 | the OSD driver we already reversed |
| `libttx.so` | 506 KB | 1,901 | teletext |
| `libACL.so` / `libAIP.so` / `libSkb.so` | — | 3.4k / 3.5k / 1.8k | application/portal layers |

Third-party libraries are the stripped ones (`libomi.so` 64 MB, `libmali.so` 8.6 MB, ffmpeg) — that
is expected and not a problem, since we do not intend to call into them at that level.

## The backlight path, end to end

aurora's own chain, decompiled:

```
DEV_PANEL_SetPWMBacklightLv(0..1000)         curve mapping (gstBacklightPara + gstVoutPwmParams)
  └─ DEV_PANEL_SetPWMBacklightLevel(duty)    picks the PWM channel via DEV_PWM_Get_ChIdx
       └─ DEV_PWM_Set_ByChannel(ch, &cfg)    takes a semaphore, calls the HAL
            └─ plat_set_dimming(ch, freq, duty, shift, mode)
                 ├─ Hal_Pwm_Init(void)
                 ├─ Hal_Pwm_Open(&{ch, freq, duty, 0, 1})
                 └─ Hal_Pwm_Set_ByChannel(ch, &{freq, duty(0..4095), shift})
```

All three `Hal_Pwm_*` symbols are exported by `libNDAL3.so`. So `mod_panel` is:

```c
void *lib = dlopen("/vendor/lib/libNDAL3.so", RTLD_NOW | RTLD_LOCAL);
init = dlsym(lib, "Hal_Pwm_Init");
open_ = dlsym(lib, "Hal_Pwm_Open");
set   = dlsym(lib, "Hal_Pwm_Set_ByChannel");
```

with the channel and frequency from config (`panel.channel`, `panel.freq`) because they come from a
per-model board table we have not located yet. That is the calibration step: try the values on the
set and pin them.

**Why `dlopen` and not a link-time dependency:** the same module then builds and loads on a laptop,
where the library does not exist. `mod_panel` logs "no vendor HAL: panel control disabled" and the
rest of the system runs — which is what the test suite asserts.

## What this means per module

| module | approach | status |
|---|---|---|
| `panel` | `Hal_Pwm_*` via dlopen | **implemented** (`mod_panel`), soft-fails off-TV |
| `audio` | `Hal_Audio_*` (also `Hal_Audio_Ctrl`, `Hal_Audio_Send`, mixer/protect init) | viable; the AD82586C DSP driver is inside the HAL, not something we write |
| GPIO-ish (`power`, panel enable) | `Hal_Gpio_Read/Write/Config/Init` | viable |
| `display` | still `ntkndpdrm.ko` + KMS; the HAL does not replace the DRM plane work | unchanged |
| `pq` | `Hal_Video_*` / `PLAT_PQ` boundary, partly inside `libNDAL3` | to assess |

## The amp, specifically

`DEV_AMP_*` are **vtable dispatchers**: `DEV_AMP_SetVolume(handle)` calls `*(handle+0x80)`, and the
concrete driver is an object created by `DEV_AMP_FACT_Create`. The chip on this set is an
**AD82586C** (strings: `AMP_AD82586C_VESTEL_Create/Close/GetVolume/...`, subwoofer, EQ, RAM/register
dumps, all over I2C). Re-implementing that is a datasheet project.

Because `Hal_Audio_Ctrl` is exported by `libNDAL3.so` (aurora imports it, does not define it), the
sane path is to drive audio through the HAL and keep the AD82586C programming where it already lives.
This also explains the `ForceAMPInitial` workaround we found earlier: the amp init can fail and is
retried.

## Risks and rules

1. **Never call the HAL while aurora owns the same hardware.** These are process-global device
   handles, not reference-counted. Our modules only run when the stock UI does not.
2. **Init order matters**: `Hal_Pwm_Init` → `Hal_Pwm_Open` → set. Skipping `Init` is untested.
3. **Probe before trusting**: some HAL functions may require a global NDAL init that aurora performs
   early. If `Hal_Pwm_Open` returns 0 with no init, we will need to find that entry point.
4. **Version coupling**: the HAL matches firmware `0.51.52.3`. A future OTA could change signatures;
   `dlsym` failure is handled, but a silent ABI change is not. Pin the firmware or re-verify.
5. These libraries are Vestel/Novatek copyright — link, do not redistribute.

## Next steps

1. On the set: run `mod_panel` standalone (aurora not running) with `panel.channel` 0..7 and find the
   panel's channel by watching the backlight.
2. Then `mod_audio`: `Hal_Audio_*` init + volume, mirroring aurora's call order from
   `DEV_AMP_Init` / `_ice_Set_AMP_UNMUTE`.
3. Re-check `mod_display`: whether the HAL exposes an OSD/plane helper that shortens the DRM work.
