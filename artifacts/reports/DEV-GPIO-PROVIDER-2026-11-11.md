# DEV GPIO descriptor provider and HDMI hotplug pulse — 2026-11-11

Status: current
Last verified: 2026-11-11
Evidence class: firmware (Aurora, libNDAL3, libNDAL_EX, kernel ntgpio),
on-TV read-only descriptor query, ARM qemu and host tests.
Supersedes: the unwired hotplug disposition in
[COLD-START-SEQUENCE-2026-11-10](COLD-START-SEQUENCE-2026-11-10.md); the
`0x008fc3b0` behavior is corrected there and here (MHL skip and runtime pins).

## Firmware contract

### DEV_GPIO_GetDescription

`DEV_GPIO_GetDescription` at `0x00850c38` (Aurora, MD5
`64c252340d3af6c5d66072611b7bad7a`) issues ioctl `0xc088671f` on
`/dev/ntgpio` with a zeroed 0x88-byte buffer. The buffer input is a pointer to
the 24-byte DTS function-name record at `+0x80` (`au8GPIOFuncName + id*24`,
table `0x0175f350`). The kernel ntgpio case `0x1f` (`0x0003d0e0`) looks the
name up in `func_pin_tbl` and copies back:

| offset | contents |
|---:|---|
| `+0x00` | pin = `gpio_fun_pin[1]` |
| `+0x04` | `gpio_fun_pin[2]` |
| `+0x08` | `gpio_mode` string (`input`/`output`) |
| `+0x20` | `normal` string (`low`/`high`) |
| `+0x38` | `active` string (`low`/`high`) |
| `+0x50` | `idle` string |
| `+0x68` | `int_type` string |

The function caches one 0x20-byte record per id at `0x01931270`:
`+0x08` id, `+0x0c` word0/pin, `+0x10` mode (output 2, input 1, other 0),
`+0x14` word1, `+0x18` normal, `+0x1c` active. It returns `record+8`; the
hotplug task reads `[ret+4]` as the pin and `[ret+0x14]` as the active level
(0 low, 1 high, 2 other). Any ioctl failure returns NULL.

The kernel copies the name from the userspace pointer and does not read a
separate id field. The 24-byte `au8GPIOFuncName` entries used here are
`MHL_CDSENSE` (192), `HDMI_HOTPLUG_5` (193) and `HDMI_HOTPLUG_1..4`
(204..207), matching the GPIO name table in
`artifacts/analysis/vendor-inventory-2026-11-07/gpio-function-names.tsv`.

### DEV_GPIO_GetLevel / SetLevel

`DEV_GPIO_GetLevel` at `0x0085145c`:

- pin < 200: `Hal_Gpio_Read(pin, &level)`; success is `rc == 0`.
- pin >= 200: `PLAT_SYS_GetStbcGpio((pin+0x38)&0xff, &level)`; the value comes
  from the NDAL result.

`DEV_GPIO_SetLevel` at `0x008516c4` rejects level > 1 and pin > `0x11a`
before any device call, then:

- pin < 200: `Hal_Gpio_Write(pin, level)`.
- pin >= 200: `PLAT_SYS_SetStbcGpio((pin+0x38)&0xff, level ? 2 : 3)`.

`PLAT_SYS_GetStbcGpio` (`0x008c66f8`) calls
`NDAL_VideoExt_STBC_GetStbcGpio(channel, &value, &status)`; `SetStbcGpio`
(`0x008c6628`) calls `NDAL_VideoExt_STBC_SetStbcGpio(channel, value & 0xff)`.
Both NDAL calls return the V4L2 send result (nonzero success) and require the
STBC device opened by `NDAL_VideoExt_OpenDevice` in the platform prelude.

## Live TV result (read-only)

`tools/tv-gpio-describe.c` was built static for ARM, uploaded to `/tmp`,
verified (sha256 `1067392a…`, 2,781,452 bytes) in a separate connection, run,
and removed. It issued only `DEV_GPIO_GetDescription` (`0xc088671f`); no pin
claim, level write, export, `0x6701/0x6702` or init call.

```text
id=192 name=MHL_CDSENSE    rc=-1 errno=524 pin=0 ...
id=193 name=HDMI_HOTPLUG_5 rc=-1 errno=524 pin=0 ...
id=204 name=HDMI_HOTPLUG_1 rc=0 pin=274 mode='output' normal='low' active='low' idle='default' int_type='no_int'
id=205 name=HDMI_HOTPLUG_2 rc=0 pin=275 mode='output' normal='low' active='low' idle='default' int_type='no_int'
id=206 name=HDMI_HOTPLUG_3 rc=0 pin=276 mode='output' normal='low' active='low' idle='default' int_type='no_int'
id=207 name=HDMI_HOTPLUG_4 rc=0 pin=277 mode='output' normal='low' active='low' idle='default' int_type='no_int'
```

