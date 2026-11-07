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
amp-state   amp enabled E busy B result R ticket T reset R mute M live L known K
amp-init    ok | failed errno N
amp-mute <0|1>  ok | failed errno N
```

`amp-init` only submits; the blocking Init runs on the module's `amp_owner`
worker. `amp-state` polls the completion nonblocking and copies the ticket,
native result, reset/mute caches, plus the live mute state. `result -1` means
no completed result yet; `busy 1` means submitted and not collected. The owner
allows one pending request or one untaken result, so the caller must collect
with `amp-state` before submitting again. `amp-mute` drives the same vendor
level path (pin 208 STBC command 3/2) after Init; it is a request state, not a
hardware readback.

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
    `amp enabled 0 busy 0 result -1 ticket 0 reset 0 mute 0 live 0 known 0`;
    `amp-init`/`amp-mute` return ENODEV (invalid `amp-mute 2` is EINVAL);
    `reload` completes under 1.5 s; stop makes `amp-state` ENODEV.
  - `enable=1` on host with the real profile: start fails at the native I2C
    gate, publishes no service, `amp-state`/`amp-init`/`amp-mute` return
    ENODEV.
- ARM build: final `build-tv-v3/modules/mod_audio_amp.so` sha256
  `d6f7ab3109ea19cb064c2e1410bf0d1f1d5bbc9a1f6a58dbe4c523dbf69d89e9`;
  NEEDED includes `libNDAL3.so`, `libNDAL_EX.so`, ALSA, libv4l2; only
  `Hal_Gpio_Write/Config` and `NDAL_VideoExt_STBC_SetStbcGpio` remain
  undefined; no glibc newer than 2.17. (The first hardware run used
  `41287a16...` before `amp-mute` existed.)
- TV read-only preflight: staged core + the module, `--check` returned
  `Runtime preflight PASSED` and `OK ./modules/mod_audio_amp.so`; staging
  removed and verified absent. No module entry/start or device access.

## Hardware runs (2026-11-07)

### Run 1 — stale core (superseded)

Stage `oa-amp-20261107` used a core binary built before `amp-state`/`amp-init`
were added. Both modules started (`plat`, `audio_amp` hardware enabled) but the
commands were unknown, so **no Init was submitted and no write occurred**. The
host was quit cleanly, staging removed, the core rebuilt
(`198e7cdc...` at the end of the session) and the run repeated.

### Run 2 — Init succeeds; Init leaves the amp muted

Stage `oa-amp-20261107b` (core `407b6103...`, module `41287a16...`), real
`mod_plat` + `audio_amp` (`enable=1`), stock 1772 stopped, watchdog disabled.

```text
pre:   amp enabled 1 busy 0 result -1 ticket 0 reset 0 mute 0 live 0 known 0
init:  ok
t+1.6s amp enabled 1 busy 0 result 1 ticket 1 reset 0 mute 1 live 1 known 1
```

The Init write loop, reset pulse and final mute all reported vendor success.
The independent `amp-probe --read-volume` then failed
`AD82587 read: Operation not permitted`, and `dmesg` showed
`ki2c: [i2c-3] Dev NACK`. The probe had worked in the pre-state.

After stock reboot, the live vendor image at `0x17640e8` was read and compared
with the embedded baseline. Every difference matched `oa_ad82587_configure`'s
patches (03..05 = `1b 05 05`, 07 |= 0x30, fine tune, power clip `06 30 e8`,
DRC `01 80 00`, release `01 80 00`, etc.). This confirms the cold-boot
baseline assumption; the NACK was not a wrong image.

`DEV_AMP_Mute` @ `0x0084f98c` dispatches through the amp vtable at `+0x70`, and
`DEV_AMP_Init` is called before the runtime audio flow; the Init sequence
deliberately ends at `Mute(1)`. The amp/MCU mute state therefore also cuts the
amp's I2C response. The module initially had no way to leave that state, which
was a real stuck-mute defect.

### Fix — explicit post-Init mute control

- `oa_amp_vendor_level()` exposes the same pin-level dispatch used by Init.
- `audio.amp:v1` gained `set_mute`; the control socket gained
  `amp-mute <0|1>` and `amp-state` now reports `live`/`known` mute state.
- A completed Init sets `live 1` (vendor parity). The caller must follow with
  `amp-mute 0` when the audio path is ready, matching the stock post-Init flow.

### Run 3 — Init, unmute, independent readback

Stage `oa-amp-20261107c` (tar `f404d5d1...`, core `198e7cdc...`, module
`d6f7ab31...`), stock 1772/uptime 280 s. Pre-state probe: `1b 05 05`.

```text
pre:    amp enabled 1 busy 0 result -1 ... live 0 known 0
init:   ok
t+1.6s amp enabled 1 busy 0 result 1 ticket 1 reset 0 mute 1 live 1 known 1
probe while init-muted: failed (EPERM/NACK)
amp-mute 0 -> ok
state:  amp enabled 1 busy 0 result 1 ticket 1 reset 0 mute 1 live 0 known 1
probe after unmute: completed=2 register03=1b register04=05 register05=05
```

This is the first complete hardware cycle: all 38 writes and the reset/mute
calls returned success, the init-muted state was reproduced, and the explicit
unmute restored I2C and the profile register values. It is **not** audible
acceptance: the TV had no active source/volume (stock volume 0), and no one
confirmed sound.

## Restore

`quit`, staging/socket/heartbeat removed, `sync`, `wdtctl set 10` reboot.
After boot (uptime 42.7 s): appmgr 1575, `aurora.elf` 1777 (zombie 1940),
no openaurora; stock fds include `/dev/i2c-3`, video24, fb1 and ALSA controls;
post-reboot amp probe `1b 05 05` succeeds; settings source 5/previous 4/volume 0;
hwprofile `1b063b2c...`, fb1 `1920x2160` pan `0,0`, backlight 1000. Staging,
socket and probe staging absent.

## Limits

- Audible output and owner listening confirmation are still missing; the
  module has no decoder/audio-routing path, and the stock volume was 0.
- No paired amp shutdown/standby, no in-module register readback, no subwoofer
  behavior beyond disabled/no-op.
- The real `mod_plat` platform sequence is required for the STBC global fd; the
  module does not open video24 or GPIO itself.
- The module is in the AV stage inventory, not the cold core bundle.
- `amp-mute` keeps software state only; the STBC getter's raw bytes are not a
  proved level readback.

## Evidence

`common/audio_amp.c` (baseline), `common/amp_vendor.c`,
`modules/audio_amp/audio_amp.c`, `include/openaurora/services.h`
(`audio.amp`), `core/control.c` (`amp-state`/`amp-init`/`amp-mute`),
`tests/audio_amp.c`, `tests/amp_vendor.c`, `tests/audio_amp_ipc.cjs`,
`openaurora/Makefile`; `artifacts/analysis/amp-module-2026-11-07/`.
