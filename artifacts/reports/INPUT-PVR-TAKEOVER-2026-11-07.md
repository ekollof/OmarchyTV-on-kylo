# Input/PVR takeover group — 2026-11-07

Owner authorization: TV read-write/full access, recovery reboot allowed. This
group exercised source/VIN switching and the offline PVR stub. The initial
BLAST timer/worker path blocked shutdown; [IR-BLAST-HELPER](IR-BLAST-HELPER-2026-11-07.md)
supersedes it with a killable helper process. Physical remote key acceptance
remains unverified.

## Pre-state (stock)

- appmgr 1574, `aurora.elf` 1764 (zombie 1941).
- Stock fds: `/dev/video28` (BLAST), `/dev/video21`, `/dev/video24`,
  `/dev/ntkdvif`, `/dev/ntkiic0`, `/dev/dvb/adapter0/demux0`, `/dev/fb1`,
  `/dev/snd/controlC0/C2/C3`.
- `/dev/dvb/adapter0`: `frontend0`, `demux0`, `dvr0`, `demux1`, `dvr1`.
- Live settingsDB copy (MD5 `9707ccd7...`): `m_current_source=5`,
  `m_previous_source=4`, `m_previous_active_source=4`, `m_volume=0`,
  `m_current_service=1`.
- `/conf/mb230_hwprofile.bin` MD5 `1b063b2c...`; fb1 `1920,2160` pan `0,0`;
  backlight 1000.

## Stage and preflight

First tar `358d73ac...` failed preflight on `mod_ir`:

```text
undefined symbol: dlopen
undefined symbol: dlclose
undefined symbol: dlerror
undefined symbol: dlsym
```

The module uses `dlopen` but the TV link line omitted `-ldl`; host glibc hides
this. The Makefile `mod_ir` rule now links `-ldl`, producing
`cbb0157a...` with `NEEDED libdl.so.2`. The clean group used the poll-gated
build `606f7859...` (see the IR blocker below).

Two clean stages were used after the failures:

- `/buffer/oa-ip-20261107d`, tar `b655bcd9...`, `mod_ir` `606f7859...`
  (readiness-gate build), config `ir.blast=0` for the BLAST-off control-plane
  run.
- `/buffer/oa-ip-20261107e`, tar
  `87a1a6cd667c27b6fd03115b407f8962484f68b7a0dae04db8024676500f814b`,
  `mod_ir` `fdf7e48c...` (worker build), config `ir.blast=1`,
  `ir.blast_video=/dev/video28`, `ir.rc_protocol=4`, `ir.device=` empty.

Modules in both: `mod_plat.so` (test plat service), `mod_source.so`
`e2b8ffe1...`, `mod_pvr.so` `73111176...`, and the stated `mod_ir`.

## IR blocker (two failed runs)

The first run started all modules, but the single host thread blocked in
`blast_io_get_key`; `oa-client` timed out on every command. The process state
was `S`, `Threads: 1`, `wchan=blast_io_get_key`, with `/dev/video28` fd 7.

The BLAST receive was a 50 ms `timer_add` callback calling
`NDAL_VideoExt_BLAST_RX`. A zero-timeout `poll(POLLIN)` readiness gate was
added, but the next run blocked identically. The vendor call can sleep even
when the v4l2 fd reports readable. Both runs were terminated, staging removed
and stock restored by `wdtctl set 10`.

**Worker implementation (2026-11-07):** BLAST_RX now runs on
`blast_worker`; the worker queues the raw result and calls `api->post`.
`blast_deliver` runs on the host loop and does the debug log/`emit_key`.
`mod_ir` was linked with `-ldl`. A fake `libNDAL_EX.so` blocks 150 ms per
`BLAST_RX`, returns one press (code `0x3056`) and one release, then the no-key
path. `check-ir-blast-worker` proves the control socket replies while the fake
blocks, both keys are posted/logged, and `quit` exits cleanly.

The UART fallback was not used because it consumes remote bytes.

## Clean control-plane run (`ir.blast=0`)

All modules started and the control loop stayed responsive:

