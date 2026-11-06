# SPI / MTD / NIM-DTV cold-start stages — 2026-11-06

Offline recovery of three `DEV_INI_Init` stages, plus an opt-in SPI transport
and module with fake-device tests. No TV, device node or HAL call was made.

## Firmware evidence

### DEV_MTD_Init @ `0x0085419c`

The whole function registers one debug menu (`PLAT_SP_DebugRegisterMenu`) and
returns 0; it performs no MTD open, scan or flash operation. The MTD/flash work
lives in separate `DEV_MTD_*` functions called by upgrade paths, not by
`DEV_INI_Init`. **No MTD owner module is needed for cold startup.**

### DEV_SPI_Init / Open / Read / Close

- `DEV_SPI_Init` @ `0x0085946c`: creates the SPI semaphore and registers a debug
  handler; no device I/O.
- `DEV_SPI_Open(0)` @ `0x00858d0c`: prefers `/conf/spi.bin` (`fopen("rb+")`,
  falls back to `"rb"`), measures it with `fseek/ftell`, and keeps the `FILE*`.
  If the file is absent (stat `ENOENT`) it opens `/dev/ntkspi` `O_RDWR` and
  keeps the fd.
- `DEV_SPI_Read` @ `0x008596bc`: file backend does `fseek(offset)` +
  `fread(size)`; device backend calls
  `ioctl(0x8018531e)` with a 24-byte `{offset,size,buf,0,0,0}` request.
  Returns 1 only on a full read, 0 otherwise.
- `DEV_SPI_Close` @ `0x00858fec`: `fclose` or `close(fd)`.
- Other SPI write/control requests (`0x531f/0x5320/0x5321`) are present but
  their semantics are not needed for the read-only boundary here.

### DEV_NIM_DTV_Init @ `0x00861678`

Initializes a mutex and creates a DTV task (`PLAT_OS_CreateTask`) plus model
branches/debug writes. This is tuner/device ownership, not a file transport;
it remains unimplemented and needs the real NIM/tuner path.

## Implementation

- `include/openaurora/spi_dev.h` + `common/spi_dev.c`: recovered transport with
  the vendor backend order, bounded file reads, the 24-byte ARM read request,
  and idempotent close.
- `modules/spi/spi.c`: opt-in `spi` v1 service (`spi.enable=0` default,
  `spi.file`, `spi.device`) publishing the read-only `size/read` boundary.
  Stop unpublishes and closes synchronously; it owns no worker or deferable
  heap/device state. It is **not** autoloaded by the current cold config; a
  future cold boot must set `spi.enable=1` to match `DEV_SPI_Open`.
- MTD needs no module; NIM-DTV remains open.

## Verification

```text
ok spi dev: recovered file/device backends, 24-byte read request, bounds/failure paths
ok spi module: opt-in gate, file backend, bounded read, stop unpublishes/closes
```

The device test wraps `open/close/ioctl`: no `/dev/ntkspi` is opened, and the
exact request number, offset/size/pointer and zero reserved words are checked.
Out-of-range, zero-length, NULL-buffer and ioctl-failure paths are covered.

Both tests pass under focused ASan+UBSan (`detect_leaks=1`, `halt_on_error=1`).
Full suite ran visibly and ended `all tests passed`; QEMU sub-launcher
value/live/launcher checks pass. ARM `mod_spi.so` is ELF32 ARM and links only
`libc.so.6`.

## Limits

- No real `/dev/ntkspi`, `/conf/spi.bin`, SPI transfer, NIM-DTV task or tuner
  hardware was exercised.
- The read-only module has no consumer yet; SPI write/control requests and
  device-specific framing are unrecovered.
- NIM-DTV/MTD upgrade paths, cold boot, MCU/watchdog authority, scanout and
  real audio/CEC/tuner/PVR/VIN remain unaccepted.
- A stable controlled-ownership scanout test still needs explicit owner
  authorization to stop stock Aurora plus independent observation.
