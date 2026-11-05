# EIT transport-stream demux and replay backend — 2026-11-05

Offline only. Replaces the tuner's synthetic-only EIT source with a real
MPEG-2 transport-stream demux and adds `tuner.ts_path`. No TV, tuner device,
vendor function or recording hardware was accessed. The NIM tune/signal side
remains the offline stub; this closes the EIT demux/filter gap, not the live
tuner backend.

## Grounding

- DVB EIT is carried on PID `0x12` with table ids `0x4e..0x6f`
  (EN 300 468 / ISO/IEC 13818-1). The PID is a DVB transport constant, not a
  recovered vendor-private value.
- `aurora.elf` exposes `EITHub`, `EITCache`, `StandardEITTuneStrategy` and
  `EASIControlInterface::suspendEITFilters` (`nm -D`), i.e. the vendor runs
  PID/EIT filters. This implementation is the project's bounded equivalent.
- Time/section decoding still uses the firmware-grounded `eit_time.c` and
  `eit_section.c` from VENDOR-BROADCAST-ABI/EPG-EIT-MODULE.

## Implementation

`common/eit_ts.c` handles one 188-byte packet at a time:

- 188-byte/sync validation, PID filter, transport-error and scrambled-payload
  drops, adaptation-field control 1/2/3;
- continuity-counter duplicate suppression and discontinuity reset;
- PUSI pointer fields (including the tail of a previous section), sections
  split across packets, back-to-back sections and trailing `0xff` stuffing;
- EIT table-id `0x4e..0x6f` delivery; other tables are assembled only to be
  skipped, and the 4096-byte section cap is enforced.

`common/nim_ts.c` is the `nim-ts-replay` backend: it opens a TS file, filters
PID `0x12`, queues up to eight assembled sections, and exposes them through
`oa_tuner_backend.next_eit`. The NIM ops are the shared stub. `mod_tuner` reads
`tuner.ts_path`, opens the replay source on start, closes it on stop/destroy,
and feeds the sections to `epg.ingest` during a scan.

Tests:

- `tests/eit_section.c`: PID filter, split/back-to-back sections, pointer
  field, adaptation field, duplicate/continuity/transport-error handling,
  stuffing, non-EIT tables and malformed packets.
- `tests/nim_stub.c`: TS replay backend, file open/close, section delivery and
  parser roundtrip.
- `tests/eit_ts_fixture.c`: deterministic two-event, three-packet TS fixture
  used by the real-socket run.

## Verification

```
ok eit section: header/CRC, BCD times, descriptors, skips and TS demux
ok nim stub: tune/signal defaults, synthetic EIT and TS replay demux
```

Full host `make check`: **423 ok**. `verify.cjs`: **45 groups**. The new
demux/replay test binary also builds and passes under ARM/qemu:

```
ok nim stub: tune/signal defaults, synthetic EIT and TS replay demux
```

Real Unix-socket run with `tuner.ts_path` pointing at the generated fixture:
the scan completed and `mod_epg` reported:

```
count 2
event id=7001 service=777 start=1677326400000 end=1677328200000 ... running=4 name=A...
event id=7002 service=777 start=1677330000000 end=1677332700000 ... running=1 name=B...
```

Evidence:

- `artifacts/analysis/eit-ts-demux-2026-11-05/generation.txt`
- `artifacts/analysis/eit-ts-demux-2026-11-05/integration.txt`
- `artifacts/analysis/eit-ts-demux-2026-11-05/arm-run.txt`
- `artifacts/analysis/eit-ts-demux-2026-11-05/check.txt`,
  `full-host-summary.txt`, `identities.txt`

## Limits

- Replay backend only: no live tuner, demux device, frontend lock, PID bank or
  hardware section filter.
- NIM tune/monitor/signal remains `nim-stub`; no transponder scan or service
  discovery.
- PID is fixed to `0x12`; no multi-PID EIT, NIT/SDT, EIT schedule-table
  merging, CRID resolution, persistence or now/next cache split.
- The tuner scan/selection state machine remains a project contract, not a
  recovered vendor API.
- No TV access or live acceptance; cold start, display and hardware backend
  remain owner-gated.
