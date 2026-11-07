# AD82587 balance and control registers — 2026-11-07

Scope: recover the vendor AD82587 balance/volume/soft-mute/HP-mute register
contracts, implement the write-only balance path with tests, and record which
controls remain unwired. Offline Ghidra/instruction work; no TV access.

## Sources

| Item | Address |
|---|---|
| `AMP_AD82587_SetBalance` | `0x00875204` |
| `AMP_AD82587_SetVolume` | `0x008752a8` |
| `AMP_AD82587_GetVolume` | `0x0087551c` |
| `AMP_AD82587_SoftMute` | `0x00875148` |
| `AMP_AD82587_HP_Mute` | `0x008753f8` |
| `AMP_AD82587_SetBalance` vtable slot | object `+0x90` |

## Balance (implemented)

Instruction-level formula for balance 0..100 (`r0` = amp object, `r1` =
balance):

```text
balance > 100 : reg4 = 127, reg5 = 0
balance > 50  : reg4 = 127 - ((100 - balance) * 127) / 50, reg5 = 0
balance == 50 : reg4 = 0, reg5 = 0
balance < 50  : reg4 = 0, reg5 = 127 - (balance * 127) / 50
```

The division is the signed helper/0x51eb851f magic for `/50`; positive values
truncate. Order is fixed: write register 4 first, write register 5 only if the
first write succeeded. A failed write returns failure and leaves the cached
state unchanged (the hardware may be partially updated; there is no rollback).

`common/audio_amp.c` adds `oa_ad82587_balance_regs()`; `audio.amp` gains
`set_balance`; the control socket gains `amp-balance <0..100>` and
`amp-state` reports `balance`/`balance_known`. Both `set_mute` and
`set_balance` reject with `-EBUSY` while an Init is submitted, closing the
race found by the adversarial review. Follow-up (AMP-JOBS): the writes now run
as typed `oa_amp_owner` worker jobs, not on the caller; both submissions still
reject with `-EBUSY` while a job is pending or its result is untaken.

Tests (`check-audio-amp`): 0/1/25/49/50/51/75/99/100 exact register pairs,
negative/101/NULL rejection. `check-audio-amp-module`: disabled ENODEV,
invalid 101/non-numeric EINVAL, enable=1 fail-closed. ARM
`mod_audio_amp.so` sha256
`7334a2366bcd5a7e10d3df9d97cebb5f2d03f3c17bc663c8bd6b505d08ba1242`.

## Other recovered controls (not wired)

- `SetVolume` writes **register 3** (the prescale register, live stock `0x1b`)
  with the argument's low byte, then reads registers 3..5 and verifies reg3.
  It is not the UI volume: UI volume goes through speaker gain/AQ tables.
  Wiring it needs a general amp register read; `oa_amp_i2c` currently exposes
  the fixed 03..05 read only.
- `SoftMute` reads register 2, sets/clears bits `0x0e`, and writes it back;
  state byte `+6` records the accepted request. Needs the same general read.
- `HP_Mute` uses GPIO function id `0xc3` and `DEV_GPIO_SetLevel(pin,
  level_on/off)`; the descriptor's pin/levels come from the live profile and
  are not recovered offline. This is a normal GPIO, not the amp I2C path.
- `AMP_AD82587_Term`/`PowerDown` are NULL for this object; see AMP-SHUTDOWN.

## Limits

- No hardware balance/soft-mute/volume run; the formula and command plumbing
  are host-tested only.
- `amp-balance` writes the vendor regs 4/5 but the audible/electrical effect is
  unverified. The stock profile has 5/5; balance 50 writes 0/0 per the vendor
  formula.
- The control runs as typed `oa_amp_owner` worker jobs since AMP-JOBS; the
  host loop only submits and polls. A pending job or untaken result still
  returns `-EBUSY`.
- UI volume/range is a separate contract (AUDIO-VOLUME/AQ), not this command.
