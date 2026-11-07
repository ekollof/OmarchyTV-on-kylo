# Adversarial agent review and fixes — 2026-11-07

Source: Grok CLI (`grok 1.0.77`) run in a Herdr tab (`wA:t2`), read-only
against `/home/ekollof/Work/tvhack`. Full transcript:
`artifacts/analysis/agent-review-2026-11-07/grok-adversarial-review.md`.
No TV contact by the reviewer.

## Fixed

### CRITICAL — MAP_FAILED treated as a valid authority page (commit `f323673`)

`sys_map` returned `mmap64` unchanged; `mmap` failure is `MAP_FAILED`
(`(void*)-1`), not NULL, and `oa_cec_authority_mem_open` only rejected NULL.
It stored the invalid pointer, published `read_mirror`, and returned success;
the CEC owner would then `memcpy` from `MAP_FAILED+0x200` and fault, reaching
the `core_helper`/MCU-reset path.

Fix: normalize `MAP_FAILED` to NULL in `sys_map`, reject `MAP_FAILED`
generically for injected providers, keep the fd-close/no-callback unwind.
Regression test feeds a `MAP_FAILED` map and asserts ENOMEM, closed fd,
cleared mapping and no published callbacks. `check-cec-authority-mem` passes.

### Close-path wording (review HIGH #6)

The CEC report now distinguishes what the disassembly supports (stub
`ICE_CECClose`, no direct internal `PLAT_CEC_Close` caller, NDAL close probe
did not stop the thread) from what is not proven (indirect callers/other
binaries, `open_device=3` return convention, the unused second probe). It no
longer claims "only a reboot can release it"; it says no tested userspace
trigger was found.

### HIGH — authority mapping lifetime across a timed-out quiesce (fixed)

`cleanup_worker` unmapped the `/dev/mem` authority page even when
`finish_owner_ptr` returned `-EAGAIN` and the owner was retained for a later
generation. Acquire copies the authority callbacks but borrows the mapping
context; a blocked acquire could touch the unmapped page. `stop`/`destroy` also
closed it while an owner was retained.

Fix: unmap only when `finish_owner_ptr` returns 0; `stop`/`destroy` keep the
mapping alive while `owner`/`cleanup_owner` remains. Regression
`check-cec-authority-lifetime` uses a fake host plus wrapped owner join/authority
open/close: the first quiesce generation is forced to `-EAGAIN` and must keep
`authority_open=1` and the sentinel mapping with zero close calls; after joins
are allowed, the next generation must unmap exactly once and reach 0. Reverting
the condition makes the test fail at `authority_open == 1`, confirming it
catches the defect. `mod_cec.so` rebuilds as sha256
`c8f4138cfe04d887c874fec099c7f11ba02d48ba3cb095db48462d23396c725a`.

### HIGH — CEC suspend/resume state and host-thread join (fixed)

`suspend` previously called `finish_owner` on the host thread, did not clear
`session_open`/`opening`/`job_pending`/`closing` or the reply queue, and
`resume` therefore skipped re-acquire because the stale session looked open.
It could also block the loop on the owner join.

Fix: suspend stops polling, clears the session state, detaches the owner to the
existing `cleanup_worker`, and waits (bounded) for that generation because this
host provides no retry for a module left SUSPENDED. Resume reaps the worker,
creates a fresh owner, reopens the authority page, restarts polling and submits
a new acquire. The `pthread_create` failure fallback now keeps a retained owner
on `-EAGAIN` instead of dropping the pointer. Regression
`check-cec-suspend-resume` (fake host + wrapped authority/device I/O and owner
join) completes an acquire, suspends, asserts the session/timer state is
cleared, the owner/authority/device were closed, and the blocking join ran on a
thread other than the caller; it then resumes and asserts a reopened authority
and the fresh acquire completing. Removing `clear_session_state()` makes it
fail at the suspend assertion. `mod_cec.so` sha256
`c8f4138cfe04d887c874fec099c7f11ba02d48ba3cb095db48462d23396c725a`.

### HIGH — IR helper pipe framing, CLOEXEC and bounded reap (fixed)

`blast_helper_read` read a full 64-byte chunk into a temporary buffer and reset
the assembly buffer when a pending partial plus that chunk overflowed it, which
desynced or dropped records. The header also understated the 8-byte ARM record,
the forked helper inherited host device/socket fds, and a SIGKILLed helper was
awaited with a blocking `waitpid`.

