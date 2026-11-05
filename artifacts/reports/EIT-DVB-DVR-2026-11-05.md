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
  read complete sections;
- `oa_nim_ndal_enable_eit(adapter, index, pid, ops, ctx)` attaches the source
  to `nim-ndal`; it opens lazily on the first `next_eit()` and closes on
  `nim->stop`;
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

## Limits

- Mock contract tests only; no frontend lock, demux section delivery or DVR
  write was exercised on hardware.
- The section filter is PID-only (no mask/table filtering); software EIT table
  validation remains in `eit_section.c`.
- DVB-T tune only in the NDAL adapter; DVB-C/S/DTMB/ISDBT return `ENOTSUP`.
- The DVR sink is not yet driven by a recording scheduler or tuner/PMT stream;
  the PVR module still schedules the offline store.
- `libNDAL3.so`/`libNDAL_EX.so` symbol presence and ABI are assumed from the
  rootfs; error returns from the shipped wrappers are treated as 0/-1.
- Hardware acceptance, cold start and display ownership remain owner-gated.
