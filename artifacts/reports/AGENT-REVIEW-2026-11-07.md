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
`7cd655f6ac40082c6490954e51dedd7f0958980a3268c7974c3bdac3685be9cb`.

## Open, severity-ranked

- **HIGH CEC suspend/resume** (`modules/cec/cec.c`): `suspend` joins/tears
  down the owner on the host thread and does not clear `session_open`, so
  `resume` does not re-acquire and sends can hit a fresh unopened device.
  `finish_owner_ptr` can also sleep/join on the loop. Move this to the existing
  cleanup worker and add a suspend-with-open/resume-acquire test.
- **HIGH amp mute/balance vs Init** (`modules/audio_amp/audio_amp.c`):
  `set_mute`/`set_balance` now reject while `submitted`, but still run the STBC
  level / I2C writes on the host loop. Submit them as owner jobs ordered after
  Init and do not let `poll_result` overwrite a later mute result.
- **HIGH IR pipe and stop** (`modules/ir/ir.c`, `include/openaurora/ir_blast.h`):
  a full 64-byte read with leftover bytes resets the stream and desyncs the
  fixed record; bound reads to the free space and kill/reap on real overflow.
  `struct oa_ir_blast_record` is 8 bytes on ARM (tail padding), not 6; the docs
  are wrong. `blast_helper_stop` can `waitpid(pid,0)` forever on the loop, and
  the child inherits `/dev/mem`, `/dev/ntgpio`, UART and other fds without
  `O_CLOEXEC`. Fix with bounded/nonblocking reap on a worker, `closefrom`/CLOEXEC,
  and a static assert for the record size.
- **MEDIUM amp Init retry/suspend** (`modules/audio_amp/audio_amp.c`):
  `iface_init` refuses while `have_result` is set, even after a failed Init;
  the module has no suspend/resume, so host standby leaves it started with the
  I2C session and last image/mute.
- **MEDIUM CEC reply queue** (`modules/cec/cec.c`): a failed reply send drops
  the whole queue; it can also send after close. Drop only the failed record,
  clear on release, never send when closed.
- **MEDIUM panel HAL errors** (`modules/panel/panel.c`): `Hal_Pwm_Init/Open/
  Set` results are ignored and `panel-state` reports the requested percent; a
  failed open is not retried. Propagate errors and keep the previous value.
- **MEDIUM tuner p4** (`common/nim_tune.c`): `demod[4] = p4 != 0` loses p4=2;
  assign the value and reject >2. Add a p4=2 test.
- **LOW CEC poll retry / labels / 50 ms sleeps**: no automatic acquire retry
  (only `cec-open`), `amp-state`'s `mute` token is the Init snapshot while
  `live` tracks later calls, and several quiesce gates nanosleep on the loop.

## Top gaps (unchanged)

Raw CEC still cannot open after stock starts; amp Init is register/GPIO
acceptance, not audible; IR has no physical press/repeat record; tuner has no
RF lock; panel has no measured brightness. Cold startup and broad UI parity
remain unproven.
