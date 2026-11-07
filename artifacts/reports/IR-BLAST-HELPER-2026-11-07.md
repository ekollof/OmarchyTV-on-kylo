# IR BLAST helper process — 2026-11-07

Owner authorization: TV read-write/full access, recovery reboot allowed. This
follow-up replaces the in-process BLAST worker with a killable helper process.
It verifies stop/reload with real BLAST; physical remote key-press acceptance
still needs an owner observation.

## Why a helper

The worker kept the control loop responsive, but the real
`NDAL_VideoExt_BLAST_RX` sleeps in `blast_io_get_key` while the remote is idle.
Closing the v4l2 fd did not interrupt it; the cleanup thread stayed in
`pthread_join` and `quit` never completed. The vendor call is therefore moved
out of the host process entirely.

## Architecture

`oa-ir-blast` is a small child process built from
`tools/ir/oa-ir-blast.c`:

- `dlopen("libasound.so.2")`, `libv4l2.so.0`, `libNDAL_EX.so`;
- opens `--video /dev/video28` unless the path is empty;
- calls `ndcommon_v4l2_set_fd`, optionally
  `NDAL_VideoExt_BLAST_SetRcProtocol`;
- loops `NDAL_VideoExt_BLAST_RX` and writes fixed
  `struct oa_ir_blast_record { uint32_t code; uint8_t aux, flag; }` records to
  stdout.

`mod_ir` spawns the helper with `ir.blast_helper`, `ir.blast_video` and
`ir.rc_protocol`, watches the stdout pipe on the host loop, parses the 6-byte
records, and emits keys from the watch callback. `blast_helper_stop` sends
SIGTERM, waits briefly, then SIGKILLs and reaps the child. No vendor BLAST call
runs in `openaurora`, so stop/reload cannot be held by `blast_io_get_key`.

The pipe uses `pipe2(O_NONBLOCK | O_CLOEXEC)`. A first TV preflight rejected
the module's `fcntl` use with `GLIBC_2.28 not found` on the TV's glibc 2.23;
`pipe2` is available at GLIBC_2.9 and passed preflight.

## Tests

`check-ir-blast-worker` (host) now runs the real helper with a fake blocking
`libNDAL_EX.so`:

- fake `BLAST_RX` sleeps 150 ms, returns press `0x3056`, then release, then the
  no-key path;
- while it blocks, `status ir` replies and the control loop stays responsive;
- the module logs `BLAST helper pid`, `BLAST code=00003056`, `rc=22 Right`;
- `quit` kills/reaps the helper and exits cleanly.

`check-ir-ipc`, `check-ir-quiesce`, the focused IR groups and the full
`make -j1 check` pass.

## Packaging

`oa-ir-blast` is now part of the normal stage/install outputs:

- `all`, `all-core` and `all-stage` build `$(BUILD)/oa-ir-blast`.
- `cold-bundle-core` and `stage-bundle` copy it next to `openaurora` and include
  it in `manifest.sha256`.
- `install`, `install-stage` and `install-core` install it in `BINDIR` next to
  `openaurora`.
- `tools/boot/openaurora-cold.conf` sets:
  `ir.blast_helper = oa-ir-blast`.
- A bare helper name is resolved against `dirname(/proc/self/exe)` by `mod_ir`,
  so the same config works in a stage bundle root and in `BINDIR`.

Verified with `TARGET=tv STAGE=core`:

```text
build-tv-v3/stage-core/oa-ir-blast  1f581b71...  (ARM, 20,860 bytes)
build-tv-v3/stage-core/openaurora.conf: ir.blast_helper = oa-ir-blast
build-tv-v3/stage-core/manifest.sha256: 1f581b71... oa-ir-blast
sha256sum -c manifest.sha256 -> OK
DESTDIR=/tmp/opencode/oa-install install-stage
  /tmp/opencode/oa-install/usr/local/bin/oa-ir-blast  1f581b71...
```

The host helper lifecycle test now uses the relative
`ir.blast_helper=oa-ir-blast` and passes. Physical remote press/repeat/release
is still the remaining manual acceptance.

## Hardware run

Stage `/buffer/oa-ip-20261107g`, tar SHA256
`eb6f534e88fdea004c11aaf24f45f05b8f0238f1a0c3a254f631b3bf38966441`;
`SHA256SUMS` and `--check` passed. Helper `1f581b71...`, module `f643f011...`.

```
host 6137 (main)       poll_schedule_timeout
child 6139 oa-ir-blast blast_io_get_key
  /buffer/oa-ip-20261107g/oa-ir-blast --video /dev/video28 --protocol 4 --set 1
```

With the real helper blocked on `/dev/video28`, the control loop stayed
responsive:

```text
source-select 17 -> type 17 available 1
source-select 0  -> type 0 available 1
pvr-add 1 1000 2000 -> id 1
pvr-list count1 state0
pvr-cancel 1 -> ok
pvr-list state3
```

`quit` completed (`bye`, rc0); no `openaurora` or `oa-ir-blast` remained and
the socket was removed. A second run tested reload:

```text
helper1 6277
reload ir -> ok
helper2 6300
status ir -> name ir state started services input:v1
quit -> bye
```

No helper remained after quit. This closes the stop/reload blocker.

## Restore

Stage/socket/heartbeat removed, `sync`, `wdtctl set 10` rebooted the set.
After boot:

```text
appmgr 1562, aurora.elf 1766 (zombie 2198), no openaurora/oa-ir-blast
fds: video28/video21/video24, ntkdvif, ntkiic0, demux0, fb1, snd controlC0/C2/C3
settingsDB source 5, previous 4, volume 0, service 1
hwprofile 1b063b2c..., fb1 1920x2160 pan 0,0, backlight 1000
stage/socket absent
```

## Limits

- **Physical remote key acceptance is not run.** The helper is blocked in the
  real `BLAST_RX`; the fake test proves the record/pipe/emit path, but no owner
  pressed a key during the hardware run. A physical press with the helper
  active is the remaining IR acceptance, including repeat/release timing.
- Packaging is now verified: the TV core stage, manifest, config and temporary
  install tree all contain the same `oa-ir-blast` and relative
  `ir.blast_helper=oa-ir-blast`. The earlier hardware run used an absolute
  manual path; the host lifecycle test covers the relative packaging path.
- Source VIN switching worked, but visible display/audio routing and UI/DB
  resync were not validated.
- PVR is the offline stub only; no frontend/demux/dvr open, recording,
  playback or TS output.
- Tuner RF lock, owner-confirmed backlight, audio/CEC hardware, cold startup
  and broad UI parity remain open.

## Next

1. Hold a BLAST helper active and have the owner press Right/Left/Up/Down;
   verify press/release and repeat timing.
2. Real DVR/recording work still needs a tuner/RF source and storage policy.
3. Continue the remaining tuner/audio/CEC/cold-start/UI acceptance gates.
