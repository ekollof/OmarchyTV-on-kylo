# EPG now/next control query — 2026-11-05

Bounded control-plane exposure of the recovered `NowNextCache::getNowNext`
semantics over the existing `epg` v1 query interface.

## Behavior

`epg-nownext <service> <now_ms>`:

- validates a nonzero service, nonnegative `now_ms` and the uint32-second
  domain;
- queries the EPG cache over `0 .. UINT32_MAX` seconds with a 512-event cap;
- **now** is the earliest event with `start <= now < end`;
- **next** is the earliest event with `start > now`;
- prints `now id=… name='…' next id=… name='…'`, with `-` for a missing side.

Boundary behavior follows the cache: exact `now == start` is current, exact
`now == end` is not; an empty/cleared cache reports `now - next -`.

## Verification

```
ok epg IPC: EIT ingest, key replacement, query/clear and reload over the control socket
ok EPG persistence IPC: reload/restart, clear, malformed fail-closed
```

`tests/epg_ipc.cjs` checks 12:30 (now 100/next 101), exact 12:00, 13:15
(now 101/next -), 11:00 (now -/next 100), exact 13:30 end (none), invalid
service/args, clear (none) and stop (`failed errno 19`). The ARM core also
links with the new command.

## Limits

- Project control query over the existing `epg` v1 service; not a new vendor
  ABI or vendor cache execution.
- Window/cap follows the 512-event project cache; full schedule/CRID/
  first-showing, persistence interaction tests and live tuner/EIT remain open.
- No TV, device or cold-start access.
