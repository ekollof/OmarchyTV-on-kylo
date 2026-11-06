# mod_ticker async quiesce — 2026-11-06

Offline host control-socket test with the real `mod_ticker`, followed by a
staged data-only ARM run on the TV. No display, audio, tuner or hardware
module was loaded on the TV.

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

## On-TV ARM scheduler gate — 2026-11-06

Staging was under `/buffer/oa-sched-verify` per the owner's current instruction
(`/buffer` is the writable 232 MB ubi volume), not `/tmp`. The TV was woken with
`tools/tvssh.sh`.

Hash-verified ARM inputs:

```text
openaurora            sha256 6bcbecaa35b2347d6ae4aee3b7965139ee36e9cf45ecc7fe6fdfe67e56f5218b
mod_ticker.so         sha256 ba361d410901c37d785f5eb396f3ec09243442723cafa0f673d7ebb72d9511ee
mod_ui_clock.so       sha256 40c319113cb3cccdf69e39fcd6ca01bd6ceebcd937f5c21e9ce1f0ca24825574
mod_ui_anim.so        sha256 a6d00160c6225f0b865b41e5e37bf4f7a2c815872ff71bfa73d36272d7ffdfd8
```

Only the three data-only modules were loaded. Config used 400 ms cleanup
delays for each. Sequence/output:

```text
ticker ticks 0; clock now 7085402; anim enabled 0 fault 0
after 1.5 s: ticker ticks 1
stop ui_anim -> failed; anim-state -> failed errno 19
wait 0.7 s; stop ui_anim -> ok
stop ui_clock -> failed; clock-state -> failed errno 19
wait 0.7 s; stop ui_clock -> ok
reload ticker -> failed; ticker-state -> failed errno 19
wait 0.7 s; reload ticker -> ok; ticker ticks 2  (8-byte state restored)
stop ticker -> failed
wait 0.7 s; stop ticker -> ok; unload all -> ok; quit -> bye
```

Afterward `/buffer/oa-sched-verify` was removed, no `openaurora` helper
remained, and stock Aurora was still PID 1781. Only the staged DSOs, a Unix
control socket and stdin were opened; no `/dev/fb*`, `/dev/ntk*`, `/dev/dvb`,
`/dev/snd`, `/dev/i2c*`, fb1, scanout, audio or tuner/HAL path was opened.

This is a CPU-only on-TV ARM run: it proves the ARM/glibc scheduler gates for
ticker, ui_clock and ui_anim against real timers and `clock_gettime`, not
display/HAL ownership, scanout, audio or tuner hardware acceptance.

## Limits

- The socket test uses one real 1 s tick; longer-term tick emission and
  corrected-time continuity across the 400 ms gate are not measured.
- The counter is stored via `save()` for the probe; this is project diagnostic
  access, not a new public service.
- The dropped-task/cancel path is implemented but not forced by the socket test.
- No ASan/UBSan or ARM execution was performed on the host; the on-TV ARM
  run covers the module ABI and scheduler gates only.
- Remaining synchronous modules (4): `plat`, `power`, `ui_console`,
  `ui_omarchy`.
- Cold startup, scanout, real audio/CEC/tuner/PVR/VIN hardware and broad UI
  parity remain unaccepted.
