# Source-switch path implementation — 2026-11-09

Status: **implemented behind `source.enable=0`, fake-HAL host-tested. No TV,
no HAL effect and no scanout claim.**

## What was implemented

`mod_source` now uses an injectable HAL seam (`include/openaurora/source_hal.h`,
production `common/source_hal_vendor.c`) and reproduces the recovered connect
half of `PLAT_VIU_ChangeInput` / `plat_vin_slot_connect`:

- Type gates:
  - type 1: `sd_video_connect` hook, DRM connect, NDAL `VideoExt_OpenDevice`;
  - types 2/3: NDAL `VideoExt_OpenDevice` only;
  - type 0 and types 4..21: VIN slot connect, DRM connect, NDAL
    `VideoExt_OpenDevice`;
  - type 25: no connect;
  - types above 25: rejected.
- Same-source early return; a different type closes the previous VIN fds
  before the new connect (native equivalent of the vendor disconnect).
- `vin_slot_connect`:
  - `Hal_Audio_EasySelect(0,1,&{id=1,"adc on"})` for types 4..21;
  - per-type open index: 0/4/5/20/21 -> `Open(0)`, 10 -> `Open(1)`,
    14..18 -> `Open(3)` then `Open(2)`, 19 -> `Open(4)`;
  - VPLT command `0x4c4` with `{tableA,tableB}`, then
    `Hal_Vin_SelectInput(fd, tableA)`;
  - single path: `SetStreamStart`, 4-word `Get/Set_V4l2Info(slot,slot,fd,
    preserved,type)`, 9-word `Get/Set_V4l2FdCommon` with the fd in word
    0/1/2 for Open(0)/Open(1)/Open(4);
  - dual path: Open(3), VPLT/select, GetInfo, GetFdCommon, Open(2),
    SelectInput(fd2,0), SetStreamStart(fd2), SetInfo(slot,slot,fd2,fd1,type),
    SetFdCommon word3=fd2 word4=fd1;
- DRM connect:
  - `Hal_Video_Get_DrmDeviceId(0)`, `VideoPath_to_PlaneType(0)`,
    `Get_DrmPlaneInfo`;
  - `plat_drm_get_dpsource` equivalent: V4L2Info word1 is the fd, word2 the
    format selector; types 1..3 use `Get_V4l2Drm`; other types issue
    `VIDIOC_G_EXT_CTRLS` (id `0xa00968`, size `0x1a4`) into the 0x1a4 output;
  - the exact format mapping (`0->2, 4/5->3, 0xe..0x12->7, 0x13->5,
    0x14/0x15->0xb, default 4`) and `out[0x67]=0`;
  - `Hal_Video_Drm_Set_DpSource(dev, word0, word1, word2)`.
- `NDAL_VideoExt_OpenDevice(0, type)`.
- Disconnect before a type change:
  - `plat_drm_disconnect_dpsource`: DRM device/plane lookup and
    `drm_get_dpsource`, then `Hal_Video_Drm_Set_DpSource(dev,0xe,plane_id,
    src[2])`;
  - `plat_vin_slot_disconnect`: V4L2Info/FdCommon state, the type-specific
    fd-word clear, VPLT type 25, close the primary/secondary fds,
    `Hal_Video_Reset_V4l2Info`, `Set_V4l2FdCommon`;
  - for old types 2/3, `video_dec_stop(0)` then `Hal_VideoDec_Close(0)`;
  - the `Hal_Audio_EasySelect` enable refcount is decremented and the
    `{id=0,"adc off"}` record is selected when it reaches zero.

Table A is the 26-entry `0x013b9e78` table; table B is 0 except types 5 and 21.

## Evidence

```text
make check-source-path
ok source path: type gates, VIN opens/VPLT, V4L2Info/FdCommon, DRM/NDAL and audio order

make check-source-fake-ipc
ok source fake-VIN IPC: reload fails fast (0ms), worker closes fd before VIN reopen, stop uses the gate, source.enable holds

make check-ui-sources
ok source selection: rail Select opens the 780x1080 panel, Up/Down moves the 748x66 bar, Exit returns

make check-plat-consumers-socket
ok actual plat-consumer IPC: audio/source/ir/panel services, plat hold refused, dependency-ordered reload and reuse
```

The C test asserts the exact event order for TV (Open0 single), HDMI1
(dual Open3/Open2), HDMI3 (Open4), type1 (SD/DRM/NDAL, no VIN), types 2/3
(video-ext only), unsupported slot type 6, type 99 and the disabled gate.
The ARM `mod_source.so` builds and links
`Hal_Audio_EasySelect`, all `Hal_Video_*` wrappers, `Hal_Vin_*` and
`NDAL_VideoExt_OpenDevice`; the cold bundle rebuilds with a verified manifest.

