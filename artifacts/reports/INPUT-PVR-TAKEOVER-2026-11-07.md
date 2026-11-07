# Input/PVR takeover group — 2026-11-07

Owner authorization: TV read-write/full access, recovery reboot allowed. This
group exercised source/VIN switching and the offline PVR stub. IR hardware
(BLAST) is **not** accepted: the vendor receive call blocks the host loop and
must move to a dedicated worker.

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

Final clean stage `/buffer/oa-ip-20261107d`, tar SHA256
`b655bcd9785e58eb499992fc5a3b0b9be1711f28b00029d4d290ec1362ef628f`;
`SHA256SUMS` and `--check` passed. Modules:

```text
mod_plat.so    test plat service
mod_source.so  e2b8ffe1...
mod_ir.so      606f7859...
mod_pvr.so     73111176...
```

Config `ir.blast=0`, `ir.device=` (BLAST and UART both off for this clean
run), `source.enable=1`, `pvr.path=/buffer/oa-ip-20261107d/pvr.db`.

## IR blocker (two failed runs)

The first run started all modules, but the single host thread blocked in
`blast_io_get_key`; `oa-client` timed out on every command. The process state
was `S`, `Threads: 1`, `wchan=blast_io_get_key`, with `/dev/video28` fd 7.

The BLAST receive was a 50 ms `timer_add` callback calling
`NDAL_VideoExt_BLAST_RX`. A zero-timeout `poll(POLLIN)` readiness gate was
added, but the next run blocked identically. The vendor call can sleep even
when the v4l2 fd reports readable. Both runs were terminated, staging removed
and stock restored by `wdtctl set 10`.

**Required fix:** move BLAST RX to a dedicated worker, marshal keys with
`api->post` (never `api->emit` from the worker), and close the v4l2 fd in the
cleanup worker to unblock/join it. Until then BLAST must not run on the host
loop. The UART fallback was not used because it consumes remote bytes.

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

## Restore

`quit`, stage/socket/heartbeat removed, `sync`, then `wdtctl set 10` rebooted
the set. After boot:

```text
appmgr 1619, aurora.elf 1773 (zombie 1964), no openaurora
fds: video28/video21/video24, ntkdvif, ntkiic0, demux0, fb1, snd controlC0/C2/C3
settingsDB source still 5, previous 4, volume 0, service 1
hwprofile 1b063b2c..., fb1 1920x2160 pan 0,0, backlight 1000
stage/socket absent
```

The DB MD5 changes at boot (`9707ccd7...` -> `60aa0039...`) but the sustained
source values are unchanged.

## Limits

- No physical IR/BLAST key test; the module was control-plane only. Moving
  BLAST to a worker is the blocking next step.
- No real PVR/DVR hardware: no frontend/demux/dvr open, recording, playback or
  TS output. The stub stores reservations only.
- Source switch hardware was exercised but the display/audio routing side
  effects were not independently validated; source 0/TV was selected before
  restore and the stock reboot restored the normal source.
- No tuner RF source, no audio/CEC hardware, no cold startup or UI parity.

Evidence: `artifacts/analysis/input-pvr-2026-11-07/`.
