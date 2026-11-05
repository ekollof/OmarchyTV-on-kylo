# Broadcast vendor-field contracts and EIT cache — 2026-11-05

Extends the project-internal broadcasts contracts with the fields recovered in
[VENDOR-BROADCAST-ABI](VENDOR-BROADCAST-ABI-2026-11-05.md), and adds an
in-memory EIT schedule cache built on the firmware-grounded
`common/eit_time.c`. Host and ARM/qemu only; no TV, tuner or vendor execution.

## Contract changes

`include/openaurora/broadcast.h`:

- `oa_tuner_status` adds `locked`, `signal_strength`, `signal_quality`, `unc`,
  `ber` and `snr`, matching `DEV_NIM_DTV_CheckDEMODLock` and
  `GetSignalStrength/Quality/BER/SNR/UNC`. `OA_TUNER_UNC_UNKNOWN` is
  `0xffffffff`; BER/SNR are floats with the vendor 0.0 default.
- `oa_epg_event` adds `program_crid_id`, `running_status`, `first_showing`,
  `state`, `version` and `actual`, mapping `EITEventKey` plus
  `ScheduleEvent.ProgramCridId/FirstShowing`. Times remain milliseconds at this
  boundary; vendor storage is uint32 seconds.
- `oa_pvr_request` replaces the old add arguments and carries
  `repeat_days`, `start_padding` and `end_padding` alongside the service and
  window. `oa_pvr_job` mirrors those fields and documents `id` as the project
  handle that maps to a vendor recording id.

The fixture (`tests/broadcast_fixture.c`) now produces vendor-default tuner
metrics, event metadata, and stores the PVR repeat/padding fields. The control
socket exposes them:

```
tuner-state
  network 1 state 2 progress 100 lock 1 services 12 selected 0 error 0
  strength 100 quality 80 ber 0.00 snr 20.50 unc 0

epg-query 1 0 4000
  event id=100 service=1 start=1000 end=2000 crid=5001 first=1 state=1
  version=0 actual=1 running=0 name=Alpha News

pvr-add 2 3000 4000 127 30 -15
pvr-list
  job id=2 service=2 state=0 start=3000 end=4000 repeat=127 pad=30/-15
  name=cli reservation
```

`pvr-add` accepts three optional trailing arguments (`repeat`, `start_pad`,
`end_pad`); a partial group, overflow or a seventh token is `EINVAL`.

## EIT schedule cache

`include/openaurora/eit_cache.h`, `common/eit_cache.c`:

- records are keyed by `(service_id, event_id)` like `EITEventKey`;
- `oa_eit_cache_put` inserts or replaces, maintains sorted order by
  service/begin/event and bounds the title on copy;
- `oa_eit_cache_get` is the `ScheduleCache::getEvent(service, event)` shape;
- `oa_eit_cache_query` takes an absolute start plus duration in **seconds**,
  builds the vendor `[start, start+duration)` range through
  `oa_eit_range_set` and uses `oa_eit_range_intersects`, reporting the full
  match count while copying at most `cap`;
- `oa_eit_cache_now_next` follows `NowNextCache::getNowNext`: `now` is the
  earliest record with `begin <= now < end`, `next` the earliest record with
  `begin > now`; an empty event is never current.

Deliberately not implemented: `EITEventRecordList::findEvents` and
`removeEventsBetween`. Their `CompareType` argument could not be recovered, so
inventing containment/overlap semantics would be ungrounded. The cache is
in-memory only; persistence and EIT section filtering are separate work.

## Verification

```
ok broadcast fixture: tuner scan state machine, EPG query bounds and PVR reservations
ok broadcast IPC: tuner scan/select, EPG query and PVR add/list/cancel over the control socket
ok eit time: firmware BCD/MJD decode, range algebra and sentinel edges
ok eit cache: keyed schedule events, duration query and now/next
```

Full host `make check`: **415 ok**. `verify.cjs`: **45 groups**. The EIT time
and cache tests also build for ARM and pass under `qemu-arm-static`; the ABI 3
control binary rebuilds.

Evidence:

- `artifacts/analysis/broadcast-vendor-fields-2026-11-05/check.txt`,
  `full-host-summary.txt`, `arm-build.txt`, `arm-run.txt`, `identities.txt`
- `openaurora/tests/broadcast_ipc.cjs`, `tests/broadcast_services.c`,
  `tests/eit_cache.c`

## Limits

- Still a fixture and project contracts, not a vendor-backed tuner/EPG/PVR
  backend. No tuner lock, signal, EIT section, recording or scheduler hardware.
  **partly superseded:** offline `mod_tuner`/`mod_epg`/`mod_pvr` modules now
  exist (TUNER-NIM-MODULE, EPG-EIT-MODULE, PVR-MODULE); no live vendor
  hardware backend exists.
- `repeat_days` bits and the now/next tie policy are project-defined; vendor
  getters exist but their full domain/ordering semantics were not recovered.
- No schedule persistence, CRID resolution (string CRIDs live in the separate
  `ProgramCrid` table), EIT filtering, series recording or conflict handling.
  **partly superseded:** [EPG-EIT-MODULE](EPG-EIT-MODULE-2026-11-05.md) adds
  EIT section parsing and an `epg` module, but section filters/PID assembly and
  persistence remain absent.
- Static vendor evidence only; no vendor function was executed. Runtime EASI
  remains blocked by disabled testtool TCP 1986 pending an agreed
  watchdog-safe/recovery-ready procedure.
