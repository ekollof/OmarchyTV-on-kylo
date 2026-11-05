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

## Initial result (superseded)

- `/dev/ttyS2` opened at 115200 8N1, but `/proc/tty/driver/serial` stayed at
  `rx:0` while the owner pressed the remote.
- The first BLAST backend opened the kernel device and set protocols 4/5/6/0xc,
  but returned no keys.
- Repeated protocol changes logged `blast: error: DRV_BLAST_Close [ntkdriver]:
  over limit`; the driver needed a reboot to clear the leaked opens.
- Stock Aurora on BACK AV **did** react to the same remote, so the remote and
  IR receiver were never the problem.

## Resolution: BLAST works

The live stock process supplied the missing constants read-only:
`/proc/1776/mem` at the `ndcommon_v4l2_set_fd` global held fd **222**, and
`/proc/1776/fd/222 -> /dev/video28` (`ndp-hdmi-switch`). The RWDB byte at
`0x017969d4+0x14` was RC type **1**, which `_HW_MCU_RcInit` maps to protocol
**4**. So the correct BLAST setup is: open `/dev/video28` with
`v4l2_open`, pass that fd to `ndcommon_v4l2_set_fd`, set protocol 4, and poll
`NDAL_VideoExt_BLAST_RX`.

Two implementation bugs were found by the live test:

1. `DEV_Ir_GetKey`/`BLAST_RX` returns **0 when a key is present**; nonzero is
   the no-key/debug path (the stock caller prints, delays 15 ms and retries).
   Our first poll had the test inverted.
2. A raw `open("/dev/video28")` fd is not enough; the vendor `v4l2_ioctl`
   wrapper needs a `v4l2_open` fd.

With stock Aurora stopped and a minimal `plat_stub` + `mod_ir` host, the
physical remote decoded cleanly:

```
BLAST code=00003056 flag=00 -> rc=22 Right   (press)
BLAST code=00003056 flag=01 -> rc=22 Right   (release)
BLAST code=00003856 flag=00 -> rc=22 Right   (RC5 toggle)
BLAST code=00003054 flag=00 -> rc=20 Up
BLAST code=00003055 flag=00 -> rc=21 Left
BLAST code=00003853 flag=00 -> rc=19 Down
```

The low six bits of the BLAST code are the profile RC code (`0x16` = Right,
`0x13` = Down, `0x14` = Up, `0x15` = Left); bit `0x800` is the RC5 toggle and
`flag` is 0 on press, 1 on release. `mod_ir` now emits that RC code directly
instead of feeding the NEC 0x66/0x11 complement frame. The MCU `_HW_MCU_RcInit`
/STBC RWDB path is therefore **not** required for the remote; ttyS2 remains a
fallback/UART path.

The full portal UI takeover with the physical remote was not run in this
session: the owner asked to restore stock after the IR-only decode acceptance.

## Restore

The final IR-only host was killed, `/buffer/oa-irprobe2` removed, and
`/buffer/wdtctl set 10` rebooted the set. After boot: uptime 67 s, stock
`aurora.elf` PIDs 2186/1782, appmgr 1580, fb1 `1920,2160` pan 0,0, no
`openaurora`; `/conf/oa_boot` and `/conf/oa_fail` absent. The source was BACK AV
(`m_current_source=1`) and stock's BLAST open succeeded in `dmesg`.

## Code and limits

`modules/ir/ir.c` now defaults to the working physical path:
`ir.blast=1`, `ir.blast_video=/dev/video28`, `ir.rc_protocol=4`,
`ir.blast_set=1`; `ir.device=/dev/ttyS2` remains the UART fallback. It
preloads `libasound.so.2`/`libv4l2.so.0`, opens video28 with `v4l2_open`, sets
the common fd and protocol, and emits the decoded RC code on press/release.

The IR-only takeover decoded all four directions from the physical remote.
The full portal UI takeover was **not** run after the fix (owner requested
stock restore), so UI-level remote navigation remains unaccepted. The earlier
backend recorded host `make check` 434 ok and `verify.cjs` 45 groups; after the
direct-RC/`v4l2_open` change, focused `make -j24 check-ui-nav` passed. Full
`make check` was not rerun for this small edit.

Evidence: `artifacts/analysis/takeover-2026-11-05/blast-rc-decode.log` (the
live Right/Down/Up/Left codes), plus the earlier `live-blast.log`,
`blast-portal.png`, `blast-right.png`, `host-check-ir.txt`, `verify-ir.txt`.

Limits: no full-UI physical-remote acceptance, no cold startup, no MCU/STBC
authority. A clean single BLAST open works; repeated protocol changes still
leak kernel opens and require a reboot.
