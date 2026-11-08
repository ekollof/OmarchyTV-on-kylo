# Panel power sequence — 2026-11-08

Offline recovery; no TV access. Source:
`artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf`,
MD5 `64c252340d3af6c5d66072611b7bad7a`. Instructions from
`artifacts/workbench/aurora.dis`; decompilation with
`DumpFirmware.java --asm` (read-only). This fills the gap left by
`DEV_PANEL_Init` (it sets timing/GPIO/PWM but does not light the panel).

## Entry points

| function | address | return |
|---|---:|---|
| `DEV_PANEL_Init` | `0x008583fc` | PWM channel index (1 on success); does not power the panel |
| `DEV_PANEL_PanelPowerSet(int on)` | `0x0085759c` | void |
| `DEV_PANEL_PanelPowerGet()` | `0x008574ac` | void (reads the panel-power GPIO) |
| `DEV_PANEL_PanelBackLightPowerSet(int on)` | `0x00857780` | void |
| `DEV_PANEL_PanelBackLightPowerGet()` | `0x0085765c` | void |
| `DEV_PANEL_PowerSequence(int on)` | `0x00857ac0` | void; takes `0x0193547c` mutex |
| `DEV_PANEL_SetPowerSequenceParams(void *p)` | `0x00857438` | void; `NULL` restores defaults |
| `DEV_GPIO_PanelPwr_GetLevel()` | `0x008529c0` | void; GPIO description 110 |

## GPIO ownership established by Init

`DEV_PANEL_Init` copies:
- GPIO description **110** (PANELPWR) to `0x0193545c` (32 bytes).
- GPIO description **111** (BKLTPWR) to `0x01935494` (32 bytes).

`DEV_PANEL_PanelPowerSet` uses the GPIO id/level fields at
`0x01935460` / `0x01935470` from the first copy.
`DEV_PANEL_PanelBackLightPowerSet` uses `0x01935498` / `0x019354a8` from the
second copy.

## `DEV_PANEL_PanelPowerSet(on)`

Exact branch shape (decompiler plus `0x008575bc..0x00857612`):

```text
layout = DEV_COMMON_GET_PCBLAYOUTTYPE();
if ((layout - 7) & ~2 == 0) {          /* layouts 7 or 9 */
    if (!on) {
        DEV_GPIO_SetLevel(desc, flag == 1);
        return;
    }
} else if (on) {
    if (!DEV_TCON_BIN_Ver_IsMatch())
        return;
    DEV_GPIO_SetLevel(desc, flag == 1);
    return;
}
DEV_GPIO_SetLevel(desc, flag != 1);
```

`desc` is `[0x01935460]`, `flag` is the word at `[0x01935470]`. The function
does not read a return value. Do not reduce this to `SetLevel(on)` until the
active-low/TCON branches are understood for this PCB layout (14). The
recovered layout check is the vendor's, not a generic rule.

## `DEV_PANEL_PanelBackLightPowerSet(on)`

It has the same layout/TCON gating around the second description and also
writes GPIO 0 in some branches:

- `layout 7/9`: `SetLevel(desc2, flag2 == 1)` for off; for on the final branch
  uses `flag2 != 1`.
- Other layouts: on checks `DEV_TCON_BIN_Ver_IsMatch`; some paths call
  `DEV_GPIO_SetLevel(0,0)` / `(0,1)` in addition to the backlight GPIO.

The exact per-layout GPIO-0 writes are in the decompile at `0x00857780` and
are not generalized here. The recovered default panel block has one
backlight-power GPIO; the extra GPIO 0 write is layout-specific.

## `DEV_PANEL_PowerSequence(on)` — exact order

Locks `0x0193547c` for the whole sequence. `PLAT_VIU_GetPanelExtraStruct()`
returns a timing block; the recovered fields are:

| offset | meaning |
|---:|---|
| `+0` | panel-on -> LVDS-on delay (ms) |
| `+2` | LVDS-on -> backlight-on delay (ms) |
| `+4` | backlight-off -> LVDS-off delay (ms) |
| `+6` | LVDS-off -> panel-power-off delay (ms) |
| `+8` | PTF PWM level (`ushort`, used `*10`) |

Power **off**:

1. If debug byte `0x01935458` is set, `PLAT_SP_DebugWrite(... "panel power off")`.
2. `PLAT_PQ_SHELL_SetAutoBacklight(0)`; on failure log at
   `0x00857adc`/`0x0135af24`.
3. `PLAT_OS_TaskDelay(0x46)` (70 ms).
4. `DEV_PANEL_PanelBackLightPowerSet(0)`.
5. Read `extra+4`; `PLAT_OS_TaskDelay(delay)` after printing
   `"BL Power off -> u16Delayms(%d) -> LVDS data off"`.
6. If `gu16PanelHighPeriodIn1000 == 0`, print the zero warning; otherwise save
   it to `gu16OrgPanelHighPeriodIn1000`.
7. If `0x019354b4 == 0`: `DEV_PANEL_SetPWMBacklightLevel(0)`,
   `plat_vpltio_mute_lvds(0)`, delay `extra+6`, `DEV_PANEL_PanelPowerSet(0)`.
8. Else: if `gu16PanelHighPeriodIn1000 == 1000`, set PWM to `extra+8 * 10`
   (and print the PTF value); otherwise call `DEV_PANEL_SetPWMBacklightLevel()`
   with its current argument. Then `plat_vpltio_mute_lvds(0)`, delay
   `extra+6`, print `"LVDS data off -> u16Delayms(%d) -> Panel Power off"`,
   `DEV_TCON_CHIPSET_Set_PMIC_GPIO(0)`, `DEV_PANEL_PanelPowerSet(0)`, and
   record `u32TimeOff = PLAT_OS_GetTimeMS()`.

