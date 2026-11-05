# Takeover IR / real-platform follow-up — 2026-11-05

Owner set the TV to **BACK AV** and confirmed the physical remote works there
with stock Aurora. The first takeover (TAKEOVER-2026-11-05) had only injected
NEC frames through the parser; this run tested the real `/dev/ttyS2` and the
vendor BLAST receiver with stock Aurora stopped.

## Setup

- Source: BACK AV (`m_current_source=1`); stock remote reacted on this source.
- Staging: `/buffer/oa-live-20261105b`, ABI 3 core/modules plus the full
  workbench UI asset set; SHA256 verified on the TV before execution.
- Watchdog disabled, stock `aurora.elf` (PID 1775) SIGTERM'd; it did not
  respawn. `com.nvt.appmgr` (1575) stayed up.
- `openaurora` PID 8676; `display_fb`, `ui_engine`, `settings`, `service_db`,
  `image_decode`, `ui_vendor` painted the vendor Startpagina portal on fb1.

## Real plat

`plat_stub` was replaced by the real `mod_plat` (`plat.enable` default 1). The
log records the full sequence:

```
plat: audio prelude (Protect/Mixer/Proxy)
[DRM Init] : drmModeGetEncoder= PASS
plat: common init (vpltio/v4l2/sharemem/drm, DVIF)
plat: VIN slot defaults (7 slots)
plat: NDAL_VideoExt_OpenDevice(0,10)
plat: sequence done (gpio/iic errors 0; common/video unverifiable: void returns)
service up: plat v1
```

The audio prelude printed ALSA `Invalid CTL hw:4..9` messages because the
module process did not inherit the vendor `ALSA_CONFIG_PATH`; it still returned
success. This is not audio acceptance.

## Physical IR result

- `/dev/ttyS2` opened at 115200 8N1, but `/proc/tty/driver/serial` stayed at
  `rx:0` while the owner pressed the remote.
- Added an opt-in BLAST backend to `mod_ir`
  (`NDAL_VideoExt_BLAST_SetRcProtocol` / `NDAL_VideoExt_BLAST_RX`, config
  `ir.blast=1`). It opened the kernel BLAST device and set protocols 4/5/6/0xc;
  no key was returned.
- The first BLAST open succeeded; repeated protocol changes logged
  `blast: error: DRV_BLAST_Close [ntkdriver]: over limit` in `dmesg`. The
  driver needed a reboot to clear the leaked opens.
- Stock Aurora on BACK AV **did** react to the same remote, so the remote and
  IR receiver are fine.

## Why the replacement is silent

The stock parser is not just a ttyS2 reader. `_dev_uart_parse_rc_key` calls
`DEV_Ir_GetKey` (`0x853fa0`) every loop with three output buffers; that calls
`NDAL_VideoExt_BLAST_RX` (V4L2 control `0xf25`) and synthesizes the same
`FF FF 50/5E A1 A2 CC ~CC FF FF` frame. Separately, `ICE_RemoteInit` calls
`_HW_MCU_RcInit` (`0x74f904`), which calls `_HW_MCU_SetRcType`,
`_HW_MCU_SetRcOkKey`, `_HW_MCU_SetRcWakeupKey` and `DEV_Ir_init` (protocol
`4/5/6/0xc`). Those setters write the STBC/RWDB block through
`_HW_RWDB_WriteBlock` -> `PLAT_STBC_Data_Write(1, block, 64)`. Our process
never runs that MCU handshake, so the MCU does not forward IR frames after
stock Aurora is stopped. `ttyS2 rx:0` and the silent BLAST path are consistent
with that missing initialization, not with a bad remote or UART termios.

## Restore

The stack was stopped/unloaded; `display_fb` cleared `cleanup_pending` on the
retry. `quit` left no `openaurora`. `/buffer/wdtctl set 10` rebooted the set.
After boot: uptime 63 s, stock `aurora.elf` PIDs 1927/1776, appmgr 1617,
fb1 `1920,2160`; no `openaurora`. The source came back as HDMI1 (`5`), so the
owner set BACK AV manually and the DB recorded `m_current_source=1`.
`/buffer/oa-live-20261105b` was removed.

## Code and limits

`modules/ir/ir.c` now has the opt-in BLAST backend (`ir.blast=1`,
`ir.rc_protocol`, zero selects a diagnostic protocol sweep). It is not
physical-IR acceptance. Host `make check` 434 ok and `verify.cjs` 45 groups.

Evidence: `artifacts/analysis/takeover-2026-11-05/live-blast.log`,
`blast-portal.png`, `blast-right.png`, `host-check-ir.txt`, `verify-ir.txt`.

Limits: manual BACK AV selection, no cold startup, no MCU/STBC authority, no
BLAST/IR hardware acceptance, and the BLAST driver open-limit leak remains a
reboot-cleared defect. Next: recover the MCU RC init/RWDB contract offline and
implement it, or keep stock Aurora as the IR front-end; do not call STBC cold
without the ownership/recovery plan.
