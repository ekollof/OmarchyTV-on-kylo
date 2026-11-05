# Broadcast CLI/socket coverage — 2026-11-05

Host-only control-plane coverage for the project-internal tuner/epg/pvr v1
contracts. No TV, tuner, EIT/EPG or PVR hardware.

## Control commands

`core/control.c` now exposes the fixture-backed services over the real Unix
control socket:

```
tuner-state
tuner-scan <network>       1=T 2=C 3=S 4=analog 5=IP
tuner-cancel
tuner-select <handle>
epg-query <service> <from_ms> <to_ms>
pvr-list
pvr-add <service> <from_ms> <to_ms>
pvr-cancel <id>
```

Argument validation happens before any service lookup: `tuner-scan` accepts
only 1..5, handles/IDs must fit `uint32_t`, EPG/PVR windows must be ordered
`from < to`, and missing/extra arguments return `failed errno 22`. A service
that is not published returns `failed errno 19` (`ENODEV`).

> Later note: [BROADCAST-VENDOR-FIELDS](BROADCAST-VENDOR-FIELDS-2026-11-05.md)
> extends `tuner-state`, `epg-query` and `pvr-list` with lock/signal, event
> metadata and repeat/padding fields, so the exact reply strings in this
> report describe the earlier output.`epg-query` and `pvr-list` emit a `count N` line followed by one record per
line, capped to the reply buffer and the 16-entry caller arrays. Service
handles are released after every call.

## IPC test

`tests/broadcast_ipc.cjs` starts the real `openaurora` binary with
`--modules build/tests`, loads `broadcast_fixture`, and drives every command
over the Unix socket:

- tuner: idle state, invalid networks, scan progress 25/50/75/100, completion
  on the follow-up poll, select bounds, rescan clearing selection, cancel and
  repeated-cancel rejection;
- EPG: per-service and all-service counts, unknown service, inverted/equal
  windows;
- PVR: empty list, two adds, ID assignment, invalid add, listed rows, cancel,
  repeat cancel (`EALREADY` 114) and unknown ID (`ENOENT` 2);
- reload: tuner idle and PVR empty again;
- stop: all three services return `ENODEV`; unload succeeds;
- shutdown: exit code 0 and socket removed.

Because the control connection stays open, multi-line replies are read until a
short quiet period rather than truncating at the first newline.

## Verification

```
ok broadcast fixture: tuner scan state machine, EPG query bounds and PVR reservations
ok broadcast IPC: tuner scan/select, EPG query and PVR add/list/cancel over the control socket
```

Full host `make check`: **413 ok**. `verify.cjs`: **45 groups**.

Evidence:

- `artifacts/analysis/broadcast-ipc-2026-11-05/check.txt`
- `full-host-summary.txt`, `identities.txt`
- `openaurora/tests/broadcast_ipc.cjs`, `openaurora/core/control.c`

## Limits

This proves the control-plane plumbing, not broadcast parity:

- still no real tuner/transponder scan, signal/lock/quality, hardware ownership
  or vendor tuner ABI; **partly superseded:** the static vendor tuner
  interface is recovered in [VENDOR-BROADCAST-ABI](VENDOR-BROADCAST-ABI-2026-11-05.md),
  but no backend uses it;
- no EIT/EITp filters, schedules, vendor EPG semantics or persistence;
  **partly superseded:** EIT time/query semantics are recovered in
  [VENDOR-BROADCAST-ABI](VENDOR-BROADCAST-ABI-2026-11-05.md) and the codec is
  implemented; filters, schedules and persistence remain absent;
- no recording engine, timeshift, storage, conflict handling or playback;
- the fixture is the only backend; no vendor-grounded tuner/EIT/PVR backend;
- no UI integration and no TV access.

Runtime EASI remains blocked: testtool TCP 1986 is disabled and enabling it
still needs an agreed watchdog-safe/recovery-ready procedure. The channel UI
remains explicitly provisional.
