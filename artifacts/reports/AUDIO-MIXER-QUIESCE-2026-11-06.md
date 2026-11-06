# mod_audio_mixer async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_audio_mixer` and the echo `tests/mixer_peer` broker. No real netlink,
vendor mixer, ALSA hardware or TV was touched.

## State held by mod_audio_mixer

`mod_audio_mixer` publishes `audio.mixer`, owns a socketpair to a spawned
broker child, a `watch` on the host loop, a 50 ms reap timer and the
pending/ready/canceled ticket state.

The old quiesce set `active=0`, closed the channel and did a nonblocking
reap; the service stayed published until `stop`, and there was no diagnostic
delay or detach path. `stop` deleted the timer and did not wait for a child.

## Gate

Stop/reload and suspend are now separate:

- `quiesce` (stop/reload) sets `active=0`/`closing`, clears ticket state,
  **unpublishes `audio.mixer` synchronously**, removes the watch, detaches the
  channel fd and closes it synchronously (no delay) or on a worker after
  `audio_mixer.cleanup_delay_ms`.
- The 50 ms reap timer stays active across suspend and after the channel
  close; it retires the broker child. `quiesce` returns `-EAGAIN` while the
  worker or child is still alive, so the host keeps retrying.
- `stop` joins the close worker, closes any detached fd, removes the reap
  timer and closes any remaining channel.
- `suspend` calls the same internal gate with `unpublish=0`, preserving the
  service and its holds across the stop/start cycle; `resume` reuses the
  published service and timer, creates a fresh socketpair and spawns a new
  broker.
- `start` refuses with `-EBUSY` if a child, close worker or detached fd is
  still live.

## Verification

`check-audio-mixer-quiesce` stages the real module and the echo broker with
`audio_mixer.cleanup_delay_ms=400`:

```text
mixer-query 0 -> ticket N
mixer-result N -> result N error 0 device 0 left 123 right 456
reload audio_mixer -> failed (fast; 1 ms in the full run)
status audio_mixer -> responsive (< 1 s)
mixer-query 0 -> failed errno 19       (service unpublished)
wait 600 ms
reload audio_mixer -> ok
mixer-query 0 / mixer-result -> left 123 right 456  (fresh broker channel)
stop audio_mixer -> failed; mixer-query -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok
broker child list empty; quit -> bye
```

```text
ok audio_mixer quiesce: reload fails fast (1ms), broker republished and reaped, retry completes
```

Existing mixer tests still pass:

```text
ok real DSO owns watches/timer, bounds requests, cancels/drains, preserves holds across suspend and invalidates reload tickets
ok real control socket queries/collects and reloads the DSO while the host reactor stays responsive
ok mismatched/oversize/EOF/error replies, final buffered reply and shutdown during a hung request release every child
ok spawn/timer/watch failures unwind without a child or stale service
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_audio_mixer.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok audio_mixer quiesce: reload fails fast (1ms), broker republished and reaped, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The broker is the test `mixer_peer`; this is not real netlink, vendor mixer
  or ALSA hardware acceptance.
- A broker that ignores EOF can keep the child and cleanup pending. The loop
  stays responsive and the host retries, but the child is not force-killed.
- Suspend still needs host retries while the child exits; the 50 ms reap timer
  is the progress mechanism.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio`, `cec` and now `audio_mixer` have the async gate; the
  remaining modules still stop synchronously.
- No TV or stock-Aurora coexistence test was performed.
