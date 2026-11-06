# mod_audio async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real `mod_audio`
linked against the test HAL stub, plus the real `mod_plat`. No audio device,
TV, ALSA hardware or audible output was touched.

## State held by mod_audio

`mod_audio` publishes the `audio` service, holds `plat`, caches the last
accepted volume/mute, and owns an `oa_audio_owner` worker. The owner may be
blocked in a client open/gain/close RPC, so joining it on the host loop is the
thing the gate must avoid.

The old `m_quiesce` only called `oa_audio_owner_quiesce`; the `audio` service
stayed published, `plat` stayed held, and `m_stop` joined the owner on the
loop.

## Gate

`m_quiesce` now:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears publication/availability, unpublishes `audio` and releases `plat`
   synchronously, so new lookups get `ENODEV` and a stale iface sees
   `enabled=0`;
3. detaches the owner and hands it to a cleanup worker;
4. the worker sleeps for `audio.cleanup_delay_ms`, calls
   `oa_audio_owner_quiesce`, polls `oa_audio_owner_join` (EBUSY until the
   owner observes shutdown), then destroys the owner.

The worker makes no host API calls. `m_stop` joins a leftover cleanup worker
and finishes any owner that was not detached; `m_destroy` joins/frees too.
`m_start` refuses with `-EBUSY` if an owner or cleanup worker is still live.
The owner's `settled` callback is suppressed while `quiescing`, so it cannot
post into a module that is being stopped. `m_suspend` keeps its existing
policy: enabled HAL audio refuses suspend; a disabled idle owner is joined
synchronously.

## Control probes

```text
audio-state             query audio enabled/volume
audio-volume <0-100>    set diagnostic audio volume
```

They hold `audio` while using the iface and release it before replying.

## Verification

`check-audio-quiesce` stages `tests/audio-mods/mod_audio.so` (built with the
HAL stub) and `mod_plat.so`, with `audio.cleanup_delay_ms=400`:

```text
load/start plat, audio
audio-state -> audio enabled 1 volume -1
audio-volume 42 -> ok
audio-state -> audio enabled 1 volume 42
reload audio -> failed (fast; 1 ms in this run)
status audio -> responsive (< 1 s)
audio-state -> failed errno 19           (service unpublished)
wait 600 ms
reload audio -> ok
audio-state -> audio enabled 1 volume 42 (saved volume restored and reapplied)
stop audio -> failed; audio-state -> failed errno 19
wait 600 ms; stop audio -> ok; unload audio -> ok; quit -> bye
```

```text
ok audio quiesce: reload fails fast (1ms), volume 42 restored and reapplied, retry completes
```

Existing audio tests still pass:

```text
ok state handoff reapplies volume/mute; invalid snapshots reject atomically
ok missing session I/O fails before a socket; enabled HAL refuses suspend; idle owner joins
ok audio worker rejects a real socket, runs gain off the caller, and closes on quiesce
```

The ARM module links the vendor libraries and `libpthread`:

```text
build-tv-v3/modules/mod_audio.so
  NEEDED libNDAL3.so, libNDAL_EX.so, libasound.so.2, libv4l2.so.0,
         libv4lconvert.so.0, libnteec.so, libmhash.so.2,
         libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok audio quiesce: reload fails fast (1ms), volume 42 restored and reapplied, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The test HAL stub returns success; this is not audible output, amplifier
  routing, DSP/PQ or real ALSA acceptance.
- The cleanup worker can wait indefinitely if the owner is blocked in a
  client RPC. The loop stays responsive and the host keeps the module in
  `cleanup_pending`, but the owner cannot be force-interrupted.
- `m_suspend` still joins a disabled idle owner synchronously; only stop/reload
  use the worker path.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu` and now `audio` have the async gate; the remaining modules still
  stop synchronously.
- No TV or stock-Aurora coexistence test was performed.
