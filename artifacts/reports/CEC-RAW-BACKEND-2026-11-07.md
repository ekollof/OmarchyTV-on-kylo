# Native CEC transport and module worker — 2026-11-07

Scope: wire the existing raw `ntkcec` device/owner backend into `mod_cec`
behind `cec.enable`, add the read-only authority mirror check and control-plane
send/open/close/state, and attempt a controlled hardware exchange with the CEC
device on HDMI1. The exchange is **blocked by a leaked kernel CEC owner**; no
frame was transmitted or received on hardware.

## Implementation

### Authority mirror (`common/cec_authority_mem.c`)

VPL_CEC_Open reads the mirror word at physical `0xfc040000 + 0x200` and, when
bit 1 is clear, writes STBC `0xf011` with `(low byte | 2)`. The helper:

- opens `/dev/mem` `O_RDONLY|O_SYNC` and `mmap64`s one page at `0xfc040000`;
- exposes `read_mirror` from offset `0x200`;
- has **no native STBC write path**: `write_register` returns vendor failure, so
  a clear bit fails closed before the device is opened;
- unmaps/closes repeatably; injected I/O covers the fake mapping tests.

A read-only ARM probe on the TV returned `mirror=000001ab bit1=1`, so the
current MCU/stock state already grants CEC authority to ARM. The probe was
staged in `/tmp` and removed.

### Module (`modules/cec/cec.c`)

New config:

```text
cec.enable = 0|1        hardware gate, default 0
cec.device = PATH       raw node, default /tmp/ntkcec
cec.poll_ms = N         receive/completion poll, 20..1000, default 100
```

With `enable=1`, start maps the authority page, creates the owner and submits
an asynchronous acquire. The 100 ms host timer only takes completions and
submits the next job; open/send/receive/release all run on the owner worker.
Received frames are logged (`cec rx I->D op XX len N [...]`), passed to the
existing audio policy, and policy replies are queued back to the worker one
at a time. `cec-state` now reports
`enabled/opening/open/rx/tx/error/last`, and the control socket gained
`cec-open`, `cec-close` and `cec-send <hex>` (TV initiator only).

The host build fails closed at the authority map (ENOTSUP) and publishes
nothing; the disabled build publishes policy only and returns ENODEV for
transport operations.

## Tests

- `check-cec-authority-mem`: read-only mirror, bit-1 success without a write,
  clear-bit EIO, map/fd unwind, repeatable close.
- `check-cec-transport-ipc`: disabled services + ENODEV open/send/close,
  invalid hex/EINVAL, host `enable=1` start failure with no publication.
- Existing `check-cec-owner`, `check-cec-quiesce`, `check-cec-device`,
  `check-cec-packet`, `check-cec-audio-policy` pass. The quiesce test string
  was extended for the new `cec-state` fields.
- ARM core `88bb2906...`, `mod_cec.so` `10ef4b50...`; preflight passed on the
  TV.

## Hardware attempt

Stage `/buffer/oa-cec-20261107` (tar `e3a6cef4...`), `SHA256SUMS` and
`openaurora --check` verified. Stock `aurora.elf` 1777 was SIGTERM'd with the
watchdog disabled; `/tmp/ntkcec` was created as char `229:0`.

The module started with hardware enabled and authority passed, but acquire
returned EPERM:

```text
[211181] WARN cec  cec acquire failed (1)
kcec: CEC Try lock fail
kcec: error: KER_CEC_Open [ntkdriver]: please fix nvt_err error
cec owner 1 audio 1 supported 1 local 1 enabled 1 opening 0 open 0
          rx 0 tx 0 error 1 last 0
```

The exclusive lock is still held by the kernel thread left by stock:

```text
 2929     2 SW   DRV_CEC_CALLBAC  [DRV_CEC_CALLBAC]
```

Stopping stock Aurora did not stop that thread or release the KER_CEC semaphore.
This is the close/open defect predicted by CEC-OWNERSHIP: process death without
the driver's close path leaves the kernel CEC owner alive. Retries for ~10 s
kept failing; there is no userspace semaphore release.