The TV has no MHL_CDSENSE or HDMI_HOTPLUG_5 DTS node (`errno 524`). The four
present pins are **274..277**, so the recovered task uses the STBC path, not
`Hal_Gpio_Write`: channels `(274..277 + 0x38) & 0xff` = **74..77**, with value
**2 for inactive (level 1)** and **3 for active (level 0)**.

Raw output: `artifacts/analysis/dev-gpio-2026-11-11/tv-describe.txt`.

## Corrected DEV_SRC_HDMIHotPlug behavior

The task entry is `0x008fc3b0`, created by `dev_src_init` (`0x008fc654`,
stack `0x1000`, priority `0x10`). It is one-shot:

1. At loop index 0, resolve MHL 192. If found, read its level.
2. Drive each present hotplug pin to inactive `(active == 0) ? 1 : 0`.
3. Delay 500 ms.
4. Re-read MHL at index 0. Drive each present pin to its active level.
5. Missing hotplug descriptors log
   `#DEV-SRC#Not found HDMI%d HotPlug Definition`.

The MHL behavior is stateful and easy to misread: when the MHL level is high,
the task logs `#DEV-SRC#MHL CD_SENSE Is High` and **skips GPIO 204 in that
loop**. The level is re-read after the delay, so a level that changes between
the loops changes whether 204 is driven inactive, active, or neither. A read
failure logs `%s,Get CDSense level of pin %d failed` and continues with 204.
The earlier `0x008fc3b0` description missed this skip; it is corrected here
and in COLD-START-SEQUENCE.

## Implementation

- `openaurora/include/openaurora/dev_gpio.h` — injected transport and the
  recovered descriptor/level contract.
- `openaurora/common/dev_gpio.c` — 0x88 buffer/name/parse logic, pin and level
  validation, STBC channel/value helpers.
- `openaurora/common/dev_gpio_vendor.c` — `/dev/ntgpio` ioctl, `Hal_Gpio_*`
  below 200, `NDAL_VideoExt_STBC_*` at/above 200. The host build compiles the
  stub returning NULL.
- `openaurora/common/src_hotplug.c` — corrected one-shot task, including the
  MHL skip/re-read.
- `openaurora/modules/source/source.c` — worker thread started only when
  `source.enable=1` **and** `source.hotplug_pulse=1` (both default 0). The
  worker opens the provider, runs the pulse and closes it; `quiesce`/`stop`
  join it. `source` already requires `plat`, so the STBC device is opened
  before the worker can run.

`source.hotplug_pulse` is a new, default-off gate. The pulse was subsequently
run on the TV under controlled ownership; see
[HOTPLUG-HW-2026-11-11](HOTPLUG-HW-2026-11-11.md).

## Verification

- `make check` — rc=0 (full host suite), including:
  - `check-dev-gpio`: 0x88 name/ioctl parse, pin/mode/normal/active, missing
    MHL node (errno 524), unknown id, pin/level validation, STBC mapping.
  - `check-src-hotplug`: live 274..277 active-low pulse, MHL high skip,
    MHL re-read change, read failure, missing HDMI3/5, active-high/other,
    invalid ops.
- `make check-qemu-real` — rc=0:
  - `qemu-hotplug-real-check.sh`: real ARM `mod_plat` + `mod_source` with the
    shipped NDAL libraries. The real provider returns pins 274..277
    active-low; the pulse logs the four toggles and uses STBC channels 74..77
    with values 2 then 3. No `Hal_Gpio_Write` occurs for these pins.
  - Existing real fbdev, panel/PWM/GPIO and VIN checks still pass.
- Raw qemu trace:
  `artifacts/analysis/dev-gpio-2026-11-11/qemu-hotplug-io.log`.

## Limits

- **The pulse was run on the TV on 2026-11-11** after `plat` opened the STBC
  device. The pass-through trace showed STBC channels 74..77 with values 2
  (inactive) then 3 (active), and the connected Deck re-ran HDCP22
  init/auth. See [HOTPLUG-HW-2026-11-11](HOTPLUG-HW-2026-11-11.md). The gate
  remains default 0; this was a controlled, temporary run.
- The provider is not a general DEV GPIO implementation: it maps only the six
  hotplug names and returns "absent" for other ids. It does not reproduce the
  firmware's 0x1fec descriptor cache or the DEV GPIO/PWM initialization state.
- On qemu, NDAL's `Hal_IfCmd` socket was `-1`; the V4L2 fallback carried the
  STBC ext-controls. On the real TV, `plat` reported
  `Hal_IfCmd_SockFd_Get@139 iSockFd=6` and the same V4L2 ext-control path
  performed the traced writes.
- The TV was woken from standby by `tools/tvssh.sh` for the read-only query.
  It was left in stock operation; no settings, services, display or boot hooks
  were changed. Staging was removed and verified.