Power **on**:

1. If debug, log `"panel power on"`.
2. `u32TimeOn = PLAT_OS_GetTimeMS()`.
3. If `gbPowerOffOnDelay != 0` and `u32TimeOff < u32TimeOn`, compute the
   off->on difference; if it is `< 1000`, print the force-delay message and
   `PLAT_OS_TaskDelay(1000)`.
4. `DEV_PANEL_PanelPowerSet(1)`.
5. `DEV_TCON_CHIPSET_Set_PMIC_GPIO(1)`.
6. Read `extra+0`; delay after `"Panel Power on -> u16Delayms(%d) -> LVDS data on"`.
7. `plat_vpltio_mute_lvds(1)`.
8. `DEV_PANEL_SetPWMBacklightLevel(gu16PanelHighPeriodIn1000)`.
9. Read `extra+2`; delay after `"LVDS data on -> u16Delayms(%d) -> BL Power on"`.
10. `DEV_PANEL_PanelBackLightPowerSet(1)`.

Unlock the mutex. There is no explicit return value; the caller uses the side
effects.

## `DEV_PANEL_SetPowerSequenceParams(NULL)` defaults

`NULL` writes the same block `DEV_PANEL_Init` stores at `0x019354b4`:
`byte 1`, halfwords `+2=10`, `+4=40`, `+6=800`, `+8=220`, `+10=55`, `+12=10`.
A non-null pointer copies 16 bytes (`3 words + 1 halfword`) to the same block.
This block is distinct from the `PLAT_VIU_GetPanelExtraStruct` timing fields
used by `PowerSequence`; do not conflate them.

## Panel-extra, LVDS mute and TCON PMIC dependencies

`PLAT_VIU_GetPanelExtraStruct` `0x00901520` returns the fixed address
`0x019d4d64`; `PLAT_VIU_SetPanelExtraStruct` `0x00901558` copies `0x5e` bytes
into it. `PLAT_VIU_init_panelextrastruct` `0x009035e8` calls
`FUN_008fec68` `0x008fec68`, which opens a panel text file, skips 42 lines,
then parses successive fields into the struct (`+0`, `+2`, `+4`, `+6`, `+8`,
...). The path argument comes from the caller
`FUN_007800ac` `0x007800ac` (call site `0x007801a6`); the path is not decoded.
The panel file itself is not in the dump; `./panel.txt` and the
`Panel*File`/`_VOUT_LoadPanelFile` strings identify the family but not this
set's file. Do not invent the timing values; read them from the live/vendor
panel file or the vendor globals.

`plat_vpltio_mute_lvds` `0x0092e0f8` sends VPLT command **`0x1cc`**:

```text
Hal_Video_Vpltio_SendCmd(0x1cc, &unused_word, &mute_byte, 0,0,0,0,0,0)
```

The first data word is not initialized before the call; the mute byte is the
second pointer. The command's semantics are the vendor's; the wrapper does not
check the return.

`DEV_TCON_CHIPSET_Set_PMIC_GPIO` `0x0085b4ac` writes **GPIO 209** (`0xd1`)
only when `panel_extra+0x4a != 0` and byte `0x01935539 != 0`; it sets GPIO 209
to 0 for off and 1 for on. The two gates are board/profile state, not
constants.

## What OpenAurora has and lacks

`mod_panel` currently owns:
- `Hal_Pwm_Open`/`Hal_Pwm_Set_ByChannel` with the vendor 0..1000 conversion;
- the recovered backlight curve (`common/panel_curve.c`);
- `panel.start`, `panel.freq=30000`, `panel.mode=0`, `panel.channel=0`,
  `panel.duty_max=4095`, and an opt-in `panel.standby` flag.

It does **not** own:
- the GPIO 110/111 descriptions or the timing block;
- `DEV_PANEL_PanelPowerSet`/`PanelBackLightPowerSet`;
- `DEV_PANEL_PowerSequence` with the panel-extra delays;
- `plat_vpltio_mute_lvds`, `DEV_TCON_CHIPSET_Set_PMIC_GPIO`,
  `PLAT_PQ_SHELL_SetAutoBacklight`, or `PLAT_VIU_GetPanelExtraStruct`.

A native panel-power owner must supply those or an explicit equivalent. Do not
call vendor `DEV_PANEL_*` code without the device-layer GPIO state
(`DEV_GPIO_Init`) and the VIU/panel-extra state. `panel.standby` is not this
sequence.

## Not recovered

- `PLAT_VIU_GetPanelExtraStruct` field values for this panel/profile and the
  writer of `gu16PanelHighPeriodIn1000`.
- The panel text file path/contents and `FUN_008fec68` field order; the caller
  `FUN_007800ac` was not decoded. The panel file is not in the dump.
- The writer of `DAT_01935539` (TCON PMIC gate).
- `plat_vpltio_mute_lvds` body and its VPLT command/state.
- `DEV_TCON_CHIPSET_Set_PMIC_GPIO` body/GPIO target.
- `PLAT_PQ_SHELL_SetAutoBacklight` body.
- `DEV_TCON_BIN_Ver_IsMatch` result for this set and the per-layout
  `PanelPowerSet`/`BackLightPowerSet` branches for layout 14.
- Whether `DEV_PANEL_PowerSequence` is the only cold-start panel-on caller or
  another caller (e.g. the panel standby/TCON path) drives it.
- GPIO 110/111 description records (pin/level/active-low fields) on this set;
  `DEV_PANEL_Init` copies them from the device layer, which is not owned yet.
