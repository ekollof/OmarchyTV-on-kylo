# Embedded DTB and GPIO function descriptors — 2026-11-17

The kernel DTB is embedded in the pristine NAND dump, not in the fs0/ap0/ap1
volumes:

- dump: `artifacts/dumps/tv_nand.bin`
- DTB start: NAND offset `0xac0040` (FDT magic `0xd00dfeed`)
- totalsize: `0x11bb3`
- DTB MD5: `7cda0296a5a1be944df93120aab26085`
- parser: `python3 openaurora/tools/re/dtb-func-pin.py --name HP_MUTE`

`mb230.dtb` is the extracted blob. It is firmware-derived and stays out of
git history (`.gitignore`); re-extract it from the dump when needed.
`func_pin_tbl.tsv` is the parsed record table committed as text.

The table is authoritative for `DEV_GPIO_GetDescription`: each node's
`gpio_fun_pin` is `{group, pin, extra}`, and `gpio_mode`/`normal`/`active` map
to the descriptor fields (input=1/output=2; low=0/high=1/other=2).

Cross-check against the live read-only queries in
`artifacts/reports/DEV-GPIO-PROVIDER-2026-11-11.md`:

| name | DTB pin | DTB mode | DTB normal/active | live query |
|---|---|---|---|---|
| EARPHONE (id 117) | 117 | input | high/low | pin 117 input normal high active low |
| HDMI_HOTPLUG_1 (id 204) | 274 | output | low/low | pin 274 output normal low active low |
| HDMI_HOTPLUG_2..4 | 275..277 | output | low/low | matching pins/levels |

Recovered entries used by AMP-BALANCE:

| name | id | pin | mode | normal | active |
|---|---|---|---|---|---|
| HP_MUTE | 0xc3 | 207 | output | high | low |
| AMP_MUTE | 0xc4 | 208 | output | high | low |

`AMP_AD82587_HP_Mute` @ `0x008753f8` writes the normal level for `on == 1`
and the active level otherwise. Pin 207 is >= 200, so `DEV_GPIO_SetLevel`
selects the STBC dispatch `((207+0x38)&0xff)=7` with level 1 -> command 2 and
level 0 -> command 3.
