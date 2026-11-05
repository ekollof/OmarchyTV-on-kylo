# Fixture-backed module IPC evidence

Interpretation: [FIXTURE-MODULES-IPC](../../reports/FIXTURE-MODULES-IPC-2026-11-05.md).

- `check.txt`: real Unix-socket lifecycle result.
- `full-host-check.txt`: full strict host `make check` (400 ok).
- `identities.txt`: host, ui_theme, ui_profile, audio_amp_profile,
  audio_curve, audio_mixer and broker hashes.

The test creates only synthetic BinSVG/EFSL/AQ fixtures plus the extracted
hwprofile and real host broker. All five modules start, publish their services,
reload, stop/start or unload/load, and the host exits cleanly. No TV/HAL/
framebuffer or persistent file is touched.
