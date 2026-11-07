# Adversarial review — 2026-11-07 hardware run and commits 6791a75..c0c18a9

Read-only. No repository writes, no builds, no TV access. HEAD is `6791a75`. Uncommitted diff is `control.c` (`panel` command and help split), `ir.c` (`ioctl(FIOCLEX)`), `tests/panel_quiesce.cjs`, and the HW-ACCEPT doc edits. Inspected source plus existing `build-tv-v3` binaries with `readelf`/`sha256sum` only.

Help literals in `oa_control_command` measure 668, 2884 and 1332 bytes, all under the C99 4095 minimum. `CONTROL_REPLY_MAX` is 16384. The split is not a truncation bug.

Shipped hashes match the report: `openaurora` `f41a2055…`, `mod_ir.so` `169945fa…` (`ioctl@GLIBC_2.4`, `pipe2@GLIBC_2.9`, no `GLIBC_2.28`), `oa-ir-blast` `36ef3665…`, `mod_audio_amp.so` `1c9553a3…`, `mod_panel.so` `46a3c3ff…`, `mod_cec.so` `5083aee9…`, `mod_tuner.so` `1896d578…`, `mod_plat.so` `81f96839…`. `readelf -V` on those, the core, the helper and `artifacts/analysis/amp-audible-2026-11-07/tone` shows no `GLIBC_` newer than 2.23. `tone` is ARM EABI, `sin@GLIBC_2.4`, NEEDED `libasound.so.2`.

## Findings

### HIGH — Audible-amp acceptance treats an out-of-band PCM tone as proof of Init → Mute(1) → Mute(0)

`artifacts/analysis/amp-audible-2026-11-07/tone.c:26-30`, `openaurora/modules/audio_amp/audio_amp.c:118-131`, `openaurora/common/amp_owner.c:29-33`, `openaurora/common/amp_init.c:3-6` and `:38`, `openaurora/common/amp_board.c:21-28`. Docs: `artifacts/reports/HW-ACCEPT-2026-11-07.md:53-64`, `FINDINGS.md:1-4`, `MEMORY.md:5-7`, `HANDOFF.md:12-13`, `artifacts/reports/README.md:16`, `TODO.md:202-206`.

The tone opens ALSA `"default"` and writes S16 stereo itself. It never calls `audio.amp`, never touches GPIO 208, and never reads an AD82587 register. `amp-init` result `1` is `oa_amp_init` returning 1 after the register write loop (`amp_init.c:38`). That function's own mute helper updates `mute_cached` only when `io->level` returns nonzero (`amp_init.c:5-6`). `amp-mute` is a later owner job that writes the mute GPIO and returns 0 when `level` is nonzero (`amp_owner.c:29-33`). `board_level` turns a zero STBC/GPIO return into boolean 1 (`amp_board.c:21-28`). The module then stores that request in `mute_state` (`audio_amp.c:129-131`). `amp-state`'s `live` field is that cache (`control.c:1343-1347`), not a pin or register readback. Init's `mute_cached` stays the Init snapshot, so "result 1 (mute 1)" and "live 0" can both be true with no second look at the hardware.

A tone while `live` stayed 1 was not recorded. Stock can leave the analog path already unmuted; a transport ACK that ignores the kernel device-result byte (AMP-STBC) still produces `live 0`. The speakers making a 440 Hz sound shows that some playback path was audible after the command sequence. It does not show that Mute(1) silenced that path or that Mute(0) opened it.

Fix: record one tone with mute left at 1 and one after mute 0, same PCM device, and a read that is not the request cache (STBC level with the device-status byte checked, or AD82587 register readback). Narrow HW-ACCEPT, FINDINGS, MEMORY, HANDOFF and the README row to "owner heard default-PCM 440 Hz after `amp-mute 0` reported live 0". Leave in-module readback and mute-gate acceptance open, as `TODO.md:205-206` already does.

### HIGH — "Press/repeat/release accepted" is not what the module emits

`openaurora/modules/ir/ir.c:291-298`, `openaurora/include/openaurora/events.h:15`, `openaurora/tests/ir_helper.c:111-136`. Docs: `HW-ACCEPT-2026-11-07.md:35-51`, `FINDINGS.md:4-7`, `MEMORY.md:7-9`, `HANDOFF.md:12-13`, `artifacts/reports/README.md:16` and the IR-HELPER index row. The accurate text is `TODO.md:270-276` and `artifacts/reports/IR-HELPER-2026-11-07.md` limits.

