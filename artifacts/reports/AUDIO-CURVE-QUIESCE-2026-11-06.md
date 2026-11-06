# mod_audio_curve async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_audio_curve`, a synthetic 1668-byte AQ table and a diagnostic
`audio_curve.cleanup_delay_ms`. No audio device, DSP, amplifier or TV was
touched.

## Ownership change and gate

`audio_curve` used to keep the decoded 101-point `oa_audio_curve` inline. It
now allocates the table on the heap so the gate has a real object to detach
and free.

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published`/`ready` and unpublishes `audio.curve` synchronously, so
   `copy`/`value` reject;
3. detaches the decoded table;
4. frees it synchronously at `audio_curve.cleanup_delay_ms=0`, otherwise on a
   worker after the diagnostic delay and returns `-EAGAIN`.

`m_stop`/`m_destroy` join and free current/detached tables. `m_start` refuses
with `-EBUSY` if published or a worker is live, then rereads the configured AQ
file and explicit profile. The curve is derived data, so no handoff snapshot
is required: a failed start/resume retry rereads the file.

## Verification

`check-audio-curve-quiesce` starts with AQ base 100 and
`audio_curve.cleanup_delay_ms=400`:

```text
audio-curve 10 -> curve 110
audio-curve-dump -> curve 101 100 101 102 ...
reload audio_curve -> failed (fast; 1 ms in the full run)
status audio_curve -> responsive (< 1 s)
audio-curve 10 -> failed errno 19      (service unpublished)
rewrite AQ base to 200
wait 600 ms
reload audio_curve -> ok
audio-curve 10 -> curve 210            (replacement reread the changed table)
stop audio_curve -> failed; audio-curve -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok audio_curve quiesce: reload fails fast (1ms), table detached, retry completes
```

Existing curve behavior still passes:

```text
  ok   both 101-entry AQ tables: exact selection, unaligned little-endian loads and 32-to-16-bit narrowing
  ok   every truncated length / oversized inputs reject atomically; null guards and unsigned volume clamping
  ok   audio-curve 10/101 and audio-curve-dump return the published AQ table over the control command
  ok   real DSO/CLI start and reload reread AQ; published snapshots and caller copies remain immutable
  ok   real registry consumer hold blocks stop/unload/reload; release allows clean stop
  ok   publication collision preserves peer service; the same module can retry start
  ok   missing/invalid profile, absent/short/long file, directory and FIFO reject without publication; aggressive retry works
  ok   standby retains immutable data; suspended reload defers I/O/publication, failed activation retries on resume
  ok   failed replacement leaves no stale provider; explicit load recovers (host has no old-binary rollback)
ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_audio_curve.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok audio_curve quiesce: reload fails fast (1ms), table detached, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- These are DSP volume-table entries, not amplifier registers or audible gain.
  No audio routing or hardware acceptance is implied.
- Profile selection is still the explicit `audio_curve.profile` value; native
  AQ profile decoding/default fallback is not integrated.
- The test uses a synthetic AQ table; the offline decoder is separately tested
  against the real extracted table semantics.
- The worker frees the decoded table, but the gate is observed through
  unpublish/fail-fast/retry and the changed curve value, not freed memory.
- Remaining synchronous modules (10): `audio_amp_profile`, `panel`, `plat`,
  `power`, `ticker`, `ui_anim`, `ui_clock`, `ui_console`, `ui_diag`,
  `ui_omarchy`.
- No TV or stock-Aurora coexistence test was performed.
