# mod_ui_anim async quiesce — 2026-11-06

TV was not touched. Offline host control-socket test with the real
`mod_ui_anim`, `mod_ui_clock` and `mod_ticker`.

## Ownership and gate

`ui_anim` owns the `ui.anim` service, the `ui_clock` hold, the recovered 0/10 ms
cadence timer and the heap `oa_ui_anim` controller.

Quiesce now:

1. fails fast with `-EAGAIN` while a scheduler task/controller is pending;
2. clears `published` and unpublishes `ui.anim` synchronously;
3. removes the cadence timer, clears the kick/fault flags, unbinds callbacks
   and releases `ui_clock` synchronously;
4. detaches the controller;
5. destroys it synchronously at delay 0, else from one nonblocking `sched`
   slice after `ui_anim.cleanup_delay_ms`.

Stale interface calls now fail closed when `published` is false or the
controller is detached. `cancel_cleanup` cancels a queued task and destroys an
unconsumed controller; `m_start` refuses while cleanup is pending and creates a
fresh controller after a stop/start cycle.

## Verification

`check-ui-anim-quiesce` starts ticker + ui_clock + ui_anim with
`ui_anim.cleanup_delay_ms=400`:

```text
anim-state -> anim enabled 0 fault 0
reload ui_anim -> failed (fast; 0 ms in the full run)
status ui_anim -> responsive (< 1 s)
anim-state -> failed errno 19        (service unpublished)
wait 600 ms
reload ui_anim -> ok
anim-state -> anim enabled 0 fault 0 (replacement republished)
stop ui_anim -> failed; anim-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok
stop/unload ui_clock and ticker -> ok; quit -> bye
```

```text
ok ui_anim quiesce: reload fails fast (0ms), controller detached, retry completes
ok ui_anim bootstrap: DesignStyle.svg animation marker and explicit override
  ok   animation completion/finalise callbacks can replace their own slot
  ok   animation timer allocation failure is visible to the starter
  ok   real animation DSO restores its timer after suspend; timer allocation failure is retryable
  ok   text-slide duration/margin match AnimationHelper (pause 800, speed 50, x1000)
  ok   full-screen animation types 1..8 match the HAL vertex rules and clamp
  ok   scene transform equals oa_fs_apply for affine types; keystone flagged
```

The ARM module links only `libm`/`libc`:

```text
build-tv-v3/modules/mod_ui_anim.so
  NEEDED libm.so.6, libc.so.6
```

Full suite ran visibly and ended `all tests passed`; QEMU:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The test starts/stops the animation service; it does not run a live animation
  across the reload.
- The controller free is one loop slice; real controller size/timing is small
  and not measured under animation load.
- The dropped-task/cancel path is implemented but not forced by the socket test.
- No ASan/UBSan, ARM execution or TV run was performed for this migration.
- Remaining synchronous modules (6): `plat`, `power`, `ticker`, `ui_clock`,
  `ui_console`, `ui_omarchy`.
- Cold startup, scanout, real hardware and broad UI parity remain unaccepted.
