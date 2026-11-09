# Audio source service — 2026-11-09

Status: **implemented behind `audio.source_enable=0`, fake-HAL host-tested.
No TV, no audible acceptance.** This is the `ICE_AudioStart` external-source
branch, not the DVB decode route.

## Recovered chain

`DvbAudioPlayer::startNonDVBAudio` `0x00a6d0b8` invokes `IceAudioIF`'s vtable
`+0x20`; the concrete path is `ICE_AudioStart` `0x007e2114`. Its external
branch (`*param_1 == 0`, `param_1[1] != 0xb`) at `0x007e2328` is:

```text
PLAT_VAUD_SetSoftMute(1, 1)
PLAT_AOUT_SpeakerConnect(0, 0) ... (3, 0)
if source != 10:
    PLAT_AIN_Connect(0, table[source], 0)
    PLAT_AIN_Enable(0)
if source != 10 and (is_adc(source) or source < 2 or DAT_007e2448 == 0):
    ice_Audio_SystemDelayUnMute(0, 0x5a)
if is_adc(source): PLAT_OS_TaskDelay(200)
PLAT_AOUT_Connect(0, table[source], 0)
PLAT_AOUT_SetMute(0, 0)
... PLAT_VAUD_SwitchHP / PLAT_AOUT_SetForeground / scart mute ...
```

`*puVar3` (the AIN id at `0x0190fb20`) is **0** on the stock process; the soft
mute mask `DAT_0197f99c` is **0**; the soft-mute speaker ids
`DAT_0176da40/da00/da6c` are **0/1/2**, with a literal **3** for the fourth.

## Source table `0x012d818c` (56 ints, indexed by ICE_DeviceSource)

```text
 0: 0x201   1: 0x202   2..7: 0x7fff  8: 0x101   9: 0x102
10: 0x201  11: 0x1200 12: 0x1201   13: 0x201  14..33: 0x7fff
34: 0x1501 35: 0x301  36: 0x601    37: 0x801  38: 0x802 39: 0x803
40: 0x804  41: 0x301  42..51: 0x7fff  52: 0x1800  53/54: 0x7fff
55: 0x201
```

`_ice_IsADCSource` `0x007dd2a8`: sources `0x22..0x29`, mask `0x87` (0x22,
0x23, 0x24, 0x29).

## `PLAT_AIN_Connect` `0x00884528`

- `src == 0x7fff` or `in > 1` → no action.
- same `(in, src)` already connected → returns 0 without HAL calls.
- class `0x800`:
  - `Hal_Audio_Ctrl(0x2a, {0xfff, 1})` and `(0x2a, {0x802, 0})` on a zeroed
    0x38-byte payload;
  - `PLAT_DEC` path for `0x801..0x804` → `plat_adec_connect(0, 2)`.
- class `0x100`: `Hal_Audio_EasySelect(1, 1, &atv_on)` (`{1,"atv on"}`).
- always `plat_audio_connect(in, src)`.

`PLAT_AIN_Enable` `0x00884e90` is only a range check (`in < 2`) and trace; it
has no HAL call.

## `plat_adec_connect(0, 2)` `0x009233dc`

```text
Hal_Audio_EasySelect(0, 1, &adc_on)          // {1,"adc on"}
Hal_Audio_EasySelect(2, 0, &decoder0)        // {0,"decoder0"}
Hal_Audio_EasySelect(3, 1, &hdmirx_on)       // {1,"hdmirx audio on"}
Hal_Audio_EasySelect(0x32, 1, &dec0_open)    // {1,"dec0 open"}
Hal_Audio_EasySelect(0x32, 2, &dec0_open)    // no id-2 record; vendor no-op
```

## `plat_audio_connect(in, src)` `0x009244ac`

```text
path   = (in == 0) ? 5 : 8
target = (src & 0x800) ? 0x801 : ((src & 0x1200) == 0x1200 ? 0x1200 : src)
Hal_Audio_EasySelect(path, target, &easy_table)
```

The table at `0x01773180` begins `{0x201,"DTV DEC0"}, {0x202,"DTV DEC1"},
{0x801,"HDMI ES"}, {0x801,"HDMI PCM"}, {0x501,"LineIn/Composite"},
{0x1501,"SCART"}, {0x101,"ATV"}, {0x1200,"MM"}, {0x801,"HDMI"}, ...` and the
HAL picks the first matching id, so `0x801` resolves to **HDMI ES**.

