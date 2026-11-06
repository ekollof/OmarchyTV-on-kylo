# mod_ui_list async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real `mod_ui_list`
and a diagnostic `ui_list.cleanup_delay_ms`. No display, input, renderer or TV
was touched.

## Ownership change

`ui_list` used to keep its owned state struct inline in the module context. It
now allocates `struct state` on the heap so the gate has something real to
detach and free. The default 9-item/5-row fixture geometry and the 16 borrowed
row handles are initialized by `state_defaults()` in both `m_init` and a
restart on the same instance.

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `ui.list` synchronously, so `state` and
   `key` reject with `ENODEV`;
3. snapshots the current OAL1 handoff blob into the module context and
   detaches the heap state;
4. frees it synchronously at `ui_list.cleanup_delay_ms=0`, otherwise on a
   worker after the diagnostic delay and returns `-EAGAIN`.

`m_stop`/`m_destroy` join and free current/detached states. `m_start` refuses
with `-EBUSY` if published or a worker is live, and recreates default state
for a stop/start cycle.

Two retry-path fixes were required:

- **save after detach:** the host calls `save()` again when the user retries a
  reload whose first attempt is still cleaning. With the live state detached,
  `save()` now returns the OAL1 snapshot taken at detach time instead of
  dereferencing NULL.
- **restore on a stopped instance:** the direct unit test stops the module
  before exercising `restore()`. `restore()` validates the blob first, then
  allocates a fresh state before applying it. Invalid blobs still cannot
  partially mutate the instance.

## Verification

`check-ui-list-quiesce` starts with `ui_list.cleanup_delay_ms=400`, advances
the selection, then:

```text
list-state -> first 0 size 5 selected 0 total 9 handled -1 anim 0 ...
list-key 523 -> handled 1
list-state -> ... selected 1 ... handled 1 anim 1 focus 2 ...
reload ui_list -> failed (fast; 1 ms in the full run)
status ui_list -> responsive (< 1 s)
list-state -> failed errno 19          (service unpublished)
wait 600 ms
reload ui_list -> ok
list-state -> ... selected 1 ... handled 1 anim 1 focus 2 ...  (OAL1 handoff survived)
list-key 732 -> handled 1
list-state -> ... first 4 selected 4 ... group 1             (replacement routes keys)
stop ui_list -> failed; list-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok ui_list quiesce: reload fails fast (1ms), state handed off, retry completes
```

Existing behavior still passes after the ownership change:

```text
ok list: CLI domains/validation, verified key chain, suspend/resume, shutdown gate, OAL1 reload and unload
ok actual Unix IPC: list keys/selection, reload handoff, standby gate and clean exit
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_ui_list.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok ui_list quiesce: reload fails fast (1ms), state handed off, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This is still the owned SVG-list fixture state, not the running vendor
  `SVGScrollList` object. Real row/widget pointers are not owned here; the
  sixteen handles are fixture values, and animation/scrollbar/group internals
  remain stubs.
- The gate frees the heap state, but the test observes it through
  unpublish/fail-fast/retry and the OAL1 handoff, not freed-memory inspection.
- The OAL1 snapshot is a module-owned format, not a vendor ABI.
- No display, renderer or visible list interaction is exercised.
- Remaining synchronous modules (13): `audio_amp_profile`, `audio_curve`,
  `panel`, `plat`, `power`, `ticker`, `ui_anim`, `ui_clock`, `ui_console`,
  `ui_diag`, `ui_navigation`, `ui_omarchy`, `ui_profile`.
- No TV or stock-Aurora coexistence test was performed.
