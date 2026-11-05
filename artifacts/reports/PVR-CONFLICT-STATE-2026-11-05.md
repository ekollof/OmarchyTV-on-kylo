# PVR conflict and recording-state fixture — 2026-11-05

Bounded offline extension of the project PVR backend. This adds a
fixture-only conflict rule and scheduled→recording→completed transitions; it
is not the vendor recording engine or a claim about vendor conflict semantics.

## Policy

- One **scheduled or recording** window per `service_handle`; a new request
  whose effective padded window overlaps one of those is rejected with
  `EBUSY` (control reply `failed errno 16`).
- Padding is applied in seconds (`start - start_padding`,
  `end + end_padding`) for the conflict window; invalid/overflowing padded
  windows are rejected.
- Different services may overlap. Tuner-count conflicts and `repeat_days`
  expansion are deliberately outside this fixture.
- `COMPLETED`, `CANCELED` and `FAILED` jobs do not block a new window.
- Transitions: `SCHEDULED -> RECORDING` (`record`), `RECORDING -> COMPLETED`
  (`finish`); `CANCELED` remains only from `SCHEDULED`. Repeating a transition
  returns `EALREADY`; unknown ids return `ENOENT`; a second active recording
  on the same service returns `EBUSY`.
- State changes are persisted atomically through the existing `OAPVR1`
  file store.

## Interface

`pvr` is now published as **v2**. `oa_pvr_iface` and
`oa_pvr_backend` gained:

```
int (*record)(uint32_t id);   /* scheduled -> recording */
int (*finish)(uint32_t id);   /* recording -> completed */
```

Control-socket commands:

```
pvr-record <id>
pvr-stop <id>       (finish; pvr-cancel remains scheduled-only)
```

`broadcast_fixture` also publishes v2 and implements the two fixture
transitions for its provider.

## Verification

```
make -j24 check-pvr-stub check-pvr-ipc check-broadcast-services check-broadcast-ipc
ok pvr stub: conflict/state transitions, persistence and DVR sink
ok pvr IPC: conflict add, record/stop states, reload and cross-process persistence
ok broadcast fixture: tuner scan, EPG bounds and PVR reservation/record states
ok broadcast IPC: tuner scan/select, EPG query and PVR add/list/cancel over the control socket

qemu-arm-static -L <toolchain-libc> build-tv-v3/tests/pvr_stub
ok pvr stub: conflict/state transitions, persistence and DVR sink
```

The socket test covers a service-1 overlapping add (`failed errno 16`),
record/repeat-record, stop/repeat-stop, cancel of a completed job, reload,
process restart, and id continuity. The unit test adds boundary touch,
padded overlap, different-service overlap, same-service active conflict, and
persistence after every transition.

## Limits

- Fixture policy only: no vendor conflict callback, tuner count, PMT/stream
  capture, storage manager, timeshift, playback, series/CRID expansion or
  broadcast clock.
- `repeat_days` is still stored, not expanded; conflicts consider the base
  window plus padding only.
- ASCII names, one process owner, no inter-process locking.
- No TV access, no vendor execution and no hardware recording acceptance.
