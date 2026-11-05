# Vendor broadcast ABI grounding — 2026-11-05

Offline reverse engineering of the shipped Aurora broadcast interfaces, plus a
firmware-faithful reimplementation of the EIT time codec. No TV access, no
vendor execution, no hardware.

Binary: `artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf`,
SHA256 `7d6e4ca589f72135ae5e13c977a5bfae7f999abac7de701f8ac5c178f09a5993`,
25,925,944 bytes. Addresses below are ELF virtual addresses; Thumb function
symbols start one byte before Ghidra's reported address.

## 1. EIT time encoding (`EITEventTimeRange`)

Recovered from:

| symbol | address |
|---|---|
| `EITEventTimeRange::set(unsigned char const*)` | `0x00accaf9` |
| `EITEventTimeRange::isValidTimeRangeData(...)` | `0x00acca71` |
| `EITEventTimeRange::set(unsigned int, unsigned int)` | `0x00accc4d` |
| `EITEventTimeRange::clear()` | `0x00acccc5` |
| `EITEventTimeRange::makeInfinite()` | `0x00accc8d` |
| `EITEventTimeRange::isInfinite()` | `0x00accf65` |
| `EITEventTimeRange::includes(...)` | `0x00acd025` |
| `EITEventTimeRange::intersectsWith(...)` | `0x00acd0e5` |
| `EITEventTimeRange::mergeWith(...)` | `0x00acced9` |
| `EITEventTimeRange::grow(int)` | `0x00acd225` |
| `EITEventTimeRange::shift(int)` | `0x00acccf9` |
| `EITEventTimeRange::clip(...)` | `0x00acd269` |
| `EITEventTimeRange::moveInto(...)` | `0x00acd06d` |

The object is two `uint32` fields, begin and end. Times are **seconds since
1970-01-01 UTC**, not milliseconds and not local time. The section decoder
(`set(const unsigned char*)`) computes, from bytes `[MJD_hi, MJD_lo, hh, mm,
ss, dur_hh, dur_mm, dur_ss]`:

```
mjd      = (p[0] << 8) | p[1]
start    = (mjd - 40587) * 86400
         + bcd(p[2]) * 3600 + bcd(p[3]) * 60 + bcd(p[4])
duration = bcd(p[5]) * 3600 + bcd(p[6]) * 60 + bcd(p[7])
end      = start + duration
```

Constants read from the binary:

- `0x00accb90` literal word = `0x15180` = 86400.
- `0x13f043b` is the BCD decode table: byte `0x00..0x09` -> 0..9,
  `0x10..0x19` -> 10..19, ..., `0x90..0x99` -> 90..99; every invalid
  nibble combination maps to **0**, not to a partial value.
- `0x9e8b` = 40587 = MJD of 1970-01-01.

Validity (`isValidTimeRangeData`, tables at `0x13f053b`, `0x13f063b`,
`0x13f073b`):

- MJD >= 40587;
- start hour BCD 00..23, minute and second BCD 00..59;
- duration hour and minute accept the full BCD 00..99 table, duration second
  is BCD 00..59. That asymmetry is in the shipped code and was reproduced.
- `set(const uint8_t*)` itself does not validate; NULL clears to `0/0`.

Range sentinels and operations:

- `0/0` empty; `0/0xffffffff` infinite (checked only by `isInfinite`).
- `set(begin,end)`: if `end < begin`, stores `0/0`; equality is allowed
  (`begin == end`, i.e. empty at that point).
- `includes(a,b)`: `b.begin >= a.begin && b.end <= a.end` (no sentinel
  special case).
- `intersectsWith`: half-open overlap, `a.begin < b.end && b.begin < a.end`.
- `mergeWith`: empty receiver copies the other range; otherwise begin takes
  the other value when `this.begin == 0 || other.begin <= this.begin`, and end
  takes the other value unless `this.end != 0 && other.end <= this.end`.
