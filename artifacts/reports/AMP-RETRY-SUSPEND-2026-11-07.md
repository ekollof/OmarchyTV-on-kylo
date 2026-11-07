# Amp Init retry and suspend/resume — 2026-11-07

Scope: close the last MEDIUM review finding in `modules/audio_amp/audio_amp.c`.
Host lifecycle/result tests and ARM module build; no TV, I2C or audible run.

## Before

- `iface_init` rejected while `have_result` was set, even when the retained
  Init result was a failure (`0` = first write failed, negative = argument
  error). A failed Init could only be retried by reloading the module.
- The module had no `suspend`/`resume`, so host standby left it STARTED with
  the owner thread and the open `/dev/i2c-3` session.

## Init retry

`iface_init` now refuses only:

- while a job is pending (`-EBUSY`), or
- when the retained Init result is a success (`native_result == 1`).

A failed result may be resubmitted. The owner keeps the retained image exactly
as the vendor's repeated-Init contract does; the retry feeds that image back
into `oa_amp_init`. A successful result stays final for the module generation,
so a second Init still returns `-EBUSY`.

## Suspend/resume

The module now exports optional hooks:

- `suspend`: rejects only if a cleanup worker is running, then runs the existing
  `teardown_now` (unpublish `audio.amp`, release the plat lookup, quiesce/join/
  destroy the owner, close the I2C session).
- `resume`: calls `m_start`, which reloads the profile, reopens I2C, creates a
  fresh owner and republishes the service.

The amp registers are not read back, so the successful Init result and the
mute/balance caches are intentionally not carried across standby. The module
reports `result -1`/`known 0` after resume, and a new `amp-init` is required to
reapply a profile. This avoids inventing a register/standby policy; the stock
MCU/panel power behavior remains external (AMP-SHUTDOWN).

During suspend the owner drain may wait for an in-flight Init; the Init write
loop and delays are bounded. A failure keeps the module SUSPENDED and
retryable, as the host contract requires.

## Tests

`check-audio-amp-jobs`:

```text
ok audio_amp lifecycle: failed Init retries, successful Init stays final
ok audio_amp lifecycle: suspend releases the owner and resume republishes
```

- The fake write fails at register 3, so Init returns 0; `amp-state` reports
  that failure. Clearing the fault and calling `amp-init` again succeeds;
  a further Init returns `-EBUSY`.
- `oa_module_entry()->suspend/resume` are non-NULL. Suspend removes the owner
  and publication; resume republishes. A disabled instance follows the same
  lifecycle with no owner.
- Negative check: restoring `if (c->submitted || c->have_result)` fails the
  retry assertion.

All amp owner/vendor/image and audio_amp module suites pass.

## ARM

```text
1c9553a343775e53c7004d1c883fe04adf9a895f0451ae11404240de1abb49f3  build-tv-v3/modules/mod_audio_amp.so
```

## Limits

- No on-TV standby/resume or audible amp run; owner/I2C lifetime is host-tested
  only.
- Resume does not restore the previous Init image or mute/balance values; a
  new Init/profile application is required. This is explicit, not readback.
