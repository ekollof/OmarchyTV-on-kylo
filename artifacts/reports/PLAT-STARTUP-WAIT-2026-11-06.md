# Cold-start precondition: vendor kernel-driver wait — 2026-11-06

Firmware-derived startup ordering piece for independent startup. Implements the
wait the vendor performs between kernel OSD registration and `PLAT_DRV_INI_Init`.
Offline host plus ASan/UBSan tests pass; a view-only TV query confirms the live
marker/OSD state. This is not a cold boot and not display/scanout acceptance.

## Firmware evidence

`aurora.elf` (vendor path `/vendor/com.nvt.idtv/aurora.elf`, read offline from
the dump; ELF MD5 unchanged `64c252340d3af6c5d66072611b7bad7a`):

- `ICE_HardwareInit` @ `0x007551a0`; the wait loop is at `0x0075548a..0x007554ae`:
  `PLAT_OS_GetTimeMS`, then `movs r1,#4` (`R_OK`), `access(path)`, and on failure
  `usleep(50000)` back to the same `access`. There is **no vendor timeout** in
  this loop.
- The literal at `0x00755540` is `0x012930c5`, the string
  `/tmp/ntkdriver_completed` (rodata `0x012930c5`).
- On success `0x00755634..0x00755648` logs the elapsed delta with
  `"[BOOT] wait for ntkdriver ready Time = %d"` (rodata `0x012930df`).
- Only then `0x0075564c` calls `PLAT_DRV_INI_Init` @ `0x0089b6e4`, i.e. the
  vendor runs the audio Protect/Mixer/Proxy prelude and the rest of the HAL
  bring-up **after** the driver marker is readable.

This is the ordering `mod_plat` was missing: it called the audio prelude
immediately on start, so a cold/early start could touch the HAL before the
kernel OSD/driver initialization had completed.

## Implementation

`modules/plat/plat.c` now has `wait_driver_marker()`, called before the audio
prelude and before any HAL stage:

- polls `access(marker, R_OK)` every 50 ms, matching the vendor;
- default marker `/tmp/ntkdriver_completed`;
- `plat.driver_wait`: `1`/`0` forces the wait; unset auto-detects the set via
  `/dev/ntkled` (same host-safety pattern as `mod_power`);
- `plat.driver_timeout_ms` default `60000`, `0` = wait forever (vendor-faithful).
  On timeout it sets `STAGE_DRIVER`, logs a refusal and returns `-1`, so no HAL
  stage runs and a retry is possible after the marker appears;
- logs `plat: waiting for ...` and `plat: driver marker ready after N ms`.

`tools/boot/openaurora-cold.conf` now opts in explicitly:

```text
plat.driver_wait = 1
plat.driver_marker = /tmp/ntkdriver_completed
plat.driver_timeout_ms = 60000
```

The wait is synchronous inside `start`, exactly where the vendor blocks. The
supervisor's 75 s startup deadline still bounds the 60 s default. No HAL/device
call is added.

## Verification

New `check-plat-startup-wait` drives the real host `mod_plat` with a temporary
marker:

```text
ok plat startup wait: pre-existing 1ms, arrived after 151ms, timeout refused at 401ms then retried
```

It asserts: pre-existing marker starts immediately; a marker created 150 ms into
start makes the module wait and then succeed; an absent marker refuses after the
configured 400 ms (`not readable ... refusing HAL bring-up`) and a retry after
the marker appears succeeds. Log lines for the wait and elapsed time are checked.

The same test and `check-plat-power-sync-stop` under a focused ASan+UBSan build
(`detect_leaks=1`, `halt_on_error=1`) pass:

```text
ok plat startup wait: pre-existing 1ms, arrived after 201ms, timeout refused at 401ms then retried
ok plat/power sync stop: plat reload 1ms stop 0ms, power reload 0ms stop 1ms, services unpublished
```

Full suite ran visibly and ended `all tests passed` (rc 0); QEMU sub-launcher
value/live/launcher checks pass.

## View-only TV state — 2026-11-06

No device was opened for write, no module was staged and no key/display action
was taken. `tools/tvssh.sh` read only:

```text
marker=readable
marker=1
marker-size=2
ntkled=present
uptime=61692.36
/proc/fb: 0 OSD_NORMAL1, 1 OSD_NORMAL2, 2 OSD_NORMAL3, 3 OSD_CURSOR1, 4 OSD_CURSOR2
/dev/fb1: crw-rw-rw- 29,1
fb1 name OSD_NORMAL2, virtual_size 1920,2160, stride 7680
stock: run_aurora.sh 1711, aurora.elf 1781, zombie 2204
```

This confirms the marker contract and OSD registration on the current boot, but
the TV had been up for ~17 h: it does **not** measure a cold marker delay, prove
early-boot ordering on hardware, or establish scanout/display ownership.

## Limits

- No reboot, cold boot, HAL call or display write was performed; the wait is
  proven offline with a temp marker only.
- The vendor wait has no timeout; ours is configurable for safety, so behavior
  differs only in the never-appearing-marker failure case.
- The marker's mtime on the live set predates the current clock (system time was
  set later); it is a boot-time flag, not a timestamp measurement.
- Cold startup, MCU/watchdog authority, clean handback, first-frame scanout and
  full display/HAL acceptance remain open.
