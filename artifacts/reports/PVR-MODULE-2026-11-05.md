# Vendor-grounded PVR module with persistent stub — 2026-11-05

> **Update 2026-11-05:** the vendor content/URI metadata schema was recovered
> from `aurora.elf` (`recordings(key,id,program_num,cicam_id,cc_version,digest)`,
> `uris(recording_key,offset,uri,license,expiry,first_playback)` and
> `pinEvents(recording_key,offset,rating,data)`). It stores recording
> content/URI/licence state, not the scheduled-job list, so this module's
> `pvr.path` scheduler store is not a drop-in replacement for it. A vendor
> metadata backend and the live recording engine remain open.
Adds a real `pvr` module with a vendor-shaped backend boundary, an offline
persistent recording store, and Unix-socket tests including a full process
restart. Host and ARM/qemu only; no TV or vendor execution.

## Firmware grounding

Recovered from `aurora.elf`:

```
DvrController::startRecording(streams, EASIReminder, DateTime start,
    DateTime end, uint, uint, DvrRecordingType, bool, uint)  @0x00af6a45
DvrController::recordingQuery(int index, DvrRecordingInfo&,
    ICEDVR_RecordParams&, DvrRecordingStats&)                 @0x00af3a25
DvrController::stopRecording(int recordingId)                  @0x00af66e5
DvrController::removeRecording(int)                            @0x00af4e69
NebulaPvrRecordingScheduler::getRecordings(uint*, handle***)
    / getStartTime / getDuration / getRepeatDays / padding getters
```

Identity is an integer recording id; enumeration is index-based. The module
keeps that split: `start()` assigns an id, `stop(id)` cancels, and `count()` +
`query(index)` expose the authoritative order.

The shipped calls live inside the vendor executable and expect stream/PMT,
EASIReminder and storage-manager state, so only an offline backend is
implemented. It is **not** the vendor SQLite metadata store or timeshift
engine.

## Backend and persistence

`include/openaurora/pvr.h` defines the backend boundary;
`common/pvr_stub.c` implements `pvr-stub`:

- bounded table of `OA_PVR_MAX_JOBS` (256) jobs; ids start at 1 and never reuse
  an id across restart (next = max persisted id + 1);
- request validation: nonzero service, `start < end`, non-empty printable
  ASCII name, capacity, duplicate-free ids;
- `start` writes the whole table atomically (`path.tmp` + fsync + rename) and
  rolls back the in-memory add if the write fails;
- `stop` only transitions `SCHEDULED` to `CANCELED`; repeat cancel is
  `EALREADY`, unknown id `ENOENT`; failed persistence restores the state;
- file format `OAPVR1` plus one tab-separated record per line with
  id/service/start/end/repeat/padding/state/name;
- a malformed file fails `open` (`EINVAL`) instead of silently truncating it;
  a directory path is `EISDIR`; a missing file starts empty.

`mod_pvr` reads `pvr.path` from config (default `/tmp/openaurora-pvr.db`),
opens it on start, publishes `pvr`, closes on stop, and therefore survives
reload as well as process restart.

## Verification

```
ok pvr stub: id/index semantics, validation and file persistence
ok pvr IPC: add/list/cancel, reload and cross-process persistence
```

The socket test adds two jobs with repeat/padding, lists them, cancels one,
checks repeat/unknown cancel errors, reloads the module, then shuts the process
down and starts a **new process** against the same file. The second process
sees both records with states preserved, assigns the next id (3), and lists
three jobs. The unit test additionally covers 256-job capacity, malformed file
fail-closed, directory rejection, invalid requests and atomic temp cleanup.

Full host `make check`: **421 ok**. `verify.cjs`: **45 groups**. `mod_pvr.so`
and the test also build for ARM and pass under `qemu-arm-static`.

Evidence:

- `artifacts/analysis/pvr-module-2026-11-05/check.txt`,
  `full-host-summary.txt`, `arm-build.txt`, `arm-run.txt`, `identities.txt`
- `openaurora/tests/pvr_stub.c`, `tests/pvr_ipc.cjs`

## Limits

- Offline scheduling only: no active recording, tuner/PMT stream capture,
  recording engine, timeshift, playback, retention or disk/storage manager.
- No conflict detection, series linking, CRID resolution or EASIReminder
  semantics; `repeat_days` is stored, not expanded into future jobs.
- Project file store, not the vendor SQLite recording-metadata schema; one
  process owner, no inter-process locking, ASCII-only names, no directory
  fsync (file data is fsynced before rename).
- The `pvr` service contract remains a project abstraction around vendor ids.
- No vendor execution and no TV access. Testtool/display work remains
  owner-gated; this module does not open devices.
