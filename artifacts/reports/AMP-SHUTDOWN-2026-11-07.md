# AD82587 paired-shutdown contract — 2026-11-07

Scope: determine what the vendor AD82587 amp object actually does for
Term/PowerDown/Close, so the module does not invent a shutdown sequence and
the remaining amplifier gap is stated correctly. Offline Ghidra/instruction
work only; no TV access.

## Sources

| Item | Address |
|---|---|
| `DEV_AMP_Term` | `0x0084f55c` (calls object `+0x54` if non-NULL) |
| `DEV_AMP_PowerDown` | `0x0084f904` (calls object `+0x68` if non-NULL) |
| `DEV_AMP_GetPowerDownStatus` | `0x0084f94c` (calls `+0x6c` if non-NULL) |
| `DEV_AMP_Reset` | `0x0084f87c` (calls `+0x60`) |
| `DEV_AMP_Mute` | `0x0084f98c` (calls `+0x70`) |
| `AMP_AD82587_Create` | `0x008755d0` |
| `AMP_AD82587_Close` | `0x00874c48` |

## AD82587 object vtable (from Create)

`Create` allocates 0xd8 bytes and installs these callbacks (word offsets):

| Offset | Callback |
|---|---|
| `+0x44` | `AMP_AD82587_Init` `0x874ef1` |
| `+0x48` | `AMP_AD82587_GetCap` `0x874c05` |
| `+0x4c` | `AMP_AD82587_Open` `0x875599` |
| `+0x50` | `AMP_AD82587_Close` `0x874c49` |
| `+0x60` | `AMP_AD82587_Reset` `0x874dad` |
| `+0x64` | `AMP_AD82587_GetResetStatus` `0x874c81` |
| `+0x70` | `AMP_AD82587_Mute` `0x874e51` |
| `+0x74` | `AMP_AD82587_GetMuteStatus` `0x874ccd` |
| `+0x78` | `AMP_AD82587_SoftMute` `0x875149` |
| `+0x7c` | `AMP_AD82587_GetSoftMuteStatus` `0x874d19` |
| `+0x80` | `AMP_AD82587_SetVolume` `0x8752a9` |
| `+0x84` | `AMP_AD82587_GetVolume` `0x87551d` |
| `+0x90` | `AMP_AD82587_SetBalance` `0x875205` |
| `+0x94` | `AMP_AD82587_GetBalance` `0x874d65` |
| `+0xa8` | `AMP_AD82587_RegDump` `0x875365` |
| `+0xcc` | `AMP_AD82587_HP_Mute` `0x8753f9` |
| `+0xbc` | `AMP_AD82587_IsAlive` `0x875459` |

The slots used by Term/PowerDown/GetPowerDownStatus (`+0x54`, `+0x68`,
`+0x6c`) are **never assigned** and remain NULL. `AMP_AD82587_Close` is an
empty function (only an unreachable return block).

## Instruction check

`DEV_AMP_Term` at `0x84f59c` executes `ldr r3,[r0,#84]` (+0x54), and
`DEV_AMP_PowerDown` at `0x84f926` executes `ldr r3,[r0,#104]` (+0x68).
`AMP_AD82587_Create` stores callbacks at +0x44/0x48/0x4c/0x50/0x60/0x64/
0x70/0x74/0x78/0x80/0x90/0xbc and the other populated slots; there is **no
store to +0x54, +0x68 or +0x6c**. Raw disassembly is saved in
`artifacts/analysis/amp-shutdown-2026-11-07/` (`create.dis`, `term.dis`,
`powerdown.dis`).

## Consequence

For this board/profile there is **no vendor paired shutdown**:

- `DEV_AMP_Term` is a no-op,
- `DEV_AMP_PowerDown` is a no-op,
- `AMP_AD82587_Close` is empty.

The only state-changing primitives are `Init`, `Reset`, `Mute`, `SoftMute`,
volume/balance and `HP_Mute`. A module stop therefore cannot "match" a hidden
vendor Term/PowerDown; the stock app also leaves the amp as-is and relies on
MCU/standby/panel power policy. The previous TODO wording ("paired shutdown
missing") conflated a nonexistent amp callback with the standby/amp-rail
policy.

Safe module behavior remains: close the I2C session on quiesce; do not invent
a reset/mute-on-stop unless it is an explicitly documented policy deviation.
`amp-mute 0/1` already exposes the vendor mute primitive; the hardware run
showed Init ends muted and `amp-mute 0` restores I2C/audio readiness.

## Open

- Audible output/owner confirmation is still the amplifier acceptance gate.
- If a standby policy is desired, it belongs to the MCU/panel/power owner, not
  a fake AD82587 Term callback. Recover that policy separately.
- No new code change: the module already closes I2C on quiesce and does not
  call a nonexistent shutdown. The TODO/FINDINGS wording is corrected.
