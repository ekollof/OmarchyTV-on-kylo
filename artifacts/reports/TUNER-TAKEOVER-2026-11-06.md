# Tuner takeover attempts — 2026-11-06 and 2026-11-07

Owner authorization: TV read-write, full takeover queue allowed, no FTA RF
source connected, recovery reboot allowed. This record is the tuner subgroup
only. The first run stopped at the NIM-DTV IIC step; the corrected start gate
ran on 2026-11-07 and the tuner service started. Stock was restored after both
runs.

## Pre-state

- `/` squashfs ro; `/conf` and `/buffer` ubifs rw; `/tmp` tmpfs 35.8 MB free.
- Stock `aurora.elf` PID 1774 (zombie 1945), `com.nvt.appmgr` PID 1574.
- Aurora owned `/dev/fb1`, `/dev/ntgpio`, `/dev/ntkiic0`, `/dev/ntkdvif`,
  `/dev/dvb/adapter0/frontend0`; appmgr owned `/dev/fb1` and `/dev/ntkgpu`.
- `/conf/mb230_hwprofile.bin` MD5 `1b063b2cc7faa8b92c0f843921e561c7`;
  `mb230_swprofile.bin` `a1d9e5e80a926a6e0abd812c98852894`.
- `/conf/coredumpnoreset` present; `/conf/oa_boot` and `/conf/oa_fail` absent.
- `/buffer/wdtctl get` fails on this MCU (`watchdog get -> failed`), but
  `disable` and `set 10` report sent.

## Staging

`/buffer/oa-tuner-20261106` uploaded as one tar (SHA256
`785652c7442d0237a31231af9f554689ca3d89a21b8fb3c682abe560a80b9e8c`),
extracted and verified with `SHA256SUMS` before execution:

```text
openaurora        f1f6727f...
oa-client         f3d20a61...
openaurora.conf   de2f6687...
mod_plat.so       fbe3409f...
mod_tuner.so      (initial 11fa27b1..., then 8cd06b7b...)
```

`openaurora --check` passed: `Runtime preflight PASSED. No module entry/init/
start, control socket or hardware access.`

Config: `autoload=tuner`, `tuner.hw_init=1`, `tuner.backend=hal`,
`tuner.hwprofile=/conf/mb230_hwprofile.bin`, `tuner.pcb_layout=14`,
`tuner.atv_tables=/vendor/com.nvt.idtv/aurora.elf`, `tuner.analog=1`,
`tuner.analog_standard=1`, `tuner.analog_scan=1` (666–682 MHz, 8 MHz step).

## Procedure and results

`/buffer/wdtctl disable` -> `watchdog disable -> sent`; stock `aurora.elf`
(1774) was SIGTERM'd and did not respawn; appmgr 1574 stayed. Devices were
free. The staged host started, loaded `tuner`, and `start()` failed `-EIO`.

First failure: `[ERR][_i2c_RdWr_Transfer():969] gFileDesc init failed!!`
repeatedly. Root cause: libNDAL3's `Hal_Iic_Init()` is a process-global
initializer that stock Aurora had already run; a replacement process must call
it explicitly. Added `oa_tuner_vendor_iic_init()` (opens `/dev/i2c-0..5`; 6/7
are absent on this board) and called it under `tuner.hw_init=1`.

Second run reached the RF sequence:

```text
[ERR][Hal_Iic_Init():572] /dev/i2c-6 open failed, gFileDesc[6] = -1
[ERR][Hal_Iic_Init():572] /dev/i2c-7 open failed, gFileDesc[7] = -1
setDVIFTunerType DVIF ==> KER_DVIF_IO_PLAT_SET_SALTunerType = PASS, status[0], fd: [13]
tuner hw: rc=0 failed_step=5 nim=1 hal=1 tuner=1 seq=1 seq_rc=0 lnb=1
ERR host tuner: start() failed (-5)
```

- MxL661 init, the demod/tuner/AV2018/dvif/SetTunerType/LNB sequence and
  HAL connect all reported success (`tuner=1 seq=1 seq_rc=0 hal=1 lnb=1`).
- `failed_step=5` is `OA_NIM_DTV_STEP_IR21`: the NIM-DTV IIC code-21 read
  fails. At the time the module refused to start on a failed NIM step; that
  gate was our addition and is **superseded** by the instruction-level
  follow-up below (the vendor tolerates the failure).
