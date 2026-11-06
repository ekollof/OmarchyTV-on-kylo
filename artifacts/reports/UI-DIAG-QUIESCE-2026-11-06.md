# mod_ui_diag async quiesce — 2026-11-06

TV was not touched. Offline host control-socket test with the real
`mod_ui_diag`, `mod_ui_engine` and the recorder `nav_display` fixture, using
the extracted telefunken18 font pack.

## Ownership and gate

`ui_diag` is a reloadable skin, not a service provider. It owns:

- the `ui.engine` hold and its registered `diag` skin;
- the module-changed subscription;
- the font pack index/offset tables plus font7/title18 pixel buffers.

Quiesce now:

1. fails fast with `-EAGAIN` while a cleanup task or detached fonts remain;
2. sets `suspended=1` so no skin paint/dispatch continues;
3. unsubscribes and unregisters the skin/releases `ui.engine` **synchronously**;
4. detaches the font pack and pixel buffers;
5. frees them synchronously at delay 0, else submits one nonblocking
   `cleanup_step` to `sched` with `ui_diag.cleanup_delay_ms`.

`cancel_cleanup` cancels a queued task and frees any unconsumed fonts.
`m_stop`/`m_destroy` use the same teardown; `m_start` refuses while cleanup is
pending. The module no longer links `libpthread`.

The engine skin query is the fail-fast observable: while the 400 ms scheduler
task is pending, `skin` reports no active `diag` skin even though `ui_diag` is
still the loaded module with cleanup pending.

## Verification

`check-ui-diag-quiesce` loads nav_display + ui_engine + ui_diag with
`ui.skin=diag`, the extracted font pack and `ui_diag.cleanup_delay_ms=400`:

```text
skin -> ... active diag ...
reload ui_diag -> failed (fast; 1 ms in the full run)
status ui_diag -> responsive (< 1 s)
skin -> ... active - ...          (skin/engine dropped synchronously)
wait 600 ms
reload ui_diag -> ok
skin -> ... active diag ...       (replacement re-registered)
stop ui_diag -> failed; skin -> active -
wait 600 ms; stop -> ok; unload -> ok
stop/unload ui_engine and nav_display -> ok; quit -> bye
```

```text
ok ui_diag quiesce: reload fails fast (1ms), skin detached, retry completes
ok ui_diag: service rows, selection, refresh, reload and teardown
  ok   Unix socket startup fallback, runtime switching, enter failure and clean process shutdown
```

The ARM module links only `libm`/`libc`:

```text
build-tv-v3/modules/mod_ui_diag.so
  NEEDED libm.so.6, libc.so.6
```

Full suite ran visibly and ended `all tests passed`; QEMU:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The test uses the recorder `nav_display`, not scanout or the TV panel.
- The gate proves skin/engine invalidation and font-buffer cleanup; it does not
  prove visible-frame timing during a reload.
- The scheduler step frees the font pack/pixels on the loop; real font-free
  timing is not measured.
- The dropped-task/cancel path is implemented but not forced by the socket
  test.
- No ASan/UBSan, ARM execution or TV run was performed for this migration.
- Remaining synchronous modules (7): `plat`, `power`, `ticker`, `ui_anim`,
  `ui_clock`, `ui_console`, `ui_omarchy`.
- Cold startup, scanout, real hardware and broad UI parity remain unaccepted.
