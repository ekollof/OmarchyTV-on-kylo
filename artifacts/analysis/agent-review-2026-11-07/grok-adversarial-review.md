# Adversarial review — working tree, 2026-11-07

Read-only. Repository not modified. TV `192.168.178.87` not contacted. No `git checkout`/`reset`.

Scope: `git status`, `git diff`, `git diff --cached`, `git log --oneline -15`, and the CEC/amp/IR/tuner/panel code those commits and the uncommitted close-path files touch.

Tree: branch `main`, even with `origin/main`. Unstaged: `artifacts/reports/CEC-RAW-BACKEND-2026-11-07.md`. Untracked: `openaurora/tools/cec/stock-close.c`, `artifacts/analysis/cec-transport-2026-11-07/close-probe-restore.log`. `artifacts/analysis/cec-close-probe.txt` is cited by the report and is not in the tree. No staged diff. FINDINGS/TODO/MEMORY/HANDOFF close-path text is already in `1ebb816`, not in the unstaged diff.

`cec-send` hex validation itself (even length, hex digits, `oa_cec_decode`, initiator 0) matches `transport_send`. No telemetry/ad/ACR sink showed up in `modules/cec`, `modules/audio_amp`, or `modules/ir`. The AD82587 baseline bytes in `common/audio_amp.c` FNV-1a-32 to `0xab635b86` (38 bytes). That part of the amp claim holds. `oa_amp_init` configures that image in place (`oa_ad82587_configure(s->image, s->image, p)`).

## CRITICAL

### 1. Failed `/dev/mem` map is treated as a live authority page

`openaurora/common/cec_authority_mem.c:14-17`, `openaurora/common/cec_authority_mem.c:85-95`, `openaurora/common/cec_authority_mem.c:40-41`

`sys_map` returns `mmap64` unchanged. `mmap` failure is `MAP_FAILED` (`(void *)-1`), which is not NULL. `oa_cec_authority_mem_open` only rejects a NULL map, then stores that pointer, publishes `read_mirror`, and returns 0. The injected tests return NULL on failure (`tests/cec_authority_mem.c`), so they never hit the system path.

`read_mirror` then `memcpy`s from `(char *)MAP_FAILED + 0x200`. That is a `SIGSEGV` in the CEC owner thread. A faulted `openaurora` is the `core_helper.sh` path: detach partitions and a one-second MCU reset unless `/conf/coredumpnoreset` catches it. `mod_cec` start has already returned success and published services before `begin_acquire` runs the read (`modules/cec/cec.c:441-481`).

The TV success probe (`mirror=000001ab`) does not exercise this branch. Open of `/dev/mem` can succeed and `mmap` still fail (permissions, range, `O_SYNC`).

Fix: in `sys_map`, return NULL and set `errno` when `mmap64` returns `MAP_FAILED`. Keep the existing `if (!map)` unwind. Add an injected test where `map` returns `MAP_FAILED` and assert `oa_cec_authority_mem_open` fails, retains no fd, and `oa_cec_authority_ensure` is not called.

## HIGH

### 2. CEC suspend/resume drops the device and leaves `session_open` set

`openaurora/modules/cec/cec.c:564-616`

