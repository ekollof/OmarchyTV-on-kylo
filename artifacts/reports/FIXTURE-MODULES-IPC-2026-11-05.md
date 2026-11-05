# Fixture-backed module socket lifecycle — 2026-11-05

Host-only test. No TV/HAL/framebuffer module.

## Coverage

`tests/fixture_modules_ipc.cjs` starts a real `openaurora --sock` host with
fixture paths and drives these modules through the Unix socket:

- `ui_theme` — synthetic BinSVG index/binary (record `SettingListBoxUFA.svg`);
- `ui_profile` — synthetic 168-byte EFSL software profile;
- `audio_amp_profile` — extracted `artifacts/profiles/mb230_hwprofile.bin`;
- `audio_curve` — synthetic 1668-byte AQ file with profile `0`;
- `audio_mixer` — real `build/oa-audio-broker` with `--serve`.

## Verified

- All five load/start and publish `ui.theme:v1`, `ui.profile:v1`,
  `audio.amp_profile:v1`, `audio.curve:v1`, `audio.mixer:v1`.
- `amp-profile` returns the saved target profile fields
  (`power '12Watt' prescale 27 ...`).
- Reload succeeds for all five (CEC-style async cleanup retry is used for
  audio_mixer).
- `audio_curve` and `audio_amp_profile` stop/start reuse their services.
- `audio_mixer` stop/unload/load/start reuses the broker service.
- `ui_theme` stop/start and `ui_profile` reload pass.
- SIGTERM exits 0 and removes the socket.

```
ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, services, reload and reuse
```

## Evidence

- `artifacts/analysis/fixture-modules-ipc-2026-11-05/check.txt`
- `full-host-check.txt` (400 ok)
- `identities.txt` (host, five modules, broker)
- `verify.cjs`: 45 groups

This broadens real socket/CLI lifecycle coverage to the remaining fixture-
backed modules. It is not physical fb1 ownership, hardware-module reload, cold
startup or stock parity.
