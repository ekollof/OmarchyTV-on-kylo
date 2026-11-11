# dev_src_init auto-color-system — 2026-11-11

Status: current
Last verified: 2026-11-11
Evidence class: firmware instructions, host tests, on-TV traced ioctl.
Supersedes: the "auto-color-system call is still missing" note in
COLD-START-SEQUENCE-2026-11-10 and TODO.

## Firmware contract

`dev_src_init` (`0x008fc654`) calls
`plat_vin_slot_set_auto_color_system(2)` after creating the
`DEV_SRC_HDMIHotPlug` task. `plat_vin_slot_set_auto_color_system`
(`0x0092ec80`):

1. `Hal_Vin_Open(0)`; on failure log `Cannot close fd_v4l2 = %d` and return
   `-1`.
2. For argument 2, read the two words at table `0x013b9f48 + 2*8`:
   `{0x00ff40ff, 0}`. (Index 0 = `{0x0000b000,0}`, 1 = `{0x0000b700,0}`,
   3 = `{0x00fff0ff,0}`; the `>=4` default is also `{0x00ff40ff,0}`.)
3. Call `Hal_Vin_SetColorSystem(fd, <ignored>, 0x00ff40ff, 0)`.
4. `Hal_Vin_Close(fd)`.

`Hal_Vin_SetColorSystem` (`libNDAL3.so` `4e7c99a0`) sends ioctl
`0x40085618` with the 8-byte `{v0, v1}` payload. The second argument is not
used by the HAL. The vendor ignores the ioctl return after the call.

## Implementation

- `openaurora/include/openaurora/source_hal.h` — new
  `vin_set_color_system(fd, v0, v1)` HAL seam.
- `openaurora/common/source_hal_vendor.c` — wrapper over
  `Hal_Vin_SetColorSystem(fd, 0, v0, v1)`; host-stub and ARM paths updated.
- `openaurora/modules/source/source.c` — `apply_auto_color_system()` runs in
  `m_start`, after the hotplug worker is started, only when
  `source.enable=1` and the new default-off `source.auto_color_system=1`.
  It opens VIN(0), applies `{0x00ff40ff,0}`, closes the fd and logs the
  result. The source test's default start still opens no VIN fd.
- `openaurora/tests/source_fake.c`/`.h` — `SF_COLOR` event and handler.
- `openaurora/tests/source_path.c` — explicit positive case:
  `SF_OPEN(0) -> SF_COLOR(0x00ff40ff,0) -> SF_CLOSE`.

## Verification

Host:

```text
ok source path: type gates, VIN opens/VPLT, V4L2Info/FdCommon, DRM/NDAL and audio order
ok source fake-VIN IPC: reload fails fast, worker closes fd before VIN reopen, stop uses the gate, source.enable holds
```

The new source path case asserts the open/color/close order and payload; the
existing default-start case still asserts no VIN open.

ARM `mod_source.so` sha256
`f81abb98f534f457c3c0a8fae1f8625f9c9cc7a0791f6b2e091b6c545633cc12`.

On-TV controlled run (stock stopped, `oa-wdtfeed`, `plat.drm_init=0`,
`source.enable=1`, `source.auto_color_system=1`,
`source.hotplug_pulse=0`) with the pass-through trace preload:

```text
source: auto color system 2 -> 0x00ff40ff (0)
vin_color cmd=0x40085618 v0=00ff40ff v1=00000000
```

`plat-state` was `plat ready 1 errors 0x0`; `source-state` was
`source type -1 available 1`. The ioctl reached the kernel with the exact
recovered table payload, and the HAL returned success.

## Restore

`quit` stopped source/plat, the feeder stop file was written, staging was
removed, and a clean reboot restored stock:

```text
uptime 0 min
appmgr 1566, run_aurora.sh 1736, aurora.elf 1770 (zombie 2224)
no openaurora / oa-wdtfeed
```

## Limits

- This proves the VIN(0) auto-color ioctl only; it does not prove picture,
  color decoding, source selection or panel output.
- The effect is behind `source.auto_color_system=0` by default; the vendor
  calls it unconditionally once source is active. Enable it with the source
  acceptance gate.
- One run; no repeatability matrix.
- No boot hook, firmware or `/conf` change. `source.enable` remains 0 in the
  shipped profile.