## `PLAT_AOUT_*` and `PLAT_VAUD_SetSoftMute`

- `PLAT_AOUT_Connect(out, src, in)` `0x00887e38`: internal output/input table
  update only, no HAL call.
- `PLAT_AOUT_SpeakerConnect(spk, out)` `0x00888498`: internal speaker table
  update only. On this set speakers 0..3 exist with types 0..3 and out 0.
- `PLAT_AOUT_SetMute(out, mute)` `0x00888950`: code `0x52` when the output's
  stored input is 0, otherwise `0x5b`; `Hal_Audio_KItem(code)` then
  `Hal_Audio_SetControl(kitem, {code, &mute, 0})`.
- `PLAT_AOUT_SetSpeakerMute` + `FUN_00887040`: speaker type 0 → KItem(0x43);
  type 1 → `EasySet(0x46, &mute, 0)`; type 2 → `EasySet(0x4e,...)`; type 3 →
  `EasySet(0x4b,...)`.
- `PLAT_VAUD_SetSoftMute(mask, second)` `0x008d1eb8` → `FUN_008d11f0`:
  speaker 0 with `last|mask` when `second <= mask` or `last == 0`; an output
  mute when `mask & 0`; speakers 1/2/3 when `second != 0`; store `last` only
  when `second == 0`. `ICE_AudioStart` calls `(1,1)`.

## ExternalAVSourceManager mapping and HP policy

`ExternalAVSourceManager` uses a 13-record table at `0x0175de8c`, stride
`0x38`, fields `+0` video device, `+4` audio device, `+8` video type,
`+0x2c` name. `ICE_ExternalGetVideoDevice/AudioDevice` read those fields;
`selectExtSource(index)` uses the interface index, and
`getExternalIndexFromSource(device)` scans the video-device field.

| idx | video | audio | type | name |
|---:|---:|---:|---:|---|
| 0 | 0x0e | 0x22 | 1 | EXT1 |
| 1 | 0x15 | 0x22 | 6 | EXT1 |
| 2 | 0x0f | 0x23 | 2 | Fav |
| 3 | 0x17 | 0x29 | 6 | Svhs |
| 4 | 0x11 | 0x25 | 7 | HDMI1 |
| 5 | 0x12 | 0x26 | 7 | HDMI2 |
| 6 | 0x13 | 0x27 | 7 | HDMI3 |
| 7 | 0x16 | 0x28 | 7 | HDMI4 |
| 8 | 0x14 | 0x29 | 3 | YPbPr |
| 9 | 0x10 | 0x29 | 8 | VGA |
| 10 | 0x20 | 0x34 | 10 | DVD |
| 11 | 0x1d | 0x0b | 13 | Widi |
| 12 | 0x1e | 0x32 | 14 | APM |

The Bronlijst/VIN source types already used by `mod_source` are the video
device values, so the implemented mapping is: 15 (CVBS) → 0x23, 16 (VGA) →
0x29, 17 (HDMI1) → 0x25, 18 (HDMI2) → 0x26, 19 (HDMI3) → 0x27, 20 (YPbPr) →
0x29, 22 (HDMI4) → 0x28. TV (0) has no external interface; WiDi/APM are 0x0b
and 0x32 but have no VIN slot in the decoded switch.

**HP policy** (`ICE_AudioStart` `0x007e23b6`):

```text
sound = _ice_Sound_Control_GetSoundOutput()
ep    = DEV_GPIO_EP_IsPlugIn()
mode  = 3
if sound == 3: mode = ep ? 1 : 0
else if sound == 4: mode = 2
else if sound == 0 and !ep: mode = 0
PLAT_VAUD_SwitchHP(mode)
```

`DEV_GPIO_EP_IsPlugIn` reads byte `+4` of the EP descriptor at `0x01931284`;
`_ice_Sound_Control_GetSoundOutput` returns the word behind `0x0078c0e8`.
Stock values are sound output **1**, EP **not plugged** → mode **3**.

