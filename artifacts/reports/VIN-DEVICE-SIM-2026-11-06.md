# Real VIN HAL against a fake device node — 2026-11-06

TV was off limits. This runs the shipped ARM `libNDAL3.so` `Hal_Vin_*`
wrappers and the real ARM `mod_source.so` under `qemu-arm-static` against a
preload fake VIN device. No TV, `/dev/ntkdvif`, kernel driver or HDMI signal
was touched.

## What the shipped wrappers do

Disassembled from `vendor/lib/libNDAL3.so` (MD5 of the full ELF not needed;
the wrapper addresses are below):

| function | behavior |
|---|---|
| `Hal_Vin_Open(idx)` 0x4e7c97e8 | builds a `/dev/videoN` path (observed `/dev/video20` for idx 0), `open64(path, O_RDWR|O_NONBLOCK)`, `fcntl(fd, F_SETFD, FD_CLOEXEC)`; returns the fd or -1; logs `strerror` on failure |
| `Hal_Vin_SelectInput(fd,id)` 0x4e7c993c | `ioctl(fd, 0xc0045627, &id)`; returns 0 or -1 and `perror` |
| `Hal_Vin_SetStreamStart(fd)` 0x4e7c9d58 | `ioctl(fd, 0x40045612, &1)`; returns 0 or -1 and `perror` |
| `Hal_Vin_Close(fd)` 0x4e7c98dc | `close(fd)`; returns 0 or -1 |

The earlier `/dev/ntkdvif` wording was imprecise: `Hal_Vin_Open(0)` targets
the VIN V4L2 node `/dev/video20`. `/dev/ntkdvif` is a separate global device
string in the library, not the path this call opened in the executed probe.

The library also imports `gs32Ndp_PQ_fd` (defined in `libNDAL_EX.so`) and
ALSA/V4L2/nteec/mhash symbols. The real module links those vendor libraries,
so the test stages them; the base `libstdc++`/`libgcc_s`/`libc` come from the
TV rootfs.

## Fake device

`openaurora/tests/fake_vin_device.c` is an ARM `LD_PRELOAD` shim:

- intercepts `open64`/`open` for paths containing `ntkdvif` or `/dev/video`,
  returns a `/dev/null` fd and logs `open path=... fd=...`;
- intercepts `close` for that fd and logs it;
- intercepts `ioctl 0xc0045627` and reads the int id, logging
  `select fd=... id=...`;
- intercepts `ioctl 0x40045612` and reads the int flag, logging
  `start fd=... on=...`;
- returns success for those two requests; unknown requests return `ENOTTY`.

This is a device-boundary mock, not the kernel driver. It exercises the real
vendor wrapper machine code and the real module's source-type/id table, but
does not register a V4L2 device, start a stream, or route HDMI.

## Test

`openaurora/tools/qemu-vin-real-check.sh` builds `build-tv-v3/openaurora`,
the real `build-tv-v3/modules/mod_source.so` and the plat stub, stages the
vendor libraries and the fake device, then starts qemu with
`LD_PRELOAD=fake_vin_device.so`. It drives the real control socket:

```text
load/start plat, source
source-state -> source type -1 available 1
source-select 17 -> ok; state 17
source-select 19 -> ok; state 19
reload source -> failed (fast)
source-state -> failed errno 19
wait 700 ms
reload source -> ok; state -1/available
source-select 17 -> ok
stop source -> failed; source-state -> failed errno 19
wait 700 ms
stop source -> ok; unload source -> ok; quit -> bye
```

VIN log from the run:

```text
open path=/dev/video20 fd=7
select fd=7 id=3
start fd=7 on=1
select fd=7 id=66
start fd=7 on=1
close fd=7
open path=/dev/video20 fd=7
select fd=7 id=3
start fd=7 on=1
close fd=7
```

`id=3` is HDMI1; `id=66` is HDMI3. The worker close precedes the reopened
device, and the stop gate closes the second fd. Host log:

```text
loaded source ... /tmp/oa-qemu-vin-real/modules/mod_source.so
source: VIN open (fd 7), switching enabled
started source
source: cleanup pending; retry stop/unload
stopped source
...
source: reloaded (started)
source: cleanup pending; retry stop/unload
stopped source
```

Output:

```text
ok qemu real VIN: libNDAL3 Hal_Vin /dev/video20 open, select id3/id66,
stream start, async reload/stop gate
```

The pure fixture test still covers the decoded 26-entry ids and the
`source.enable=0` refusal; this test adds the real vendor library path and the
real module's async gate.

## Reproduction

```sh
make -C openaurora check-vin-real
# or
openaurora/tools/qemu-vin-real-check.sh
```

The script requires the ARM toolchain, qemu-arm-static, the TV rootfs and the
shipped vendor libraries. A missing input is an error, not a skip.

## Limits

- The fake device returns success unconditionally for the two ioctls; it does
  not prove the real driver accepts the ids/start or that V4L2/HDMI routing
  works.
- `Hal_Vin_Open` targeted `/dev/video20` in this probe. The `/dev/ntkdvif`
  global device and other VIN indices/slots are not exercised.
- Only the known select/stream-start ioctls are implemented. Format, status,
  lock, color-system and V4L2 registration calls are not simulated.
- The real kernel driver, HDMI signal, audio routing and hardware timing
  remain a separate, unauthorized TV gate.
- The staged vendor libraries are test inputs; the no-redistribution rule
  still applies.
