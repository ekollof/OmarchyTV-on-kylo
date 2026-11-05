# EIT section parser and EPG module — 2026-11-05

> **Update 2026-11-05:** complete EIT sections can now be produced by the real
> TS demux/replay backend; see [EIT-TS-DEMUX](EIT-TS-DEMUX-2026-11-05.md). The
> parser/cache and live-source limits below are otherwise unchanged.
Adds a DVB EIT section parser feeding `common/eit_cache.c`, publishes it as a
real `epg` module, and covers ingest/query/reload over the Unix control socket.
Host and ARM/qemu only; no TV, tuner or vendor execution.

## Parser

`include/openaurora/eit_section.h`, `common/eit_section.c`:

- accepts table ids `0x4e..0x6f` (present/following actual/other and schedule
  actual/other); `actual` is derived from the table id (`0x4e` or `0x50..0x5f`);
- validates the section-syntax indicator, section length bounds and the
  trailing MPEG-2 CRC32 (whole section CRC must be zero);
- reads `service_id`, section version and current/next; non-current sections
  are valid but deliver no events;
- iterates the event loop with 12-byte event headers, bounds-checks the
  descriptor loop, decodes start/duration through `common/eit_time.c` and
  skips events whose MJD/BCD time fails the firmware validity rules;
- extracts the short-event descriptor (`0x4d`) name/text with 96/160-byte
  bounds and NUL termination; other descriptors are skipped;
- reports running status, free-CA mode, actual flag and version per event.

CRC and structural failures return `EINVAL`; a callback error aborts the parse
and is returned unchanged.

## Module

`modules/epg/epg.c` (built as `mod_epg.so`):

- owns a 512-event `oa_eit_cache` and clears it on start/reload;
- publishes `epg` (query) and `epg.ingest` (`feed_section`, `clear`, `count`);
- `epg-query` converts contract milliseconds to the vendor second domain with
  floor-start/ceil-end, queries `[start, start+duration)` and copies the
  recovered event metadata (CRID/first-showing remain 0 because EIT does not
  carry them);
- ingest parses one complete section and `oa_eit_cache_put` replaces matching
  `(service,event)` keys.

Control commands:

```
epg-ingest <hex>   feed one complete EIT section
epg-clear          drop cached events
epg-count          cached event count
epg-query <svc> <from_ms> <to_ms>
```

Example from the IPC test:

```
epg-ingest <section hex>   -> ok events 2
epg-query 7 1677326400000 1677331800000
count 2
event id=100 service=7 start=1677326400000 end=1677330000000 crid=0 first=0
  state=0 version=3 actual=1 running=4 name=Hello
```

## Verification

```
ok eit section: EIT header/CRC, BCD times, descriptors and skips
ok epg IPC: EIT ingest, key replacement, query/clear and reload over the control socket
```

Full host `make check`: **417 ok**. `verify.cjs`: **45 groups**. `mod_epg.so`
and the EIT section test build for ARM; the test passes under `qemu-arm-static`.

Evidence:

- `artifacts/analysis/epg-eit-module-2026-11-05/check.txt`,
  `full-host-summary.txt`, `arm-build.txt`, `arm-run.txt`, `identities.txt`
- `openaurora/tests/eit_section.c`, `tests/epg_ipc.cjs`

## Limits

- The parser expects a **complete, already-assembled** section; there is no
  PID/demux section assembly, continuity handling or EIT filter/payload
  dispatch.
- It does not merge sections by version/last-section or retain a schedule
  across reloads; each ingested section replaces matching event keys only.
- No CRID/first-showing extraction: the EIT key lacks them and the
  `ScheduleEvent`/`ProgramCrid` database is not wired.
- No persistence, series linking, conflict handling or vendor EIT hub/section
  handler ABI.
- Ingest is explicit (`epg-ingest`/module API); no live EIT source is
  connected. **partly superseded:** the offline `mod_tuner` in
  [TUNER-NIM-MODULE](TUNER-NIM-MODULE-2026-11-05.md) pushes synthetic EIT
  sections into `epg.ingest`; a live demux/tuner source remains absent. Static firmware grounding only; runtime EASI/testtool TCP 1986
  remains blocked pending the owner's watchdog-safe/recovery decision.
