# DRM plane records: offline findings, 2026-10-31

> **Status: Current evidence for generic NDAL discovery/records. OSD-BUFFER-CONTRACT supersedes its earlier allocation questions and describes a separate OSD cache.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

**Follow-up:** [OSD-BUFFER-CONTRACT-2026-10-31.md](OSD-BUFFER-CONTRACT-2026-10-31.md)
decodes the separate 68-byte OSD records, private allocation and enable payload,
corrects the palette/dumb-buffer assumptions, and identifies a missing fbdev
commit fixed offline. Cold scanout remains unverified.

Recovered from this set's `libNDAL3.so` using the existing Ghidra `ndal3` project,
with `-noanalysis -readOnly`, plus ARM disassembly and the library's diagnostic
strings. No device ioctl or property write was needed.

## The twelve words

`Hal_Video_Get_DrmPlaneInfo` at `0x4e7c3464` copies exactly 48 bytes from
`g_pstHalVideoDrmInfo + 8 + index * 0x30`; the setter at `0x4e7c3558` copies them
back. `_hal_video_drm_info_init` zeros `0x1b8` bytes (8-byte header + 9 records).
These are userspace records, not the 420-byte `0xc1a46451` ioctl payload.

| word | offset | field | evidence |
|---:|---:|---|---|
| 0 | 0x00 | plane ID | `plane_type` property discovery and `Print_DrmPlaneInfo` |
| 1 | 0x04 | CRTC ID | encoder/resource discovery and diagnostic `crtc_id` |
| 2 | 0x08 | framebuffer ID | `_hal_video_drm_create_fb` and diagnostic `fb_id` |
| 3 | 0x0c | flags position in the equivalent SETPLANE layout | wrapper layout; not named by the record diagnostic |
| 4, 5 | 0x10, 0x14 | CRTC x, y | diagnostic `crtc(x,y,w,h)` |
| 6, 7 | 0x18, 0x1c | CRTC width, height | same diagnostic |
| 8, 9 | 0x20, 0x24 | source x, y | diagnostic `src(x,y,w,h)` |
| 10, 11 | 0x28, 0x2c | source height, width | diagnostic prints word 11 before word 10 |

The records match the field order of the 48-byte SETPLANE ioctl. Do not confuse
that with the function argument order, which puts source width before height.
`Hal_Video_Drm_Set_VideoSize` at `0x4e7c481c` takes integer source coordinates,
shifts them left by 16, then calls `Hal_Video_Libdrm_drmModeSetPlane` at
`0x4e7c81bc`. The latter packs the 48-byte buffer and submits `0xc03064b7`.
The diagnostic alone does not establish the units stored in every cached record.

The getter and setter are **int-returning**, despite Ghidra's current `void`
prototypes: disassembly sets r4 to 0 after copying, or -1 if the shared pointer
is NULL, then moves r4 to r0 immediately before returning. Neither validates
the index. A future caller must allocate all 12 words, constrain the index and
check the return.

## What DrmInit actually does

The earlier report overstates `Hal_Video_DrmInit(1)` as enabling OSD scanout.
Its actual path is:

1. Open the DRM device, retrying for about five seconds.
2. Initialise the userspace record storage.
3. `_hal_video_drm_create_fb` at `0x4e7c3a70`: CREATE_DUMB for **4096 x 2304,
   32 bpp**, ADDFB2 with `XR24`, and copy that framebuffer ID into all nine
   records. This is not evidence of a mapping to `/dev/fb1`.
4. `_hal_video_drm_kms_init` at `0x4e7c364c`: enumerate connectors, encoders,
   CRTCs and planes; read each plane's **`plane_type`** property; populate record
   plane IDs, CRTC IDs and existing nonzero framebuffer IDs. Plane types below 8
   are accepted by this discovery branch, although the allocation has 9 slots.

There is **no SETPLANE call in `_hal_video_drm_kms_init`**. In this library's
disassembly, the SETPLANE wrapper's PLT call is at `0x4e7c48e6`, inside
`Hal_Video_Drm_Set_VideoSize`. `Hal_Video_FlushPlaneInfo` at `0x4e7c3a34` calls
`_hal_video_drm_kms_init` again: it refreshes discovery, not a hardware commit.
This does not rule out driver side effects of discovery or other call paths;
it does rule out the claimed explicit SETPLANE in this init function.

`Hal_Video_Drm_Set_Property` at `0x4e7c4c88` uses its second argument as DRM
object type and third as object ID. The vendor window-order path therefore uses
`DRM_MODE_OBJECT_CRTC` (`0xcccccccc`) and record word 1, **not a plane ID**.
No property write has been added or retried.

## The vendor's explicit OSD commit path

Further offline decompilation found the OSD-specific commit, distinct from the
generic `Hal_Video_Get_DrmPlaneInfo` cache above:

```
PLAT_OSD_Plane_Enable @008af8fc
  -> plat_OSDPlaneMute @00927ad0
  -> plat_Gfx_Window::SetMute @00940804
  -> plat_Gfx_Window::Update @00942024
       Sync_OutPutMode2HW
       Sync_FrontSurface2HW @00940388
       plat_Gfx_DRM_osd_drv::Update_Enable @00944860
```

`SetMute` changes a cached flag and marks output state dirty; `Update` performs
the hardware work. `Sync_FrontSurface2HW` supplies color mode, stride, physical
buffer address/size and display window to the OSD driver. In `Update_Enable`,
a dirty buffer/window record (byte `+0x39`) can cause GETFB, a new ARGB ADDFB2,
then SETPLANE. The SETPLANE arguments come from the OSD driver's own state:
plane/CRTC/fb IDs, integer source and destination rectangles (source shifted
left 16). In the other branch it submits **`0xc00c6455` with three words**
containing the plane ID and cached bytes at `+0x38`/`+0x39`.

This explains a concrete omission in our current implementation: `plat` runs
generic discovery and `display` writes fbdev/region state, but neither reproduces
this OSD driver's explicit buffer and commit sequence. It does not yet prove
that this omission is the sole reason for invisible output. The cached flags,
buffer allocation and physical-address-to-handle path were decoded later in
OSD-BUFFER-CONTRACT; independent hardware acceptance is still open.
Logs (now preserved in `artifacts/analysis/osd-2026-10-31/`):
`oa-osd-enable-2026-10-31.txt`, `oa-osd-window-2026-10-31.txt`,
`oa-osd-commit-2026-10-31.txt`.

## Next evidence to collect

The vendor OSD binding/enable path is now traced in OSD-BUFFER-CONTRACT.
Continue from its remaining initialization questions, not this old search. A future cold
trace belongs to openaurora under the tested supervisor. Do not treat a
successful first present or framebuffer pixel comparison as scanout proof.

Decompilation logs are also preserved in that analysis directory:
`oa-plane-contract-2026-10-31.txt`,
`oa-plane-layout-2026-10-31.txt` and `oa-videosize-contract-2026-10-31.txt`.
They can be regenerated with `/opt/ghidra/support/analyzeHeadless`, project
`/tmp/opencode/ghidra_ndal3 ndal3`, program `libNDAL3.so`, and the existing
`DumpFuncs.java` script. The conclusions and addresses above are the durable record.
