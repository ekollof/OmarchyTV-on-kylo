# mod_channels async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_channels`, the recovered ACTIVECHANNELLIST XML shape and a diagnostic
`channels.cleanup_delay_ms`. No TV, runtime EASI or tuner hardware was touched.

## Ownership correction

The runtime list used to live in process-global arrays in `common/channels.c`
(`g_entries`/`g_snapshot`), and `mod_channels` merely published a pointer to
that global iface. There was no per-module list to detach or free.

`common/channels.c` now has a heap-owned `struct oa_channel_list` with
`oa_channel_list_create/destroy/snapshot/ingest_xml/clear`. The existing
`oa_channel_runtime_iface` remains as a thin wrapper over one static default
instance for the direct unit test. `mod_channels` allocates its own list and
publishes a module iface whose callbacks use its instance and fail closed when
`published=0` (`snapshot` returns NULL, `ingest_xml` returns `ENODEV`).

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `channels` synchronously, so lookups get
   `ENODEV` and a stale iface cannot read or replace the list;
3. detaches the list (`c->list = NULL`);
4. destroys it synchronously at `channels.cleanup_delay_ms=0`, otherwise on a
   worker after the diagnostic delay and returns `-EAGAIN`.

The worker makes no host API calls. `m_stop` joins a leftover worker and frees
the list; `m_destroy` joins/frees too. `m_start` refuses with `-EBUSY` if
published or a worker is live, and creates a fresh empty list for a
reload/stop-start cycle.

## Verification

`check-channels-quiesce` starts with `channels.cleanup_delay_ms=400`, ingests a
two-row XML list, then:

```text
channel-state -> mode stored count 0
channel-ingest -> ok channels 2
channel-state -> mode runtime count 2
reload channels -> failed (fast; 0 ms in this run)
status channels -> responsive (< 1 s)
channel-state -> failed errno 19       (service unpublished)
wait 600 ms
reload channels -> ok
channel-state -> mode stored count 0   (replacement starts with an empty list)
channel-ingest -> ok channels 2
channel-list -> runtime order Alpha & Beta / O'Neil
stop channels -> failed; channel-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok channels quiesce: reload fails fast (0ms), list detached, retry completes
```

Existing behavior still passes after the ownership change:

```text
ok channels: runtime active-list XML order, fields and rejection
ok channels IPC: runtime XML order, replacement guards, clear and reload
ok channel list: stored fallback and runtime order markers/selection
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_channels.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok channels quiesce: reload fails fast (0ms), list detached, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This is the project ACTIVECHANNELLIST XML model, not the running vendor EASI
  list or its channel-manager object. LCN/visibility are still unknown and the
  UI keeps its provisional stored-order fallback.
- The worker destroys the detached list, but the test observes the gate through
  fail-fast/unpublish/retry timing; it cannot inspect freed memory from the
  control socket.
- A single module instance is assumed, because the published callback table has
  no user pointer. Two channels instances are not supported by this iface.
- A malformed replacement still preserves the active list inside the module;
  the gate does not add crash-consistency or cross-process sharing.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio`, `cec`, `audio_mixer`, `tuner`, `epg`, `pvr` and now
  `channels` have the async gate; the remaining modules still stop
  synchronously.
- No TV or stock-Aurora coexistence test was performed.