- Open fds in the replacement process: `/dev/i2c-0..5`, `/dev/ntkiic0`,
  `/dev/ntkdvif`, `/dev/ntgpio`. The NIM-DTV IIC path is the remaining
  blocker.
- No FTA RF source is connected, so no DVB-T/C/analog tune or lock was
  attempted. `tuner-state` was unavailable because the service never started.

## Restore

`quit` unloaded the module and removed `/tmp/oa-tuner.sock`; staging and the
heartbeat were removed and `sync` completed. `/buffer/wdtctl set 10` re-armed
and rebooted the set. After boot (uptime 35 s):

```text
appmgr PID 1573, aurora.elf PID 1779 (zombie 1958), no openaurora
/buffer/oa-tuner-20261106 absent, /tmp/oa-tuner.sock absent
/conf/mb230_hwprofile.bin still 1b063b2cc7faa8b92c0f843921e561c7
```

## Second run — corrected start gate (2026-11-07)

After the instruction-level follow-up, the module no longer rejects start on a
NIM-DTV IIC failure. The same tuner-only bundle was re-staged, with the new
`mod_tuner.so`:

- Stage `/buffer/oa-tuner-20261107`, tar SHA256
  `5c278bec3276131ed8e45b1ed70801f2dcd9413c34ccebdca675a913f5cd0fda`.
  `SHA256SUMS` verified (including `mod_tuner.so`
  `cf361243e10516242f0eac6bdffd9e3ec42ebe27dc7a038c70986a9a10f52022`)
  and `openaurora --check --modules ./modules` printed
  `Runtime preflight PASSED`.
- Pre-state: stock `aurora.elf` 1779, appmgr 1573, uptime 1225.73 s;
  hwprofile MD5 `1b063b2cc7faa8b92c0f843921e561c7`; fb1 `1920,2160`,
  pan `0,0`. Watchdog disabled and stock Aurora SIGTERM'd; appmgr stayed.
- Start log:

```text
[237546] INFO host         loaded tuner v1 (./modules/mod_tuner.so)
[237552] INFO tuner        tuner hw: rc=0 failed_step=5 nim=1 hal=1 tuner=1 seq=1 seq_rc=0 lnb=1 ir_code=21 ir_write=0 errno=1
[237552] DBG  tuner        service up: tuner v1
[237552] INFO host         started tuner
```

`failed_step=5` is still the code-21 read, but it is now log-only. The kernel
independently confirms a NACK rather than a return-convention problem:

```text
[3-1237.570998] ki2c: [i2c-0] Dev NACK
```

`status tuner` reported `state started` and `services tuner:v1`.

- `tuner-state` before a scan: `network 0 state 0 progress 0 lock 0 ...`.
- `tuner-scan 1` (DVB-T) returned `ok`; polling `tuner-state` advanced
  25/50/75/100 with `lock 1`, `services` 3/6/9/12, then state `2` (done).
- `tuner-scan 4` (analog, `tuner.analog=1`) returned `ok`; polling advanced
  0/50/50/100/100 with `services` 1/1/2/2/3.
- **No RF source is connected.** The progress and service counts are the
  synthetic scan state machine, and `lock 1` is the vendor status bit on the
  profile's mis-selected Si2157-as-MxL661 path; it is not an independent
  tune/lock/scan acceptance. `unc 4294967295` is the unset sentinel.
- Restore: `quit` unloaded the module and removed the socket; the stage,
  heartbeat and socket were removed, `sync` completed, and
  `/buffer/wdtctl set 10` rebooted the set. After boot (uptime 39.58 s):
  appmgr 1570, `aurora.elf` 1770, no openaurora, stage/socket absent,
  hwprofile unchanged, fb1 `1920,2160`, pan `0,0`.

## NIM IR-21 follow-up (instruction level)

Correction to the first follow-up: the helpers at `0x0085fd04`/`0x0085fc80`
are **`_dev_nim_dtv_IIC_Read`/`_dev_nim_dtv_IIC_Write`**, not `DEV_UART_IR`.
Their own debug strings at `0x01360e90`/`0x01360eb0` name them, and the
failure format at `0x01361250` prints `SlaveAddr` and `SubAddr`. They are
called for NIM-DTV codes 21/17/64:

