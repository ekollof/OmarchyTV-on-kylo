# PVR TS recording source feeding the mockable DVR sink — 2026-11-19

Status: current (host/mock tests; no device I/O, no recording acceptance)
Last verified: 2026-11-19
Evidence class: firmware (libNDAL3 instructions, existing recovery) + host tests.

## Grounding

`EIT-DVB-DVR-2026-11-05` recovered the DVR boundary in `libNDAL3.so`:
- `Hal_Demux_Dvb_Dvr_Open` @`0x4e7b9f44` — mode 0 selects `O_WRONLY`;
- `Hal_Demux_Dvb_Dvr_Write` @`0x4e7ba000` — returns `write`'s result (the sink
  loops over partial writes);
- `Hal_Demux_Dvb_Dvr_Flush` @`0x4e7b9e48` — ioctl `0xc0086f68`;
- `Hal_Demux_Dvb_Dvr_Close` @`0x4e7b9fc4` — `close`.

The 188-byte transport packet is the MPEG-TS framing rule (ISO/IEC 13818-1),
not a vendor ioctl requirement. No DVB device path is opened by the new source:
it reads through an injected `oa_pvr_ts_io` and writes through the existing
mockable `oa_dvr_sink_*` boundary.

## Implementation

- `include/openaurora/pvr_ts_source.h` + `common/pvr_ts_source.c`: a bounded
  source with three operations — `begin` opens the sink through the supplied
  `oa_dvr_ops`; `pump` reads at most `max_packets` whole 188-byte packets and
  writes them (a short read is EOF, a partial trailing read is never written);
  `end` flushes then closes. Errors are returned and recorded; `end` still
  flushes/closes after a write failure. Maximum 64 packets per call, so a
  recording pump never blocks the host loop on a large buffer.
- `mod_pvr`: opt-in `pvr.dvr_ts=<adapter>:<index>` plus `pvr.ts_path=<file>`
  (optional `pvr.ts_max_packets`, default 64) enables a live DVR session on
  `pvr-record`, driven from the configured TS file; `pvr-finish`, module stop
  and quiesce flush/close the session before the store backend is closed. With
  no gate configured, recording remains the existing offline persistent stub.
- Control probe `pvr-ts-state` reports active/packets/bytes/error.

## Verification

```
ok pvr ts source: open/write/flush/close, 188-byte framing, bounded pump, EOF, error paths
ok pvr IPC: conflict add, record/stop states, reload and cross-process persistence
ok pvr quiesce: reload fails fast (1ms), store republished, retry completes
```

`check-pvr-ts-source` asserts the exact fake-op event order
(`open → write → flush → close`), one 12032-byte write for 64 packets, a second
bounded call, no write at EOF, the short-read case, read/write errors and
argument guards. The IPC test asserts the default-off state (`active 0`).

## Limits

No `/dev/dvb` device, frontend, demux or DVR fd was opened; no real TS stream
from the tuner, no recording/playback acceptance and no vendor DVR metadata
engine. The recording store remains the offline stub. The default-off gate
must be enabled deliberately in an authorized window before any device effect.