```text
plat    started, plat:v1
source  started, source:v1
ir      started, input:v1
pvr     started, pvr:v2
```

Controlled source operation:

```text
source-select 17 -> ok; source-state: source type 17 available 1
source log: source -> type 17 (vin id 3)
source-select 0  -> ok; source-state: source type 0 available 1
source log: source -> type 0 (vin id 1)
```

This is the recovered `Hal_Vin_SelectInput`/`SetStreamStart` path. The source
module does not write the settings DB; the live `m_current_source` remained 5.
The visible screen effect was not independently captured.

PVR offline-stub operation:

```text
pvr-add 1 1000 2000 -> id 1
pvr-list -> count 1; job id=1 service=1 state=0 start=1000 end=2000 repeat=0 pad=0/0
pvr-cancel 1 -> ok
pvr-list -> count 1; state=3 (cancelled record retained by the stub)
```

The host held `/dev/video20` (VIN) but no `/dev/video28` (BLAST off), no
`/dev/ntkdvif`, and no DVB frontend/demux/dvr fds. The PVR backend is the
persistent offline stub, not the vendor DVR engine.

## BLAST worker hardware run (`ir.blast=1`)

Stage `/buffer/oa-ip-20261107e`, manifest/`--check` verified; appmgr 1566,
`aurora.elf` 1769 stopped, watchdog disabled. All four modules started and the
BLAST v4l2 fd 7 was open. While the remote was idle the two host threads were:

```text
5924 poll_schedule_timeout   (main: control loop)
5926 blast_io_get_key        (BLAST receiver worker)
```

Control commands stayed responsive (`status source`, `status ir`,
`source-state`), unlike the old timer path. Source and PVR operations still
worked with BLAST active:

```text
source-select 17 -> type 17 available 1
source-select 0  -> type 0 available 1
pvr-add 1 1000 2000 -> id 1; pvr-list count1 state0
pvr-cancel 1 -> ok; pvr-list state3
```

**Shutdown blocker:** `quit` did not complete. The cleanup worker closed the
v4l2 fd, but the vendor thread remained in `blast_io_get_key`; the cleanup
thread sat in `pthread_join` (`futex_wait_queue_me`) and the host retried
`ir: cleanup pending`. Closing the fd is not sufficient to interrupt the
vendor call. The host had to be killed and stock restored by reboot.

The worker therefore fixed the **running** control-loop hang but not the
**stop/reload** path. This was superseded by
[IR-BLAST-HELPER](IR-BLAST-HELPER-2026-11-07.md): BLAST_RX moved into the
killable `oa-ir-blast` child, and real stop/reload now pass. The physical
remote press remains unverified.

## Restore

The BLAST-off run was stopped with `quit` and rebooted. The BLAST-worker run
was killed because quit could not join. In both cases staging, socket and
heartbeat were removed, `sync` completed, and `wdtctl set 10` rebooted the set.
After the final boot:

```text
appmgr 1566, aurora.elf 1769 (zombie 2181), no openaurora
fds: video28/video21/video24, ntkdvif, ntkiic0, demux0, fb1, snd controlC0/C2/C3
settingsDB source still 5, previous 4, volume 0, service 1 (MD5 1920d185...)
hwprofile 1b063b2c..., fb1 1920x2160 pan 0,0, backlight 1000
stage/socket absent
```

(The BLAST-off run restored appmgr 1619/aurora 1773; the DB MD5 changes at
boot but the sustained source values are unchanged.)

## Limits

- BLAST_RX now runs in the killable `oa-ir-blast` helper; real `quit` and
  `reload ir` pass (see IR-BLAST-HELPER). **Physical remote key acceptance is
  still not run**; the fake test proves the record/pipe/emit path.
- No real PVR/DVR hardware: no frontend/demux/dvr open, recording, playback or
  TS output. The stub stores reservations only.
- Source switch hardware was exercised but the display/audio routing side
  effects were not independently validated; source 0/TV was selected before
  restore and the stock reboot restored the normal source.
- No tuner RF source, no audio/CEC hardware, no cold startup or UI parity.

Evidence: `artifacts/analysis/input-pvr-2026-11-07/`.
