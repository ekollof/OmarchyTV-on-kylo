# Tuner takeover attempt — 2026-11-06

Owner authorization: TV read-write, full takeover queue allowed, no FTA RF
source connected, recovery reboot allowed. This record is the tuner subgroup
only. Stock was restored after the tuner gate failed.

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
- `failed_step=5` is `OA_NIM_DTV_STEP_IR21`: the NIM UART IR code-21 IIC
  read/write still fails. The module correctly refuses to start with a
  failed NIM step.
- Open fds in the replacement process: `/dev/i2c-0..5`, `/dev/ntkiic0`,
  `/dev/ntkdvif`, `/dev/ntgpio`. The NIM IR path is the remaining blocker.
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

## NIM IR-21 follow-up (instruction level)

The one-shot task at `0x008601ac` does not call the IIC helper directly. For
code 21 it calls `0x0085fd04` and `0x0085fc80`, the two `DEV_UART_IR`
system-code helpers:

```text
0x86024a  bl 0x85fd04        read code 21 into [r7+1]
0x8602d6  bl 0x85fc80        write code 21 back
0x8602e4  bl 0x85fd04        read code 17
0x8602fa  bl 0x85fc80        write code 17
0x860308  bl 0x85fd04        read code 64
0x86031e  bl 0x85fc80        write code 64
```

Those helpers build the `DEV_IIC` call `(bus=0, dev=0, reg=0x80, size=1,
code, len=1, &byte)` and call `DEV_IIC_Read/Write`, which in turn call
`Hal_Iic_Read/Write`. So the path is the same i2c-0/0x80 bus, not the
`/dev/ntkiic0` fd; the replacement already has `/dev/ntkiic0` open but the IR
helpers use `Hal_Iic`.

`DEV_NIM_DTV_Init` @ `0x00861678` only checks the model, logs, initializes the
mutex and creates the task; it does **not** power the NIM. NIM power is split
into callbacks:

- `DEV_NIM_PowerSetting(nim)` calls `*(code **)(nim + 0x48)()`;
- `DEV_NIM_DTV_PowerControl` calls the global function pointer
  `DAT_01937040`.

The replacement module runs the task body directly and never invokes either
power callback. The IR read returning failure is therefore consistent with an
unpowered/unreset NIM chip, not necessarily a wrong bus. Before changing the
IIC adapter, the next step is to recover where `DAT_01937040`/the `+0x48`
callback are registered and what they toggle, then add the same power/reset
sequence before the task. A raw `Hal_Iic_Read` return log would distinguish a
real NACK from a return-convention mistake.

No code was changed for this follow-up; the TV was already restored.

## Evidence

`artifacts/analysis/tuner-takeover-2026-11-06/`: `pre-state.txt`,
`stop-stock.txt`, `start.log`, `start2.log`, `start3.log`, `run-fail.log`,
`run.log`, `fds.log`.

TV `mod_tuner.so` with the IIC init and diagnostic is sha256
`8cd06b7be6a2a850ca8932e6c122b732ddc646f17e2685e7bf0212a28b1c7b80`.

## Next

1. Recover why NIM IR code 21 fails: verify whether `DEV_IIC` bus 0 must use
   `/dev/ntkiic0` rather than `Hal_Iic` i2c-0, and whether NIM power/reset
   (`DEV_NIM_PowerSetting`/`DEV_NIM_DTV_PowerControl`) must run first.
2. Re-run the tuner subgroup with an FTA RF source for tune/lock/scan.
3. The display/audio/CEC/input/PVR groups were not started; the tuner gate
   failed first and the set was restored.