`OA_KEY_REPEAT` exists and has no emitter in the tree (`grep OA_KEY_REPEAT` hits only the enum). BLAST handling is:

```c
emit_key(c, rc, r.flag ? OA_KEY_RELEASE : OA_KEY_PRESS);
```

`aux` is logged and then ignored. A held key recorded as `aux=01 flag=00` is a second `OA_KEY_PRESS`. `tests/ir_helper.c` builds records with `aux = 0x10+i` and asserts every `flag==0` event is `OA_KEY_PRESS`, so the suite forbids a repeat mapping. The parentheticals `(press)` / `(repeat)` / `(release)` in HW-ACCEPT are annotations; the module log format does not print those words, and the first quoted line omits `aux=` even though the format string always prints it.

`DEV_Ir_GetKey` does not synthesize the UART repeat byte (IR-BLAST-HELPER). Four keys produced press and release edges plus an `aux=01` record. That is not acceptance of the third event kind. `TODO.md` says this. FINDINGS, MEMORY, HANDOFF, HW-ACCEPT and the README say the opposite in the "Accepted" sentence.

Fix: change those five "accepted press/repeat/release" sentences to "press and release records for four keys; held keys arrived as `aux=01 flag=00` and are still emitted as `OA_KEY_PRESS`". After the firmware BLAST `aux` contract is recovered from instructions, emit `OA_KEY_REPEAT` only for that encoding and add a helper test whose `aux` is the repeat value, not `0x10+i` with a PRESS expectation.

### HIGH — `audio_amp` suspend joins the Init worker on the host loop

`openaurora/modules/audio_amp/audio_amp.c:520-528` calling `teardown_now` → `finish_owner` (`audio_amp.c:283-298`). Introduced by `6791a75`. The worker body is `oa_amp_init`, which does `io->delay_ms(..., 1000)` before the register writes (`amp_init.c:31`).

`m_quiesce` detaches the owner and returns `-EAGAIN` so the loop stays responsive (`audio_amp.c:449-506`). `m_suspend` does not. `finish_owner` sets `stopping` and `pthread_tryjoin_np`s in a sleep loop until the worker exits. The worker does not look at `stopping` until `run_job` returns (`amp_owner.c:45-59`). `oa_module_suspend_all` calls `suspend` on the host thread (`core/module.c:591`). A standby or `suspend` command during Init blocks command processing for the rest of that sequencer. The module comment at `audio_amp.c:4-5` says the host loop only submits and polls.

The new test (`tests/audio_amp_jobs.c` `test_suspend_resume`) suspends an idle owner and a disabled module. It never submits Init before suspend.

Fix: use the quiesce detach path from `m_suspend` (unpublish, hand the owner to the cleanup thread, return `-EAGAIN` / `-EBUSY` while it runs) and make resume wait until that worker has joined before `m_start`. Add a test that blocks the fake Init write and asserts `suspend` returns without waiting out the block.

### MEDIUM — The tuner tuple is not the stub, so "fixture scan only" is not shown

`openaurora/common/nim_stub.c:46-82`, `openaurora/modules/tuner/tuner.c:192-199`, `openaurora/tests/tuner_ipc.cjs:93-100`. Docs: `HW-ACCEPT-2026-11-07.md:84-90`, `FINDINGS.md:12-14`, `MEMORY.md:12-13`, `README.md:16`.

The stub's first `tuner-state` after `tuner-scan 1` is `progress 25 lock 0 services 3 strength 30 quality 10`. The next poll is `progress 50 lock 1 services 6 strength 80 quality 75`. There is no stub sample with `lock 1` and `strength 0 quality 0` at `services 3`. `services` and `progress` are not backend results: every non-analog scan does `progress += 25` and `services_found = progress * 12 / 100` (`tuner.c:197-199`). `lock` and `strength` come from the backend signal. `failed_step=5` is the `tuner.hw_init` log (`tuner.c:490-495`), which runs even when `tuner.backend` is still the stub, so that log does not identify the scan. If this bundle set `tuner.backend=hal`, `tuner-scan 1` calls `hal_start` / `set_channel` (`nim_hal.c:96-106`). That is a tune attempt. The report's "no RF/lock acceptance" conclusion is right. "Only the fixture scan ran" and "matching the documented synthetic scan" are not.

