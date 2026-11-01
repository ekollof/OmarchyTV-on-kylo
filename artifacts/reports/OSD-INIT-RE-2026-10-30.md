# OSD initialization experiments — historical, corrected 2026-10-31

> **Status: Historical experiments with corrected interpretations. Palette handshake, generic dumb backend and DRM exclusion are withdrawn; use current OSD reports.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

**Mixed historical evidence.** The full chronological report, including withdrawn
interpretations, is preserved in
[the archive](../archive/docs-before-tidy-2026-10-31/reports/OSD-INIT-RE-2026-10-30.md).
Use [OSD-BUFFER-CONTRACT](OSD-BUFFER-CONTRACT-2026-10-31.md),
[OSD-KERNEL-INIT](OSD-KERNEL-INIT-2026-10-31.md),
[OSD-MAU-SELECTION](OSD-MAU-SELECTION-2026-10-31.md) and
[DRM-PLANE-CONTRACT](DRM-PLANE-CONTRACT-2026-10-31.md) for current conclusions.

## Measurements retained, with their actual scope

- DRM enumerated ndp, eight planes, including live OSDs; a CRTC reported
  mode_valid=0/fb=0. This did not rule out the vendor DRM commit path.
- CREATE_DUMB/MAP_DUMB yielded unusable large mmap attempts; PRIME mmap and
  SET_MASTER also failed in recorded tests. Later instructions prove the dumb
  allocator allocates only 4096 bytes. This did not test private GEM allocation.
- fb0/1/2 reported multi-frame mappings; fb2 paint was invisible in its tested
  configuration. “fb2 is always unusable” and “all raw writes are invisible”
  are not established conclusions.
- Raw fb1 paint after vendor initialization was visible. FBIOPAN_DISPLAY returned
  success; concurrent vendor writes made readback/tearing interpretation uncertain.
- An earlier pre-Aurora splash failed; a later boot paint appeared at quarter
  screen. Neither experiment establishes complete cold display initialization.
- display opened/mapped/rendered under vendor-initialized state; cold full-screen
  ownership and resume still need acceptance.
- Audio calls without prelude crashed; later calls with prelude did not. Audible
  behavior and complete routing were not verified.

## Corrections: do not repeat these detours

131/132 are palette set/get with {start,count,32-bit pointer}, not acquire/
commit descriptors. Old EFAULT permutations do not motivate another handshake
probe. Presentation uses FBIOPAN_DISPLAY even without rotation; its omission in
the default path was fixed offline later.

The missing fb ioctl handler was found in vendor2/modules/ntkdriver_nondovi.ko.
Do not search sibling kernels or wake the TV to locate it again. That driver
registers the five framebuffers; node publication and presentation are separate.

Aurora has its own explicit OSD private GEM/ADDFB2/SETPLANE path. Generic NDAL
_kms_init only discovers IDs. Neither standard dumb-buffer KMS nor “DRM cannot
scan out” is a justified backend decision from these old experiments.

All measured 32bpp OSD layers are BGRA; older RGBA descriptions came from the
capture decoder bug. Region dimensions alone did not prove the cause of the
quarter-screen/invisible-output failures. Use the recovered source/destination
state, backing size, enable and commit contract before a new experiment.

No hardware changes are authorized while the TV is in use. This report is
history, not a request to rerun drmprobe --paint, boot paint, audio or takeover.
