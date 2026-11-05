# Broadcast service database inventory — 2026-11-05

First core-TV data slice. Offline/host only; no TV access.

## Firmware mapping

`SQLiteStreamStorage::write` (`0x9b4754`) and `open` (`0x9b4b9c`) map the
service table types to:

| type | table |
|---|---|
| `0xc9` | `DVBSerTable` |
| `0xca` | `EASISerTable` |
| `0xcb` | `AnalogSerTable` |

The dump contains 445 DVB rows, 67 EASI rows and 13 analog rows (525 total).
The adapter reads the ordinary columns by name:

- DVB: `m_id`, `m_handle`, `m_channel_no`, `m_lcn`, `m_type`, `m_flags`,
  `m_nid`, `m_onid`, `m_tsid`, `m_service_locator`, video/audio/PMT/PCR PIDs,
  network type, country, serialized name/provider.
- EASI: `m_handle`, `m_rsn`, `m_fine_tune_offset`, `m_flags`, name.
- Analog: `m_handle`, `m_channel_no`, `m_lcn`, `m_flags`, `m_type`,
  `m_tv_system`, `m_vps_code`, `m_aps_db_index`, name/provider.

Text blobs are bounded, NUL-terminated and truncate at an embedded NUL. A
missing required table is an error, not a silent empty list. Rows preserve
table order; this layer deliberately does **not** implement service-list
filtering, LCN sorting, tuner selection, EPG or PVR semantics.

## Provider

`mod_service_db` publishes `service.list` v1. It reads
`service_db.path` (default `/conf/sqlite_db/settingsDB.db`) at start, keeps an
immutable snapshot, and reloads by re-reading the DB. The CLI is:

```
service-list              -> count 525
service-list <index>      -> source=dvb id=... handle=... lcn=... type=... flags=... name=... provider=...
```

`service.list` is a borrowed immutable snapshot for the service hold lifetime;
suspend/resume retains it. Stop/unload removes the service.

## Verification

```
ok service DB inventory: DVB/EASI/analog rows, bounded text, table order, missing-table failure
ok service DB dump: 525 rows (445 dvb, 67 easi, 13 analog)
ok service_db module: 525 DVB/EASI/analog rows via reloadable service.list and CLI
ok service_db IPC: 525-row inventory, service publication, reload, stop/unload
```

Full host `make check`: **409 ok**. `verify.cjs`: **45 groups**.

A fresh ARM/TV build of `mod_service_db.so` and the adapter test also passes
under qemu with the same 525-row dump, so the provider compiles and reads the
database in the target ABI (no TV execution).

The adapter test also uses a synthetic database for embedded NULs, text
truncation, EASI/analog field mapping and a missing-table failure. The module
test uses the dumped database and checks reload, stop and ENODEV after stop.
The IPC test uses the real Unix socket and verifies the published
`service.list:v1` lifecycle.

## Limits

This is a raw inventory provider, not a complete channel list. Still open:

- vendor service-list filtering, visibility, LCN sorting, duplicate/simulcast
  handling and active-list selection;
- tuner acquisition, DVB-T/DVB-S/Cable scan and signal state;
- EPG/EIT, PVR/timeshift, teletext/MHEG, CI+/CAM;
- channel-list UI and selection/tuning integration;
- real-TV service DB behavior and writes (this adapter is read-only).

No stock parity or hardware acceptance is claimed. The cold bundle has not
been rebuilt with this module.
