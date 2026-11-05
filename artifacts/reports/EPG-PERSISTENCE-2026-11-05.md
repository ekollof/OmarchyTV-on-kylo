# Persistent EPG schedule cache — 2026-11-05

Bounded offline extension of `mod_epg`: opt-in persistence of the EIT-backed
schedule cache across module reload and process restart. It is a project file
store, not the vendor EIT hub or SQLite metadata.

## Design

- `epg.path` config enables persistence. When absent/empty the module remains
  in-memory and clears on start exactly as before.
- On start the module loads `OAEPG1` and refuses to publish on a missing
  header, malformed record, directory path or read error. A missing file
  starts empty.
- `epg-ingest` saves the whole cache atomically (`path.tmp`, fsync, rename)
  after storing events; `epg-clear` and `epg-stop` save too. A failed save is
  reported to the caller but the in-memory cache remains authoritative.
- Records are tab-separated service/event/begin/end/CRID/running/first/state/
  version/actual/title; titles are bounded and control characters/embedded
  tabs are sanitized to `?` on save. Capacity remains 512.
- `oa_eit_cache_get_at` is a new project enumeration accessor (sorted cache
  order) used by the store; it is explicitly not a recovered vendor API.

## Verification

```
make -j24 check-eit-cache check-epg-ipc check-epg-persist-ipc
ok eit cache: keyed schedule events, duration query and now/next
ok epg IPC: EIT ingest, key replacement, query/clear and reload over the control socket
ok EPG persistence IPC: reload/restart, clear, malformed fail-closed

qemu-arm-static .../eit_cache
ok eit cache: keyed schedule events, duration query and now/next
```

The socket test ingests two EIT events, reloads `epg` and queries them, starts
a fresh process against the same file and sees both, clears and persists an
empty cache, then corrupts the file and verifies `start epg` refuses
publication and `epg-count` is `failed errno 19`. The file ends as
`OAEPG1\n`; no temporary file remains.

## Limits

- Project text store, not vendor EIT/schedule-cache persistence; no SQLite,
  EIT filtering, series/CRID resolution, first-showing policy, retention or
  incremental merge semantics.
- Saves on every ingested section; no debounce/transaction log.
- Cache capacity remains 512; vendor limits and eviction policy are not
  reproduced.
- No TV access, no tuner/EIT hardware, no display or cold-start acceptance.
