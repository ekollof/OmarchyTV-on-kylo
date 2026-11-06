# mod_ui_navigation async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_ui_navigation` and a diagnostic `ui_navigation.cleanup_delay_ms`. No
display, input, hardware or TV was touched.

## Ownership change

`ui_navigation` used to keep its selection and 16-entry pending-navigation
queue inline. It now allocates `struct state` on the heap so the gate has a
real object to detach and free.

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `ui.navigation` synchronously, so
   state/result/operation/take/set_* reject with `ENODEV`;
3. snapshots the OAN2 handoff blob and detaches the heap state;
4. frees it synchronously at `ui_navigation.cleanup_delay_ms=0`, otherwise on
   a worker after the diagnostic delay and returns `-EAGAIN`.

`m_stop`/`m_destroy` join and free current/detached states. `m_start` refuses
with `-EBUSY` if published or a worker is live.

Two handoff paths needed explicit fixes:

- **retry `save()` after detach:** the host may save again when retrying a
  reload whose first gate is still cleaning. `save()` now returns the
  detach-time OAN2 snapshot when the live state is gone.
- **failed resume/start:** `oa_module_start` quiesces after a failed start, so
  a publication-collision resume detached the restored state. The next start
  now reconstructs from the saved OAN2 blob instead of silently starting from
  defaults.

`restore()` still validates before allocating/mutating; invalid blobs cannot
partially change the instance.

## Verification

`check-ui-navigation-quiesce` starts with
`ui_navigation.cleanup_delay_ms=400`, queues two navigation requests, then:

```text
nav-state -> tab 0 item 0 submenu 0 pending 0
nav-result 109 -> queued
nav-operation 6 -> queued
nav-state -> tab 2 item 109 submenu 1 pending 2
reload ui_navigation -> failed (fast; 0 ms in the final full run)
status ui_navigation -> responsive (< 1 s)
nav-state -> failed errno 19          (service unpublished)
wait 600 ms
reload ui_navigation -> ok
nav-state -> tab 2 item 109 submenu 1 pending 2   (OAN2 handoff survived)
nav-take -> navigation 19 tab 2 item 109 submenu 1 pending 1
stop ui_navigation -> failed; nav-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok ui_navigation quiesce: reload fails fast (0ms), state handed off, retry completes
```

Existing navigation coverage still passes, including the failed-resume path
that exposed the start-from-snapshot bug:

```text
ok navigation: CLI domains/validation, copied current selection at take, retained fields and pending reload
ok navigation v2: independent setters preserve queue, signed32/raw byte domains, v1 prefix and OAN2 reload/take
ok navigation: atomic full queue, shutdown gate, standby retention, suspended reload and publication-failure retry
ok navigation: holds, OAN1 migration/OAN2 validation and 1792 raw-state roundtrips, signed retained row and cleanup
ok actual Unix IPC: queued modal/operation state, reload, standby/deferred activation, destructive take and clean exit
ok actual hardware-free module matrix IPC: clock chain, navigation/list, power/cec reloads
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_ui_navigation.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok ui_navigation quiesce: reload fails fast (0ms), state handed off, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This is the project navigation selection/queue model, not the running vendor
  `DemoApp`/`EASI` navigation object. OAN1/OAN2 are module-owned formats.
- The worker frees the heap state, but the test observes the gate through
  unpublish/fail-fast/retry and the OAN2 handoff, not freed-memory inspection.
- `take()` remains destructive; the test covers the retained-selection fields
  but not visible settings-screen transitions or input latency.
- No display, renderer or input device is exercised.
- Remaining synchronous modules (12): `audio_amp_profile`, `audio_curve`,
  `panel`, `plat`, `power`, `ticker`, `ui_anim`, `ui_clock`, `ui_console`,
  `ui_diag`, `ui_omarchy`, `ui_profile`.
- No TV or stock-Aurora coexistence test was performed.
