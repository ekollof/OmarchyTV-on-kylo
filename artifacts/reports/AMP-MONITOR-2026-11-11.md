# DEV_AMP_ID monitor check — 2026-11-11

Status: current
Last verified: 2026-11-11
Evidence class: firmware instructions and host tests. No amp I2C/Gpio write or
TV hardware operation.
Supersedes: the "monitor task not reproduced" note for the check state machine
only; the live owner wiring remains open.

## Firmware contract

`DEV_AMP_ID` (`0x0084ebf0`) creates the factory monitor task at `0x84e110`
(stack `0x1000`, priority 10):

1. Delay 15 s.
2. Every 2.5 s walk the factory list at `0x0192fa5c`.
3. For each object with a `+0xbc` check pointer, call it unless the global
   byte `0x0192fa55` is set.
4. The AD82587 object's `+0xbc` is `AMP_AD82587_IsAlive` (`0x875458`):
   read amp registers 03..05 and compare them with the retained configured
   image at `0x017640e8+3..5`.
5. On mismatch it logs and calls object `+0x44` (`AMP_AD82587_Init`,
   `0x874ef0`) then `+0x70` (`AMP_AD82587_Mute`, `0x874e50`, level 0). Both
   are called even if Init fails.
6. `AMP_AD82586CSubWoofer_IsAlive` (`0x8763bc`) is a literal `return 1`, so
   the subwoofer step is a no-op.

`_DEV_AMP_ReadIIC` (`0x84ffc0`) dispatches through the object's `+0xb8`; the
default `__DEV_AMP_ReadIIC` (`0x84ff50`) selects table
`0x0192fac4 + index*36` and calls `DEV_IIC_Read(bus, speed, slave, width,
subaddr=3, len=3, buf)`.

The vendor wrapper's return convention is inconsistent: `IsAlive` treats a
first zero return as a reason to read again, treats two zero returns as
`IIC fail to read data!!`, and otherwise compares the buffer. `DEV_IIC_Read`
returns 0 on success, so the wrapper's 0/1 use is ambiguous. The monitor is
therefore **not** ported bug-for-bug without a live amp read check.

## Implementation

`openaurora/include/openaurora/amp_monitor.h` and
`openaurora/common/amp_monitor.c` implement the recovered check with injected
transport:

- `read_regs(ud, first=3, count=3, out)`, 0 on success;
- compare against the caller's configured image bytes 3..5;
- on mismatch, log, call `reinit`, then `mute(ud, 0)`;
- on read failure, log/count and return `-EIO` **without** reinitializing from
  stale data (deliberate correction of the vendor's ambiguous branch).

`check-amp-monitor` covers match, mismatch with Init→Mute(0) order, reinit
failure still followed by mute, read-failure isolation, counters and invalid
inputs:

```text
ok amp monitor: regs03..05 compare, Init->Mute(0) recovery,
   read-failure isolation and invalid inputs
```

## Limits

- The helper is **not wired** into `audio_amp`: there is no 15 s/2.5 s owner
  timer and no amp-owner register-read submission. `audio_amp.enable=0`.
- The helper uses the corrected 0-success contract. The vendor's inverted
  double-read behaviour and the live `DEV_IIC_Read` return need a controlled
  amp read if strict bug-for-bug parity is required.
- No amp register was read or written on the TV for this work.
- The `DEV_AMP_ID` factory/profile path remains covered by
  `audio_amp_profile`/`amp_board`; this report adds the monitor check only.
