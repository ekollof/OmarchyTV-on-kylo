# Adversarial review 2 and fixes — 2026-11-07

Source: Grok CLI in Herdr tab `wA:t3` (`grok-review`), read-only against
`/home/ekollof/Work/tvhack` after the first hardware run. Full transcript:
`artifacts/analysis/agent-review-2026-11-07/grok-adversarial-review-2.md`.
No TV access by the reviewer.

## Confirmed and fixed

### HIGH — audible-amp acceptance lacked the muted half (retracted)

The review correctly noted that the 440 Hz ALSA probe never proved the amp
mute gate. A focused on-TV A/B (bundle `/buffer/oa-amp-20261107`,
`mod_audio_amp` `75bbea2e...`, tar
`8c326c63053d05ccafb01d8d699c831ecbf88e1cc5c0ebc73aa35e8268614be0`) ran
`amp-init` (result 1, mute 1): the owner still heard the tone. After
`amp-mute 0` (`live 0`) it was audible again. The mute state does not gate the
default PCM path; audible-amp acceptance is retracted in HW-ACCEPT/FINDINGS/
MEMORY/HANDOFF/README/TODO. Register readback, mute gating, volume/balance and
standby remain open.

### HIGH — IR “repeat” was an annotation, not an emitted event (corrected)

The module emits `flag ? RELEASE : PRESS` and ignores `aux`; `OA_KEY_REPEAT`
has no emitter. Firmware `_dev_uart_parse_rc_key` @ `0x0085d7dc` passes the
BLAST aux pointer but reads only `flag` (`+0x33`) to select `0x50`/`0x5e`; aux
is not used afterward. The docs now say “press/release records for four keys;
held keys arrive as `aux=01 flag=00` and are emitted as PRESS; repeat remains
unimplemented.” The IR TODO item stays open.

### HIGH — amp suspend joined a running Init on the host loop (fixed)

`m_suspend` now returns `-EBUSY` immediately when `c->submitted` is set or a
cleanup worker runs, without touching the owner. An idle owner is the only
path that calls `teardown_now`; the join is then short. `check-audio-amp-jobs`
adds a blocked-Init case asserting the rejection returns in <100 ms with the
owner/publication untouched; after collecting the result, suspend succeeds.

The reviewer also noted the host-level mixed state: modules earlier in the
reverse suspend walk may already be `SUSPENDED` while audio_amp stays
`STARTED`; a retry after `amp-state` skips the already-suspended ones. This
host behavior is documented as a limit; no cleanup worker was added because
standby is allowed to fail/retry rather than block.

### MEDIUM — panel 0 could resume as 100, and unknown saved as start (fixed)

`m_resume` now calls `apply(c, c->percent)`, so an applied 0 stays 0. A valid
handed-over percent sets `restored`; `m_start` honors it (including 0) instead
of overwriting with `panel.start`, and clears the flag after the apply.
`m_save` writes `0xFF` when no set has succeeded; `m_restore` treats `0xFF` as
unknown (does not store 255/0 as a percent) and rejects >100. Tests cover
0 suspend/resume, restored-0 across a started reload, and the 0xFF unknown.

### MEDIUM — tuner was called a fixture scan (corrected)

The run used `tuner.hw_init=1` and `tuner.backend=hal`. The observed
`progress 25 / services 3` is the module's +25 counter, not the `nim_stub`
tuple; the HAL signal was lock 1 / strength 0. The report now says “HAL
attempt, synthetic progress, no RF acceptance” and does not cite the stub.

### MEDIUM — IR child called `snprintf` after fork (fixed)

`proto_arg`/`set_arg` are formatted in the parent before `fork`; the child does
`dup2`, the `FIOCLEX` sweep, `execl` and `_exit` only. `check-ir-blast-worker`
passes with the real helper, proving the exec arguments still work.

### LOW — CEC reply failure comment (corrected)

The failed record is dropped (the earlier review asked for exactly that); later
records are attempted on the next poll. The comment now says so. EBUSY/EINTR
requeueing remains an option if a stronger delivery guarantee is wanted.

### LOW — CEC wording (corrected)

The HW report quotes this run's `cec-state` (`open 0 rx 0 tx 0 error 1`) and
says the acquire finished errno 1. The `KER_CEC` thread listing remains the
earlier CEC-RAW-BACKEND evidence; this run did not recapture it.

## Reviewed but not changed

- Balance still writes reg4 then reg5 and can leave a partial write on reg5
  failure. That is the recovered vendor order; the cache is not updated. A
  transactional write would deviate from the vendor.
- `panel <0-100>` still runs the vendor PWM ioctl on the host loop. This is a
  documented MEDIUM; the direct path is already negative on hardware and the
  command is a diagnostic, not a production UI path. Moving it to a worker is
  a follow-up.
- GLIBC audit: all `build-tv-v3` artifacts (core, helper, client, modules) have
  a maximum required `GLIBC_` of 2.23 or older. `mod_ir` is 2.4/2.9.

## Tests / hashes

Focused suites pass: `check-audio-amp-jobs`, `check-panel-curve`,
`check-panel-quiesce`, `check-ir-helper`, `check-ir-blast-worker`. Final ARM
modules: `mod_audio_amp` `75bbea2e...`, `mod_panel` `239e76b0...`, `mod_ir`
`c7bcbdeb...`, `mod_cec` `5083aee9...` (comment-only change), core
`f41a2055...`.

## Remaining blockers

Backlight path, CEC raw ownership, IR repeat event/OK, amp mute gate/readback/
volume/standby, RF tune/lock, DVR, cold start and UI parity.