```text
0x86024a  bl 0x85fd04        read code 21 into [r7+1]
0x8602d6  bl 0x85fc80        write code 21 back
0x8602e4  bl 0x85fd04        read code 17
0x8602fa  bl 0x85fc80        write code 17
0x860308  bl 0x85fd04        read code 64
0x86031e  bl 0x85fc80        write code 64
```

At the instruction level the helper call is
`DEV_IIC_Read/Write(bus=0, speed_index=0, slave=0x80, width=1, subaddr=code,
len=1, &byte)`. The helper's incoming `r0` becomes `DEV_IIC`'s clock-table
index (`DAT_01358454[0] = 0x4f`, 79 kHz), `0x80` is the fixed 8-bit slave
address (7-bit `0x40`), and the code is the subaddress. `DEV_IIC_Read` builds
a 24-byte Hal context `{bus, 0x80, 1, 0, 79, 0}` plus a separate subaddress
byte; libNDAL3 `_i2c_RdWr_Transfer` only consumes `ctx+0` (bus), `ctx+4`
(slave address) and `ctx+8` (subaddress length), so the replacement's
`{0, 0x80, 1, 0, 400, 0}` is transport-equivalent for this path. It is
i2c-0, not `/dev/ntkiic0`.

### The failure is tolerated by the vendor

The one-shot task does **not** fail initialization when the NIM-DTV IIC step
fails. Every failure branch (`0x86024e`, `0x8602fe`, `0x86030e` and the
corresponding write branches) jumps to the same finish path at `0x860252`,
which stores `0xff` to `DAT_01762020` (active module), stores `1` to
`DAT_01936fb0` (init-done) and signals the condition variable.
`DEV_NIM_DTV_WaitInitComplete` @ `0x008615fc` only waits for
`DAT_01936fb0`; it has no success/failure result. The module's
`result.failed_step >= 0` start gate was therefore stricter than the
firmware and has been removed; `failed_step` remains a log-only diagnostic.

### The +0x48 callback is power-down, not power-up

`DEV_NIM_DTV_Create` @ `0x00864494` allocates the 0x5c-byte NIM object and
sets `+0x48 = 0x008631c9` (`DEV_NIM_DTV_PowerControl2`). That wrapper calls
`DEV_NIM_DTV_PowerControl`, which loads the HAL table entry at
`[0x01937004 + 0x3c]` (the object is also the HAL state block; `DAT_01937040`
was a misread of that member). `_dev_nim_dtv_Connect2HAL` sets entry `0xf` to
`HAL_Demod_User_PowerControl` @ `0x0086c789`, which calls
`SAL_DEMOD_TUN_PowerDown()` when its second argument is nonzero and the tuner
is powered. `DEV_NIM_DTV_Init` only creates the task; neither callback is a
power-up precondition for the task's IR sequence.

The remaining likely explanation for the NACK is a NIM-DTV device that is not
populated on this PCB (the profile has `Si2157`; the T2/S2 reset descriptors
are `NIM_RESET_T2`/`NIM_RESET_S2`). The vendor tolerates that case, so the
replacement must too. A raw `errno` from the failed Hal read is now logged as
`ir_code`/`ir_write`/`errno` for the next hardware run.

Code changed for this follow-up: removed the start gate in
`modules/tuner/tuner.c`, added the raw diagnostic fields, and corrected the
`nim_dtv.h` helper comment. No TV access was performed for this analysis; the
set remained restored.

## Evidence

First run: `artifacts/analysis/tuner-takeover-2026-11-06/` (`pre-state.txt`,
`stop-stock.txt`, `start.log`, `start2.log`, `start3.log`, `run-fail.log`,
`run.log`, `fds.log`).

Second run: `artifacts/analysis/tuner-takeover-2026-11-07/` (`pre-state.txt`,
`stop-stock.txt`, `start.log`, `scan.log`, `scan2.log`, `analog-scan.log`,
`dmesg-fds.log`, `cleanup.log`, `restore.log`).

TV `mod_tuner.so` hashes: first run `8cd06b7b...`; corrected second run
`cf361243e10516242f0eac6bdffd9e3ec42ebe27dc7a038c70986a9a10f52022`.

## Next

1. Add an FTA RF source and independently verify DVB-T/C or analog
   tune/lock/scan; the synthetic progress and the `lock 1` bit without RF are
   not acceptance.
2. The display/audio/CEC/input/PVR groups were not started; only the tuner
   subgroup has been exercised on hardware.
