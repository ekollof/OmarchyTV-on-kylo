# Live CPU-only module acceptance — 2026-11-05

First live run of the built ABI 3 ARM binaries on the actual TV under stock
Aurora. Scope: module load/start/reload/stop/unload and tuner→EPG composition
over the real Unix socket. **Not** display/fb1/HAL acceptance and **not** a
vendor takeover.

## Inputs and staging

- Host binaries cross-built into `openaurora/build-tv-v3`; modules are
  `mod_epg.so` and `mod_tuner.so` (no vendor-library imports).
- Uploaded one file per SSH connection into `/tmp/oa-live-20261105`, then
  hashed on the TV in a separate connection **before execution**; all four
  hashes matched the host files exactly (see `identities.txt`).
- Ran with an empty config and an isolated socket:
  `openaurora --modules /tmp/oa-live-20261105/modules --config empty.conf
  --sock /tmp/oa-live-20261105/control`.
- The staged binary has no `/dev/fb`, `/dev/v4l`, `/dev/i2c` or `/dev/cec`
  strings and imports only libc/libdl/libm/libpthread; both modules import only
  libc. No display module was loaded.

## Live results

All commands were sent through the new `tools/probe/oa-client` over the TV
Unix socket; full transcript in `live-session.txt`.

- `load/start epg` then `load/start tuner` succeeded.
- Idle tuner status reported zeroes and UNC unknown.
- `tuner-scan 1` advanced 25→50→75→100, then DONE with 12 services; lock went
  0→1, strength 30→80, quality 10→75, SNR 0.00→20.50, UNC unknown→0.
- `epg-count` reported **12**, proving the tuner's synthetic EIT sections
  reached `mod_epg` over the live service registry.
- `epg-query 101 ...` returned event 1001 with the expected window, metadata
  and name.
- `tuner-select 5` succeeded; `reload epg` cleared the cache (`count 0`);
  rescan/cancel and `reload tuner` reset state correctly.
- Stop/unload of both modules succeeded; `list` reported no modules loaded.

## Cleanup and stock state

- The exact staged PID `24665` was confirmed from `/proc/24665/cmdline`,
  terminated with SIGTERM, and disappeared; the control socket was removed.
- Stock `aurora.elf` PIDs were **1967 1785** before and after the run.
- Staging `/tmp/oa-live-20261105` was removed; see `cleanup.txt`.
- Read-only testtool state before the run: flag `00 00 00 00`, no TCP 1986
  listener; `/conf/coredumpnoreset` present.

## What this does and does not prove

Proven: the ARM ABI 3 core, real socket command handling, module
load/start/reload/stop/unload, service lookup handoff tuner→EPG and the
firmware-shaped tuner state machine all execute on the TV's glibc 2.23 ARM
userspace without touching stock Aurora's processes.

Not proven: fb1 ownership/restoration, display/EGL/scanout, HAL/tuner hardware,
vendor-backed EPG/PVR, independent cold startup, watchdog takeover, or
bug-for-bug parity. The run loaded no display or vendor-HAL module.

## Blocked owner decisions

1. **Testtool TCP 1986** is still disabled. Enabling it requires either a
   profile change plus a stock-Aurora restart or an in-process pause/call
   (gdbserver) while the vendor MCU watchdog is active. Both can perturb/stop
   stock Aurora and physical USB recovery is not currently available.
2. **fb1/display ownership and restoration** against stock Aurora requires
   either a coordinated stop of stock rendering or an agreed temporary
   display window; only temporary display tests are authorized, not a takeover.

No TV state was changed beyond the isolated staged process, which was removed.
