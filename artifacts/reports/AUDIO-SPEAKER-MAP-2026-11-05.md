# Speaker level/balance mapping — 2026-11-05

Pure, firmware-grounded speaker volume/balance clamp policy. Read-only; no
speaker id selection, HAL/ALSA write, amplifier access or audible test.

## Firmware grounding

`PLAT_VAUD_SetSpeakerVolumeBalance` (`0x8d1c3c`) for the two accepted speaker
ids:

- level `>99` becomes `100`;
- balance is clamped to `[-50,50]`;
- `PLAT_AOUT_SetSpeakerBalance` receives `balance + 50` (0..100);
- current left volume is read; if it differs from the clamped level,
  `PLAT_AOUT_SetSpeakerVolume(id, level, level)` sets both channels equal.

`PLAT_AOUT_SetSpeakerVolume` (`0x889680`) clamps each channel to 100 and stores
the cached left/right values. `PLAT_AOUT_SetSpeakerBalance` stores the balance
and emits HAL control `0x16` only for valid output units.

## Added

- `include/openaurora/audio_speaker.h` and `common/audio_speaker.c`:
  `oa_audio_speaker_map(level, balance, out)` returns clamped level, HAL
  balance and equal left/right levels.
- `audio-speaker <level> <balance>` control query returns
  `speaker level L left L right L balance B`.
- `tests/audio_speaker.c` covers level 0/50/99/100/101/UINT_MAX, balance
  -100/-51/-50/-1/0/1/50/100 and the null output.
- `fixture_modules_ipc.cjs` checks `audio-speaker 50 -50` →
  `balance 0`, and `audio-speaker 150 100` → level/balance `100`.

## Verification

```
ok audio speaker: level 0..100, balance -50..50, HAL balance 0..100, equal channels
ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse
```

The cross compiler also rebuilds `build-tv-v3/openaurora` with
`common/audio_speaker.o`.

## Limits

- Models the clamp/state mapping only. It does not validate the two vendor
  speaker ids, read/write the cached volume, call HAL/ALSA, touch the AD82587
  or any GPIO/MCU path.
- No mute, routing, DAP/balance filter, source-specific policy, default/profile
  fallback or audible acceptance.
- `audio-speaker` is a project query, not a vendor API.
- No TV access or device open.
