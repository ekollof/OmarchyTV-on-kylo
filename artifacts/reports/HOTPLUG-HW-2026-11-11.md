# HDMI hotplug pulse on hardware — 2026-11-11

Status: current
Last verified: 2026-11-11
Evidence class: on-TV controlled ownership, firmware-backed device effect.
Supersedes: the "actual pulse not run on TV" limit in
[DEV-GPIO-PROVIDER-2026-11-11](DEV-GPIO-PROVIDER-2026-11-11.md) and the
corresponding COLD-START-SEQUENCE note.

Owner authorization: full TV access, 2026-11-11. Stock Aurora was stopped
temporarily, an external MCU watchdog feeder kept the set alive, and a clean
reboot restored stock. No persistent change was installed.

## Setup

The Steam Deck is connected on HDMI1 and speaks CEC. The live descriptor
query maps `HDMI_HOTPLUG_1..4` to pins 274..277, active low, so the recovered
task drives STBC channels 74..77. `MHL_CDSENSE` and `HDMI_HOTPLUG_5` are
absent.

Staged under `/buffer/oa-hotplug` and hash-verified on the TV:

```text
openaurora      b1ee4832435d4d4f93293d8a54ca6936c137ba8fc09b02edc17259d44a3c4aab
mod_plat.so     ac3dda292267d7ebf8c01b1313ec3383bb57f3b3631b1e1a8d598f4039c795dc
mod_source.so   aa8ea105ce201314d3d84ca8db9cfdca58937a59d6cff19699a969e349bb8fd7
oa-wdtfeed      206c1ab5c3c2f223fab96d2825ff83b8eb7b4c2572b2c3017893481d80edb486
trace_tv_io.so  5fbbf70645f2cc3aad04dabad67185aaf0c6b8ac8cdc6e53a6bebc35882c7dcc
hotplug.conf    998016d1ef97d972a7f58d9b091a2d0ea2b8dfc72b6586532ca7dcedc438d2fe
```

`trace_tv_io.c` is a **pass-through** LD_PRELOAD diagnostic: it logs the NDAL
STBC V4L2 ext-control write `0xc0185648` (channel/value) and forwards every
ioctl/syscall unchanged. It does not fake returns or open devices.

Config: `autoload=plat,source`, `plat.drm_init=0`, `source.enable=1`,
`source.hotplug_pulse=1`. The source module requires `plat`, so
`NDAL_VideoExt_OpenDevice(0,10)` and the STBC command socket are already open
when the pulse worker runs.

## Run

1. `oa-wdtfeed --node /proc/nvtstbcipc --interval 3 --timeout 10` logged two
   beats.
2. The appmgr was paused (`SIGSTOP 1573`) and stock
   `run_aurora.sh`/`aurora.elf` (1762/1809) were stopped.
3. `dmesg -c` cleared the kernel ring.
4. `openaurora` started with `LD_PRELOAD=trace_tv_io.so`, the source module
   autoloaded, and the pulse worker ran.
5. `quit` stopped source/plat, the feeder stop file was written, and a clean
   reboot restored stock.

## Result

Source log:

```text
source: HDMI hotplug pulse started
source: #DEV-SRC#Not found HDMI5 HotPlug Definition
source: End toggle PIN=274, Active level=0
source: #DEV-SRC#Toogle HDMI1 HotPlug Successfully
source: End toggle PIN=275, Active level=0
source: #DEV-SRC#Toogle HDMI2 HotPlug Successfully
source: End toggle PIN=276, Active level=0
source: #DEV-SRC#Toogle HDMI3 HotPlug Successfully
source: End toggle PIN=277, Active level=0
source: #DEV-SRC#Toogle HDMI4 HotPlug Successfully
source: HDMI hotplug pulse finished (4 descriptor(s))
```

The pulse start is at `189443 ms`, the active toggles at `189945/189946 ms`
(the recovered 500 ms delay). MHL and HDMI5 are absent, so only four pins are
toggled.

STBC trace (pass-through, actual calls):

```text
stbc cmd=0xc0185648 count=3 channel=74 value=2
stbc cmd=0xc0185648 count=3 channel=75 value=2
stbc cmd=0xc0185648 count=3 channel=76 value=2
stbc cmd=0xc0185648 count=3 channel=77 value=2
stbc cmd=0xc0185648 count=3 channel=74 value=3
stbc cmd=0xc0185648 count=3 channel=75 value=3
stbc cmd=0xc0185648 count=3 channel=76 value=3
stbc cmd=0xc0185648 count=3 channel=77 value=3
```

This is the exact inactive → active sequence: value 2 for level 1, value 3
for level 0, on channels `(274..277 + 0x38) & 0xff`.

Kernel log after the pulse (ring cleared before the run):

```text
[0-1191.624599] [HDMI-CMD]:[HDCP22] init()
[0-1191.903779] [HDMI-CMD]:[HDCP22] Auth Done
```

The connected Deck re-ran HDCP22 initiation/authentication after the HPD
toggle. This is an observed source-side re-detection effect, not merely a
successful ioctl return.

## Restore

```text
uptime 0 min
appmgr 1578, run_aurora.sh 1706, aurora.elf 1776 (zombie 1946)
no openaurora / oa-wdtfeed
/buffer/oa-hotplug removed
```

No boot hook, firmware or `/conf` change. The TV is in stock operation.

## What this establishes

- The real `DEV_SRC_HDMIHotPlug` one-shot pulse runs on the TV with the shipped
  NDAL STBC path.
- The live descriptor mapping (pins 274..277, active low) and the recovered
  STBC conversion (channels 74..77, values 2 then 3) are correct on hardware.
- The connected HDMI1 source reacted to the HPD pulse (HDCP22 re-init/auth).
- Missing MHL/HDMI5 descriptors are handled with the recovered log-and-skip
  path.

## Limits

- This is the HPD pulse only. It does not capture EDID content, CEC traffic or
  the exact source-side state; the Deck's CEC stack was not queried.
- The other three channels (75..77) had no connected source; their writes are
  traced but not source-confirmed.
- The run used `plat.drm_init=0`; it is not a cold boot, UI/scanout test or
  display acceptance.
- One run only; no repeatability or hotplug-storm/stress matrix.
- `source.hotplug_pulse` remains default 0; this run was explicit and
  temporary.