Fix: publish the bundle `tuner.backend` / `tuner.hw_init` lines and the raw `tuner-state` string. If it was `hal`, say "synthetic +25 counter over a HAL status of lock 1, strength 0" and point at TUNER-VENDOR-CHANNEL, which already says that lock bit is not RF acceptance. Do not cite the stub.

### MEDIUM — Resume turns an applied panel percent of 0 into 100

`openaurora/modules/panel/panel.c:308-318`. The `panel` command makes 0 a successful applied value (`panel.c:134-138`, `control.c:551-574`).

```c
int rc = apply(c, c->percent ? c->percent : 100);
```

`m_suspend` closes PWM and leaves `percent` and `have_percent` alone. After `panel 0`, `percent` is 0 and `have_percent` is 1. Resume then applies 100. `m_save` (`panel.c:322-328`) also writes `percent` when `have_percent` is still 0, so a failed initial apply is saved as the configured start and restored as if it had been applied.

The HW-ACCEPT / TODO / FINDINGS negative on brightness is accurate: `ok` plus a state update is vendor nonzero from `Hal_Pwm_Set_ByChannel`, and the owner saw no dim at 50 or 0. That wording should stay. It does not make the resume expression right.

Fix: if `have_percent`, resume and restore that percent, including 0. If not, keep the configured start and leave `have_percent` clear until `apply` returns 0. Test `panel`/suspend/resume at 0 on the fake HAL (duty 0 in, duty 0 out).

### MEDIUM — `panel <0-100>` runs the vendor PWM ioctl on the host loop

`openaurora/core/control.c:567` → `openaurora/modules/panel/panel.c:103-135`.

`set_backlight_percent` calls `Hal_Pwm_Init` / `Open` / `Set_ByChannel` before it returns. The control socket is served on the host thread (`control.c` `on_client`). Quiesce already refuses to close PWM on that thread (`panel.c:231-263`). The new command puts the set path back on it. A stuck `/dev/ntkled` ioctl stops every other command, including `quit`.

`tests/panel_quiesce.cjs:91-95` checks host `ENOTSUP` (95), `EINVAL` for `panel 101` and a bare `panel`, and that a rejected set leaves percent 40. It does not cover 0, 100, or a slow HAL.

Fix: submit the set to the existing panel worker (or a one-shot thread), return `ok` only from the completion poll, and keep `have_percent` updates on that completion. Until then, do not treat a fast `ok` on the TV as evidence the ioctl is safe on the loop.

### MEDIUM — The forked IR child still calls `snprintf` after `fork`

`openaurora/modules/ir/ir.c:424-435`. The uncommitted comment at `236-237` says the child uses only async-signal-safe calls. `mark_inherited_fds_cloexec` does (`sysconf` + `ioctl(FIOCLEX)`). The child then formats `ps` with `snprintf` and calls `execl`. The host is multi-threaded (amp, CEC, panel, IR cleanup). `snprintf` can take the malloc lock. A holder in another thread at `fork` deadlocks the child; the parent then keeps a live helper until the bounded SIGKILL reap, which cannot reap `D`-state.

`FIOCLEX` itself is the right glibc-2.23 substitute. `mod_ir.so` `169945fa…` has `ioctl@GLIBC_2.4` and no `fcntl@GLIBC_2.28`. `tests/ir_helper.c:160-168` does call the real sweep and checks `FD_CLOEXEC`. Stdout is dup2'd before the sweep, so the pipe is not marked close-on-exec. That part held on the TV: the helper produced records.

Fix: format the protocol and `--set` strings in the parent and pass pointer arguments only. In the child, `dup2`, the `FIOCLEX` sweep, `execve`, `_exit`.

### LOW — A failed CEC reply submit is discarded, and the comment says it is retried

`openaurora/modules/cec/cec.c:312-323` (`1be8fe4`). `handle_completion` for `OA_CEC_ASYNC_SEND` (`cec.c:235-245`) does not put a failed TX back on the queue either. The head was already consumed when `send_async` returned 0.

