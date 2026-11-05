# Live TV staged acceptance — 2026-11-05

Owner authorization: **“I have full control of the TV now. You can test things
and draw things on the screen.”** Temporary staged modules and fb1 drawing
were allowed. Staging was placed under `/buffer` at the owner’s request and
removed afterward. Stock Aurora was not stopped, replaced or modified.

## Build and staging

- Current ABI 3 ARM tree built with the TV cross compiler.
- Found and fixed a real portability defect: `mod_display`,
  `mod_display_fb` and `mod_scene_display` linked the toolchain libm and
  required `GLIBC_2.27`; they now link the TV libm and require at most
  `GLIBC_2.17`.
- Staged without `downloaded_images`: `/buffer/oa-live-20261105` (4.0 MB,
  38 files). All 38 SHA256 values matched the local build before execution.
- `/buffer` had 191.9 MB free; `/tmp` was not used.
- Host regression after these changes: `make check` 434 ok,
  `verify.cjs` 45 groups.

## CPU/module checks on the TV

`openaurora` ran from `/buffer` with a real Unix control socket and the staged
modules:

```
ok epg / ok tuner / ok pvr / ok lua
ok (start each)
pvr-add 3 5000 6000 -> id 3
pvr-list -> count 3 (jobs survived reload)
reload pvr -> ok, count 3
lua load/start/reload/stop/unload -> ok
```

The PVR scheduling backend is still the offline store; the DVR sink is not
wired into it.

## NDAL / DVB hardware

- Read-only frontend inventory: `name=internal_fe`, `status=0x20` (no lock),
  `frequency=0`; the stock frontend is not tuned.
- Explicit `tuner.backend=ndal` start plus `tuner-scan 1` returned
  **EBUSY (errno 16)** because stock Aurora owns the frontend descriptor.
  The default stub scan still completes (it does not touch hardware).
- The standard Linux DVB section-filter source opened
  `/dev/dvb/adapter0/demux0`, installed PID `0x12`, and read for 5 s:
  `sections=0 events=0` because no tuned DVB stream was present.
- `libNDAL3.so` cannot be loaded standalone: it requires the stock process’s
  `gs32Ndp_PQ_fd` symbol. The EIT source now falls back to the standard Linux
  DVB API, avoiding that dependency.

## fb1 display draw and restore

`mod_display_fb` was loaded against the vendor-initialized fb1:

```
/dev/fb1: 1920x1080 32bpp, 2 frame(s), back=0
full snapshot saved (16588800 bytes, mau 0)
self-test: magenta band painted
```

`display-active-buffer.png` is the fb1 capture while active: a magenta band
across the top of frame 0. `display-restored-buffer.png` is blank, matching
the pre-test state. Stop initially returned `cleanup pending`; retrying the
same stop command let the ABI 3 worker restore the snapshot and close. Unload
then succeeded.

This is a temporary vendor-initialized fb1 binding, not cold startup or native
scanout. The stock UI was blank before and after the test; no HDMI photo is
claimed.

## Recovery

After stopping the staged core:

```
pids=1967 1785
/vendor/com.nvt.idtv/aurora.elf
staging-absent
no-oa-process
/buffer: 232.5M total, 191.4M available
```

Stock Aurora PID 1785 remains the live process; PID 1967 is the earlier
zombie. `/tmp` and `/buffer` staging were removed, and no helper remains.

## Evidence

`artifacts/analysis/live-tv-2026-11-05/`:

- `buffer-hashes.txt`, `staged-hashes.txt`
- `display-active-buffer.png`, `display-restored-buffer.png`
- `display-buffer.log`
- `buffer-modules.txt` (PVR/Lua)
- `ndal-tuner.txt`, `ndal.log`
- `dvb-status.txt`, `eit-dvb-probe.txt`
- `final-state-buffer.txt`

## Limits

- No independent cold startup, boot-hook change or native scanout.
- Stock Aurora still owns the TV and the DVB frontend.
- NDAL tuning was not accepted; EIT saw no live stream.
- PVR DVR recording, timeshift and playback remain unwired.
- The display test was a temporary module on vendor-initialized fb1, not a
  replacement UI or HDMI/cold-boot proof.