No frame was transmitted or received. The raw device was never opened.

## Restore

`quit`, stage/socket/node removed, `sync`, `wdtctl set 10` reboot. After boot:
aurora 1740 (zombie 1934), appmgr 1679, stock fds include video24/fb1/snd/i2c-3;
stock's own `DRV_CEC_CALLBAC` thread 2937 exists; `/dev/ntkcec`, `/tmp/ntkcec`
and the stage/socket are absent; hwprofile `1b063b2c...`; fb1 `1920x2160`
pan `0,0`.

## Close-path recovery (follow-up, same day)

The next question was whether stock Aurora ever closes CEC gracefully, so the
leaked kernel owner could be released without a reboot.

Instruction-level findings:

- `ICE_CECClose` @ `0x00806e44` is a **stub**: it only calls
  `UTILI_DBG_ChkPrtLevel`/`printf` and returns. It performs no close.
- `~CECInterface` @ `0x00a2d93c` calls that stub, then stops its own message
  thread/queue and destroys objects. It does not call `PLAT_CEC_Close`.
- `PLAT_CEC_Close` @ `0x0088bff4` is the real close: when its `DAT_0197a3b8`
  init flag is set, it clears the flag and calls
  `NDAL_VideoExt_CEC_Init(0)` (null config -> VPL close -> DRV close).
- `PLAT_CEC_Close` has **no internal caller** in aurora.elf. The only
  reference FindCallers reports is its own `.dynsym` entry; there is no `bl`
  or data callback to it. `NDAL_VideoExt_CEC_Init(0)` is called only from
  `PLAT_CEC_Close`.

A linkable close probe (`tools/cec/stock-close.c`, temporary ARM binary) loaded
libNDAL_EX, opened the platform video context with
`NDAL_VideoExt_OpenDevice(0,10)`, then called `NDAL_VideoExt_CEC_Init(NULL)`:

```text
open_device(0,10)=3 cec_init(NULL)=1
```

A matching `cec_init(config)` then `cec_init(NULL)` also returned
`cec_open=1 cec_close=1`. In both cases the orphaned kernel thread remained:

```text
 2937  2 SW  DRV_CEC_CALLBAC  [DRV_CEC_CALLBAC]
```

`dmesg` showed no CEC close. A different process's VPL close reports vendor
success but cannot stop the context owned by the dead stock process. There is
therefore **no userspace stock-close trigger** in this firmware for the
SIGTERM case; the kernel thread/semaphore is only cleared by a reboot (or by
starting the replacement before stock opens CEC, which is a boot-order gate).
The probe source is kept for reproducibility; it must run only with stock
stopped and is not a production helper.

## Limits and next options

- No CEC bus exchange, logical-address claim on the wire, one-touch, standby
  or ARC transmission. The raw open is the remaining blocker.
- Native authority is read-only; a clear mirror bit would need the recovered
  STBC `0xf011` write path.
- The follow-up proved there is **no userspace stock-close trigger** for the
  SIGTERM case: `ICE_CECClose` is a stub, `PLAT_CEC_Close` is never called, and
  a new process's `NDAL_VideoExt_CEC_Init(NULL)` reports success without
  stopping the orphaned kernel thread.
- Options for the next window:
  1. start the replacement early enough at boot, before stock opens CEC (a
     boot-hook/order gate, separate authorization);
  2. keep the stock-owned/shared VPL path for read-only monitoring only.
- Do not unload the driver or patch driver globals to force a release; do not
  substitute `DRV_STBC_SwitchCEC`.

Evidence: `artifacts/analysis/cec-transport-2026-11-07/` (`pre-state.txt`,
`start.log`, `lock-retry.log`, `blocker.txt`, `cleanup.log`, `restore.log`,
`close-probe-restore.log`, `close-probe.txt`), plus `tools/cec/stock-close.c`
(build with the TV cross compiler, `-ldl`, rpath
`/vendor/lib:/vendor/lib/libs`; run only with stock stopped).
