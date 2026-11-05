# NDAL tuner backend evidence

Interpretation:
[NDAL-TUNER-BACKEND](../../reports/NDAL-TUNER-BACKEND-2026-11-05.md).

- `check.txt`: full host `make check` log.
- `full-host-summary.txt`: host/verifier/ARM results.
- `arm-run.txt`: ARM `nim_stub` under qemu, including the NDAL mock group.
- `identities.txt`: libNDAL_EX/aurora/backend/test/ARM hashes.

No TV, frontend device or vendor function was executed. The mock ops table
validates the backend contract; the default Linux/NDAL ops are compiled but
not hardware-accepted.
