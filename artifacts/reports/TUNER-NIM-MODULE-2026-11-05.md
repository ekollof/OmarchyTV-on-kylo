# Vendor-grounded tuner module and EIT handoff — 2026-11-05

> **Update 2026-11-05:** the synthetic-only EIT source now has a real MPEG-2
> TS replay path (PID filter, continuity, split/back-to-back sections) selected
> by `tuner.ts_path`; see [EIT-TS-DEMUX](EIT-TS-DEMUX-2026-11-05.md). A
> selectable `nim-ndal` frontend adapter with mock contract tests is in
> [NDAL-TUNER-BACKEND](NDAL-TUNER-BACKEND-2026-11-05.md); it is not hardware-
> accepted. The default NIM tune/signal side remains the stub described below.
Adds a `tuner` module shaped after the recovered `DEV_NIM_DTV_*` calls, with an
offline NIM stub and synthetic EIT output that is pushed into `mod_epg`. Host
and ARM/qemu only; no TV, tuner hardware or vendor execution.

## Firmware grounding

Recovered from `aurora.elf` (see
[VENDOR-BROADCAST-ABI](VENDOR-BROADCAST-ABI-2026-11-05.md)):

```
DEV_NIM_DTV_StartTune(uint path, uint *params, int *status)  @0x008619e1
DEV_NIM_DTV_MonitorTune(uint path, int *status)              @0x00862889
DEV_NIM_DTV_StopTune / Abort
DEV_NIM_DTV_GetSignalStrength / Quality / BER / SNR / UNC
```

- `status[0]` is `3` unlocked / `4` locked; frequency is `status[3]` in kHz.
- paths 1/2/3/5/7 are TERRESTRIAL/CABLE/QPSK/DTMB/ISDBT; 6/8 were not named.
- strength/quality are uint32, BER/SNR are floats defaulting to 0.0 and UNC is
  a uint32 with `0xffffffff` meaning unavailable.

The shipped functions live inside the vendor executable, not a linkable
library, so `include/openaurora/nim.h` defines a project boundary with the
same call shape (`oa_nim_ops`) plus a backend descriptor. Only
`common/nim_stub.c` is implemented.

## Stub backend

`oa_nim_stub_backend()` returns `nim-stub`:

- `start` validates path/params, records `params[1]` as frequency (default
  666000 kHz) and returns UNLOCKED;
- `monitor` stays unlocked for the first poll and reports LOCKED from the
  second; `stop` clears the tune;
- `signal` returns the vendor defaults (0/0, BER/SNR 0.0, UNC unknown) when
  untuned, rising strength/quality before lock and 80/75/0/20.5 after lock;
- `next_eit` emits 12 synthetic EIT sections (services 101..112, events
  1001..1012, MJD 60000 12:00, 30 minutes) via the shared section builder.

This is deterministic test data, not a tuner model. A future live backend must
replace both the NIM ops and the EIT source (demux/PID filters).

## Module

`modules/tuner/tuner.c` (`mod_tuner.so`):

- publishes the project `tuner` contract and keeps the vendor-like metrics in
  `oa_tuner_status`;
- maps network → NIM path: DVB-T/C/S → 1/2/3, IP → synthetic path 0, analog
  returns `ENOTSUP` because the ATV path was not recovered for this interface;
- scan poll advances 25% per `status()` call to 12 services, calling
  `MonitorTune`/signal getters each time;
- during the scan it looks up `epg.ingest`, drains `next_eit` and feeds each
  complete section synchronously, then releases the lookup. If EPG is absent
  the scan still completes; if EPG is stopped mid-call `ENODEV` is ignored;
- cancel calls `StopTune`, resets lock/UNC and marks `CANCELED`; select is
  valid only after `DONE` for handles 1..12 (project handle space);
- start/reload resets state and metrics; stop calls `StopTune` and unpublishes.

## Verification

```
ok nim stub: tune/monitor/signal defaults and synthetic EIT source
ok tuner IPC: NIM stub scan/signal, EIT handoff to epg, cancel/reload over the socket
```

The IPC test loads `epg` then `tuner`, scans DVB-T, checks every progress/lock/
signal transition, confirms `epg-count` becomes 12 and queries event 1001 with
its metadata, checks analog `ENOTSUP`, select/cancel bounds, a scan with EPG
stopped, reload, stop/unload and clean shutdown.

Full host `make check`: **419 ok**. `verify.cjs`: **45 groups**. `mod_tuner.so`,
`mod_epg.so` and the NIM test build for ARM; the test passes under
`qemu-arm-static`.

Evidence:

- `artifacts/analysis/tuner-nim-module-2026-11-05/check.txt`,
  `full-host-summary.txt`, `arm-build.txt`, `arm-run.txt`, `identities.txt`
- `openaurora/tests/nim_stub.c`, `tests/tuner_ipc.cjs`

## Limits

- Offline stub only: no tuner lock, transponder scan, signal measurement,
  DVB-T/C/S/analog hardware or NIM register access.
- Synthetic EIT only; no demux/PID section assembly, DVB frequency tables,
  service discovery or NIT/SDT parsing. Service ids/event times are fixed.
- The `tuner` query contract (handles, progress state machine) remains a
  project abstraction; the vendor NIM interface has no handle/scan API.
- No vendor code was executed. A live backend would need a recovery-ready
  integration decision because the DEV_NIM symbols are internal to the stock
  executable and the TV watchdog/stock owner remains in place.
- No TV access; testtool TCP 1986 remains disabled pending the owner's
  watchdog-safe/recovery decision.
