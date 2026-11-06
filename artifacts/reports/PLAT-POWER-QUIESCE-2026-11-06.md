# mod_plat / mod_power quiescence decision — 2026-11-06

Audit of the last two synchronous modules and a real control-socket test of
their synchronous stop/reload path. Host build only: `plat` runs its
`OA_NO_VENDOR` stubs, `power` is disabled and never enters RAM. No HAL,
GPIO/I2C/audio/DRM, CEC, tuner, PVR or standby hardware was touched.

## Decision

Neither module gets a `quiesce()` hook. Neither owns a worker, a queued
callback, a heap cleanup or a reversible device close that could be deferred.
An asynchronous gate would require inventing disposable state or delay, and a
hook returning 0 is the ABI promise that workers and fallible device releases
are complete — for these modules there is simply nothing to release, so stop
already finishes synchronously. The absence of the hook is therefore the
correct contract, not an unfinished migration, and is now recorded here and in
`docs/ARCHITECTURE.md`.

The remaining gate count is **33/35 deferred/scheduler-backed**; `plat` and
`power` are the two audited synchronous stops. All 35 descriptors now have an
explicit lifecycle decision.

## mod_plat

- Publishes `plat` v1 from `m_start`; consumers (`audio`, `source`, `ir`,
  `panel`) require/hold it, so an ordinary stop/reload is refused while they
  are running.
- `m_start` runs the vendor-order bring-up: audio Protect/Mixer/Proxy prelude,
  vpltio/v4l2/share-memory/DRM, GPIO/I2C init, `plat_vin_slot_init` defaults and
  `NDAL_VideoExt_OpenDevice(0, 10)`.
- `m_stop` unpublishes `plat` and clears `ready`. That is the whole teardown.
- The firmware has no recovered de-init for this HAL sequence, and
  `OpenDevice(0,10)` installs a process-global fd, not a per-module lease.
  Consequently a reload is **not** a driver/HAL reset. A real-HAL reload after
  bring-up remains unaccepted; the host-stub reload only proves lifecycle
  ordering.
- `plat.c` now states this at `m_stop`, so the limitation is not hidden in a
  report.

## mod_power

- Publishes `power` v1 and holds `host.sleep` v1; stop unpublishes and drops the
  refcount synchronously.
- The only fd (`/sys/power/state`) is opened, written once and closed inside the
  final entry callback. Preparation/resume hooks are empty.
- `MODULE-SAFETY-2026-10-31` already recorded that the module has no persistent
  device/worker to quiesce; the RAM write exists solely after coordinated
  preparation. `power.c` now repeats that at `m_stop`.
- `save()`/`restore()` carry the accepted-full-write counter (4 bytes), which
  still survives reload; it is not proof of physical sleep.

## Verification

`check-plat-power-sync-stop` starts the real host modules and asserts service
invalidation plus immediate completion:

```text
load plat -> ok plat; start plat -> ok
status plat -> state started, plat:v1
reload plat -> ok (1 ms); state started, plat:v1
stop plat -> ok (0 ms); state loaded, no plat:v1
start plat -> ok; unload plat -> ok

load power -> ok power; start power -> ok (power.enable=0)
status power -> state started, power:v1
reload power -> ok (1 ms); "power: restored 4 bytes of state"
status power -> state started, power:v1
stop power -> ok (0 ms); state loaded, no power:v1
start power -> ok; unload power -> ok; quit -> bye
```

```text
ok plat/power sync stop: plat reload 1ms stop 0ms, power reload 1ms stop 0ms, services unpublished
```

The same test under a focused ASan+UBSan build (`detect_leaks=1`,
`halt_on_error=1`) passes:

```text
ok plat/power sync stop: plat reload 1ms stop 1ms, power reload 1ms stop 0ms, services unpublished
```

The existing `power_cycle` wrapped-I/O groups still pass in the full suite, as
do the plat consumer hold/refusal tests. Full suite ran visibly and ended
`all tests passed` (rc 0); QEMU sub-launcher value/live/launcher checks pass.

## Limits

- This is a host-stub lifecycle test. It does not run the vendor HAL, audio
  prelude, DRM, VIN, GPIO/I2C, MCU watchdog or a real `mem` suspend.
- No on-TV run was attempted: starting real `plat` would take vendor HAL
  ownership and entering standby would suspend the set. Both are outside this
  test's authorization and unnecessary for the lifecycle decision.
- Real-HAL `reload plat` after bring-up and any native standby preparation/
  restoration remain unaccepted. Cold startup, scanout, real audio/CEC/tuner/
  PVR/VIN hardware and broad UI parity are still open.
