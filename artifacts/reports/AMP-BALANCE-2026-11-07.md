# AD82587 balance and control registers — 2026-11-07

Status: current (SetVolume/GetVolume/SoftMute wired 2026-11-16; HP_Mute
wired 2026-11-17 from the embedded DTB)
Last verified: 2026-11-17 (firmware instructions, DTB descriptor, host tests
and ARM build)
Evidence class: firmware instructions, extracted DTB and host fake-service
tests; no TV GPIO write or audible/electrical amp acceptance.

Scope: recover the vendor AD82587 balance/volume/soft-mute/HP-mute register
and GPIO contracts and implement them with tests. The HP_Mute descriptor came
from the kernel DTB in the NAND dump, not from a live query. Offline
Ghidra/instruction work; no TV access.

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

## Other recovered controls (SetVolume/SoftMute/GetVolume wired 2026-11-16)

- `SetVolume` writes **register 3** (the prescale register, live stock `0x1b`)
  with the argument's low byte, then reads reg3 back and returns vendor `1`
  only when the byte matches (`AMP_AD82587_SetVolume` @ `0x8752a8`,
  instruction-checked: `_DEV_AMP_WriteByte`/`_DEV_AMP_ReadIIC` use 1=success).
  It is not the UI volume: UI volume goes through speaker gain/AQ tables.
- `GetVolume` reads reg3 (`AMP_AD82587_GetVolume` @ `0x87551c`).
- `SoftMute` reads register 2, ORs `0x0e` for on / ANDs `0xf1` for off, writes
  it back and returns the vendor boolean (`AMP_AD82587_SoftMute` @ `0x875148`,
  instruction-checked). The vendor state byte `+6` is not reproduced offline.
- 2026-11-16: `common/amp_controls.c` models these three registers with the
  injected vendor-boolean transport; `oa_amp_i2c_read_regs()` adds the general
  read; the amp owner has typed VOLUME/SOFTMUTE/GET_VOLUME jobs (blocking I2C
  only on the worker), and `mod_audio_amp` publishes `audio.amp` v3 with
  `set_volume`/`get_volume`/`soft_mute`/`hp_mute`/`hp_state` behind default-off
  `audio_amp.extended_controls=1`. Control: `amp-volume <0..255>`,
  `amp-volume-get`, `amp-softmute <0|1>`, `amp-ext-state`. Host tests pass
  (`check-amp-controls`, owner jobs, `check-audio-amp-jobs`), full suite rc=0
  and ARM `mod_audio_amp.so` `5cfe5825…` builds. No hardware/audible run.
- `HP_Mute` (`AMP_AD82587_HP_Mute` @ `0x008753f8`, instruction-checked) calls
  `DEV_GPIO_GetDescription(0xc3)`, then writes descriptor[1] (pin) at
  descriptor[4]/[5]: normal (`+0x10`) for `on == 1`, active (`+0x14`)
  otherwise. It returns vendor `1` on the amp/descriptor path even when
  `DEV_GPIO_SetLevel` fails; only a NULL amp or absent descriptor returns `0`.
  Since pin >= 200, `DEV_GPIO_SetLevel` selects the STBC dispatch
  (`(pin+0x38)&0xff`, level 1 -> command 2, 0 -> 3).
- 2026-11-17: the descriptor was recovered offline from the kernel DTB
  embedded in the NAND dump (`0xac0040`, totalsize `0x11bb3`). Its
  `func_pin_tbl` gives `HP_MUTE` id `0xc3`: pin **207**, mode output, normal
  **high**, active **low**; `AMP_MUTE` id `0xc4` is pin 208 normal high/active
  low, and `EARPHONE` is pin 117 input normal high/active low. The live
  read-only queries in DEV-GPIO-PROVIDER (EARPHONE 117, HDMI 274..277) match
  the same table, validating the parse. Extraction is repeatable with
  `openaurora/tools/re/dtb-func-pin.py`; the blob and full table are in
  `artifacts/analysis/dtb-2026-11-17/` (DTB MD5
  `7cda0296a5a1be944df93120aab26085`).
- HP_Mute is wired through `common/amp_controls.c`
  (`oa_ad82587_hp_mute`, vendor-boolean level callback with the result
  ignored), `oa_amp_board`'s `hp_mute={207,1,0}` record, a typed
  `OA_AMP_JOB_HP_MUTE` owner job and `audio.amp` v3 `hp_mute`/`hp_state` behind
  `audio_amp.extended_controls=1`; control commands `amp-hp-mute <0|1>` and
  `amp-ext-state` (hp fields). The board level dispatch was corrected to select
  STBC at `pin >= 200` rather than only pin 208.
- ABI: `oa_audio_amp_status` keeps its v2 size. HP state is the separate v3
  `hp_state` accessor, and the v1 table's `state` copies only the bytes before
  `volume_known`, so an older caller's smaller buffer is never overrun.
  `check-audio-amp-jobs` includes a guard test for that prefix write. Host
  tests pass, full suite rc=0, ARM `mod_audio_amp.so` `728feca7…` builds. No
  TV GPIO write; STBC needs the platform video authority as documented in
  AMP-VIDEO-OWNER.
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
