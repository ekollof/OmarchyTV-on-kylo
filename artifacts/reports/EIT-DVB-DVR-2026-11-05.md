# DVB section-filter EIT source and DVR sink — 2026-11-05

Offline implementation with mock contract tests. No TV, frontend/demux/DVR
device or vendor function was executed. The owner's current decision remains
**offline only**; hardware acceptance is a separate gate.

## Grounding

- `libNDAL3.so` exposes `Hal_Demux_Dvb_Open`,
  `Hal_Demux_Dvb_Set_Sectionfilter`, `Hal_Demux_Dvb_Close`,
  `Hal_Demux_Dvb_Dvr_Open`, `Hal_Demux_Dvb_Dvr_Write`,
  `Hal_Demux_Dvb_Dvr_Flush` and `Hal_Demux_Dvb_Dvr_Close`. The strings and
  decompiled calls use `/dev/dvb/adapter%i/demux%i` and
  `/dev/dvb/adapter%i/dvr%i`; section filters use the PID/mask ioctl sequence
  and DVR uses file open/write/flush/close.
- The NDAL frontend adapter from NDAL-TUNER-BACKEND is the adjacent live-
  capable tuner path. This change adds the demux/EIT and DVR sides behind the
  same module boundaries.

## Implementation

### EIT section-filter source

`include/openaurora/eit_dvb.h` + `common/eit_dvb.c`:

- mockable `oa_eit_dvb_ops` (open_demux / set_section_filter / read_section /
  close_demux);
- default ops resolve the `Hal_Demux_Dvb_*` symbols from `libNDAL3.so`,
  open the configured adapter/demux, install a PID-only section filter and
  read one complete section. Close uses `Hal_Demux_Dvb_Close` (see the review
  correction; the first revision called POSIX `close` only);
- `oa_nim_ndal_enable_eit(adapter, index, pid, ops, ctx)` attaches the source
  to `nim-ndal`; it opens lazily on the first `next_eit()` and `nim->stop`
  closes it even when the frontend was never opened;
- `mod_tuner` config `tuner.eit_demux=adapter:index[:pid]` (pid is hex,
  default `0x12`) selects the live DVB source for `tuner.backend=ndal`.

### DVR sink

`include/openaurora/dvr.h` + `common/dvr_sink.c`:

- mockable `oa_dvr_ops` (open_dvr / write_dvr / flush_dvr / close_dvr);
- default ops resolve the `Hal_Demux_Dvb_Dvr_*` symbols, open the configured
  adapter/DVR in write mode, loop over partial writes and expose
  flush/close/is_open;
- the sink is a reusable PVR boundary. The scheduling/record engine that
  supplies the TS stream remains the existing offline store; wiring live
  recording start/stop is not claimed here.

## Verification

```
ok nim stub: stub/replay/NDAL/EIT-DVB mock backends
ok pvr stub: id/index semantics, persistence and DVR sink
ok tuner TS reload IPC: scan/reload/re-scan keep EIT replay and EPG state
full host make check: 424 ok
verify.cjs: 45 groups
ARM/qemu nim_stub: ok
ARM/qemu pvr_stub: ok
```

The EIT-DVB mock covers filter pid 0x12, section delivery/parse, no-data and
read/filter/open errors, close-on-failure and the integrated `nim-ndal
next_eit` path. The DVR mock covers open/write partial-loop/flush/close,
closed-before-open and write/open errors. The new Unix-socket test scans,
reloads `mod_tuner` and re-scans from the TS fixture, confirming the source is
reopened and EPG state remains consistent.

Evidence: `artifacts/analysis/eit-dvb-dvr-2026-11-05/`.

## Review correction

The first implementation resolved `Hal_Demux_Dvb_Close` and then ignored it:
`linux_close` called POSIX `close` only, so `DMX_STOP` never ran. `next`
returned success for any positive read. `ndal_stop` returned `-EINVAL` before
closing a filter that `next_eit` had opened while the frontend was still
closed. A HAL open of `-1` with errno 0 was mapped to 0, which the caller
treats as a live descriptor. Those claims in the implementation section above
are superseded by this section.

Read-only Ghidra (`analyzeHeadless -readOnly -noanalysis`, project
`artifacts/workbench/ghidra_ndal3` / `ndal3`, program `libNDAL3.so`, MD5
`c842bad804302ec822076c3d9d3ce31d`, `DumpFirmware.java --asm`) shows:

- `Hal_Demux_Dvb_Open` at `4e7b9bcc` uses `open64` flags `0x80802` and returns
  that fd, or `-1` when `asprintf` fails. The success epilogue moves the saved
  fd back into `r0` after `free`.
- `Hal_Demux_Dvb_Close` at `4e7b9c48` calls `ioctl(fd, 0x6f2a)` and then
  `close`. The `close` result stays in `r0`.
- `Hal_Demux_Dvb_Set_Sectionfilter` at `4e7ba12c` takes fd, pid, length clamped
  to 16, filter/mask/mode, and a flag byte at the third stack slot. Flag 0
  stores flags 4; any other value stores flags 5. The buffer-size ioctl
  `0x6f2d` / `0x8000` failure is printed and the filter ioctl `0x403c6f2b` still
  runs. The function returns 0, or `-1` when that filter ioctl returns `-1`.
  The existing call `(fd, pid, 0, NULL, NULL, NULL, 1)` matches this.
- `Hal_Demux_Dvb_Dvr_Open` at `4e7b9f44` does `clz(mode) >> 5`. Mode 0 is flags
  1 (`O_WRONLY`). The sink still passes 0.
- `Hal_Demux_Dvb_Dvr_Close` at `4e7b9fc4` is `close` only. `Dvr_Write` at
  `4e7ba000` returns `write`'s `r0`. `Dvr_Flush` at `4e7b9e48` uses ioctl
  `0xc0086f68` and returns 0 or `-1`.

`oa_eit_dvb_next` now accepts a positive read only when the 12-bit
section_length is 4..4093 and the byte count is `3 + section_length`, the same
bounds as `oa_eit_ts`. `ndal_stop` closes the lazy EIT source even when the
frontend is not open, and still returns `-EINVAL` when neither is open. A
negative HAL open or DVR write with errno 0 is `-EIO`.

Re-checked after the fix, with injected ops only (no `/dev/dvb`, no dlopen of
`libNDAL3.so` on the success path):

```
make -C openaurora check-nim-stub check-pvr-stub
  ok nim stub / ok pvr stub
BUILD=build-eit-san CFLAGS='-std=c11 -Wall -Wextra -Werror -pedantic
  -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g'
  LDFLAGS='-fsanitize=address,undefined'
  ok nim stub / ok pvr stub
TARGET=tv BUILD=build-tv-v3/eit-review, ARM glibc, bwrap + qemu
  ok nim stub / ok pvr stub
```

The earlier full host 424 / verify.cjs 45 run was not repeated.

## Limits

- Mock contract tests only; no frontend lock, demux section delivery or DVR
  write was exercised on hardware.
- The section filter is PID-only (no mask/table filtering); software EIT table
  validation remains in `eit_section.c`.
- DVB-T tune only in the NDAL adapter; DVB-C/S/DTMB/ISDBT return `ENOTSUP`.
- The DVR sink is not yet driven by a recording scheduler or tuner/PMT stream;
  the PVR module still schedules the offline store.
- The Ghidra listing fixes the wrapper return/ioctl contract. It is not a live
  demux or DVR acceptance.
- Hardware acceptance, cold start and display ownership remain owner-gated.
