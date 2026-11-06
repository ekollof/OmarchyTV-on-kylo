# Host scheduler second use: channels — 2026-11-06

TV was not touched. This migrates `mod_channels`’ pure-free list cleanup from
a pthread worker to the host `sched` service, following `ui_theme`.

## Implementation

`channels` keeps `cleanup_task` (scheduler id), `cleanup_list` and the
diagnostic delay. Quiesce:

1. fails fast with `-EAGAIN` while a task/list is pending;
2. clears `published` and unpublishes `channels` synchronously;
3. detaches the runtime list;
4. destroys it synchronously at delay 0, else submits `cleanup_step` with
   `channels.cleanup_delay_ms` and returns `-EAGAIN`.

`cleanup_step` clears its task/list and destroys the detached
`oa_channel_list` in one nonblocking slice. `cancel_cleanup` cancels a queued
task through `sched`, releases the service and frees any unconsumed list.
`m_start` refuses while a task or list is live.

The ARM module no longer links `libpthread` for this cleanup:

```text
build-tv-v3/modules/mod_channels.so
  NEEDED libc.so.6
```

## Verification

```text
ok channels quiesce: reload fails fast (1ms), list detached, retry completes
ok channels: runtime active-list XML order, fields and rejection
ok channels IPC: runtime XML order, replacement guards, clear and reload
```

The 400 ms gate test still proves the scheduler path: first reload fails fast,
`channels` lookups are ENODEV while pending, retry after 600 ms succeeds and
the replacement starts with an empty list, and stop uses the same gate.

Full suite ran visibly and ended `all tests passed`; QEMU:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- Only a pure `free` moved to the scheduler. Blocking close/join paths remain
  workers.
- The list is small in the test; real 512-entry ingest/free timing on the loop
  is not measured.
- The dropped-task/cancel path is implemented but not forced by the socket
  test.
- No ASan/UBSan, ARM execution or TV run was performed for this migration.
- Cold startup, scanout, real hardware and broad UI parity remain unaccepted.
