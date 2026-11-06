# mod_audio_amp_profile async quiesce — 2026-11-06

The gate test is offline (host control socket with a synthetic hwprofile),
followed by a data-only ARM execution on the real TV against the TV's own
`/conf/mb230_hwprofile.bin`. No amplifier, GPIO, MCU, I2C or audio device was
touched, and no display/HAL module was loaded on the TV.

## Ownership change and gate

`audio_amp_profile` used to keep the decoded `oa_ad82587_profile` and its
32-byte `speaker_power` string inline. Both now live in one heap
`profile_data`, and `profile.speaker_power` points at that owned buffer.

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `audio.amp_profile` synchronously, so
   `copy` rejects with `ENODEV`;
3. detaches profile+power;
4. frees them synchronously at `audio_amp_profile.cleanup_delay_ms=0`,
   otherwise on a worker after the diagnostic delay and returns `-EAGAIN`.

`m_stop`/`m_destroy` join and free current/detached data. `m_start` refuses
with `-EBUSY` if published or a worker is live, then rereads the configured
hwprofile. The profile is derived data, so no handoff snapshot is needed.

## Verification

`check-amp-profile-quiesce` starts with prescale 17 and
`audio_amp_profile.cleanup_delay_ms=400`:

```text
amp-profile -> power '12Watt' prescale 17 ch1 192 ch2 192 fine 192 ...
reload audio_amp_profile -> failed (fast; 1 ms in the full run)
status audio_amp_profile -> responsive (< 1 s)
amp-profile -> failed errno 19       (service unpublished)
rewrite hwprofile to prescale 31
wait 600 ms
reload audio_amp_profile -> ok
amp-profile -> power '12Watt' prescale 31 ...
stop audio_amp_profile -> failed; amp-profile -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok audio_amp_profile quiesce: reload fails fast (1ms), profile detached, retry completes
```

Existing amplifier-profile behavior still passes:

```text
ok amp provider: DSO/query/reload, file snapshot and caller-owned name/value lifetime
ok amp provider: holds block lifecycle and removal callbacks cannot read a stopped snapshot
ok amp provider: publication collision, invalid/FIFO/oversized/malformed files and retry cleanup
ok amp provider: suspend snapshot, deferred reload/resume retry and failed replacement recovery
ok amp profile Unix IPC: saved target hwprofile passes through the real module
ok amp profile Unix IPC: query validation, immutable snapshot, reload/suspend/resume, failed replacement recovery and clean exit
ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_audio_amp_profile.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok audio_amp_profile quiesce: reload fails fast (0ms), profile detached, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## On-TV ARM and profile-identity check — 2026-11-06

The TV was woken from standby for this check; stock Aurora kept running
throughout.

```text
local artifacts/protocol/mb230_hwprofile.bin
  md5 1b063b2cc7faa8b92c0f843921e561c7, 24514 bytes
TV /conf/mb230_hwprofile.bin
  md5 1b063b2cc7faa8b92c0f843921e561c7, 24514 bytes
```

The ARM `build-tv-v3/openaurora` and `mod_audio_amp_profile.so` were copied to
`/tmp/oa-amp-verify`, hash-verified there, and run with only that module:

```text
openaurora ...                     554700 bytes  sha256 c03a68a1...
mod_audio_amp_profile.so ...        43504 bytes  sha256 cce8e2da...
```

Config used the real `/conf/mb230_hwprofile.bin` and
`audio_amp_profile.cleanup_delay_ms=400`. Stdin control sequence:

```text
load/start audio_amp_profile
amp-profile -> power '12Watt' prescale 27 ch1 5 ch2 5 fine 128 clip 405736 drc 98304 state -1 attack -1 release -1
reload audio_amp_profile -> failed (cleanup pending)
amp-profile -> failed errno 19
wait 0.7 s
reload audio_amp_profile -> ok
amp-profile -> ... prescale 27 ...        (ARM replacement reread the real profile)
stop audio_amp_profile -> failed (same gate)
wait 0.7 s
stop audio_amp_profile -> ok; unload -> ok; quit -> bye
```

Afterward the staging directory was removed, no `openaurora` helper remained,
and stock Aurora was still PID 1781. This proves the ARM module, real glibc,
real profile data and gate sequence on the TV. It does not prove amplifier
register writes or audible output.

## Limits

- This is the immutable AD82587_D profile data and power string, not the
  amplifier, GPIO/MCU/I2C lifecycle or an audible gain path.
- The test uses a synthetic hwprofile; the real extracted `mb230_hwprofile.bin`
  remains covered by the existing module/IPC tests.
- The worker frees profile+power, but the gate is observed through
  unpublish/fail-fast/retry and the changed prescale, not freed memory.
- The TV run proves data-only ARM/glibc module behavior and the gate sequence
  against the real profile; it does not exercise amplifier wiring, GPIO/MCU,
  register writes or audio output.
- Remaining synchronous modules (9): `panel`, `plat`, `power`, `ticker`,
  `ui_anim`, `ui_clock`, `ui_console`, `ui_diag`, `ui_omarchy`.
- No stock-Aurora coexistence test was performed.
