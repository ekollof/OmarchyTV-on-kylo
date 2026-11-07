# Native amp transport composition — 2026-11-07

Scope: recover the normal-GPIO/STBC write contract used by the AD82587 init
sequence and compose it with the existing I2C/owner/app-init code. This is
offline work with injected fakes; no device was opened or written and no TV
access occurred.

## Sources

| Asset | MD5 | Addresses |
|---|---|---|
| `aurora.elf` | `64c252340d3af6c5d66072611b7bad7a` | `DEV_GPIO_SetLevel` `0x008516c4`, `DEV_GPIO_SetDirection` `0x00851978`, `PLAT_SYS_SetStbcGpio` `0x008c6628` |
| `libNDAL3.so` | `c842bad804302ec822076c3d9d3ce31d` | `Hal_Gpio_Write` `0x4e7be754`, `Hal_Gpio_Config` `0x4e7be880` |
| `libNDAL_EX.so` | `e6a465861d1869b8b138b3f299c89f87` | `NDAL_VideoExt_STBC_SetStbcGpio` `0x4db5a3b8`, `ndcommon_v4l2_ioctl` `0x4db66c70`, `ndcommon_v4l2_set_fd` `0x4db66cdc`, `NDAL_VideoExt_OpenDevice` `0x4db59d10` |

Projects: `artifacts/workbench/ghidra_proj` (Aurora) and
`artifacts/workbench/ghidra_ndal_ex` (ndal_ex), read-only.

## Recovered dispatch

`DEV_GPIO_SetLevel(pin, level)` (`0x008516c4`):

```text
pin <  200: rc = Hal_Gpio_Write(pin, level)        // rc == 0 -> success
pin >= 200: cmd = level ? 2 : 3
            rc = PLAT_SYS_SetStbcGpio((pin + 0x38) & 0xff, cmd)
```

`DEV_GPIO_SetDirection(pin, direction)` (`0x00851978`):

```text
pin <  200: rc = Hal_Gpio_Config(pin, direction)   // rc == 0 -> success
pin >= 200: direction == 0 -> PLAT_SYS_SetStbcGpio((pin + 0x38) & 0xff, ?)
            direction != 0 -> vendor no-op success
```

The STBC direction-0 call's second argument is not recovered; `oa_amp_init`
only requests direction 1 for the mute pin, which is the vendor no-op path.

`PLAT_SYS_SetStbcGpio(pin, cmd)` forwards to
`NDAL_VideoExt_STBC_SetStbcGpio(pin, cmd & 0xff)` and prints a failure when
the result is 0.

## Instruction-level return conventions

- `Hal_Gpio_Write`: `670a` claim, `670e` (0), `670c` (level); returns 0 on
  success, -1 on an ioctl failure.
- `Hal_Gpio_Config`: maps direction 0 -> `670e` value 1 and direction 1 ->
  value 0, after the same `670a` claim; returns 0/-1.
- `ndcommon_v4l2_ioctl`: `v4l2_ioctl(fd, cmd, arg)`, then
  `r0 = iVar1 + 1; if (r0 != 0) r0 = 1`; returns **1 on success, 0 on
  failure**. `_send_v4l2_command` returns that value unchanged, so
  `NDAL_VideoExt_STBC_SetStbcGpio` is vendor-boolean.
- `ndcommon_v4l2_ioctl` uses the process-global fd installed by
  `ndcommon_v4l2_set_fd`; `NDAL_VideoExt_OpenDevice(0, index)` opens the
  table-selected video device and installs that fd. The STBC call therefore
  requires the platform video open first (`mod_plat` currently performs
  `NDAL_VideoExt_OpenDevice(0,10)`).

The two GPIO wrappers issue the `670a` claim. They are not view-only and must
run only under a caller that owns the GPIO device; the earlier GET-only query
contract is unaffected.

## Implementation

`include/openaurora/amp_vendor.h` + `common/amp_vendor.c` compose the existing
pieces into `oa_amp_init_io`:

- `pin < 200` -> injected `gpio_write`/`gpio_config` (default:
  `Hal_Gpio_Write`/`Hal_Gpio_Config`).
- `pin >= 200` -> injected `stbc_set` with
  `pin' = (pin + 0x38) & 0xff`, `level 0 -> 3`, `level 1 -> 2` (default:
  `NDAL_VideoExt_STBC_SetStbcGpio`).
- register writes -> injected `i2c_write`, else the borrowed `oa_amp_i2c`
  session via `oa_amp_i2c_write_cb`.
- delays -> injected callback or `nanosleep`.
- subwoofer -> disabled/no-op; the helper path is not recovered.

The board records (`reset 114`, `mute 208`) come from `oa_amp_board`; the
composition owns no device and performs nothing until `oa_amp_init` invokes a
callback.

## Tests

`make check-amp-vendor` passes. It covers:

- record selection from the board, pin routing and the STBC no-op direction;
- `208 -> (8,3)` and `(8,2)` command mapping, with vendor-boolean failure;
- normal GPIO errno-to-boolean mapping (`0` success only);
- 38 register writes through the injected callback and fail-closed without a
  session;
- the full `oa_amp_init` sequence through the composition (prelude, direction,
  reset pulses, mute commands, 38 writes, subwoofer policy);
- STBC failure does not stop Init and leaves the mute cache unchanged;
- NULL-argument rejection.

ARM compile/link check: `common/amp_vendor.c` + `common/amp_i2c.c` build with
the TV toolchain; the only unresolved vendor symbols are `Hal_Gpio_Write`,
`Hal_Gpio_Config` and `NDAL_VideoExt_STBC_SetStbcGpio`, all exported by the
shipped libraries. No GLIBC newer than 2.17 is required.

## Limits

- No hardware run. The GPIO claim, reset pulse, mute command and register
  writes are not accepted until an owner-authorized audio window.
- The STBC default fails closed only if the platform global fd is invalid;
  the caller must run the platform video open first.
- No module or CLI consumer is wired yet; `amp_owner` and this composition are
  library pieces.
- Paired shutdown/standby, amp readback and audible output remain
  unimplemented.
- Subwoofer helper behavior is not modeled beyond disabled/no-op.

## Next

1. A gated, reloadable amp consumer (module or audio start option) that opens
   `oa_amp_i2c`, runs `oa_amp_init` on `amp_owner`, and exposes completion
   without blocking the loop.
2. Owner-authorized hardware run with mute/reset/register writes and audible
   confirmation, then restore.
