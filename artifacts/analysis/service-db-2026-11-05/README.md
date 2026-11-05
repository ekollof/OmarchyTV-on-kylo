# Broadcast service database evidence

Interpretation: [SERVICE-DB](../../reports/SERVICE-DB-2026-11-05.md).

- `full-host.txt`: final strict host `make check` (409 ok).
- `verify.txt`: ARM/reference verifier (45 groups).
- `ipc.txt`: real Unix-socket `service_db` lifecycle.
- `arm-build.txt`, `arm-test-build.txt`, `arm-test.txt`: fresh ARM build and
  qemu adapter run against the same 525-row dump.
- `identities.txt`: host/ARM provider DSOs, host binary, adapter test and
  dumped settingsDB hashes.

The provider reads the three vendor service tables in table order and exposes
`service.list` v1. It is a raw inventory: filtering, LCN sorting, tuner
selection, EPG/PVR and UI integration remain open. No TV access.