- `grow(n)`: begin -= n when begin != 0; end += n when end != 0xffffffff.
- `shift(n)`: same, but adds to both non-sentinel edges.
- `clip(other)`: a range strictly containing `other` is returned unchanged
  (return 0); identical or covered ranges collapse to empty at the end and
  return 1; partial overlap trims only the overlapping edge and returns 1;
  an empty `other` returns 1 unless it lies strictly inside the range, where
  the early containment check returns 0.
- `moveInto(other)`: infinite becomes `other`; finite ranges are shifted to
  overlap `other` and clipped to it, otherwise copied.

Reimplementation: `openaurora/include/openaurora/eit_time.h`,
`openaurora/common/eit_time.c`, tests in `openaurora/tests/eit_time.c`
(`ok eit time: firmware BCD/MJD decode, range algebra and sentinel edges`).
This is project code that reproduces the recovered arithmetic; it is not
vendor code and has not been differentially executed against the binary.

## 2. EPG query model

`EITEventKey` (`0x00acb951`) is the event identity. The serialised form is
**21 (0x15) bytes total**, starting with the `'E','K'` tag and followed by 19
bytes of fields:

```
+0x00 uint32 begin seconds
+0x04 uint32 end seconds
+0x08 uint16 event id
+0x0a uint8  flags: bit7 actual, bits6..5 state, bits4..0 version
+0x0b uint8  unused byte (not serialised)
+0x0c uint32 running status
+0x10 uint8  scrambled
+0x11 uint8[3] language code
```

Setters confirm the flag bits: `setVersion` masks `0x1f`, `setState` uses bits
5..6, `setActual` uses bit 7; `setRunningStatus` stores a 32-bit field at
`+0xc`, `setScrambled` sets `+0x10`, and `setLanguageCode` writes three bytes
at `+0x11`.

Runtime query chain:

- `EASIControlTask::querySchedule(DateTime const&, unsigned int duration,
  int serviceIndex, ServiceEventList&)` at `0x0099e0e5`.
- `EASIControlTask::queryNowNextFromSchedule(...)` at `0x00988d35`.
- Both delegate to `ScheduleCache::getEvents` / `getNowNextEvents`
  (`0x0096b299`, `0x0096b089`), which look up three service identifiers
  (likely ONID/TSID/SID; exact order not proven) and call a vtable method with
  `(DateTime const&, duration, list, SDT*)`.
- `ScheduleCache::getEvent(int serviceIndex, unsigned int eventId,
  ServiceEvent&)` at `0x0096b395`.
- `DateTime::increment(unsigned int)` at `0x00c81eb5` is a plain
  `uint32 += n`; therefore the query duration is **seconds**, and `DateTime`
  is a 32-bit seconds-since-epoch value.
- Vendor quirk: `getEvents` computes `local = start + duration` but then passes
  the original `start` and separate `duration` to the delegate. The incremented
  copy is not used.
- Now/next is a separate cache (`NowNextCache`) and separate API, not a
  window query.

Persistent EPG schema (SQLite string in the binary):

```sql
CREATE TABLE ScheduleEvent(
  ProgramCridId INTEGER, EIT_EventId INTEGER, ServiceId INTEGER,
  StartTime INTEGER, EndTime INTEGER, FirstShowing INTEGER,
  PRIMARY KEY(ProgramCridId, EIT_EventId, ServiceId, StartTime, EndTime));
CREATE INDEX fk_se_start_end_service_id ON ScheduleEvent(ServiceId, StartTime);
CREATE INDEX fk_se_eit_event ON ScheduleEvent(EIT_EventId);
```

Text is stored separately in `ProgramInformation(ProgramCridId, Title,
SecondaryTitle, ShortSynopsis, Synopsis, Genre, EditorialLabel)` and long
synopsis in `ProgramInfoLongSynopsis`. So an EPG event is keyed by
`(service id, EIT event id, start, end)` with a CRID foreign key; name and
description are a join, not fields of the event key.

### Delta against `include/openaurora/broadcast.h`

