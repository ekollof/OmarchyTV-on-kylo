# Audio curve control query — 2026-11-05

Bounded read-only integration of the existing `audio.curve` provider into the
control plane. No audio device, gain, mute, routing or HAL path is touched.

## What was added

- `audio-curve <volume>`: looks up `audio.curve` v1 and returns the selected
  16-bit AQ table entry as `curve <value>`.
- `audio-curve-dump`: returns `curve 101` followed by all 101 published
  entries on one bounded line.
- Both reject extra/missing arguments, non-decimal volume and values above
  `UINT_MAX`; a missing provider returns `ENODEV` and provider failure `EIO`.
- The provider remains the immutable `audio.curve` v1 data module; values are
  still read from the explicitly configured AQ/profile, with no default or
  hardware fallback.

## Verification

`tests/audio_curve_module.c` now checks `audio-curve 10` = `110`,
`audio-curve 101` = `200` (unsigned clamp), and the `curve 101 100 101 ...`
dump before/after reload (`210` after rereading base 200).

The real Unix-socket fixture test also queries the provider after start:

```
ok   audio-curve 10/101 and audio-curve-dump return the published AQ table over the control command
ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse
```

Focused `make -j24 check-audio-curve check-audio-curve-module
check-fixture-modules-ipc` passes.

## Limits

- Read-only DSP-table data: no speaker-out gain, ALSA control write, amplifier
  register, balance/mute/routing, decoder opcode33, profile/default fallback or
  audible acceptance.
- The query exposes a project `audio.curve` v1 provider, not the vendor HAL or
  its complete volume path.
- No TV access; no HAL/device was opened.
