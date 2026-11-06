# mod_service_db async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_service_db`, the project settingsDB dump and a diagnostic
`service_db.cleanup_delay_ms`. No TV or live database was touched.

## State held by mod_service_db

`mod_service_db` reads the DVB/EASI/analog inventory from `settingsDB.db` into
a heap `oa_service_list` (525 rows in the project dump) and publishes
`service.list`. The snapshot points into `c->list`; queries and formatting are
borrowed reads.

The old stop unpublished and freed the row array synchronously, with no
quiesce hook or service-invalidated interval.

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `service.list` synchronously, so lookups
   get `ENODEV` and the snapshot is NULL;
3. detaches the row array (`cleanup_list = list`, `list` zeroed);
4. frees it synchronously at `service_db.cleanup_delay_ms=0`, otherwise on a
   worker after the diagnostic delay and returns `-EAGAIN`.

The worker makes no host API calls. `m_stop` joins a leftover worker and frees
both lists; `m_destroy` joins/frees too. `m_start` refuses with `-EBUSY` if
published or a worker is live, then reads the database into the (empty) list.

## Verification

`check-service-db-quiesce` starts with the 525-row dump and
`service_db.cleanup_delay_ms=400`:

```text
service-list -> count 525
service-list stats -> count 525 dvb 445 easi 67 analog 13
reload service_db -> failed (fast; 1 ms in the full run)
status service_db -> responsive (< 1 s)
service-list -> failed errno 19          (service unpublished)
wait 600 ms
reload service_db -> ok
service-list -> count 525                (replacement re-read the inventory)
service-list 0 -> source=dvb ...
service-list name 0 -> sorted row
stop service_db -> failed; service-list -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok service_db quiesce: reload fails fast (1ms), inventory republished, retry completes
```

Existing DB behavior still passes:

```text
ok service DB dump: 525 rows (445 dvb, 67 easi, 13 analog)
ok service_db module: 525 DVB/EASI/analog rows via reloadable service.list and CLI
ok service_db IPC: 525-row inventory, service publication, reload, stop/unload
```

The ARM module links `libpthread` and `libsqlite3`:

```text
build-tv-v3/modules/mod_service_db.so
  NEEDED libsqlite3.so.0, libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok service_db quiesce: reload fails fast (1ms), inventory republished, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This is the project dump-backed row inventory, not a live vendor
  `settingsDB` writer or the runtime EASI channel-manager state. Reads still
  use the documented shipped table schema and stay read-only.
- Database opening/reading is still synchronous in `m_start`; the gate only
  moves the row-array free off the loop. A slow/corrupt DB on the TV remains a
  separate start-cost and failure-path test.
- The worker frees the detached rows, but the test observes the gate through
  unpublish/fail-fast/retry timing, not through freed-memory inspection.
- Stored visibility/encryption predicates are not the vendor active-list
  filter; runtime EASI visibility remains unimplemented.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio`, `cec`, `audio_mixer`, `tuner`, `epg`, `pvr`, `channels`
  and now `service_db` have the async gate; the remaining modules still stop
  synchronously.
- No TV or stock-Aurora coexistence test was performed.
