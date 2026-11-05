# Broadcast service database evidence

Interpretation: [SERVICE-DB](../../reports/SERVICE-DB-2026-11-05.md).

- `full-host-summary.txt`: final strict host `make check` (409 ok) + service
  DB lines. `full-host.txt` is the earlier full log.
- `verify.txt`: ARM/reference verifier (45 groups).
- `ipc.txt`: real Unix-socket `service_db` lifecycle.
- `arm-build.txt`, `arm-test-build.txt`, `arm-test.txt`: fresh ARM build and
  qemu adapter run against the same 525-row dump.
- `stored-flags.txt`: recovered `StoredDvbServiceData` predicates and dumped
  flag totals.
- `live-db-check.txt`: read-only live DB counts/hash and the runtime-EASI LCN
  gate.
- `identities.txt`: host/ARM provider DSOs, host binary, adapter test and
  dumped settingsDB hashes.

The provider reads the three vendor service tables in table order and exposes
`service.list` v1. `service-list stats` totals the exact stored-flag predicates
recovered from `StoredDvbServiceData`; `service-list name/channel` queries add
deterministic stored ordering/filtering. `live-db-check.txt` records a read-only
live DB fetch (54/67/13, m_lcn 0x....ffff) and the runtime-EASI LCN/visibility
gate. Runtime active-list filtering, tuner selection, EPG/PVR, channel-list UI
and stock parity remain open. The fetch only read the DB; tvssh sent WoL because
the set was asleep.