`submit_async` returns `EBUSY` when `async_busy` or `job` is set (`cec_owner.c:241`). This poll only sends when `job_pending` is 0, so `EBUSY` is uncommon, but the failure path still advances `reply_head`. The comment says later replies "are retried". The failed head is not. `tests/cec_reply_queue.c` asserts the drop. A Give-Device-Power-Status style reply is then gone.

Fix: on `EBUSY`/`EINTR`, leave `reply_head` where it is. On a hard encode error, drop that one record. On TX completion failure, requeue that message once or surface it as an explicit lost reply. Change the comment to match.

### LOW — This run's CEC wording is stronger than the lines it quotes

`HW-ACCEPT-2026-11-07.md:92-97`, `FINDINGS.md:11-12`. Code: `cec.c:123-128`, `cec.c:222-232`, `cec_device.c:47-57`, `control.c:1574-1594`.

`cec-open` returns `ok` when `oa_cec_owner_acquire_async` queues. `EPERM` is stored later in `last_error` from `open` or the `CEC_OPEN` ioctl (`0x401c4301`), then printed as `error 1` with `open 0 rx 0 tx 0`. That matches "no TX/RX". The report says `cec-open` yielded `EPERM` and that the stock `KER_CEC` thread surviving "confirms" the blocker. This file does not quote `kcec: CEC Try lock fail` or a thread listing; those are in `CEC-RAW-BACKEND-2026-11-07.md:70-83` from the earlier attempt. `EPERM` from `open()` of the new node (mode/uid) is the same errno as the driver's lock failure.

Fix: quote this run's `cec-state` line and say the async acquire finished with errno 1. Repeat the kernel line or `/proc` thread only if it was captured again.

### LOW — Balance can update register 4 and still report failure

`openaurora/common/amp_owner.c:35-41` (`5ba0bb5`). `io->write` of register 4 succeeds, register 5 fails, the job returns `-EIO`. `audio_amp.c:142-155` does not set `balance_known`. The chip and the cache disagree. A later successful job overwrites both, but a single failure leaves the previous balance half-applied.

Fix: write both bytes in one I2C transaction, or write reg5 first and restore the previous reg4 if the second write fails. Test the second-write failure.

## Not filed

- Panel brightness docs match the owner negative. `068a6ef` keeps the last successful percent when `Set_ByChannel` returns 0. The TV `ok` is the nonzero vendor return, which that commit already says is not readback.
- `8c87e5f` stores `p4` in `demod[4]` and rejects `p4 > 2` (`nim_tune.c:222-236`). `tests/nim_tune.c:320` requires `p4==2` to stay 2. No regression seen in that diff.
- `1be8fe4` no longer wipes the whole reply queue on one submit failure, and it does not send while `session_open` is 0. The remaining hole is the LOW above.
- `c0c18a9` bounded read and `WNOHANG` reap are still in `ir.c:266-281` and `246-263`. The FIOCLEX swap did not reintroduce `fcntl@GLIBC_2.28` in the built `mod_ir.so`.
- Tuner and CEC conclusions that there was no RF acceptance and no frame TX/RX are consistent with `open 0 / rx 0 / tx 0` and with strength 0. The defects are the fixture label and the kernel-thread sentence.

## Acceptance blockers

1. Backlight. PWM `Hal_Pwm_Set_ByChannel` success does not dim this panel. The `DEV_PANEL` / MCU / `/dev/ntkled` path is still unrecovered. `panel 0` must survive suspend (`panel.c:313`) before any later brightness run.
2. CEC raw open. Acquire still ends at errno 1 with `open 0`. No TX/RX until the `KER_CEC` owner is actually released by a recovered close, not by another `/tmp/ntkcec` node.
3. IR repeat. Held-key `aux=01 flag=00` is a second PRESS. `OA_KEY_REPEAT` is unimplemented. OK was not captured. Do not check off the IR item.
4. Amp. Speaker PCM was heard from a separate ALSA process. Mute gating, register readback, volume, balance and standby were not shown. `m_suspend` must get off the host loop before a standby run.
5. Tuner. No RF source. The published `lock 1 / strength 0 / services 3` sample is not `nim_stub`. Do not treat it as a fixture pass, and do not treat `lock 1` as demod lock.
6. DVR, cold start before stock, and UI/display parity were not in this bundle.
