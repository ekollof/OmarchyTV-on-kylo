# Native boot recovery

`oa-supervise` is a separate process that launches openaurora and falls back to
`/vendor/com.nvt.idtv/aurora.elf` in the same boot. It uses the existing, verified
`wdtctl` protocol, never the partially initialised `NDAL_VideoExt_STBC_*` path.

The hook selects it only with `/conf/oa_boot` present, `/conf/oa_safe` absent,
`/conf/coredumpnoreset` present, and the binary, supervisor, watchdog tool and
configuration available. Installing these files does not enable takeover.
The browser opener is skipped for a native boot so it cannot issue DIAL keys into
the replacement UI. Fallback goes to the stock UI; it does not start that opener.

Two separate receipts are required:

| file | writer | meaning |
|---|---|---|
| `/tmp/openaurora.alive` | host dispatch loop, once per second | PID + `CLOCK_MONOTONIC` milliseconds; loop is progressing |
| `/tmp/openaurora.ready` | `ui_vendor`, once after `present()` returns success | PID + dimensions; a first frame was submitted |

Receipts are published with rename, checked against the child PID, and removed
before each attempt. They live on tmpfs to avoid writing flash every second.
Neither is evidence that the panel is scanning the painted buffer. Physical
scanout still needs a driver readback and confirmation at the screen.

Startup has 75 seconds to produce both receipts, covering the existing display
open timeout and platform initialisation. Afterwards a heartbeat older than
5 seconds triggers recovery. Module startup and callbacks may block, but cannot
keep the separate supervisor's deadline from expiring. `CLOCK_MONOTONIC` excludes
system suspend; wall-clock changes do not affect these deadlines.

Recovery kills and reaps the child, runs `wdtctl set 10`, then execs the vendor
UI. A watchdog helper gets a two-second deadline. `/conf/oa_fail` increments;
three failures remove `/conf/oa_boot`. A healthy attempt clears the streak after
30 seconds. Child exit, a signal to the supervisor, or creating `/conf/oa_safe`
also returns to the vendor UI. SIGTERM to openaurora requests orderly shutdown;
a blocked callback still needs the supervisor's SIGKILL.

This recovers userspace stalls and crashes. It cannot recover a kernel hang,
failure of the supervisor itself, or prove that a live UI still works correctly.
The MCU watchdog is disabled while the native child runs. Do not enable native
boot until scanout and input are verified.

`make check` includes recovery integration tests using a fake display and stub
watchdog/fallback executables. They never touch the TV's devices. The same ARM
binaries were also tested on the TV in `/tmp/oa-recovery-smoke-20261031` with a
fake display and stub watchdog/fallback: the heartbeat advanced, SIGSTOP made it
stale, and the child was killed and reaped before the stub fallback ran. The real
MCU watchdog, display, vendor process and boot markers were not changed by that test.

For a future trace, `OA_CHILD_PRELOAD` applies `LD_PRELOAD` only to the native
child; leave the supervisor, watchdog helper and vendor fallback untraced. The
existing tracer is filtered and bounded, so an absent ioctl is not proof that
it did not occur.
