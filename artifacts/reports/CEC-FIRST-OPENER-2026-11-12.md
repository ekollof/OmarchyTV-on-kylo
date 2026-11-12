# CEC first-opener boot and recovery — 2026-11-12

Status: current
Last verified: 2026-11-12
Evidence class: on-TV controlled boot plus physical USB recovery.
Supersedes: the "first-opener boot not tested" limit in
[CEC-RAW-ACQUIRE-2026-11-11](CEC-RAW-ACQUIRE-2026-11-11.md) for the acquire
step only. The lock finding in that report still stands.
Corrects: the 2026-11-01 cache “LA8” reading in
[CEC-LIVE-2026-11-01](CEC-LIVE-2026-11-01.md) as a wire logical address; the
boot 4/5 wire test shows LA8 NACK and LA4 ACK.

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
- Retry staging for boots 3–5: `mod_cec.so` `f3337e29…` (TX queue),
  script `7d498a3f…` and hook `afbf1656…`. The hook used
  `timeout -t 40 -s KILL`; the script moved itself aside on first run, truncated
  `cec.log`, never called `quit`/`wait`, and SIGKILLed the module. After boot 5
  the original hook `c3943712…` was restored, `/buffer/oa-cec-first` removed,
  and a clean reboot verified stock `aurora.elf` 1773 and kernel thread
  `DRV_CEC_CALLBAC` 2855.

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

## Boots 3–5: queued TX, LA discovery and the -512 return

The send queue (commit `b9269fc`) removed the boot-1 `EBUSY` path. The retry
staging used module `f3337e29…`, hardened script `7d498a3f…` and hook
`afbf1656…`; the script has a one-shot guard, truncates its log, uses
`timeout -t 2 -s KILL` around each client call, never calls `quit`/`wait`,
SIGKILLs the module and leaves a self-terminating MCU feeder plus an independent
reaper.

### Boot 3: queue works, first transmit fails

`send 0883` returned `ok` (queued) and the module then logged
`cec tx failed (512)` with `tx 0`, `error 512`.

`0xfffffe00` is the driver's generic TX failure return from `KER_CEC_IocTl`
(Ghidra `0xd94e0`): the TX branch retries up to five completion waits.
`KER_CEC_InterrutHandler` (`0xd9064`) sets TX status 0 on INT bit 3 (success)
and 2 on INT bit 2 (failure), then completes. After five status-2 results the
ioctl returns -512. This is a bus NACK/failure, not an errno from `mod_cec` or
a packet/open error. Raw dumps: `../analysis/cec-first-2026-11-12/
nondovi-cec-isr.txt`, `nondovi-cec-tx.txt`, `nondovi-refs-aea324.txt`
(`ntkdriver_nondovi.ko` MD5 `fded7005651e37b8a1b5e8e89f73f4ff`).

The repeated dmesg pair `No data in SW RX Buffer` / `please fix nvt_err error`
is the driver's error-level logging of the normal empty raw receive branch
(`KER_CEC_IocTl` sets length 0 and returns success). The `please fix` line is
the `nvt_err` macro's generic trailer, not a second fault; the 100 ms receive
poll makes it frequent. The later `CEC H/W re-open` / `Can't open CEC` is the
known process-death leftover after the harness SIGKILLs the raw owner; a reboot
restores stock.

### Boot 4: LA8 NACK, LA4 ACK

Sequence: `0883`, `0483`, then `0f83` (broadcast).

```text
send 0883: ok -> cec tx failed (512), tx 0, error 512
send 0483: ok -> cec tx 1
send 0f83: ok -> cec tx 2
final: rx 0 tx 2 error 0
```

The directed LA8 frame was not ACKed; LA4 and broadcast were. This corrects
the earlier “Deck at LA8” reading: the CEC-LIVE cache loop printed the array
slot index, not a proved wire logical address; slots 4 (`seen=0`) and 8
(`seen=1`) both carried the Deck's copied PA/name. On this boot LA4 is the
active peer on the wire.

### Boot 5: TV announcement and LA4 queries

After the acquire, the script announced the TV and queried LA4:

```text
0f84000000  broadcast Report Physical Address, TV at PA 0.0.0.0
04          poll LA4
0483        Give Physical Address
0446        Give OSD Name
048f        Give Device Power Status
```

