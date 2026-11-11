# QEMU TV device simulation — 2026-11-10

Status: current offline simulation milestone. The real ARM display module and
the real ARM platform/panel modules now run under `qemu-arm-static` against
model device boundaries. No TV, kernel driver, framebuffer, GPIO, PWM, I2C or
panel was touched.

Last verified: 2026-11-10 (host build + qemu-arm-static; no hardware effect).
Evidence class: qemu/host simulation; not on-TV acceptance.

## What already existed

- `tests/fake_fb_preload.c` — host LD_PRELOAD fb1 model with var/fix/mmap,
  region/MAU ioctls, pan and snapshot state. It was only used by the host
  `fake_fb_socket` test.
- `tools/display/sim-display.c` — replacement display provider used by the
  qemu UI layout oracle; it captures RGBA frames but does not exercise the
  real fbdev module.
- `tests/fake_vin_device.c` — ARM LD_PRELOAD model for `/dev/video*` used by
  the qemu real-VIN check.
- `tools/qemu-vin-real-check.sh` — real ARM `mod_source` against the fake VIN
  device; now also supplies the post-DRM id/plane boundary and accepts the
  `VIDIOC_G_EXT_CTRLS` request used by the DP-source path. It passes types 17
  and 19 with open/select/start and reload/stop ordering.

## New real-fbdev check

`tools/qemu-fb-real-check.sh` builds the ARM `mod_display.so`, compiles
`fake_fb_preload.c` for ARM, and runs the real module under qemu with
`LD_PRELOAD`. The model provides:

- `/dev/fb1` open/access, `FBIOGET_VSCREENINFO`/`FBIOGET_FSCREENINFO`;
- 1920×1080, 2×2160 virtual frames, 32 bpp, stride 7680;
- `mmap` of the 16.6 MB layer, region ioctls `0x8020c86c`/`0x4020c86b`,
  MAU read `0x8004c8bd`, and `FBIOPAN_DISPLAY` (0x4606).

The test starts the real `mod_display` with `display.test=2` (eight full-screen
colour bars), verifies `display-info` reports `1920x1080 32bpp 2 frames`, and
checks that the model saw changed pixels, region and pan. It then stops the
module and checks that the full snapshot restore returned pixels, region,
pan and MAU to their initial values.

Result: passes on the host. This exercises the real fbdev/snapshot/region/pan
path, not just the replacement `sim_display` service.

## New real platform/panel slice

`tools/qemu-panel-real-check.sh` builds the real ARM `mod_plat.so` and
`mod_panel.so`, compiles `tests/fake_tv_io.c` for ARM, and runs them under qemu
with the TV device boundary mocked. The boundary is deliberately permissive and
logged; unknown TV ioctls return success.

Recovered calls observed from the shipped NDAL HAL:

```text
open /dev/nvt/ntkvplt
open /dev/ntgpio
open /dev/i2c-0 .. /dev/i2c-7
open /dev/nvt/ntkiic0
open /dev/nvt/ntkled
ioctl 0x403c4c4c   PWM open (channel 0, 30000 Hz, duty 1000)
ioctl 0xc03c4c53   PWM set  (channel 0, 30000 Hz, duty 1000)
ioctl 0x670a / 0x670e / 0x670c   GPIO write
```

The test starts `plat` and `panel` with `panel.power_on_start=1` and
`panel.start=100`; it verifies `panel-state` is `panel percent 100` and that the
log contains the PWM open/set and the GPIO 110=1 / 111=0 writes. The real
platform audio prelude runs too: `Hal_Audio_Mixer_Ctrl_Init` returned -1 on the
mock boundary, and the vendor-continue path was exercised as documented in
COLD-START-SEQUENCE-2026-11-10.

This is a call-path and ordering check, not a PWM/GPIO/panel hardware effect.

## VIN/DP-source check

The existing `qemu-vin-real-check.sh` now passes. The fixture supplies the
post-DRM boundary values (`Hal_Video_Get_DrmDeviceId` = 13,
`Hal_Video_VideoPath_to_PlaneType` = 0, VPLTIO success) and accepts
`VIDIOC_G_EXT_CTRLS` id `0xa00968`. The real `mod_source` then executes:

```text
type 17: open /dev/video28 -> select id 3
         open /dev/video21 -> select id 0 -> start -> ext_ctrls
type 19: open /dev/video66 -> select id 66 -> start -> ext_ctrls
reload:  close before reopening /dev/video28 + /dev/video21
```

This proves the real ARM VIN open/select/start/ext-controls and async
reload/stop path against the modeled boundary. It does not model V4L2 formats,
HDMI PHY, DP-source register effects or picture.

## Limits and next slices

- The TV io model is **permissive**: it returns success without maintaining
  GPIO/PWM/I2C/SPI state. It cannot prove a register value, bus transaction,
  panel timing or scanout.
- `/dev/i2c-*` reads/writes, `/dev/ntkspi`, `/dev/ntkcec`, `/dev/ntkdvif`,
  `/dev/dri/card*`, ALSA and the tuner/frontend are opened or stubbed but not
  modeled statefully.
- The fb model is fixed to the recovered fb1 geometry and does not model the
  other four OSD layers or driver SetPar.
- The qemu VIN check now passes with the DRM/DP-source boundary; it still does
  not model V4L2 formats or HDMI/DP register effects. The full host suite now
  passes (`make check`, rc=0) after the settle-repaint fixtures were isolated
  from the real-time `pump()` helper and the registry capacity test was updated
  to the current 128-entry table.
- Every result here is qemu/host simulation. It is not a TV cold boot, panel
  acceptance, scanout proof or device-ownership claim.

Next useful slices, in firmware order: stateful GPIO/PWM assertions (pin,
level, channel, duty), then IIC/SPI read/write models, then VIN/V4L2 and DRM
only where the recovered ioctl contract is known. Keep the permissive model
separate from a claimed device contract.

## Reproduce

```sh
# Real ARM display over the modeled fb1:
openaurora/tools/qemu-fb-real-check.sh

# Real ARM plat+panel over the permissive TV io boundary:
openaurora/tools/qemu-panel-real-check.sh
```

Both require the local ARM toolchain, extracted TV rootfs/vendor libraries and
`qemu-arm-static`; they do not use the TV or host device nodes.
