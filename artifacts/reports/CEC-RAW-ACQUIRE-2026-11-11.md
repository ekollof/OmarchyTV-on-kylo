# Raw CEC acquire attempt — 2026-11-11

Status: current
Last verified: 2026-11-12
Evidence class: on-TV controlled ownership; no CEC frame sent.
Supersedes: the "raw ownership needs an authorized TV session" limit in
CEC-MODULE-TRANSPORT-2026-11-09 for the acquire step only.

> **2026-11-12 correction:** the CEC-LIVE “LA8” was a cache array slot, not a
> proved wire logical address. Raw first-opener TX now shows LA8 NACK and LA4
> ACK; the LA4 peer accepts queries but sends no reply. See
> CEC-FIRST-OPENER-2026-11-12. The raw-acquire/lock result below is unchanged.

## Setup

The Steam Deck is connected on HDMI1 and speaks CEC; CEC-LIVE-2026-11-01
recorded it at LA8/PA1000 (`Steam Deck`). The TV's raw CEC device is major 229
(`ntkcec`) with no persistent node.

Staged under `/buffer/oa-cec-test` and hash-verified:

```text
openaurora       b1ee4832435d4d4f93293d8a54ca6936c137ba8fc09b02edc17259d44a3c4aab
mod_cec.so       4ab2e9d319f8206d8a2ecce580f7afb335b22fd8f43446e827da6d4ef6b1af9e
oa-wdtfeed       206c1ab5c3c2f223fab96d2825ff83b8eb7b4c2572b2c3017893481d80edb486
cec.conf         4ac160557d5b844733f1b132a17923402d7562902197db69ed1ae7a5d22f2fec
```

A temporary `/tmp/ntkcec` char node (major 229, minor 0) was created. Config:
`autoload=cec`, `cec.enable=1`, `cec.device=/tmp/ntkcec`. The module maps the
authority mirror at physical `0xfc040200`; the mirror was previously verified
read-only as `0x000001ab` (bit 1 set), so no STBC authority write is needed.

## Run

1. `oa-wdtfeed` logged two beats.
2. The appmgr was paused (`SIGSTOP 1578`) and stock
   `run_aurora.sh`/`aurora.elf` (1706/1776) were stopped.
3. `/tmp/ntkcec` was created and `openaurora` started with the `cec` module.
4. The authority map succeeded and the module became ready, but the raw
   acquire failed.
5. `quit`, feeder stop, node/staging removal, and a clean reboot restored
   stock.

## Result

```text
cec: hardware enabled (/tmp/ntkcec)
host: ready receipt (all autoload starts succeeded)
cec acquire failed (1)

cec-state: cec owner 1 audio 1 supported 1 local 1 enabled 1
           opening 0 open 0 rx 0 tx 0 error 1 last 0
```

Kernel ring:

```text
kcec: CEC Try lock fail
kcec: error: KER_CEC_Open [ntkdriver]: please fix nvt_err error
```

A scan of every `/proc/<pid>/fd` found **no userspace process holding a CEC
fd**; only the test's own log fds matched the search. The exclusive CEC lock
therefore survived the stock Aurora exit, or is held outside a visible
userspace fd. The module's `CEC_OPEN` ioctl returned EPERM before any TX/RX
frame; no CEC message was sent in this attempt.

This is consistent with the CEC-TRANSPORT-CONTRACT close/reopen hazard and the
CEC-OWNERSHIP authority/lifetime gap. Stopping stock Aurora and starting a raw
owner is **not sufficient** to obtain the CEC lock on this set.

## Restore

```text
uptime 0 min
appmgr 1568, run_aurora.sh 1706, aurora.elf 1772 (zombie 1953)
no openaurora / oa-wdtfeed
/tmp/ntkcec and /buffer/oa-cec-test removed
```

A reboot cleared the stale lock; stock Aurora reacquired CEC and the boot log
shows its own CEC TX-permission attempts. No boot hook, firmware or `/conf`
change.

## What this establishes

- The authority mirror gate passes (`0xfc040200` bit 1 already set), and the
  module loads and publishes the CEC owner/audio services.
- The raw `CEC_OPEN` is refused with `CEC Try lock fail` after the stock
  process is stopped; this is not a missing-node or authority-mirror failure.
- CEC raw ownership needs a **first-opener boot** (our process takes CEC before
  stock Aurora) or a proper vendor handoff/close, not a kill-and-restart.

## Limits

- No CEC TX or RX occurred; the Deck's live response was not exercised in
  this attempt.
- The shared-owner `cec-probe` path remains the only currently usable live
  identity query while stock Aurora runs. CEC-LIVE's cache slot 8 was not a
  proved wire LA; the 2026-11-12 first-opener test shows LA8 NACK / LA4 ACK.
- A first-opener boot requires a boot-time entry / controlled boot gate, which
  is a separate deployment decision. `cec.enable=0` remains in every profile.
- No audio/ARC, routing or CEC policy acceptance is claimed.
