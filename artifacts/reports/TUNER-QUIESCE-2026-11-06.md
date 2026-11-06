# mod_tuner async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_tuner`, the generated DVB EIT TS fixture and a diagnostic
`tuner.cleanup_delay_ms`. No TV, NIM/frontend hardware, `/dev/dvb` or real
transport stream was touched.

## State held by mod_tuner

`mod_tuner` publishes `tuner`, drives the NIM scan state machine and, for
`tuner.ts_path`, owns the module-global TS replay `FILE*` in
`common/nim_ts.c`. The TS file is a real descriptor, so `/proc/<pid>/fd`
shows whether the old source was closed before the reload reopens it.

The old stop path closed the TS file synchronously. There was no quiesce
hook, no published-service invalidation before the close and no diagnostic
delay.

## Gate

`m_quiesce`:

1. tryjoins a running close worker (`-EAGAIN` while busy, 0 when done);
2. stops an active scan and unpublishes `tuner` synchronously, so lookups
   get `ENODEV`;
3. if a TS source is actually open (`ts_open`), closes it synchronously at
   `tuner.cleanup_delay_ms=0`, otherwise closes it on a worker after the
   diagnostic delay and returns `-EAGAIN`.

The worker makes no host API calls; it sleeps and calls `oa_nim_ts_close`,
which owns the global `FILE*`. `m_stop` joins a leftover worker and closes
only an open TS source; `m_destroy` joins and closes too. `m_start` refuses
with `-EBUSY` if the service is published or a worker is live, and reopens
the TS source only after the old worker has been joined.

The `ts_open` flag matters for retry: unload calls quiesce once through
stop and once through unload. Without the flag the second call would start
another delay worker even though the file was already closed, and the retry
would never complete.

## Verification

`check-tuner-quiesce` starts the host with the generated EIT TS fixture and
`tuner.cleanup_delay_ms=400`:

```text
load/start tuner
tuner-state -> network 0 state 0 progress 0 lock 0 services 0 selected 0 error 0 ...
/proc/<pid>/fd -> 1 entry for the TS replay file
reload tuner -> failed (fast; 1 ms in this run)
status tuner -> responsive (< 1 s)
tuner-state -> failed errno 19            (service unpublished)
wait 500 ms -> TS fd count 0              (worker closed the old source)
wait 200 ms
reload tuner -> ok
tuner-state -> initial idle state
/proc/<pid>/fd -> 1 entry                 (new instance reopened the source)
tuner-scan 1 -> ok; tuner-state reaches state 2
stop tuner -> failed; tuner-state -> failed errno 19
wait 500 ms -> TS fd count 0              (stop gate closed the second source)
stop tuner -> ok; unload tuner -> ok; quit -> bye
```

```text
ok tuner quiesce: reload fails fast (1ms), TS fd closed then reopened, retry completes
```

Existing tuner behavior still passes:

```text
ok tuner IPC: NIM stub scan/signal, EIT handoff to epg, cancel/reload over the socket
ok tuner TS reload IPC: scan/reload/re-scan keep EIT replay and EPG state
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_tuner.so
  NEEDED libdl.so.2, libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok tuner quiesce: reload fails fast (1ms), TS fd closed then reopened, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This is the TS replay `FILE*`, not a real DVB frontend/demux, NIM lock,
  signal measurement or PVR transport. No `/dev/dvb` node was opened.
- `oa_nim_ts_close` owns a module-global `FILE*`; the test proves the host
  serializes reload start until the worker joins, but two instances of the
  module must never be started concurrently.
- The NDAL backend path (`tuner.backend=ndal`) and `tuner.eit_demux` are not
  exercised here; existing stub/TS tests cover their state machine only.
- A worker close delayed beyond the host retry window keeps cleanup pending;
  the loop stays responsive, but the test uses a bounded 400 ms delay.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio`, `cec`, `audio_mixer` and now `tuner` have the async
  gate; the remaining modules still stop synchronously.
- No TV or stock-Aurora coexistence test was performed.
