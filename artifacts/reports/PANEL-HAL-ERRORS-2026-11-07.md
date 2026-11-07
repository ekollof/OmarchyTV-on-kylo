# Panel HAL error propagation — 2026-11-07

Scope: close the MEDIUM review finding in `modules/panel/panel.c`. Offline
instruction review, host fake-HAL tests and ARM module build; no TV access and
no measured brightness.

## Vendor return contract (verified)

From `libNDAL3.so` MD5 `c842bad804302ec822076c3d9d3ce31d`:

| Function | Address | Result |
|---|---:|---|
| `_hal_Pwm_GetLock` | `0x4e7c0bc0` | always returns 1 (`movs r0,#1` after the stack check) |
| `Hal_Pwm_Open` | `0x4e7c0d40` | 0 for NULL, channel >= 14, fd-open failure or ioctl failure; 1 on the success path |
| `Hal_Pwm_Set_ByChannel` | `0x4e7c0fc0` | 0 for NULL, fd < 0, channel >= 13, mode >= 9 or ioctl failure; 1 on success |
| `Hal_Pwm_Init` | `0x4e7c0c38` | 1 = initialized now, 0 = already initialized; both success (semaphore failure is only printed) |

The earlier report warned that a nonzero result is not panel-level readback;
that remains true. It is still the only vendor failure signal available:
`0` is a definite failure, nonzero is the vendor success path.

Raw disassembly: `artifacts/analysis/osd-2026-10-31/oa-pwm-disassembly.txt`;
decompiles in `oa-pwm-current-contract.txt`.

## Fix

`apply()` previously ignored all three results, set `opened = 1` even when
`Hal_Pwm_Open` failed, and always copied the requested percent into the state.
Now:

- `Hal_Pwm_Init` is called but its 0/1 is informational and not treated as a
  failure.
- `Hal_Pwm_Open == 0` returns `-EIO`, calls `Hal_Pwm_Close` to release any fd
  cached before the failed ioctl, and leaves `opened = 0` so the next set
  retries the open.
- `Hal_Pwm_Set_ByChannel == 0` returns `-EIO` without changing the cached
  percent.
- `percent`/`have_percent` are updated only by a successful set. `iface_get`
  returns the last applied percent or `-1` when no set has succeeded.
- A failed initial apply logs a warning but still publishes the service so a
  later set can retry; `m_resume` propagates the failure so the host keeps the
  module retryable.
- `panel-state` help now says "last applied panel percent (-1 unknown)".

On host builds (`OA_NO_VENDOR`) the configured start value remains the
simulated state, preserving the existing lifecycle tests.

## Tests

`check-panel-curve` (`tests/panel_hal.c`, fake HAL):

- Init `0` (already initialized) still opens and sets.
- Open `0` -> `-EIO`, no Set call, the previous percent is retained, the cached
  fd is closed; the next set retries and succeeds.
- Set `0` -> `-EIO`, the previous percent is retained; a later set succeeds.
- Failed initial open -> start still publishes, `get == -1`, then a set retries
  and reports the new value.
- Negative check: ignoring the Open/Set results fails at
  `set_backlight_percent(40) == -EIO`.

Existing host panel lifecycle and plat-consumer tests still pass.

## ARM

```text
46a3c3ff8ced3eac4876039442045d2c537c4f937e099a14e0d5af9f8d19d967  build-tv-v3/modules/mod_panel.so
```

## Limits

- No measured brightness, PWM duty or electrical acceptance; the vendor scalar
  is not a hardware readback.
- A successful Open/Set return does not prove the panel changed (lock/ioctl
  result only). The owner-confirmed backlight gate remains open.
- `-1` is a sentinel in the existing int interface; consumers must treat it as
  unknown rather than a percent.
