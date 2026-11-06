# mod_panel async quiesce — 2026-11-06

Offline host control-socket test with the real `mod_panel` and `mod_plat`
stub HAL. No PWM device, backlight, panel or TV was touched.

## Gate

`panel` owns the PWM-open flag and requested level. The gate:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `panel` synchronously, so lookups get
   `ENODEV`;
3. arms one cleanup per start cycle (`needs_cleanup`), then runs the vendor
   `Hal_Pwm_Close()` on a worker after `panel.cleanup_delay_ms`.

The diagnostic delay is honored on the host build too, where the HAL is a
no-op; that keeps the gate timing testable without touching hardware.
`m_stop`/`m_destroy` join and close leftovers. `m_start` refuses if published
or a worker is live.

The `needs_cleanup` flag was added after the first test failed: unload calls
quiesce twice (stop then unload), and without the flag the second call armed a
second delay worker after the first had already completed, so the retry never
succeeded.

New control probe:

```text
panel-state     query panel requested percent
```

## Verification

`check-panel-quiesce` starts plat+panel with `panel.start=40` and
`panel.cleanup_delay_ms=400`:

```text
panel-state -> panel percent 40
reload panel -> failed (fast; 0 ms in the final full run)
status panel -> responsive (< 1 s)
panel-state -> failed errno 19       (service unpublished)
wait 600 ms
reload panel -> ok
panel-state -> panel percent 40      (replacement republished)
stop panel -> failed; panel-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; stop/unload plat -> ok; quit -> bye
```

```text
ok panel quiesce: reload fails fast (0ms), publish invalidated, retry completes
ok actual plat-consumer IPC: audio/source/ir/panel services, plat hold refusal, dependency-ordered reload and reuse
```

The ARM module links the vendor libraries and `libpthread`:

```text
build-tv-v3/modules/mod_panel.so
  NEEDED libNDAL3.so, libNDAL_EX.so, libasound.so.2, libv4l2.so.0,
         libv4lconvert.so.0, libnteec.so, libmhash.so.2,
         libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok panel quiesce: reload fails fast (0ms), publish invalidated, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The host test uses the no-op PWM stubs, so it proves publish invalidation,
  one-shot gate arming and retry, not a real `Hal_Pwm_Close` or backlight
  effect.
- No TV panel/PWM run was performed; changing backlight could disturb the
  owner's display. The module's real close path remains hardware-unaccepted.
- Panel channel/frequency remain configuration; the board table is still
  unfound. No restore of a prior duty is claimed.
- Remaining synchronous modules (8): `plat`, `power`, `ticker`, `ui_anim`,
  `ui_clock`, `ui_console`, `ui_diag`, `ui_omarchy`.
- No stock-Aurora coexistence test was performed.
