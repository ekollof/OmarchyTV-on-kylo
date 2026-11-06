# Host scheduler first module use: ui_theme — 2026-11-06

This is the first non-Lua module to use the host `sched` service. TV was not
touched.

## Why this module

`ui_theme`’s delayed quiesce cleanup is just `oa_svg_pack_free()`: pure heap
release with no blocking driver or process wait. That belongs on the host loop,
unlike `dlclose`, PWM close or child reaping, which still use workers.

Using `sched` instead of a dedicated pthread means:

- no per-reload thread is created for the pack free;
- cleanup runs as one nonblocking slice under the module’s owner id;
- the host loop’s scheduler ordering serializes it with other module slices;
- standby defers the slice (matching the scheduler contract);
- unload/stop cancellation is explicit, with the module retaining and freeing
  the pack if the task never runs.

## Implementation

`mod_ui_theme` now keeps `cleanup_task` (scheduler id), `cleanup_pack` and the
diagnostic delay. Quiesce:

1. fails fast with `-EAGAIN` while a task/pack is pending;
2. clears `published` and unpublishes `ui.theme` synchronously;
3. detaches the pack;
4. frees synchronously at delay 0, else submits `cleanup_step` with
   `ui_theme.cleanup_delay_ms` and returns `-EAGAIN`.

`cleanup_step` clears `cleanup_task`/`cleanup_pack` and frees the pack in one
nonblocking slice. `cancel_cleanup()` looks up `sched`, cancels a queued task,
releases the service and frees any unconsumed pack. `m_start` refuses while a
task or pack is live.

The ARM module no longer links `libpthread` solely for quiesce:

```text
build-tv-v3/modules/mod_ui_theme.so
  NEEDED libc.so.6
```

## Verification

```text
ok ui_theme quiesce: reload fails fast (0ms), pack detached, retry completes
ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse
ok ui_anim bootstrap: DesignStyle.svg animation marker and explicit override
```

The existing gate test still proves the scheduler path: the first reload fails
fast, `theme-state` is ENODEV while the 400 ms task is pending, and the retry
after 600 ms sees the freed pack and publishes the changed two-record fixture.

Full suite ran visibly and ended with `all tests passed`; QEMU:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- Only `ui_theme` was migrated. Modules with blocking close/join/driver work
  still need their worker or owner-thread paths.
- The scheduler is loop-thread and nonblocking; a slow `oa_svg_pack_free` would
  delay the loop. The real 702-record pack free is not timed here.
- A scheduler task is dropped on unload without running; `m_stop`/`m_destroy`
  cancel and free, but that path is not currently forced by the socket test.
- No ASan/UBSan, ARM execution or TV run was performed for this migration.
- Cold startup, scanout and real hardware remain unaccepted.
