# Plat-consumer socket lifecycle — 2026-11-05

Host-only test. No TV, HAL, GPIO, I2C, UART, PWM, audio device, video device or
framebuffer is touched.

## What was added

`tests/plat_fixture.c` is a host-only module that publishes `plat:v1` with
`is_ready() == 1` and `stage_errors() == 0`. It has no HAL calls and does not
initialize any platform resource. It exists so the consumers that declare
`requires = plat` can be exercised through the real socket without loading the
hardware `mod_plat`.

`tests/plat_consumers_socket.cjs` starts a real `openaurora --sock` host from a
temporary modules directory containing the fixture plus the real host-build
`audio`, `source`, `ir` and `panel` modules. Configuration keeps every hardware
path disabled:

- `audio.enable=0`
- `source.enable=0`
- `ir.device=` (empty, UART closed)

## Verified

1. `plat_fixture` starts and publishes `plat:v1`.
2. `audio`, `source`, `ir` and `panel` all load/start and publish
   `audio:v1`, `source:v1`, `input:v1` and `panel:v1`.
3. `reload plat_fixture` is refused while those consumers are active, and the
   provider stays started.
4. After stopping all four consumers, `plat_fixture` reloads and republishes.
5. All four consumers start again; `audio`, `source`, `ir` and `panel` reload
   successfully (audio's asynchronous quiesce is retried).
6. A fresh unload/load/start cycle works for the fixture and all consumers.
7. SIGTERM exits 0 and removes the control socket.

```
ok actual plat-consumer IPC: audio/source/ir/panel services, plat hold refusal, dependency-ordered reload and reuse
```

The test passed five consecutive runs.

## Evidence

- `artifacts/analysis/plat-consumers-socket-2026-11-05/check.txt`
- `repeated.txt` — five consecutive passes
- `full-host-check.txt` — full strict host `make check`: **403 ok**
- `identities.txt` — host, fixture, audio, source, ir and panel hashes
- `verify.cjs`: **45 groups**

## Limits

This proves module/service lifecycle and dependency ordering under a fake plat
provider. It does not prove the real `mod_plat` HAL sequence, audio routing,
GPIO/I2C, UART input, PWM/panel control, physical devices, vendor coexistence,
hardware-module socket reload or cold startup. The fake provider intentionally
does not emulate HAL return values or hardware effects.
