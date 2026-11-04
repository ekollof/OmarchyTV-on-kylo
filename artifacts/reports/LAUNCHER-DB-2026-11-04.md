# settingsDB LauncherItem read path — 2026-11-04

`common/launcher_db.c` now reads the real `LauncherItem` table, and
`mod_settings` publishes it as a reloadable `launcher.db` service with a
`launcher-items` CLI command. The adapter's row decode
(`SqliteLauncherDatabaseAdapter::copyLauncherInfoToItem`, `0xfac334`) is the
shipped body and was differentially verified. The dumped profile database
returns **50 rows**; the live TV database returned **54 rows** in a read-only
probe. The new shipped-body group has **8** native/C cases; `verify.cjs` has
**39** ok lines. Host, ASan/UBSan, ARM/qemu, full `make check` (265 ok) and
four focused TV runs pass.

## Recovered structure

`NovoLauncherItemInfo::launcher_item_info_struct` (`0xfabf50`) builds the
member map used by `SQLiteDB::SequentialQuery`:

- 16 scalar columns: `m_application_image_crc` (+0x04),
  `m_application_thumb_update_time` (+0x08),
  `m_application_update_time` (+0x0c), `m_channel_handle` (+0x10), `m_id`
  (+0x14), `m_is_delete_disabled` (+0x18), `m_is_deleted` (+0x19),
  `m_is_featured` (+0x1a), `m_is_moved_by_user` (+0x1b),
  `m_is_out_of_sync` (+0x1c), `m_item_type` (+0x20), `m_menu_return_id`
  (+0x24), `m_order_index` (+0x28), `m_original_id` (+0x2c), `m_parent_id`
  (+0x30), `m_server_id` (+0x34).
- 10 text columns with fixed sizes: `m_application_ad_image_url` (+0x38,
  300), `m_application_ad_tracking_url` (+0x164, 300), `m_application_ad_url`
  (+0x290, 300), `m_application_name` (+0x3bc, 60),
  `m_application_thumb_file_name` (+0x3f8, 300), `m_application_thumb_url`
  (+0x524, 300), `m_application_url` (+0x650, 300),
  `m_fvp_application_id` (+0x77c, 300), `m_menu_string_id` (+0x8a8, 150),
  `m_recommendation_url` (+0x93e, 300).

The database stores each text value as a blob of exactly that size (60, 150,
300 measured on every dumped row), NUL-padded; `artifacts/dumps/.../settingsDB.db`
has 26 columns and 50 rows. `struct oa_launcher_info` in
`include/openaurora/launcher_item.h` now models this record with compile-time
offset assertions.

## Recovered behavior

`SqliteLauncherDatabaseAdapter::copyLauncherInfoToItem` (`0xfac334`, 0xf4
bytes, CRC `0xa089d47e`) is branch-free. It copies `m_id` to item+0,
`m_parent_id` to +4, `m_order_index` to +8, `m_item_type` to +0x14,
`m_original_id` to +0xc, `m_server_id` to +0x10 and `m_menu_return_id` to
+0x18; assigns the ten text fields through `UString::operator=(char const*)`
in the shipped order; copies the update times and image CRC to +0x44/+0x48/
+0x4c; copies five flag bytes to +0x50..+0x54; and `m_channel_handle` to +0x90.

The two read entry points are:

- `getAllLauncherItems` (`0xfacff1`): clear, iterate every row, decode and
  push. No filtering.
- `getLauncherItemsByParentId` (`0xfacef9`): same, but keep rows whose
  `m_parent_id` equals the requested id, and unless the caller sets the flags,
  skip `m_is_deleted` and `m_is_out_of_sync` rows.

`NovoLauncherDataProvider::getLauncherItems` (`0x107a0c4`) is a thin vtable
forward to the adapter (slot +0x30); there is no extra provider logic to port.
The C adapter merges the provider/adapter pair.

## Implementation

- `include/openaurora/launcher_db.h`, `common/launcher_db.c`: read-only
  `sqlite3` adapter (`oa_launcher_db_read_all`,
  `oa_launcher_db_read_parent`), fixed-size text truncation at the first NUL
  or size-1, parent/deleted/out-of-sync filtering.
- `include/openaurora/launcher_item.h`, `common/launcher_item.c`:
  `oa_launcher_info_to_item` (the shipped decode).
- `mod_settings` publishes `OA_SVC_LAUNCHER_DB` ("launcher.db") with read,
  count, format and release callbacks, so the control plane never links
  sqlite or the row layout. `launcher-items <parent|all> [deleted]
  [outofsync]` prints `count N` plus one line per row.
- The control plane's `launcher.db` service and CLI were exercised on the host
  and ARM: load `settings`, query, `reload settings`, query again, `stop
  settings` (ENODEV).

## Verification

- Shipped-body differential: `oa_launcher_info_to_item` against the verbatim
  body at `0xfac334`, 8 cases (varied scalars/text), comparing the full 0xa0
  item and the ten `UString::operator=` destinations/sources.
- Adapter test against the dumped database: 50 rows; row 23 fields (id,
  parent, order, type, menu-return, channel, server, original, image CRC,
  update times, Netflix name, thumb file/URL, application URL); parent 0 =
  23 non-deleted / 24 including deleted; parent 1 = 23/24; parent -1 = 2;
  missing parent = 0; deleted rows 43/49; synthetic-DB truncation to 59/299
  bytes; missing DB is an error; `stat` confirms the source file is unchanged.
- Module test: 50 rows via the reloadable `settings` service, filters,
  reload, stop/ENODEV.
- Focused TV runs (ARM, read-only): `launcher_item` strict (32-bit refcount
  path), `launcher_db` strict against the staged dump copy, `launcher_db`
  probe against the live `/conf/sqlite_db/settingsDB.db` (`all=54`,
  `parent0=27`, `parent1 incl-deleted=24`), and `launcher_db_module` with the
  staged `mod_settings.so` (load, query, reload, stop). All exit 0; staging
  hash-verified and removed; no helper left; no-reset flag intact.

## Limits

- The shipped `SQLiteDB::SequentialQuery`/`MemberAccess` engine is not ported;
  the adapter uses sqlite3 directly with the recovered member map. The query
  path is a new implementation, not a body-level differential.
- `struct oa_launcher_info` reconstructs the offsets used by the body, not the
  complete vendor `NovoLauncherItemInfo` object (the leading/reserved words at
  +0x00/+0x1d are not modelled).
- Text decoding assumes the fixed-size blobs are NUL-terminated C strings.
  That holds for every row in the dumped and live databases but is not proof
  for other encodings.
- The service is read-only and no UI consumes it yet: `ui_vendor` still draws
  its static `DEFAULT_CARDS`. Wiring these rows into the portal and scrollbar/
  skin work are next.
- Nothing runs independently of vendor Aurora; this is a live read path and a
  reloadable module service, not a replacement startup.
