# EIT-DVB section filter and DVR sink evidence

Interpretation:
[EIT-DVB-DVR](../../reports/EIT-DVB-DVR-2026-11-05.md).

- `check.txt`: full host `make check` log (424 ok).
- `full-host-summary.txt`: host/verifier/ARM results.
- `arm-nim.txt`: ARM `nim_stub` under qemu (NDAL/EIT-DVB mock groups).
- `arm-pvr.txt`: ARM `pvr_stub` under qemu (DVR sink group).
- `identities.txt`: component/test/ARM hashes.

No TV, frontend/demux/DVR device or vendor function was executed. The ops
tables are mock-driven; the default NDAL/Linux implementations are compiled
but not hardware-accepted.
