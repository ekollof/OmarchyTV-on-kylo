# mod_ui_clock async quiesce — 2026-11-06

TV was not touched. Offline host control-socket test with the real
`mod_ui_clock` and `mod_ticker`.

## Ownership and gate

`ui_clock` owns the `ui.clock` service, the raw `clock` hold, its one-second
sampling timer and the FROST time state (previous raw, remainder, correction).

Quiesce now:

1. fails fast with `-EAGAIN` while cleanup is pending;
2. clears `published` and unpublishes `ui.clock` synchronously;
3. removes the timer and releases the raw clock synchronously;
4. snapshots the time state and detaches it;
5. frees it synchronously at delay 0, else from one nonblocking `sched` slice
   after `ui_clock.cleanup_delay_ms`.

The snapshot makes retry `save()` work after detach, and a start that follows a
failed activation restores from it before sampling again. `cancel_cleanup`
cancels a queued task and frees an unconsumed state; `m_start` refuses while
cleanup is pending and recreates the state after a stop/start cycle.

## Verification

`check-ui-clock-quiesce` starts ticker + ui_clock with
`ui_clock.cleanup_delay_ms=400`:

```text
clock-state -> clock now N
reload ui_clock -> failed (fast; 0 ms in the final run)
status ui_clock -> responsive (< 1 s)
clock-state -> failed errno 19        (service unpublished)
wait 600 ms
reload ui_clock -> ok
clock-state -> clock now N            (replacement republished)
stop ui_clock -> failed; clock-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok
stop/unload ticker -> ok; quit -> bye
```

```text
ok ui_clock quiesce: reload fails fast (0ms), time state detached, retry completes
  ok   FROST quantum boundaries, correction carry and raw/corrected 90000000-ms rollover
  ok   real DSO reload preserves clock state, replaces owned timer and rejects invalid snapshots atomically
  ok   suspend removes timer; failed resume can retry without losing elapsed time/correction
  ok   ARM raw 32-bit truncation and strict > period comparison are preserved
  ok   missing raw clock, full timer table and publication collision clean up; start can retry
  ok   real ticker/ui_clock dependency chain quiesces both timers and resumes in provider order
  ok   owned one-second sampling advances idle clock state without a client query
```

The ARM module links only libc:

```text
build-tv-v3/modules/mod_ui_clock.so
  NEEDED libc.so.6
```

Full suite ran visibly and ended `all tests passed`; QEMU:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The socket test observes service presence and timer invalidation, not exact
  corrected-time continuity across the 400 ms delay.
- The unit test covers save/restore arithmetic and the failed-resume handoff;
  no live UI animation consumes the clock during the gate test.
- The time state is small; scheduler free timing is not measured.
- The dropped-task/cancel path is implemented but not forced by the socket test.
- No ASan/UBSan, ARM execution or TV run was performed for this migration.
- Remaining synchronous modules (5): `plat`, `power`, `ticker`, `ui_console`,
  `ui_omarchy`.
- Cold startup, scanout, real hardware and broad UI parity remain unaccepted.
