# mod_ticker async quiesce — 2026-11-06

TV was not touched. Offline host control-socket test with the real
`mod_ticker`.

## Ownership and gate

`ticker` owns the `clock` service, its one-second timer and the `ticks` counter
used by the save/restore path.

Quiesce now:

1. fails fast with `-EAGAIN` while cleanup is pending;
2. removes the timer and unpublishes `clock` synchronously;
3. snapshots and detaches the tick counter;
4. frees it synchronously at delay 0, else from one nonblocking `sched` slice
   after `ticker.cleanup_delay_ms`.

The snapshot makes retry `save()` work after detach and preserves the counter
for the replacement. `cancel_cleanup` cancels a queued task and frees an
unconsumed counter; `m_start` refuses while pending and recreates the counter
after a stop/start cycle.

A `ticker-state` probe checks the published clock service and reads the saved
tick counter. It is a project diagnostic, not a new service contract.

## Verification

`check-ticker-quiesce` starts ticker with `ticker.cleanup_delay_ms=400`, waits
1.1 s for a tick, then:

```text
ticker-state -> ticker ticks 1
reload ticker -> failed (fast; 1 ms in the final run)
status ticker -> responsive (< 1 s)
ticker-state -> failed errno 19       (clock service unpublished)
wait 600 ms
reload ticker -> ok
ticker-state -> ticker ticks 1        (counter survived the delayed reload)
stop ticker -> failed; ticker-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok ticker quiesce: reload fails fast (1ms), counter handed off, retry completes
  ok   FROST quantum boundaries, correction carry and raw/corrected 90000000-ms rollover
  ok   real DSO reload preserves clock state, replaces owned timer and rejects invalid snapshots atomically
  ok   suspend removes timer; failed resume can retry without losing elapsed time/correction
  ok   real ticker/ui_clock dependency chain quiesces both timers and resumes in provider order
ok ui_clock quiesce: reload fails fast (0ms), time state detached, retry completes
ok ui_anim bootstrap: DesignStyle.svg animation marker and explicit override
```

The ARM module links only libc:

```text
build-tv-v3/modules/mod_ticker.so
  NEEDED libc.so.6
```

Full suite ran visibly and ended `all tests passed`; QEMU:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The socket test uses one real 1 s tick; longer-term tick emission and
  corrected-time continuity across the 400 ms gate are not measured.
- The counter is stored via `save()` for the probe; this is project diagnostic
  access, not a new public service.
- The dropped-task/cancel path is implemented but not forced by the socket test.
- No ASan/UBSan, ARM execution or TV run was performed for this migration.
- Remaining synchronous modules (4): `plat`, `power`, `ui_console`,
  `ui_omarchy`.
- Cold startup, scanout, real hardware and broad UI parity remain unaccepted.
