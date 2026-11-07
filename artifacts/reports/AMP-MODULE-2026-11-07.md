# Gated audio_amp module — 2026-11-07

Scope: wire the reviewed amp transport into a reloadable module with an
explicit hardware gate, the real profile parser and the existing `amp_owner`
worker. Offline/host work only: the module is built and preflighted for the TV,
but no module start, device open or register write occurred on hardware.

## Module

`modules/audio_amp/audio_amp.c`, descriptor `audio_amp` v0.1, service
`audio.amp:v1`.

Config:

```text
audio_amp.enable = 0|1          hardware gate, default 0
audio_amp.hwprofile = PATH      EFSL profile, default /conf/mb230_hwprofile.bin
audio_amp.cleanup_delay_ms = N  diagnostic async-quiesce delay
```

Control socket:

```text
amp-state   amp enabled E busy B result R ticket T reset R mute M
amp-init    ok | failed errno N
```

`amp-init` only submits; the blocking Init runs on the module's `amp_owner`
worker. `amp-state` polls the completion nonblocking and copies the ticket,
native result, reset/mute caches. `result -1` means no completed result yet;
`busy 1` means submitted and not collected. The owner allows one pending
request or one untaken result, so the caller must collect with `amp-state`
before submitting again.

Startup with `enable=1` requires a ready `plat` service (platform GPIO/video
prelude; the STBC path needs the video-open global fd), loads the profile,
opens the fixed MB230 AD82587 `/dev/i2c-3` session for writing and creates the
owner with the recovered cold-boot image. Any failure before publication
closes the session, releases plat and publishes nothing. Quiesce unpublishes
the service, detaches owner/session and finishes the join plus I2C close on a
worker; the host retries until it exits.

## Recovered cold-boot image

The vendor Init patches a retained 38-byte global. Its initializer is in
`aurora.elf` `.data` at virtual `0x017640e8` (file `0x017440e8`; `.data`
`0x017563c0/0x017363c0`). Added as `oa_ad82587_baseline` in
`common/audio_amp.c` with FNV-1a-32 `0xab635b86`, asserted in
`tests/audio_amp.c`. This is the firmware cold-boot image, not the live stock
image (the live 03..05 read `1b 05 05`; the baseline has `0e 0d 0d`). The
configure function still invents no baseline.

## Tests

- `check-audio-amp` (existing image tests) now also asserts the recovered
  baseline length/hash and the `0e/0d/0d` bytes.
- `check-amp-vendor` passes (composition, routing, vendor booleans, full
  sequencer).
- New `check-audio-amp-module` (`tests/audio_amp_ipc.cjs`) uses the real host
  binary and control socket:
  - `enable=0`: start succeeds, `amp-state` reports
    `amp enabled 0 busy 0 result -1 ticket 0 reset 0 mute 0`; `amp-init`
    returns ENODEV; `reload` completes under 1.5 s; stop makes `amp-state`
    ENODEV.
  - `enable=1` on host with the real profile: start fails at the native I2C
    gate, publishes no service, `amp-state`/`amp-init` return ENODEV.
- ARM build: `build-tv-v3/modules/mod_audio_amp.so` sha256
  `41287a161697e97cab82fbbb6bc4161528aae450b79e938c241d82c985f33ada`;
  NEEDED includes `libNDAL3.so`, `libNDAL_EX.so`, ALSA, libv4l2; only
  `Hal_Gpio_Write/Config` and `NDAL_VideoExt_STBC_SetStbcGpio` remain
  undefined; no glibc newer than 2.17.
- TV read-only preflight: staged core + the module, `--check` returned
  `Runtime preflight PASSED` and `OK ./modules/mod_audio_amp.so`; staging
  removed and verified absent. No module entry/start or device access.

## Hardware plan (not run)

1. Owner-authorized audio window: stop stock Aurora with the watchdog
   disabled.
2. Stage the real `mod_plat` + `mod_audio_amp` (enable=1) and the read-only
   `amp-probe`; verify the manifest and `--check`.
3. Start plat, start audio_amp, `amp-init`, then poll `amp-state` until
   `busy 0` and a result. Expect `result 1`, `reset 0`, `mute 1`.
4. Independently read registers 03..05 with `amp-probe --read-volume`; expect
   the profile values (`1b 05 05` for 12Watt/27/5/5).
5. With the owner watching/listening, confirm no stuck mute/standby and that
   audio recovers. This is the audible acceptance; the scalar result is not
   audible proof.
6. `quit`, remove staging, `wdtctl set 10` reboot, verify stock PIDs, amp
   registers, source/fb/backlight and no helper/staging.

## Limits

- No hardware start, GPIO claim/reset, STBC mute or register write was run.
- The real `mod_plat` platform sequence is required for the STBC global fd;
  the module does not open video24 or GPIO itself.
- No paired amp shutdown/standby, no readback-in-module, no subwoofer behavior
  beyond disabled/no-op.
- The module is in the AV stage inventory, not the cold core bundle.
- Owner audible confirmation and stock restoration remain the acceptance gate.

## Evidence

`common/audio_amp.c` (baseline), `modules/audio_amp/audio_amp.c`,
`include/openaurora/services.h` (`audio.amp`), `core/control.c`
(`amp-state`/`amp-init`), `tests/audio_amp.c`, `tests/audio_amp_ipc.cjs`,
`openaurora/Makefile`.