`suspend` calls `finish_owner` on the host thread. On success that joins and `oa_cec_owner_destroy`s the worker (the worker's shutdown path also `oa_cec_device_close`s). `session_open`, `opening`, and `job_pending` are not cleared. `resume` creates a new owner and acquires only when all three are clear:

```c
if (!c->session_open && !c->opening && !c->job_pending)
    (void)begin_acquire(c);
```

After a suspend that actually closed the fd, `session_open` stays 1, so resume does not acquire. The 100 ms poll then calls `oa_cec_owner_receive_async` / `transport_send` against an owner whose `oa_cec_device` is `OA_CEC_DEVICE_INIT`. Sends fail at the device; the service still reports `open`.

`finish_owner_ptr` (`cec.c:367-380`) also `nanosleep`s up to 40×5 ms and then `pthread_join`s on that same host thread. `oa_cec_owner_quiesce` cannot cancel an ioctl already inside `CEC_TX` (`docs/ARCHITECTURE.md` and CEC-ASYNC). Standby therefore blocks the loop for the whole transmit/close. `module.h:139-148` requires the quiesce gate not to block; suspend is the path host standby actually calls (`core/module.c:574-592`), and this module bypasses its own cleanup thread there.

Fix: on a successful finish, set `session_open = opening = job_pending = closing = 0` and drop the reply queue before resume. Run the join on the existing cleanup worker; make `suspend` only detach and return `-EBUSY`/`-EAGAIN` until that worker finishes. Add a host test that suspends with `session_open` set, resumes, and asserts a new acquire is submitted.

### 3. Quiesce unmaps the authority page while the owner still borrows it

`openaurora/modules/cec/cec.c:394-402`, contract in `openaurora/include/openaurora/cec_owner.h:36-37`

`cleanup_worker` always `oa_cec_authority_mem_close`s after `finish_owner_ptr`, including when that returns `-EAGAIN` and the owner is stored back in `cleanup_owner`. Acquire copies the `oa_cec_authority_io` by value, but `read_mirror`'s context is the live `oa_cec_authority_mem` (`cec_authority_mem.c:33-41`, `cec_owner.c:274-275`). The borrowed mapping must stay until release or a successful quiesce+join.

`finish_owner_ptr` gives up after ~200 ms while a job is still in `perform` (`cec_owner.c:47-63` calls `oa_cec_authority_ensure` before the open ioctl). Unmap on the cleanup thread concurrent with that first touch is a fault in the owner thread, same MCU-reset consequence as finding 1.

Fix: unmap only after `finish_owner_ptr` returns 0. On `-EAGAIN`, leave `authority_open` set and the mapping intact for the retained owner. Test a blocked acquire across one timed-out quiesce and assert the mapping pointer is still non-NULL.

### 4. `amp-mute` races Init and can publish a lie

`openaurora/modules/audio_amp/audio_amp.c:109-122`, `openaurora/modules/audio_amp/audio_amp.c:130-149`, `openaurora/modules/audio_amp/audio_amp.c:175-189`, `openaurora/common/amp_init.c:21-38`, `openaurora/common/amp_vendor.c:92-102`

`iface_set_mute` runs `oa_amp_vendor_level` on the caller. Control commands run on the host loop (`core/control.c:1320-1331`). For mute pin 208 that is `NDAL_VideoExt_STBC_SetStbcGpio` in the loop. Init was deliberately moved off the loop because it blocks (the sequence includes `delay_ms(..., 1000)` at `amp_init.c:31` and ends in `mute(..., 1)`).

Nothing serializes `set_mute` with the owner worker. Both use the same `oa_amp_vendor` / I2C session / STBC global fd (`audio_amp.c:327-339`). `iface_init` does not reject mute while `submitted` is set.

`poll_result` overwrites module mute after the fact:

```c
c->mute_known = 1;
c->mute_state = completion.state.mute_cached ? 1 : 0;
```

Sequence: `amp-mute 0` returns ok and sets `mute_state = 0`; the worker's trailing `mute(s, io, m, 1)` then runs; the next `amp-state` takes the completion and forces `mute_state` back to 1. Or the opposite: mute 0 lands after the worker's last mute and before `take`, and `poll_result` still reports the snapshot. `amp-state`'s `mute` column is `mute_cached` from that snapshot (`core/control.c:1314-1317`) and does not follow later mute calls; only `live` does. A successful unmute therefore still prints `mute 1`.

`oa_amp_owner` has no mute job. `v_level` is not safe to call from two threads.

Fix: reject `set_mute` with `-EBUSY` while `submitted` is set. Submit mute as an owner job ordered after Init, and set `mute_state` only from that job's result. Do not let `poll_result` clobber a mute result that completed after Init. Keep `NDAL_VideoExt_STBC_SetStbcGpio` off the host thread.

### 5. IR pipe framing loses sync, and stop waits forever on the host thread

`openaurora/modules/ir/ir.c:231-256`, `openaurora/modules/ir/ir.c:282-310`, `openaurora/modules/ir/ir.c:541-561`, `openaurora/include/openaurora/ir_blast.h:7-11`

`blast_rxbuf` is 64 bytes. `blast_helper_read` reads up to 64 bytes. After a parse, `blast_rxn` is the leftover `0..sizeof(record)-1`. The next full read satisfies `blast_rxn + n > 64`, the code sets `blast_rxn = 0`, and copies the new bytes at offset 0. Those bytes start mid-record. The record has no magic or length. Every later `memcpy` into `oa_ir_blast_record` is shifted, so `code`/`flag` are wrong until the helper is restarted. A stalled loop (amp Init's 1 s delay, a blocking CEC ioctl, this function's own `waitpid`) is what fills the pipe.

`sizeof(struct oa_ir_blast_record)` is 8 on the ARM EABI (4-byte alignment tail padding), not 6. Writer and reader both use `sizeof`, so they match each other. FINDINGS.md:302, MEMORY/HANDOFF, and `artifacts/reports/IR-BLAST-HELPER-2026-11-07.md:30` say the helper writes 6-byte records. That claim is false. Two padding bytes are part of the stream.

`blast_helper_stop` is called from `m_quiesce` and from `suspend` (`ir.c:610`). It `nanosleep`s up to 100 ms on the host thread, then `waitpid(pid, &status, 0)` with no timeout. The helper's `NDAL_VideoExt_BLAST_RX` is the call that previously stuck `quit` because it does not wake on fd close (`IR-BLAST-HELPER-2026-11-07.md:10-14`). Moving it to a child does not make `waitpid` return if the ioctl is uninterruptible. The report's sentence that stop "cannot be held by `blast_io_get_key`" (`IR-BLAST-HELPER-2026-11-07.md:32-33`) is not what the code does.

The child never closes inherited fds (`tools/ir/oa-ir-blast.c:382-392`). `oa_cec_authority_mem_open` opens `/dev/mem` without `O_CLOEXEC` (`cec_authority_mem.c:80`). `oa_tuner_vendor` opens `/dev/ntgpio` without `O_CLOEXEC` (`tuner_vendor.c:116`). `open_port` opens the UART without `O_CLOEXEC` (`ir.c:131`). A helper that survives parent close keeps those descriptions.

Fix: read at most `sizeof blast_rxbuf - blast_rxn` bytes; on a real overflow, kill the helper rather than resynchronising by guessing. Put `_Static_assert(sizeof(struct oa_ir_blast_record) == 8)` in the header and correct the 6-byte sentences. Reap with `WNOHANG` on the loop and finish a stuck child on the cleanup worker, with a bounded kill. `closefrom(3)` in the child after `dup2`, and add `O_CLOEXEC` on `/dev/mem`, `/dev/ntgpio`, and the UART.

### 6. Close-path "no userspace trigger" is ahead of the evidence in the tree

`artifacts/reports/CEC-RAW-BACKEND-2026-11-07.md` (unstaged section starting "Close-path recovery"), `FINDINGS.md:288-293`, `TODO.md:210-215`, `openaurora/tools/cec/stock-close.c:38-40`

What the disassembly does support (`artifacts/workbench/aurora.dis`):

- `ICE_CECClose` at `0x806e44` only calls `UTILI_DBG_ChkPrtLevel` and `printf`, then returns. `~CECInterface` is a direct caller (`bl 806e44` at `0xa2d958`).
- `PLAT_CEC_Close` at `0x88bff4` loads the byte at `0x197a3b8` and, if nonzero, clears it and `blx`es `NDAL_VideoExt_CEC_Init` with `r0 = 0`.
- `objdump -d` contains no `bl 88bff4`. That is a direct-call result, not a proof about function pointers or other binaries.

What the tree does not support:

- `stock-close.c` only `dlopen`s, calls `NDAL_VideoExt_OpenDevice(0,10)`, then `NDAL_VideoExt_CEC_Init(NULL)`, and prints the two integers. It does not pass a config, read `/proc`, or check `dmesg`. It is not a Makefile target.
- The report states a second experiment, `cec_init(config)` then `cec_init(NULL)`, returned `cec_open=1 cec_close=1`, and that thread 2937 stayed in `S` with no CEC close in `dmesg`. `artifacts/analysis/cec-close-probe.txt` is not in the tree. `close-probe-restore.log` is a post-reboot stock snapshot (aurora 1773, `DRV_CEC_CALLBAC` 2930, nodes absent). A reboot always creates a new callback thread. It does not show the probe printf or a same-boot before/after.
- `open_device(0,10)=3` is printed and then described as having opened the video context. The return convention of `NDAL_VideoExt_OpenDevice` is not established in this probe. Vendor `cec_init` returning 1 is the same boolean NDAL already loses against the driver result byte (CEC-TRANSPORT-CONTRACT).
- "The kernel thread/semaphore is only cleared by a reboot" does not follow from one other-process `CEC_Init(NULL)` plus a missing direct `bl`. `PLAT_CEC_Close` is exported. Indirect callers were not rechecked for this review.

Fix: keep the stub and the direct-call statement; delete the config/close experiment and the "only a reboot" sentence until a log from the same boot shows the probe command, both return values, the thread id before and after, and `dmesg`. Do not treat `open_device=3` as success until the instruction-level return is cited. Do not check the TODO item or change the boot-hook plan on this report.

## MEDIUM

### 7. A failed or finished amp Init cannot be submitted again

`openaurora/modules/audio_amp/audio_amp.c:135-137`, `openaurora/common/amp_owner.c:61-68`

`iface_init` returns `-EBUSY` when `have_result` is set. `poll_result` sets `have_result` for every collected completion, including `native_result == 0` (a write failed, and `amp_init.c:34-35` then mutes). The owner itself would accept another submit once `ready` is cleared by `take`. The module gate does not. Recovery is reload, while the partial register image and mute GPIO/STBC state remain.

`audio_amp` has no `suspend`/`resume` (`audio_amp.c:438-449`). Host standby skips modules without `suspend` (`core/module.c:577`) and leaves this one `OA_ST_STARTED` with `/dev/i2c-3` open and the last mute/image applied.

Fix: allow a new submit when `submitted` is 0; clear `have_result` when the new ticket is accepted. Add `suspend` that runs the existing quiesce close, and `resume` that reopens I2C only under `audio_amp.enable`.

### 8. UART path never emits `OA_KEY_REPEAT`

`openaurora/modules/ir/ir.c:166-187`, `openaurora/include/openaurora/events.h:15`, `TODO.md:235-237`

The recovered UART parser marks a second `P` (`0x50`) with the same code as a repeat and `^` (`0x5e`) as release. `on_frame` emits `OA_KEY_PRESS` for every valid `0x50` and `OA_KEY_RELEASE` for `0x5e` using `last_cmd`. `OA_KEY_REPEAT` is never produced. A held key is a series of new presses. `46378ce` records that the BLAST flag stream is still unclassified; the UART path is not in that gap, and the code already classifies `0x50`/`0x5e`.

BLAST (`ir.c:255`) maps every `flag == 0` to press and every nonzero flag to release. That matches the documented "forward the raw flag" choice only if flag 1 is the sole release value. Any other nonzero flag becomes a release.

Fix: in `on_frame`, if `kind` would be press and `cmd == last_cmd` and no release has been seen since the last press, emit `OA_KEY_REPEAT`. Keep BLAST edge-only until a captured press/hold/release record stream is in the report. Add a `feed()` test with two identical `0x50` frames and one `0x5e`.

### 9. One failed CEC reply send drops the whole reply queue

`openaurora/modules/cec/cec.c:311-320`

On `oa_cec_owner_send_async` failure the poll sets `reply_head = reply_tail`. Audio-policy replies already removed from the policy (System Audio / ARC) are discarded, not retried, and the failure is only `last_error`. The queue also still sends after a successful close, because this block runs before the `session_open` check.

Fix: leave the head record in place on `-EBUSY`/`-ESHUTDOWN` is different: on a hard error, drop only that record and log it. Do not send when `session_open` is 0; clear the queue in the release-success arm of `handle_completion`.

### 10. Panel open/set failures are reported as a new backlight level

`openaurora/modules/panel/panel.c:102-126`

`apply` ignores `Hal_Pwm_Init`, `Hal_Pwm_Open`, and `Hal_Pwm_Set_ByChannel`. It sets `opened = 1` and `percent = pct` anyway. `iface_get` returns that percent. A failed open is never retried. `panel-state 50` in PANEL-BACKLIGHT-2026-11-07 is the requested value, which the report already says was not a measured brightness. The code makes that structural: the service cannot report failure.

Open mode is hardcoded to 1 (`panel.c:110`) while `panel.mode` defaults to 0 and is used only on set (`panel.c:122-183`). FINDINGS states that split. It is not an extra claim. It does mean a failed open still latches mode 1.

Fix: if open or set returns nonzero, leave `opened` unchanged, keep the previous percent, and return the HAL error. Retry open on the next set when `opened` is still 0.

### 11. DVB-T `HAL_Demod_Set` plan stores `p4 != 0`, not p4

`openaurora/common/nim_tune.c:232-237`, `FINDINGS.md:248-251`, `openaurora/include/openaurora/nim_tune.h:54`

FINDINGS says the demod block is `[4]=p4` with p4 from the record at `+0x14` (range 0..2 per the header). `oa_nim_raw_channel_setup_dvbt` assigns `demod[4] = rd32(rec->raw + 0x14) != 0`. Values 1 and 2 both become 1. The builder writes 0 (`nim_tune.c:203`), so the host tests and the no-RF run do not see it.

Fix: assign `out->demod[4] = rd32(rec->raw + 0x14)` after rejecting values above 2. Add a setup test with p4 = 2.

### 12. `stock-close` has no stock guard

`openaurora/tools/cec/stock-close.c:14-41`

Not built by the Makefile. If compiled and run, it `dlopen`s `libNDAL_EX`, calls `NDAL_VideoExt_OpenDevice(0,10)` (process-global video fd, PLAT-POWER-QUIESCE) and `NDAL_VideoExt_CEC_Init(NULL)` with no check that `aurora.elf` is stopped and no restore. The report's "run only with stock stopped" is a comment, not a check.

Fix: keep it out of `all`/`install`. If it stays, refuse to run unless a documented environment flag is set, and do not describe its return codes as a close.

## LOW

### 13. `amp-state` label `mute` stays at the Init snapshot

`openaurora/core/control.c:1314-1317`, `openaurora/include/openaurora/services.h:403-405`

The printed `mute` field is `mute_cached` from the Init completion. `amp-mute` updates `live`/`known` only. After the hardware unmute, a later `amp-state` still prints `mute 1` when Init ended in `Mute(1)`. The commit text mentions live/known; the token `mute` still reads as the hardware level.

Fix: print `mute_cached` and `mute` under those names.

### 14. CEC poll does not retry a failed acquire

`openaurora/modules/cec/cec.c:222-233`, `openaurora/modules/cec/cec.c:323-329`

Acquire failure clears `opening` and leaves `session_open` 0. The timer only receives when `session_open` is set. Nothing calls `begin_acquire` again. `cec-open` can. The "retries for ~10 s" sentence in the report is not this loop.

Fix: either document that `cec-open` is the only retry, or resubmit acquire on a capped backoff when `last_error` is `EPERM` and no job is pending.

### 15. Quiesce `nanosleep` on the host thread

`openaurora/modules/cec/cec.c:499-518`, `openaurora/modules/audio_amp/audio_amp.c:372-393`, `openaurora/modules/ir/ir.c:544-557`

Each gate sleeps up to about 50 ms on the loop while `tryjoin`ing. `module.h:147` says the gate must not block. Bounded, unlike findings 2 and 5.

Fix: `tryjoin` once and return `-EAGAIN`. Let the host timer call the gate again.

## Top gaps blocking the stated objective

The objective is a reloadable Aurora replacement that can take CEC, audio, IR, tuner, and panel, then restore stock. These are the blockers the code and the evidence actually leave:

1. Raw CEC still cannot open after stock has started. `KER_CEC` stays held across `SIGTERM`. The new report does not prove a userspace close, and `cec.enable=1` will not get a frame until that owner is gone by a path that is still unauthorized (boot order) or unproven (shared VPL). Findings 1-3 mean a failed map or a standby/quiesce race can fault the process and pull the MCU reset.

2. Amp Init on hardware is a register/GPIO/STBC run, not audio. `amp-mute` is unsynchronized with that Init, can undo or be undone by the trailing `Mute(1)`, and reports success on the loop. There is no second Init, no suspend close, and no audible check. Stock restore for this milestone is still "reboot with `wdtctl`".

3. IR is not a remote yet. The helper can desynchronise its own pipe, stop can block the loop inside the same BLAST sleep the helper was meant to escape, repeat is not implemented on the UART path, and no physical press/repeat/release has been accepted. Default `ir.blast=1` takes `/dev/video28` whenever `mod_ir` starts.

4. Tuner and panel are not acceptance. The DVB-T path has no RF lock, and `demod[4]` is not p4. Panel reports the requested percent whether or not `Hal_Pwm_*` succeeded, and the open mode is still the hardcoded 1. Neither is a stock-restorable device owner.

5. No reviewed test covers `MAP_FAILED`, CEC suspend/resume, authority lifetime across `-EAGAIN`, `amp-mute` during Init, a short-then-full BLAST read, or `OA_KEY_REPEAT`. Existing host tests pass the injected NULL-map and the happy-path owner slot, which is why these paths are still green.