`PLAT_VAUD_SwitchHP` `0x008d3fd8` takes the stock speaker branch
(`DAT_0176da40 == DAT_0176da00`). The mode table at `0x0198128c` is five
16-byte records (`A,B,C,D`); stock mode 3 is `0f 01 34 00`. The sequence is
`SetSpeakerDACCICGain(4,0,A,B)` and `(4,1,C,D)`, each looping spin 0..8 into
`Hal_Audio_Ctrl(0x2c/0x2d, {i,4,A,B})`, then `FUN_008d0dc4(0/1)` →
`Hal_Audio_Ctrl(0x2b,{1})` and `(0x2b,{0})`. The module carries the five
records read from the stock process and computes the mode from
`audio.source.sound_output` (default 1) and `audio.source.ep_plugged`
(default 0), so the default path emits the exact stock mode-3 sequence.

## Implementation

- `include/openaurora/audio_source_hal.h`: injectable seam for
  `audio_ctrl`, `easy_select`, `easy_set`, `kitem_set` and `delay_ms`.
- `common/audio_source_hal_vendor.c`: direct libNDAL3 calls
  (`Hal_Audio_Ctrl/EasySelect/EasySet/KItem/SetControl`).
- `modules/audio_source/audio_source.c`: mirrors the PLAT state machines and
  the `ICE_AudioStart` order, with the 56-entry source table, the 16-entry
  EasySelect table and the recovered records.
- `audio.source_enable` (default 0) gates every HAL call; the service returns
  -2 when disabled.
- Control commands: `audio-source-select <n>`, `audio-source-video <type>`,
  `audio-source-state`.

Deliberate omissions (not recovered/portable here): foreground/scart routing,
speaker main gain, `ice_Audio_SystemDelayUnMute`, and the non-stock
`PLAT_VAUD_SwitchHP` branch. The service takes either the device source or the
Bronlijst/VIN video type.

## Evidence

```text
$ make check-audio-source
ok audio source: soft mute, AIN/ADEC/audio EasySelect, output mute, gate

$ nm -D -u build-tv-v3/modules/mod_audio_source.so
U Hal_Audio_Ctrl
U Hal_Audio_EasySelect
U Hal_Audio_EasySet
U Hal_Audio_KItem
U Hal_Audio_SetControl
```

The host test asserts the exact HDMI-class sequence including the 20-call HP
switch (KItem 0x43,
EasySet 0x46/0x4e/0x4b, Ctrl 0x2a/0xfff and 0x2a/0x802, EasySelect 0/1,
2/0, 3/1, 0x32/1, 0x32/2, 5/0x801 with "HDMI ES", KItem 0x52), the ATV
class (atv EasySelect + path 5 "ATV"), the source-10 skip, the AIN cache, the
video-type mapping (17 → 0x25, 15 → 0x23 + ADC delay), and the
disabled/out-of-range gates. The ARM module builds and links all five
vendor HAL symbols; the cold bundle includes it but does not autoload it.

## Live audio acceptance attempt (2026-11-09)

Status: **route executed on the TV; audible acceptance is still unproven.
The TV was rebooted and stock restored.**

The first gated run (`source.enable=1`, `audio.source_enable=1`,
`plat.drm_init=1`) returned `ok` for `source-select 17` and
`audio-source-video 17`, but the NDAL audio controls failed with
`Invalid CTL hw:3..9`: the process did not have stock's
`ALSA_CONFIG_PATH=/vendor/etc/alsa.conf` (`vendor/run.sh:254`). The new
`set_vendor_alsa_config()` in `core/main.c` sets it before any module starts.

The second run had no ALSA errors and logged:

```text
[588293] INFO host         source -> type 17 (vin id 3, 2 fd)
[588370] INFO host         audio source -> 37 (ain 0x801, out0 in 0)
main out source select   enum  2
speaker out mute         bool  1
```

Direct `Hal_Audio_EasySelect(5, id, table)` probes returned 0 and changed the
main-out enum index for `0x1200` (7) and `0x1800` (14), but
`oa-alsactl get` printed the label as `DTV DEC0` for every index. The tool's
enum-item label lookup is therefore unreliable, and the `0x801` probe wrote
index 2, which the tool also labels `DTV DEC0`. **The main-out selection
(HDMI ES vs DTV) is not yet independently verified**, and no audible
confirmation was obtained. Stock was restored and the TV rebooted; the gates
remain 0 in every profile. Fixing the enum-label diagnostic (or reading the
kernel enum table directly) is the next acceptance step.
