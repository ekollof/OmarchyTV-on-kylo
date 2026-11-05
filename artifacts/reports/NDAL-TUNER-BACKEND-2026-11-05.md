# Live-capable NDAL tuner backend — 2026-11-05

Adds a selectable NDAL/DVB frontend backend behind the existing
`oa_nim_ops`/`oa_tuner_backend` boundary. This is an offline implementation
with mock contract tests only: no TV, frontend device or vendor function was
executed. The default module backend remains the offline stub; `tuner.ts_path`
still selects the TS replay source.

## Grounding

- `libNDAL_EX.so` exports `Nd_NIM_SetScanMode`, `Nd_NIM_GetFrequency`,
  `Nd_NIM_GetDeliverySystem`, `Nd_NIM_GetSymbolRate`, `Nd_NIM_SetTsOutput`
  and `Nd_NIM_SetStandy`. Decompiling the Ghidra project shows they take an
  open frontend fd: `GetFrequency`/`GetSymbolRate` read a comma-separated
  frontend parameter and divide by 1000, `GetDeliverySystem` writes a boolean,
  and the setters use `_ndal_fe_set_parms`.
- `libNDAL3.so` contains `/dev/dvb/adapter%i/frontend%i`,
  `/dev/dvb/adapter%i/demux%i` and `/dev/dvb/adapter%i/dvr%i`, and exports
  `Hal_Demux_Dvb_*`. The configured frontend path is the standard DVB node.
- No public `Nd_NIM_*` set-frequency symbol was recovered. The default tune
  operation therefore uses the Linux DVB `FE_SET_FRONTEND` API for DVB-T and
  returns `ENOTSUP` for other paths instead of guessing vendor-private tuning
  structures.

## Implementation

- `include/openaurora/nim.h`: `oa_ndal_nim_ops` (open/close/tune/scan_mode/
  frequency/delivery_system/symbol_rate/status/metrics/set_ts_output/
  set_standby), `oa_nim_ndal_backend(path)` and the `oa_nim_ndal_set_ops`
  test/embedding hook.
- `common/nim_ndal.c`: default ops open the configured frontend, tune DVB-T
  with `FE_SET_FRONTEND`, read lock/strength/SNR/BER/UNC with `FE_READ_*`,
  and resolve the `Nd_NIM_*` calls with `dlopen("libNDAL_EX.so")`. The ops
  table is replaceable, so tests never touch a device.
- `mod_tuner`: selects the backend explicitly:
  * `tuner.ts_path` -> existing `nim-ts-replay` EIT source;
  * `tuner.backend=ndal` + `tuner.frontend=...` -> `nim-ndal`;
  * otherwise the offline stub.
- `tests/nim_stub.c`: mock NDAL ops validate open/tune/scan-mode/frequency,
  lock/status, metrics, close, repeated start/stop and failure cleanup
  (tune/status/metrics/open errors).

## Verification

```
ok nim stub: stub/replay/NDAL-mock tune, signal and EIT sources
full host make check: 423 ok
verify.cjs: 45 groups
ARM/qemu: ok nim stub: stub/replay/NDAL-mock tune, signal and EIT sources
```

The mock test asserts the open fd is closed on tune failure, status/metrics
errors propagate, the requested 482000 kHz tune is passed, the reported
474000 kHz from the NDAL frequency getter is used, and the lock bit maps to
`OA_NIM_TUNE_LOCKED`.

## Limits

- DVB-T only in the default tune path; DVB-C/S/DTMB/ISDBT return `ENOTSUP`
  until their frontend parameters are firmware-grounded.
- The `Nd_NIM_SetScanMode` call is advisory in this adapter; its exact
  hardware semantics were not executed. `libNDAL_EX.so` presence and symbol
  versions are assumed from the rootfs.
- No live EIT section filter/demux or PVR stream is wired here. EIT remains
  the TS replay backend, and the PVR scheduler remains the offline store.
- Mock contract tests prove call/ownership/error mapping, not frontend lock,
  signal accuracy, scan discovery or recording. Real hardware acceptance,
  cold start and display ownership remain owner-gated.
