# mod_audio_amp_profile async quiesce — 2026-11-06

This gate evidence is offline: host control socket with a synthetic hwprofile.
The TV was not used for the gate test. No amplifier, GPIO, MCU, I2C or audio
device was touched.

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

## Limits

- This is the immutable AD82587_D profile data and power string, not the
  amplifier, GPIO/MCU/I2C lifecycle or an audible gain path.
- The test uses a synthetic hwprofile; the real extracted `mb230_hwprofile.bin`
  remains covered by the existing module/IPC tests.
- The worker frees profile+power, but the gate is observed through
  unpublish/fail-fast/retry and the changed prescale, not freed memory.
- No TV, amplifier or hardware behavior is established by this report.
- Remaining synchronous modules (9): `panel`, `plat`, `power`, `ticker`,
  `ui_anim`, `ui_clock`, `ui_console`, `ui_diag`, `ui_omarchy`.
- No stock-Aurora coexistence test was performed.