- `oa_epg_event` uses `service_handle` + `event_id` + `start_ms/end_ms`; this
  maps to vendor `ServiceId`/`EIT_EventId`/`StartTime`/`EndTime`, but the
  vendor stores **seconds** and our contract uses milliseconds. The conversion
  belongs at the service boundary.
- Our `query(service_handle, from_ms, to_ms, ...)` is a window query; vendor
  takes `(DateTime start, unsigned duration seconds, int serviceIndex)`.
  `from + (to - from)` is equivalent for a single service, but vendor does not
  express an absolute end time and uses a service index rather than a handle.
- Missing from our contract: `program_crid_id`, `first_showing`,
  `running_status`, `state`, `version`, `actual`, `scrambled`, language code.
  These are needed for schedule/now-next parity and CRID series linking.

## 3. Tuner interface

`DEV_NIM_DTV_*` is the vendor tuner front end. The common entry point is:

```
void DEV_NIM_DTV_StartTune(uint path, uint *params, int *status);  @0x008619e1
```

`params` is a word array interpreted per path; `status` receives a tune result
word at `status[0]` (`3` = unlocked, `4` = locked) plus demodulation details at
`status[2..]` and the tuned frequency at `status[3]`.

Path values confirmed by the binary's debug strings (2/3/5/7 are named
CABLE/QPSK/DTMB/ISDBT; 1 matches the terrestrial parameter set and lock path):

| value | path |
|---|---|
| 1 | `EN_DEV_NIM_PATH_TERRESTRIAL` |
| 2 | `EN_DEV_NIM_PATH_CABLE` |
| 3 | `EN_DEV_NIM_PATH_QPSK` |
| 5 | `EN_DEV_NIM_PATH_DTMB` |
| 7 | `EN_DEV_NIM_PATH_ISDBT` |

Values 6 and 8 are handled (frequency in kHz divided by 1000) but their enum
names were not recovered; 4 was not observed. Do not guess them.

Related calls:

- `DEV_NIM_DTV_MonitorTune(path, status*)` `0x00862889`: polls lock and fills
  `status[0]` with 3/4, frequency at `status[3]`, demod details after.
- `DEV_NIM_DTV_CheckDEMODLock(path)` `0x00863cb5`: returns the lock byte; a
  missing callback or wrong path returns 0.
- `DEV_NIM_DTV_GetSignalStrength(path)` `0x008636c9`: returns uint32 strength.
- `DEV_NIM_DTV_GetSignalQuality(path)` `0x008633a5`: returns uint32 quality.
- `DEV_NIM_DTV_GetSignalBER(path)` `0x008634b1`: returns **float**, default 0.0.
- `DEV_NIM_DTV_GetSignalSNR(path)` `0x00863e2d`: returns **float**, default 0.0.
- `DEV_NIM_DTV_GetSignalUNC(path)` `0x008645bd`: returns uint32, default
  `0xffffffff` when unavailable.

The strength/quality getters dispatch on the front-end system type:
2 or 5 -> DVB-T, 0 -> DVB-T2, 1 -> DVB-C, 6 -> DVB-S, 7 -> DTMB, otherwise a
generic callback or 0. That mapping is itself firmware evidence; it is not a
claim about every board profile.

`TunerCollection` (`0x0095c329` ctor, `addTuner` `0x0095c131`, `clear`
`0x0095c2e5`) is an owned list of `AnyTuner*`; it is passed by reference into
service/EIT/MHEG constructors. It is not a public query API.

### Delta against `include/openaurora/broadcast.h`

`oa_tuner_status` currently exposes only network/scan state/progress/services/
selected/error. Vendor additionally exposes per-path lock, strength, quality,
BER (float), SNR (float) and UNC (`0xffffffff` sentinel). These should be added
before claiming tuner/scan parity. Our handle-based selection and scan state
machine have no vendor equivalent in `DEV_NIM_DTV_*`; they are project UI
contracts, not recovered ABIs.

## 4. PVR / recording interface

`DvrController` is the high-level recorder:

- `recordingQuery(int index, DvrRecordingInfo&, ICEDVR_RecordParams&,
  DvrRecordingStats&)` `0x00af3a25`.
