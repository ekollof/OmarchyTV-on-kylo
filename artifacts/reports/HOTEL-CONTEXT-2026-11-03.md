# Persisted hotel context and settings.hotel — 2026-11-03

The existing settings module now publishes read-only `settings.hotel` v1 for
the three persisted fields used by tab availability. `hotel-state` exposes the
same query through CLI/Unix IPC. `oa_tab_apply_settings` combines a successful
read with held ui.profile facts while preserving entry/media/app inputs.
No automatic navigation consumer, hotel-setting writer or native startup is claimed.

## Firmware and database authority

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`; source extracts are in
`../analysis/content-lifecycle-2026-11-03/`. HotelModeConfiguration::readFromNvram
1054cc8 reaches NvramStorage::readAndApplyHotelModeConfiguration f56840.
The latter uses **db_tables+60 -> HMCSettings**, not columns on Settings.
db_tables is BSS at1a1c718; it is populated by the NvramStorage constructor.
db-table-binding.txt shows index24 assigned from literal f565b8; verify-binding.cjs
checks the ELF literal/string and the three selected column pointers. Reading
the BSS as if it contained initialized file bytes is not a way to recover this.

hotel_mode_configuration_struct f5517c maps names, destination pointers and
widths. Table schema order differs from this member-array order; use names.

| column | HotelModeConfiguration field | width |
|---|---|---|
| m_hotel_mode_availability | +1d | 1 byte |
| m_osd_menu_mode | +28 | 4 bytes |
| m_pic_mode_availability | +2d | 1 byte |

OSD mode is **+28**, not the neighboring+20 field. Getter105449c confirms it.
hotel-fields.txt/hotel-selected-pointers.txt/hotel-column-names.txt retain the
name/field associations. Native SQLiteDB::SequentialQuery::getNextRow bd2f2c
uses sqlite3_column_int, then a raw byte or32-bit store according to these widths.
It does not normalize the two type1 fields to bool. NULL/text/large integers
therefore follow SQLite's int conversion before narrowing. Widths other than
the ones above have other behavior; the new service only exposes these fields.

Saved /conf database copy artifacts/dumps/apdat_files/sqlite_db/settingsDB.db:
MD5 `150e2be30e5948571860a8c60c9da525`, row **0/1/0** in that order. Read-only
queries and the module corpus leave its content hash unchanged. This is a
saved persisted-state observation, not a fresh TV poll or proof of Aurora's
current in-memory cache. ui.profile's hotel support1 does not imply activation:
the wrapper requires nonzero persisted mode availability AND profile HotelMode1.

The vendor read path calls beginTransaction/syncMixTypeTable and can mutate
schema. **Do not call that initializer as a harmless query on the TV.** Our
companion uses a single SELECT only. Missing-table/default constructor behavior,
including RF/IP-hotel overrides, remains a separate native initialization gate.

## Service and module lifetime

settings.hotel.read returns copied raw values from one SELECT/first row. It
performs local SQLite/file I/O, using the existing connection and default zero
busy timeout; it is not described as an I/O-free cached service. It returns
positive errno: EINVAL null output, ENODEV unavailable, ENODATA no row, EBUSY
locked/busy, ENOMEM allocation failure, EIO schema/other SQL errors. Output
changes only after successful step and finalize. Errors do not invent defaults.
No HMC write or schema synchronization exists, even with legacy Settings writes
enabled. The existing settings service interface/version remains unchanged.

`hotel-state` accepts no arguments/force flag and prints raw persisted fields,
for example `hotel 0 osd 1 picture 0`. It is not a claim that hotel support is
disabled or that the stock process has reread this row. Reload uses the ordinary
module lifecycle; subsequent reads observe committed file changes directly.

This exposed and fixed an existing settings publication bug: start ignored
publication failure, and stop could unpublish somebody else's service after a
failed start. Ownership flags now track both services, roll back only our own
successful publication, preserve colliding peers and close failed-start handles.
Stop invalidates the connection and both flags before removal fan-out. Cached
hotel reads then return ENODEV and legacy getters refuse; is_writable also
returns0 when closed, including a formerly writable connection. Either service's
normal consumer hold pins the provider. No worker/timer/child is introduced.

The common helper applies raw profile equality-to1, raw persisted hotel nonzero,
picture byte and OSD word. Entry/media/app flags remain caller-owned. It does
no SQL or device access and rejects null arguments without changing context.

## Evidence and remaining acceptance

- Real DSO host/ASan/UBSan/ARM-qemu tests cover fresh persisted reads, SQLite
  NULL/text/int32/byte conversions, busy/error/no-row output preservation,
  either-service holds, both peer-collision paths, cached removal calls, reload,
  read-only and writable legacy connection cleanup, missing schema and first row.
- The ARM DSO/corpus test uses the actual shipped libsqlite3.so.0, MD5
  `5455ae4b89ea97e662b11b438abbaafb`, with no platform/device initialization.
- The optional firmware reference executes shipped bd2f2c code (272 bytes,
  CRC32 90099888), redirecting only its stack guard and stubbing step/column_int.
  **345** cases verify byte/word destination stores and untouched surrounding
  fields for rows, DONE and BUSY. This verifies the member-copy contract, not
  the complete vendor SQL initializer. Prior reference groups still pass.
- Actual local Unix IPC covers field output, narrowing, strict syntax, committed
  fixture changes, reload, errors, provider stop and clean process/socket cleanup.
- Full strict host suite and focused sanitizers/ARM policy tests pass. Host
  loader preflight covers exactly23 source modules; the companion adds no module.

Final isolated build-tv-v3/tab-policy settings module24588 bytes/MD5
636e6abd4526dcea42dd523d349bdc76/maxGLIBC2.4; DSO test485204/
6705c8bb048fe4526d49b67a7c7b1902; optional reference85152/
4fc0b948eb52f57cbe8a3c403b9e46d3. No target loader or hardware acceptance.
No TV connection/wake/staging/deployment, keys, drawing or persistent/recovery
mutation. No helper/build/Ghidra job remains pending after verification.

The pending controller can now hold ui.profile AND settings.hotel for the
profile-backed persisted-row path. It still needs actual entry/media/app state,
description/content construction, permission/key/modal/writeback behavior and
real display acceptance. Full native persistence/defaults, mutable hotel setters
and divergence from the vendor cache are not solved by reading three columns.
