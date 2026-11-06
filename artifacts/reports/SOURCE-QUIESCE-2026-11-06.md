# mod_source async quiesce — 2026-11-06

TV was off limits. This is an offline host control-socket test with the real
`mod_source` linked against a fake `Hal_Vin` fixture. No TV, `/dev/ntkdvif`,
vendor HAL or fbdev was touched.

## State held by mod_source

`mod_source` holds the VIN fd returned by `Hal_Vin_Open(0)`, the decoded
source-type/current state and the published `source` service. Switching is
gated by `source.enable`; the input id comes from the 26-entry table at
`0x013b9e78`.

## Gate

`m_quiesce`:

1. if a worker is already running, `pthread_tryjoin_np` it; return 0 when it
   has finished, `-EAGAIN` while it is busy, `-EIO` on a join error;
2. release/unpublish `source` synchronously, so new lookups get `ENODEV` and
   a stale iface call sees `fd < 0`/unavailable;
3. detach the fd (`c->fd = -1`, `current = -1`) so no select/stream-start can
   reach the closing device;
4. with `source.cleanup_delay_ms=0`, call `Hal_Vin_Close(fd)` synchronously;
5. otherwise hand the fd to a worker that sleeps for the diagnostic delay and
   calls `Hal_Vin_Close(fd)`, returning `-EAGAIN`.

The worker makes no host API calls and touches only the fd/delay captured
before it starts. `m_stop` joins a leftover worker, then closes any fd that
was not detached; `m_destroy` joins and closes too. The gate is idempotent:
`oa_module_unload` calls `quiesce` again after `stop`, and the second call
returns 0 with no fd.

The module's service calls remain loop-thread-only. The worker only performs
the final close after the service is unpublished; no concurrent
select/stream-start is possible through the replacement.

## Verification

`check-source-fake-ipc` now configures `source.cleanup_delay_ms=400` and
stages the real `mod_source` linked with `tests/source_fake.c`:

```text
reload source -> failed (fast; first elapsed 0 ms in this run)
status source -> responsive (< 1 s)
source-state -> failed errno 19           (service already unpublished)
wait 600 ms
reload source -> ok
source-state -> source type -1 available 1
source-select 17 -> ok
VIN log: open idx=0 (initial), open idx=0 (reload), close fd=100,
         close before the second open
stop source -> failed; source-state -> failed errno 19
wait 600 ms; stop source -> ok; unload source -> ok
VIN log: second close fd=100               (stop gate)
```

```text
ok source fake-VIN IPC: reload fails fast (0ms), worker closes fd before VIN
reopen, stop uses the gate, source.enable holds
```

The same test still proves the decoded VIN ids for HDMI1/2/3, YPbPr, VGA and
TV (3/3/66/3/2/1), the `source-state` query, and a fresh process with
`source.enable=0` returning `failed disabled` without a select.

Later VIN-DEVICE-SIM-2026-11-06 runs the real ARM `mod_source.so` against the
shipped `libNDAL3.so` and a fake `/dev/video20` under qemu. The real
`Hal_Vin_Close`/`close` path is exercised, and the same reload/stop gate
closes before reopening (`check-vin-real`).

The ARM build links the new pthread dependency and the vendor libraries:

```text
build-tv-v3/modules/mod_source.so
  NEEDED libNDAL3.so, libNDAL_EX.so, libasound.so.2, libv4l2.so.0,
         libv4lconvert.so.0, libnteec.so, libmhash.so.2,
         libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This fixture's `Hal_Vin_Close` only appends to a log. The later
  VIN-DEVICE-SIM executes the real `libNDAL3.so` `Hal_Vin_Close` (a `close`
  call) against a fake `/dev/video20`; real kernel-driver close latency and
  vendor-library thread safety are still not proven.
- The test does not reload during an active select or a real HDMI signal.
- Service calls are assumed to stay on the host loop thread; a future caller
  from another thread would need explicit locking.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display` and now
  `source` have the async gate; the remaining modules still stop
  synchronously.
- No TV or stock-Aurora coexistence test was performed.
