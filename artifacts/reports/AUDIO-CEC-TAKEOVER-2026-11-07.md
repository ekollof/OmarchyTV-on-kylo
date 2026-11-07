# Audio/CEC read-only takeover group — 2026-11-07

Owner authorization: TV read-write/full access, recovery reboot allowed. This
group staged the audio and CEC modules with hardware writes disabled. It is a
control-plane/start/status and read-only hardware-access test, **not** audible
audio or CEC bus acceptance.

## Pre-state (stock)

- appmgr 1577, `aurora.elf` 1774, zombie 2211.
- Stock fds: `/dev/snd/controlC0`, `controlC2`, `controlC3`, `/dev/i2c-3`,
  `/dev/video28` (two fds), `/dev/fb1`.
- CEC: `/proc/devices` has major **229 ntkcec**; no `/dev/ntkcec` node.
  The stock HDMI-switch/CEC path owns `/dev/video28`.
- Read-only mixer: `card=0 control=speaker out gain count=1 min=0 max=100
  values=0`.
- Read-only amp: `/dev/i2c-3`, slave `0x34`, registers `03..05` =
  `1b 05 05`, two completed I2C messages.
- `/conf/mb230_hwprofile.bin` MD5 `1b063b2c...`; `/conf/AQ.bin` MD5
  `0a56db61...`; fb1 `1920,2160` pan `0,0`; backlight level 1000.

## Stage

`/buffer/oa-av-20261107`, tar SHA256
`d8d2006af27302677a98026cb1e96c93bb37e4bf28442ba6787572ac57968575`;
`SHA256SUMS` and `--check` verified. Modules:

```text
mod_plat.so              test stub, descriptor name "plat"
mod_audio_amp_profile.so
mod_audio_curve.so
mod_audio.so
mod_cec.so
```

Config (original `openaurora.conf`) had `audio_amp_profile.hwprofile`; the
module key is `amp_profile.hwprofile`. The first start therefore failed
`audio_amp_profile: start() failed (-22)`. A read-only ARM parser probe on the
TV's profile returned `size=24514 rc=0 power='12Watt' prescale=27 ch1=5 ch2=5
fine=128`, proving the profile/parser was fine. The corrected
`openaurora-fixed.conf` (SHA256 `6f2262ba...`) was used for the clean run.

## Clean run

All five modules started:

```text
plat                started, plat:v1
audio_amp_profile   started, audio.amp_profile:v1
audio_curve         started, audio.curve:v1
audio               started, audio:v1
cec                 started, cec.owner:v1 cec.audio:v1
```

Queries:

```text
amp-profile   power '12Watt' prescale 27 ch1 5 ch2 5 fine 128 clip 405736 drc 98304 state -1 attack -1 release -1
audio-curve 50  curve 8137
audio-state     audio enabled 0 volume -1
cec-state       cec owner 1 audio 1 supported 0 local 1
```

`audio.enable=0` means the audio owner was created but no volume/mute/ALSA/HAL
write was requested. `mod_cec` reports `supported 0`: its hardware-open backend
is not installed, so it published policy services but did not open the raw
`ntkcec` device, claim a logical address or transmit a CEC frame.

The host process had **no** `/dev/snd`, `/dev/i2c-3`, `/dev/video28` or
`/dev/ntkcec` fds; only the module mappings were present. The read-only
hardware probes still matched stock:

```text
mixer values=0
amp completed=2 register03=1b register04=05 register05=05
```

## Restore

`quit`, stage/socket/heartbeat removed, `sync`, then `wdtctl set 10` rebooted
the set. After boot:

```text
appmgr 1564, aurora.elf 1764 (zombie 1941), no openaurora
fds: controlC0/C2/C3, i2c-3, video28, fb1
CEC major 229 ntkcec, no /dev/ntkcec
post-probes: mixer values=0; amp 1b/05/05
hwprofile 1b063b2c..., AQ 0a56db61..., fb1 1920x2160 pan 0,0, backlight 1000
```

The probe staging `/tmp/oa-av-probe` was removed and verified absent. No
staging, helper or disabled-watchdog state remains.

## Limits

- No audible audio path: no speaker gain/volume write, amp init/reset/mute/
  standby, DSP table application or amplified output was tested.
- No CEC hardware path: no raw device open, logical-address claim, authority,
  one-touch play, standby, ARC/audio or TX/RX. `supported 0` is explicit.
- The last stock read-only mixer value was 0 and amp registers `1b/05/05`; the
  probes confirm access and unchanged state, not audio health.
- `audio_amp_profile` and `audio_curve` are immutable/data providers over the
  TV's own profile/AQ files.

## Next

- Native amp lifecycle/worker with an audited mute or volume operation and
  owner-confirmed audible output; complete AD82587 init/reset/standby remains
  unimplemented.
- CEC authority/worker/raw backend, then spec-based identity/ARC/volume policy
  and a controlled TX/RX acceptance.
- Do not treat this group as closing the audio/CEC TODO items.

Evidence: `artifacts/analysis/audio-cec-2026-11-07/`.