Fix: read only into the buffer's free space (64 is a multiple of the 8-byte
record); pin size/offsets with `_Static_assert` and zero the padding in the
writer; open the UART with `O_CLOEXEC` and mark every fd above stderr
close-on-exec in the forked child with an async-signal-safe `ioctl(FIOCLEX)`
sweep (3..`_SC_OPEN_MAX`, capped at 65536; `fcntl` was rejected by TV
preflight for a GLIBC_2.28 binding); reap
with `WNOHANG` only, bounded to 100 ms after SIGTERM and 100 ms after SIGKILL,
retaining the pid for a later retry if it still survives. Regression
`check-ir-helper` covers a 7+65-byte split, the 8-byte layout, the CLOEXEC
sweep, and a SIGTERM-ignoring child (`~113 ms`, then `ECHILD`). Restoring the
unbounded read fails `nevents == 9`; a no-op sweep fails the `FD_CLOEXEC`
assertion. ARM hashes: `mod_ir.so`
`169945faa8c65479ff6a4f266c79fcb870f02838d4b04bb1c400a886cade7275`,
`oa-ir-blast`
`36ef366586e7dc4a78d6b21d9fa2aed99421d9ea0e5dc79d2e7d6e71a98c3678`.

### HIGH — amp mute/balance on the host loop (fixed)

`set_mute`/`set_balance` rejected while Init was submitted but still ran the
STBC/GPIO level and I2C register writes on the host loop. The owner API now
has typed jobs (`INIT`/`MUTE`/`BALANCE`) on its existing single ordered worker
slot; `submit_mute`/`submit_balance` reject with `-EBUSY` while a job is
pending or its result is untaken, so they cannot overtake Init. Mute uses the
board mute record (`gpio(1)` + level_on/off), balance writes reg4 then reg5
through the same vendor boolean write callback. `poll_result` dispatches on the
kind: a later mute/balance completion updates only its cache and never
overwrites the retained Init result/ticket, and failures leave the cache
unchanged. `check-amp-owner-jobs` and `check-audio-amp-jobs` cover ordering,
worker-only callbacks, successful/failed results and result retention; allowing
a second submit while pending fails the ordering assert, and copying every
completion over the Init result fails the module state assert. ARM
`mod_audio_amp.so` sha256
`67d346529c443423074013ec7f2804fbd56f991fc9cb0c84c661b119e2a97153`.

### MEDIUM — tuner p4 collapsed to a boolean (fixed)

`oa_nim_raw_channel_setup_dvbt` wrote `demod[4] = p4 != 0`, losing the
recovered 0..2 operand. It now rejects p4>2 and copies the value. The
`check-tuner-backend` p4=2 case fails if the boolean form is restored. ARM
`mod_tuner.so` sha256
`1896d5786a5ad64a0397d78fe7816c2d14e28f0550501ac2d259c36fa261eb04`.

### MEDIUM — panel HAL errors ignored (fixed)

`apply` ignored `Hal_Pwm_Init/Open/Set`, marked the session open after a failed
Open, and reported the requested percent even when the write failed. The
recovered contract treats Open/Set 0 as a definite failure (1 is the vendor
success path) and Init 0 as "already initialized". Failed Open now closes any
cached fd and retries next time; failed Set keeps the last applied value;
`iface_get` returns -1 until a set succeeds. `check-panel-curve` covers failed
open/set, retry and failed-start reporting; ignoring the results fails the new
assertions. ARM `mod_panel.so` sha256
`46a3c3ff8ced3eac4876039442045d2c537c4f937e099a14e0d5af9f8d19d967`.

### MEDIUM — CEC reply queue dropped everything (fixed)

`cec_poll` sent queued policy replies without checking `session_open` and set
`reply_head = reply_tail` on a failed submission, discarding every later
reply. The send path is now gated on an open session, a failure advances only
the failed record, and a successful release clears the queue (failed release
keeps it). Regression `check-cec-reply-queue` covers a failed first send with a
later reply still delivered, no send/receive while closed, and release
clear/retain. Restoring the drop-all line fails the queue assertion. ARM
`mod_cec.so` sha256
`5083aee9aa75f87b12f4b32a3139a7c60cb29c65591f3531ba93b2ac06ffa185`.

### MEDIUM — amp Init retry and suspend/resume (fixed)

`iface_init` refused after any retained result, including a failed Init, and
the module had no suspend/resume, holding the owner/I2C session through
standby. It now refuses only a pending job or a successful Init
(`native_result == 1`); a failed result can be resubmitted with the owner's
retained image. `suspend` runs `teardown_now` (unpublish, plat release, owner
join/destroy, I2C close) and `resume` calls `m_start` to reopen and republish;
the Init result/caches are intentionally not carried across standby because
there is no register readback. `check-audio-amp-jobs` covers the failed-Init
retry, success finality and the descriptor suspend/resume hooks; restoring the
old refusal fails the retry assertion. ARM `mod_audio_amp.so` sha256
`1c9553a343775e53c7004d1c883fe04adf9a895f0451ae11404240de1abb49f3`.

## Open, severity-ranked

- **LOW CEC poll retry / labels / 50 ms sleeps**: no automatic acquire retry
  (only `cec-open`), `amp-state`'s `mute` token is the Init snapshot while
  `live` tracks later calls, and several quiesce gates nanosleep on the loop.

## Top gaps (unchanged)

Raw CEC still cannot open after stock starts; amp Init is register/GPIO
acceptance, not audible; IR has no physical press/repeat record; tuner has no
RF lock; panel has no measured brightness. Cold startup and broad UI parity
remain unproven.
