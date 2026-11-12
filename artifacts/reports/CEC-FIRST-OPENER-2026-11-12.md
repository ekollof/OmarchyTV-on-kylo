# CEC first-opener boot and recovery — 2026-11-12

Status: current
Last verified: 2026-11-12
Evidence class: on-TV controlled boot plus physical USB recovery.
Supersedes: the "first-opener boot not tested" limit in
[CEC-RAW-ACQUIRE-2026-11-11](CEC-RAW-ACQUIRE-2026-11-11.md) for the acquire
step only. The lock finding in that report still stands.

## Purpose

CEC-RAW-ACQUIRE and CEC-RAW-BACKEND established that the exclusive `KER_CEC`
lock survives a stock Aurora stop, so the documented path is an early
boot-order first opener. This test ran the raw `mod_cec` before stock Aurora
through a temporary, reversible hook branch.

## Setup

- Original hook `/conf/remotelf/aurora.elf` 8753 bytes, sha256
  `c3943712…`, md5 `0b1cd32a…`; backed up as
  `/conf/remotelf/aurora.elf.pre-cec-20261112`.
- Temporary hook 9099 bytes, sha256 `dea27cad…`; it ran
  `timeout -t 25 sh /buffer/oa-cec-first/run-cec-first.sh --boot` before the
  native/stock decision. `/conf/oa_safe` remained present; `oa_boot` stayed
  absent.
- Staged `/buffer/oa-cec-first`: `openaurora` `b1ee4832…`, `mod_cec.so`
  `4ab2e9d3…`, `oa-client` `f3d20a61…`, `oa-wdtfeed` `206c1ab5…`,
  `cec.conf` `0d97b3d5…`, script `eaf3c4c3…` (later retry script
  `6a33ea6c…`).
- The script started the MCU feeder in boot mode, started `openaurora` with
  `autoload=cec`, `cec.enable=1`, `cec.device=/tmp/ntkcec`, waited for
  `cec-state open 1`, sent `0883`/`0846`/`088f`, logged, quit, stopped the
  feeder and removed `/tmp/ntkcec`.
- A manual rehearsal on the running stock owner (lock held) failed with
  `open 0 error 1`, sent ENODEV and exited cleanly with no leftovers.

## Boot 1 result

After `reboot -f`, the hook branch ran before stock Aurora. The log:

```text
cec raw transport open (/tmp/ntkcec)
cec owner 1 audio 1 supported 1 local 1 enabled 1
     opening 0 open 1 rx 0 tx 0 error 0 last 0
failed errno 16
failed errno 16
failed errno 16
```

This proves the first-opener acquire path: the raw `ntkcec` open succeeded
before stock Aurora, with no `CEC Try lock` failure. The three identity sends
returned `EBUSY` because the owner has a single job slot and the 100 ms
receive poll occupied it. No CEC frame was transmitted or received.

The script continued to stock: appmgr 1581, `run_aurora.sh` 1694,
`aurora.elf` 1781 (zombie 2071).

## Failed retry and recovery

A second boot used bounded `cec-send` retries (`run-cec-first.sh`
`6a33ea6c…`). The TV became unreachable; `/buffer/oa-cec-first/cec.log` was
0 bytes. The likely hang is the hook's `timeout`/trap `wait` path around a
blocked CEC close, or the script's cleanup; it is **not proven** and is not a
CEC module/driver defect claim.

Recovery was required. The USB stick already contained the tested pair but was
cluttered and its FAT was dirty/corrupt. It was rebuilt as a clean FAT32
`TVHACK` partition containing only:

```text
kylo_usb_update.scr   584 bytes       MD5 de1591298d633cf6422a1ade4e8835a9
apdat0_full.ubifs     7,872,512 bytes MD5 994da7ff76a0b2fbafb8f6fa0c682627
```

Holding OK while reconnecting mains restored the tested `/conf` image. The
old small logs/scripts from the stick were backed up on the host at
`/tmp/opencode/tvhack-usb-backup-20261112`; the canonical NAND dump remains
`artifacts/dumps/tv_nand.bin`.

After recovery:

```text
uptime 1 min
appmgr 1549, run_aurora.sh 1709, aurora.elf 1758 (zombie 1911),
dialserver 1824
hook 8753 bytes c3943712…, no CEC-first branch
/conf/oa_boot absent; /conf/oa_safe present
/buffer/oa-cec-first removed; no openaurora/oa-wdtfeed; /tmp/ntkcec absent
```

## What this establishes

- A first-opener boot really is sufficient to acquire the raw CEC device;
  the lock is free before stock Aurora opens it.
- The raw module opens `/tmp/ntkcec` with the authority mirror in that boot
  position.
- The single-job owner returns `EBUSY` for a send while receive polling is in
  flight; the control path needs a queue/wait, not a one-shot retry loop.
- The boot harness needs a hard-kill timeout, no blocking `wait` in its trap,
  feeder stop on every exit, and an unconditional fallback to stock.

## Limits

- No CEC frame was sent or received; the Deck's `84` reply was not captured.
- The exact boot-2 hang cause is unproven.
- USB recovery was required; the stick was rebuilt. The recovery image is the
  tested `/conf` image, not a pristine no-root revert.
- `cec.enable` remains 0 in every shipped profile.

## Next

1. Fix the CEC owner send path to queue or wait for its single job slot;
   add a regression that sends while receive polling is in flight.
2. Harden the first-opener harness (`timeout -s KILL`, no `wait` in the trap,
   unconditional feeder stop and stock fallback, persistent log).
3. Re-run `0883` and capture the Deck's `Report Physical Address` reply.
