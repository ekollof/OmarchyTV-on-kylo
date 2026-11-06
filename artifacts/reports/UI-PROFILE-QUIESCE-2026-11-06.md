# mod_ui_profile async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_ui_profile`, a synthetic EFSL profile and a diagnostic
`ui_profile.cleanup_delay_ms`. No TV, settings DB or hardware was touched.

## Ownership change and gate

`ui_profile` used to keep the copied `oa_tab_profile` inline. It now allocates
it on the heap so the gate has an object to detach and free.

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `ui.profile` synchronously, so `copy`
   rejects with `ENODEV`;
3. detaches the profile copy;
4. frees it synchronously at `ui_profile.cleanup_delay_ms=0`, otherwise on a
   worker after the diagnostic delay and returns `-EAGAIN`.

`m_stop`/`m_destroy` join and free current/detached profiles. `m_start`
refuses with `-EBUSY` if published or a worker is live, then reads the
configured file into the heap copy.

New control probe:

```text
profile-state     query ui.profile facts
```

It holds `ui.profile`, copies the immutable profile, then releases.

## ui_list failed-start audit

The previous `ui_list` gate left a gap: a failed start/resume
(publication collision) makes `oa_module_start` quiesce, which detached the
restored state; the retry used defaults. `ui_list` now rebuilds from the
detach-time OAL1 snapshot on start, matching the `ui_navigation` behavior.
Restoring into a fresh state also reinitializes the fixture handles before
applying OAL1.

`tests/ui_list.c` now exercises the sequence: suspend, reload, peer publishes
`ui.list`, resume fails, peer unpublishes, resume succeeds, and the original
selection/animation state is checked.

## Verification

`check-ui-profile-quiesce` starts with `ui_profile.cleanup_delay_ms=400`:

```text
profile-state -> profile connectivity 1 enabled 1 hotel 0 enabled 0 present 1/1
reload ui_profile -> failed (fast; 0 ms in the final full run)
status ui_profile -> responsive (< 1 s)
profile-state -> failed errno 19       (service unpublished)
rewrite fixture to connectivity 2 / hotel 1
wait 600 ms
reload ui_profile -> ok
profile-state -> profile connectivity 2 enabled 0 hotel 1 enabled 1 present 1/1
stop ui_profile -> failed; profile-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok ui_profile quiesce: reload fails fast (0ms), profile detached, retry completes
```

Existing profile/list/skin behavior still passes:

```text
ok ui.profile: immutable copied facts, exact-one values and CLI reload snapshot replacement
ok ui.profile: consumer holds, removal callback unavailability, publication collision and retry
ok ui.profile: standby keeps facts, suspended reload defers I/O, failed resume activation retries
ok ui.profile: invalid/special/oversized/malformed inputs reject, service never reads deleted file
ok list: CLI domains/validation, verified key chain, suspend/resume, shutdown gate, OAL1 reload and unload
ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse
ok ui_list quiesce: reload fails fast (1ms), state handed off, retry completes
ok ui_navigation quiesce: reload fails fast (1ms), state handed off, retry completes
```

The ARM modules link `libpthread`:

```text
build-tv-v3/modules/mod_ui_profile.so
build-tv-v3/modules/mod_ui_list.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok ui_profile quiesce: reload fails fast (1ms), profile detached, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- This is the project EFSL software-profile adapter, not the vendor
  `UserPreferences`/PQ object or a mutable HotelMode writer.
- The control test uses a tiny synthetic profile; the real extracted profile
  fixture is still covered through the module unit test.
- The worker frees the profile copy, but the gate is observed through
  unpublish/fail-fast/retry and the changed facts, not freed-memory inspection.
- The ui_list failed-start test publishes a peer service directly in-process;
  it does not exercise a real UI consumer.
- Remaining synchronous modules (11): `audio_amp_profile`, `audio_curve`,
  `panel`, `plat`, `power`, `ticker`, `ui_anim`, `ui_clock`, `ui_console`,
  `ui_diag`, `ui_omarchy`.
- No TV or stock-Aurora coexistence test was performed.
