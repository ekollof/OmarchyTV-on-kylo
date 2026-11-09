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
- Control commands: `audio-source-select <n>`, `audio-source-state`.

Deliberate omissions (not recovered/portable here): the sound-output/HP
detection policy (`PLAT_VAUD_SwitchHP`), foreground/scart routing, speaker
main gain, `ice_Audio_SystemDelayUnMute`, and the
`ExternalAVSourceManager` video-type → `ICE_DeviceSource` mapping. The service
takes the device source directly.

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

The host test asserts the exact 13-call HDMI-class sequence (KItem 0x43,
EasySet 0x46/0x4e/0x4b, Ctrl 0x2a/0xfff and 0x2a/0x802, EasySelect 0/1,
2/0, 3/1, 0x32/1, 0x32/2, 5/0x801 with "HDMI ES", KItem 0x52), the ATV
class (atv EasySelect + path 5 "ATV"), the source-10 skip, the AIN cache, and
the disabled/out-of-range gates. The ARM module builds and links all five
vendor HAL symbols; the cold bundle includes it but does not autoload it.
