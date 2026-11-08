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

## What OpenAurora has

`mod_panel` already implements the recovered sequence:

- `panel_power_on`: GPIO 110 on, `on_delay_ms`, LVDS on, PWM restore,
  `bl_on_delay_ms`, GPIO 111 on.
- `panel_power_off`: `off_bl_delay_ms`, GPIO 111 off, `off_lvds_delay_ms`,
  LVDS off, `off_panel_delay_ms`, PWM 0, GPIO 110 off.
- `standby_lvds`: VPLT `0x1cc` with the mute byte.
- `Hal_Gpio_Write` for pins 110/111 (`DEV_GPIO_SetLevel` uses the same HAL
  call for pins below 168; the device-layer table is not required for these
  two pins).
- `panel.standby` gates suspend/resume; the defaults are exactly the runtime
  values above (`30/580/220/30`, GPIO 110 on=1, GPIO 111 on=0).

Added 2026-11-08: `panel.power_on_start` (default 0) runs the same power-on
sequence at `m_start` for a cold boot. It is off for takeovers because stock
already owns the panel; set it to 1 only when booting without stock.
`PLAT_PQ_SHELL_SetAutoBacklight` and the TCON PMIC GPIO are omitted/disabled
because the latter's gates are 0 on this set; the former has no owner here.

`tests/panel_standby.c` covers the suspend/resume order and the opt-in cold
start order with fake PWM/GPIO/VPLTIO. This is a userspace sequence proof, not
visible panel acceptance.

Do not call vendor `DEV_PANEL_*` code without the device-layer GPIO state
(`DEV_GPIO_Init`) and the VIU/panel-extra state.

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

## Runtime values from the stock process (read-only)

Read from `/proc/1770/mem` (stock `aurora.elf`, PID verified by cmdline) with
`dd`/`od` only. No write, ptrace stop, key or display operation.

`PLAT_VIU_GetPanelExtraStruct` (`0x019d4d64`, 0x5e bytes):

| offset | value | decoded |
|---:|---:|---|
| `+0` | `0x001e` (30) | panel-on -> LVDS-on delay, ms |
| `+2` | `0x0244` (580) | LVDS-on -> backlight-on delay, ms |
| `+4` | `0x00dc` (220) | backlight-off -> LVDS-off delay, ms |
| `+6` | `0x001e` (30) | LVDS-off -> panel-power-off delay, ms |
| `+8` | `0x0064` (100) | PTF PWM level; used as `*10` = 1000 |
| `+0x4a` | `0` | TCON PMIC GPIO gate off |

The Init/power block at `0x019354b4` is
`01 00 0a 00 28 00 20 03 dc 00 37 00 0a 00`:
byte `1`, halfwords `10, 40, 800, 220, 55, 10`. This matches
`DEV_PANEL_Init` and `DEV_PANEL_SetPowerSequenceParams(NULL)`.

Other stock state:

| address | value | meaning |
|---|---|---|
| `0x01761876` | `1000` | `gu16PanelHighPeriodIn1000` |
| `0x01761874` | `1000` | `gu16OrgPanelHighPeriodIn1000` |
| `0x0176186c` | `1` | `gbPowerOffOnDelay` |
| `0x019354c4` | `e8 03 e8 03 00 00 00 00` | `gstBacklightPara`: two 1000 halfwords |
| `0x01935539` | `0` | TCON PMIC second gate |
| `0x019354e0` | all zero | `u32TimeOff`/`u32TimeOffOnDiff`/`u32TimeOn` |

GPIO table pointer at `0x01931270` was `0x031c7000`. Entries (32 bytes,
`GetDescription` returns `entry+8`):

```text
GPIO 110: ff ff ff ff ff ff ff ff 6e 00 00 00 6e 00 00 00
          02 00 00 00 00 00 00 00 02 00 00 00 01 00 00 00
GPIO 111: ff ff ff ff ff ff ff ff 6f 00 00 00 6f 00 00 00
          02 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00
GPIO 108: ff ff ff ff ff ff ff ff 6c 00 00 00 6c 00 00 00
          00 00 00 00 01 00 00 00 02 00 00 00 02 00 00 00
```

`DEV_PANEL_Init` copies 32 bytes from `entry+8`, so:
`DAT_01935460` (panel pin) = 110, `DAT_01935470` (panel flag) = 1;
`DAT_01935498` (backlight pin) = 111, `DAT_019354a8` (backlight flag) = 0.
GPIO 108 (`desc+4` = 108) maps through the PWM table to channel 0.

Consequences for this set (PCB layout 14):

- Panel power on is layout-14 `else` + `DEV_TCON_BIN_Ver_IsMatch`; the TCON gate
  is currently 0, so the recovered `PanelPowerSet(1)` path performs no GPIO
  write. Panel power **off** writes GPIO 110 with `flag != 1` = 0.
- `DEV_TCON_CHIPSET_Set_PMIC_GPIO` is a no-op while `panel_extra+0x4a` and
  `0x01935539` are both 0.
- The effective on sequence is: panel power (possibly gated), 30 ms, LVDS
  unmute, PWM 1000, 580 ms, backlight power on. Off: 70 ms, backlight power
  off, 220 ms, PWM/LVDS mute, 30 ms, panel power off.

These are the values to use for a native panel-power owner. Do not substitute
the Init block (10/40/800/220/55/10) for the panel-extra timing fields.