All five were ACKed (`cec tx 1..5`), with no receive frames (`rx 0`,
`cec-first: rx frame lines=0`). The LA4 peer acknowledges directed and
broadcast frames but did not answer any identity query, including after the TV
announced LA0/PA 0.0.0.0. A `84` reply was not captured. This is consistent
with an ACK-only/limited peer stack (asleep, CEC disabled, or an adapter
without full message handling); it is not a TV TX failure.

### Boot 6: armed monitor attempt and recovery

A raw-RX monitor was staged as `openaurora` `b1ee4832…`, `mod_cec.so`
`f3337e29…`, monitor script `d4c72b90…` and hook `fe9205fd…`
(`timeout -t 420`). It opened `/tmp/ntkcec` (`open 1`) and logged
`monitor armed; waiting for /tmp/cec-go`; the MCU feeder logged 66 beats
(132 s) before the recovery.

The pre-stock hook's Ethernet/DHCP did not come up:

```text
keepalive: eth0 brought up
keepalive: requesting DHCP lease
udhcpc: sending discover
udhcpc: read error: Network is down, reopening socket
Killed
```

SSH was therefore unreachable, the `/tmp/cec-go` trigger could not be created,
and the user restored the tested `/conf` image with the USB stick. The monitor
did not send a frame and did not hang the CEC path; the failure was the long
pre-stock armed wait that depended on an unreliable early network.

Recovery restored the original hook `c3943712…`, no branch, stock
`aurora.elf` 1752 (zombie 2184) and `DRV_CEC_CALLBAC` 2863.
`/buffer/oa-cec-monitor` survived because it is not part of `/conf`; its
`cec.log`, `wdt.log` and `tvhack.log` tail were preserved and the staging was
removed.

Rule: do not leave a long pre-stock block that depends on SSH. A retry must use
a short fixed capture window that starts immediately and unconditionally falls
through to stock, with the feeder/reaper bounding it.

## What this establishes

- A first-opener boot really is sufficient to acquire the raw CEC device;
  the lock is free before stock Aurora opens it.
- The raw module opens `/tmp/ntkcec` with the authority mirror in that boot
  position.
- The module-level TX queue accepts a send while the receive poll owns the
  single owner slot; boot 3 proved the queued path reaches the driver.
- Raw CEC transmit is hardware-accepted: boot 4 ACKed LA4 and broadcast while
  LA8 NACKed, and boot 5 ACKed the TV announcement plus the LA4 poll and
  `0483`/`0446`/`048f`. The old cache-slot reading was not a wire address.
- The LA4 peer does not answer `83`/`46`/`8f`; a peer reply is not yet proven.
- The boot harness needs a hard-kill timeout, no blocking `wait` in its trap,
  a one-shot guard, feeder stop on every exit, an unconditional fallback to
  stock, and no long pre-stock wait that depends on SSH.

## Limits

- No `84` (Report Physical Address) or any other reply was received; the LA4
  peer ACKs but stayed silent. Whether it is the Deck itself, a limited
  adapter, or a device whose CEC stack is asleep is not established.
- The exact LA4 device identity and reply behavior need a peer-side check or a
  known-good CEC device.
- Raw RX during the boot 3–5 test window was empty. Later dmesg showed RX-buffer
  activity in the leaked context after stock started, but no userspace frame
  was captured and loopback of our own frames is not excluded.
- Boot 6 captured no RX: the pre-stock network failed, no trigger was created,
  and recovery was required. It is not evidence about the CEC bus.
- The exact boot-2 hang cause is unproven.
- USB recovery was required for boots 2 and 6; the stick was rebuilt. The
  recovery image is the tested `/conf` image, not a pristine no-root revert.
- `cec.enable` remains 0 in every shipped profile.

## Next

1. Capture the Deck suspend/resume frames with a fixed-window raw-RX monitor:
   start immediately after acquire, run at most 60 s, create no SSH-dependent
   trigger, and unconditionally exec stock at the end. The owner sleeps the
   Deck, waits, then resumes it inside the window. This also provides the raw
   RX evidence and the exact opcode (0x9D/0x36 versus 0x82).
2. If the peer stays silent, verify the Deck's CEC stack or use a known-good
   CEC device and record whether it answers `83`; do not treat ACK as reply
   acceptance.
3. Keep the queue regression (`check-cec-module-fake`) and the hardened
   fixed-window harness in the next first-opener run; no persistent hook stays
   installed.
