# mod_epg async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real `mod_epg`,
a locally built DVB EIT section and a diagnostic `epg.cleanup_delay_ms`. No TV
or EIT hub was touched.

## State held by mod_epg

`mod_epg` publishes `epg` and `epg.ingest`, and owns a heap
`oa_eit_cache` (512-record vector). With `epg.path` set, stop persists the
cache to a tab-separated `OAEPG1` file; a reloaded instance loads that file
back. Without a path the cache is in-memory only and a reload clears it.

The old stop saved the cache and unpublished both services synchronously.
There was no quiesce hook and no way to invalidate the services before the
cache work finished.

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done) and
   logs a pending persistence error on the next call;
2. clears `published` and unpublishes `epg.ingest` and `epg` synchronously,
   so lookups get `ENODEV`;
3. detaches the cache (`c->cache = NULL`);
4. with `epg.cleanup_delay_ms=0`, saves (when persistent) and destroys the
   cache synchronously; otherwise saves/destroys it on a worker after the
   diagnostic delay and returns `-EAGAIN`.

The worker makes no host API calls. `m_stop` joins a leftover worker and frees
an undetached cache; `m_destroy` joins/frees too. `m_start` refuses with
`-EBUSY` if published or a worker is live, and recreates the cache for a
stop/start cycle on the same instance.

Persisting on the worker means a reload cannot republish the service before
the old schedule file is written, and the replacement still loads the same
events.

## Verification

`check-epg-quiesce` starts with `epg.path` and
`epg.cleanup_delay_ms=400`, ingests a two-event EIT section, then:

```text
epg-count -> count 2
reload epg -> failed (fast; 1 ms in the full run)
status epg -> responsive (< 1 s)
epg-count -> failed errno 19             (both services unpublished)
wait 600 ms -> persisted file starts OAEPG1 and has 2 records
reload epg -> ok
epg-count -> count 2                     (replacement reloaded the file)
epg-query -> count 2, Hello / Second
epg-clear -> ok; epg-count -> count 0
stop epg -> failed; epg-count -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok epg quiesce: reload fails fast (1ms), cache persisted/reloaded, retry completes
```

Existing EPG/tuner behavior still passes:

```text
ok epg IPC: EIT ingest, key replacement, query/clear and reload over the control socket
ok EPG persistence IPC: reload/restart, clear, malformed fail-closed
ok tuner IPC: NIM stub scan/signal, EIT handoff to epg, cancel/reload over the socket
ok tuner TS reload IPC: scan/reload/re-scan keep EIT replay and EPG state
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_epg.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok epg quiesce: reload fails fast (1ms), cache persisted/reloaded, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This is the project EIT schedule cache, not the vendor EIT hub, SQLite EPG
  store or live broadcast feed. The section is built locally.
- Persistence is a simple `OAEPG1` file and `fsync`; it is not the vendor
  database schema or a crash-consistency guarantee under power loss.
- A worker still saving when the host gives up can keep cleanup pending; the
  loop stays responsive, but the test uses a bounded 400 ms delay.
- Without `epg.path` a reload intentionally starts empty; only the persistent
  case proves the cache crosses the reload.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio`, `cec`, `audio_mixer`, `tuner` and now `epg` have the
  async gate; the remaining modules still stop synchronously.
- No TV or stock-Aurora coexistence test was performed.