- `recordingQueryNext(int index, int &next, ...)` `0x00af5255`.
- `recordingQueryTotal(int *, int *)` `0x00af52d1`.
- `getRecordingId(int tuner, int &id)` `0x00af4e29`.
- `startRecording(std::vector<DvrStream> const&, EASIReminder const&,
  DateTime start, DateTime end, uint, uint, DvrRecordingType, bool, uint)`
  `0x00af6a45`.
- `stopRecording(int recordingId)` `0x00af66e5`.
- `removeRecording(int)` `0x00af4e69`.
- `changeRecordingEndTime(int, DateTime, DateTime)` and similar in
  `DvrRecordingMonitor`/`EASIDvrInterface`.

So vendor identity is an integer **recording id** assigned to an in-progress or
monitored recording, and enumeration is **index-based with out-parameters**,
not a flat "list all jobs" call. `stopRecording` scans per-tuner in-progress
recordings for the matching id, then calls the recordings manager and stops the
timer; `removeRecording` only removes the monitor entry.

Scheduled/EPG recording is `startRecording(streams, EASIReminder, start, end,
...)`: the EASIReminder carries the EPG event/CRID, and explicit `DateTime`
start/end are passed in seconds. Immediate recording is the separate
`ICEDVR_RecordStart` (`0x007ee611`), a large multi-argument call with a packed
parameter block; `ICEDVR_RecordStop`, `ICEDVR_FindFirstRecording`,
`ICEDVR_FindNextRecording`, `ICEDVR_GetRecordingInfo`,
`ICEDVR_DeleteRecording` are the middleware layer.

The newer scheduler API is `NebulaPvrRecordingScheduler` with opaque
`NEBULA_PvrHandle*` objects:

- `getRecordings(uint*, NEBULA_PvrHandle***)` `0x0118c445`;
- `getStartTime(handle, uint*)` `0x0118b875`;
- `getDuration(handle, uint*)` `0x0118b9a9`;
- `getChannel(handle, int*)` / `getChannelID(handle, char**)`;
- `getProgramme(handle, NEBULA_Program*)` / `getProgrammeId`;
- `getRepeatDays`, `getStartPadding`, `getEndPadding`,
  `setStartPadding`, `setEndPadding`;
- `recordSeries(NEBULA_Program const*)`, `removeSeries(handle)`;
- `getRecording(uri)` and `getRecordingId(handle, char**)`.

### Delta against `include/openaurora/broadcast.h`

`oa_pvr_job`/`oa_pvr_iface` use a project-assigned `id` with
`add(service, start, end, name)`, `list` and `cancel`. Vendor has no such
add-by-window call:

- scheduling takes an `EASIReminder` plus explicit start/end and a stream list;
- cancellation is by recording id, and removal only drops monitoring;
- listing is index-based, with separate info/params/stats out-structs;
- the scheduler exposes repeat days, padding, CRID/series, save days,
  guidance, bookmarks and channel ids that our job struct does not carry.

The fixture contract is therefore a UI/session abstraction, not a vendor ABI.
It must be labelled as such until a vendor-backed module maps these calls.

## 5. Verification and limits

```
ok eit time: firmware BCD/MJD decode, range algebra and sentinel edges
```

- Host test binary: `openaurora/build/tests/eit_time`.
- Full suite result is recorded in `artifacts/analysis/vendor-broadcast-abi-2026-11-05/`.
- No vendor function was executed. Signatures are decompiler output cross-
  checked against Thumb call sites, mangled names and literal pools; they are
  not proven ABI for struct-valued arguments (`ICEDVR_RecordStart` in
  particular).
- No TV operation, tuner access, recording or EASI runtime probe was performed.
  Testtool TCP 1986 remains disabled pending an agreed watchdog-safe/recovery-
  ready procedure.
- The tuner path enum is only confirmed for 1/2/3/5/7; 6/8 remain unnamed.
- The EPG/PVR schema evidence is static SQL embedded in the binary, not a live
  database read.
