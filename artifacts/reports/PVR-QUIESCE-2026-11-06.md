# mod_pvr async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real `mod_pvr`
and the project file-backed PVR stub. No TV, recording hardware, storage
manager or media capture was touched.

## State held by mod_pvr

`mod_pvr` publishes `pvr` and drives the `oa_pvr_backend` file store. The
stub keeps up to 256 jobs plus the next-id sequence in memory, opens a
`pvr.path` file at start and saves atomically on every mutation. `close` currently
only clears the backend's `open` flag; the data is already persisted on each
operation.

The old stop unpublished and closed synchronously with no quiesce hook, so a
reload had no service-invalidated interval and no bounded backend-close path.

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `pvr` synchronously, so lookups get
   `ENODEV`;
3. if the backend is open, closes it synchronously at
   `pvr.cleanup_delay_ms=0`, otherwise on a worker after the diagnostic delay
   and returns `-EAGAIN`.

The `opened` flag prevents the stop+unload double quiesce from spawning a
second delay worker after the backend has already been closed. `m_stop` joins
a leftover worker and closes an open backend; `m_destroy` joins/closes too.
`m_start` refuses with `-EBUSY` if published, a worker is live or the backend
is still open, and marks `opened` only after `backend->open()` succeeds.

## Verification

`check-pvr-quiesce` starts with `pvr.path` and
`pvr.cleanup_delay_ms=400`, then:

```text
pvr-list -> count 0
pvr-add 1 1000 2000 -> id 1
pvr-add 2 3000 4000 -> id 2
reload pvr -> failed (fast; 1 ms in the full run)
status pvr -> responsive (< 1 s)
pvr-list -> failed errno 19              (service unpublished)
wait 600 ms -> OAPVR1 file has 2 records
reload pvr -> ok
pvr-list -> count 2, both reservations
pvr-add 1 5000 6000 -> id 3              (reopened store continues the id sequence)
pvr-record 3 -> ok; pvr-stop 3 -> ok
stop pvr -> failed; pvr-list -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok pvr quiesce: reload fails fast (1ms), store republished, retry completes
```

Existing PVR behavior still passes:

```text
ok pvr IPC: conflict add, record/stop states, reload and cross-process persistence
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_pvr.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok pvr quiesce: reload fails fast (1ms), store republished, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The stub `close` is only an in-memory flag, so this test proves fail-fast
  service invalidation and a completing retry, not a slow external close.
  A future backend that flushes or joins real storage uses the same worker
  contract, but that timing is unproven.
- This is the project file store, not the vendor SQLite metadata/timeshift
  engine. There is no media capture, playback, storage manager, tuner-count
  conflict model or repeat-day expansion.
- The test does not reload during an active recording or concurrent storage
  write; persistence is tested through the normal mutation path.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio`, `cec`, `audio_mixer`, `tuner`, `epg` and now `pvr` have
  the async gate; the remaining modules still stop synchronously.
- No TV or stock-Aurora coexistence test was performed.
