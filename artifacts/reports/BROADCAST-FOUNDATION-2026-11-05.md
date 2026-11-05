# Broadcast service foundations — 2026-11-05

Host-only contracts and fixture. No TV, tuner, EIT/EPG or PVR hardware.

## Added contracts

`include/openaurora/broadcast.h` defines project-internal v1 interfaces:

- `tuner` — status, scan start/cancel, service selection;
- `epg` — bounded event query by service and time window;
- `pvr` — reservation list/add/cancel.

These are **not claimed to match a vendor object ABI**. They establish the
boundaries a future vendor-backed tuner/EIT/PVR module must satisfy. The only
implementation is the test fixture `tests/broadcast_fixture.c`; no production
module publishes these services yet.

## Fixture behavior

- tuner: starts idle; supports DVB-T/C/S/analog/IP request values; deterministic
  scan advances in 25% steps to 12 services; cancel moves to `CANCELED`;
  selection is `EAGAIN` until the scan is done and bounded to handles 1..12.
- EPG: four synthetic events across three services; overlap filtering by
  service and `[from,to)`; copies up to the caller cap while reporting the full
  match count; rejects inverted/NULL requests.
- PVR: bounded 16-job table; add validates service/time/name and assigns an ID;
  list copies and reports full count; cancel transitions only a scheduled job
  and rejects repeat/unknown IDs.

The fixture is loaded by a real `oa_host`, starts/stops/publishes the three
services, survives `reload` (state reset and services republished) and is
unloaded at the end.

## Verification

```
ok broadcast fixture: tuner scan state machine, EPG query bounds and PVR reservations
```

Full host `make check`: **412 ok**. `verify.cjs`: **45 groups**.

Evidence:

- `artifacts/analysis/broadcast-foundation-2026-11-05/check.txt`
- `full-host-summary.txt`, `identities.txt`
- `openaurora/tests/broadcast_fixture.c`, `broadcast_services.c`

## Limits

This is scaffolding, not tuner/EPG/PVR parity:

- no DVB-T/C/S/analog tuner, transponder scan, signal/lock/quality or hardware
  ownership;
- no EIT/EITp/filter/schedule persistence or vendor EPG semantics;
- no recording engine, timeshift, storage, conflicts, retention or playback;
- no production module, CLI/socket query, UI integration or persistence;
- no validation against vendor code because these are project contracts.

The testtool probe remains blocked by the disabled TCP 1986 server and the
unagreed watchdog/recovery procedure. The channel UI remains explicitly
provisional.