## Deliberate limits / not recovered

- The disconnect path above is implemented at the HAL-call level; the
  aurora-internal `PLAT_VIU_NdVideoSetting`/`VinParam`/TDLayer table updates
  and the full VDEC state machine are still not ported. `video_dec_stop` is an
  explicit no-op hook because `PLAT_DEC_StopVideoDecoding` is aurora-internal;
  `Hal_VideoDec_Close` is linked.
- `PLAT_VIU_NdVideoSetting`, `PLAT_SRCIN_GetVinParam` and
  `PLAT_VIN_UpdateTDLayerInputSource` are aurora-internal table/state updates,
  not library calls; they are not guessed.
- HDMI PHY get/set for types 14..18 is not implemented (aurora-internal
  `plat_vin_slot_get/set_hdmi_phy`).
- `PLAT_DEC_SdVideoConnect` is an explicit hook; the production table uses a
  no-op because it is internal to aurora. Type 1 is not in this set's
  Bronlijst.
- The high-level `AudioIF::setTransportStreamSource`/`startNonDVB*` transport
  is not implemented; only the low-level `Hal_Audio_EasySelect` is. The
  recovered call chain and the PLAT_AOUT/AIN/VAUD blocker are in
  AUDIO-SOURCE-TRANSPORT-2026-11-09.
- `source.enable=0` in every shipped profile. No TV, `source-select` effect,
  display switch, scanout or audio acceptance was obtained.

## Live TV acceptance (2026-11-09, owner-authorized window)

Status: **controlled takeover run; stock restored. This proves the HAL call
path and the OSD layer paint. It is not an independent visual confirmation of
the HDMI picture or the panel scanout.**

Deployed the 33-module `full-replacement` bundle to a staging directory
(`/buffer/openaurora-source-test`, removed after the run) with
`source.enable=1`. Stock `aurora.elf` was stopped under `oa-supervise`; the
watchdog was disabled for the run and re-armed afterwards.

### Takeover mode cannot pass the DRM stage

First run kept `plat.drm_init=0` (the normal takeover profile). The new
read-only diagnostic:

```text
$ ./oa-client /tmp/openaurora.sock source-probe
source drm dev=-1 plane=5 info=7,4294967295,4294967295,25 fd=-1,...
```

`Hal_Video_Get_DrmDeviceId(0)` is `-1` when `Hal_Video_DrmInit(1)` was skipped,
so `drm_connect` cannot reach `Hal_Video_Drm_Set_DpSource`. **A source switch
requires `plat.drm_init=1`; the takeover profile is not sufficient.**

### DRM-initialized run

Second run used `plat.drm_init=1`, `display.restore_snapshot=0`,
`display.osd_reinit=1`, `source.enable=1`:

```text
$ ./oa-client ... source-probe
source drm dev=10 plane=5 info=7,-1,-1,25 fd=-1,...

$ ./oa-client ... "source-select 17"
ok
[258057] INFO host source -> type 17 (vin id 3, 2 fd)
$ ./oa-client ... source-probe
source drm dev=10 plane=5 info=0,35,34,17 fd=-1,-1,-1,35,34,-1,-1,-1,-1

$ ./oa-client ... "source-select 0"
ok
[266889] INFO host source -> type 0 (vin id 1, 1 fd)
$ ./oa-client ... source-probe
source drm dev=10 plane=5 info=0,34,-1,0 fd=34,-1,-1,-1,-1,-1,-1,-1,-1
```

The process held `/dev/ntkdvif`, `/dev/video28`, `/dev/video21` (HDMI1 dual
open) and a second `/dev/video28` from `NDAL_VideoExt_OpenDevice`; after the
type-0 switch it held one `/dev/video20`. The V4L2Info words are exactly the
recovered contract (`word1`/`word2` dual fds, `word3` type; then the reset
defaults `7,-1,-1,25` and the single-fd reconnect).

`fb1` was reinitialized to `1920x2160`, stride `7680`, 32bpp, and the
`ui_vendor` apps grid was captured from the OSD layer (frame 0 non-blank) at
1920x1080. That is OSD-layer content, not an HDMI picture or panel scanout
check.

### Restore

`quit` returned to stock; stock `aurora.elf` restarted in place, repainted the
same apps-grid OSD layer, appmgr 1567 stayed up, `/conf/oa_safe` was restored,
the watchdog was re-armed (`wdtctl set 10`), the staging directory was removed,
and `settingsDB.db` mtime was unchanged (`08:23`, before the run). No
settings-set was issued.

Not proven: the HDMI1/TV picture actually appearing on the panel, audio
routing, and a cold boot. Those remain acceptance gates.
