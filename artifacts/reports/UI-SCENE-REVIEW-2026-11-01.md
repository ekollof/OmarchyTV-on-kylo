# Review of scene rendering and UI lifecycle — 2026-11-01

Reviewed the UI work through c42dd32 against source, tests and fresh target
firmware instructions. Fixed renderer and consumer gaps; tested actual fb1
drawing twice with stock Aurora still running. This is existing-layer testing,
not independent startup or complete animation/visual acceptance.

## Firmware and corrections

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Read-only/no-analysis Ghidra
extracts in ../analysis/ui-animation-2026-10-31/review-scene-* establish:

- onRenderCompleted at b33074 builds four vertices with w=1 and UV0..1;
  b33182..b33188 calls glDrawArrays(6,0,4), GL_TRIANGLE_FAN. UV interpolation
  is affine within triangles (0,1,2) and (0,2,3). The earlier bilinear quad
  inverse mapped keystone texture coordinates incorrectly.
- createOffscreenFBO b32be8 calls createFrameBufferObject b32ac0 and retains
  the texture at object+64(hex), bound by onRenderCompleted. b32b32..b32b6e
  selects CLAMP_TO_EDGE and GL_LINEAR for both min/mag. Earlier nearest texel
  sampling missed this filter. This scene-texture filter is separate from the
  triangle-versus-quad interpolation correction.
- iterateFullScreenAnimation b3298a loads progress into s0; b32a7e..b32a82
  passes it to glUniform1f for active types1..8, including the geometry-free6.
  createShaders b331e8 connects the uniform at object+88(hex) to uAlpha
  (string13f87c3), using the texture fragment source at13f8a5e through
  pointer178b8c4. That shader multiplies sampled alpha by uAlpha.
- initialise b33428 enables blending and sets
  glBlendFuncSeparate(SRC_ALPHA,ONE_MINUS_SRC_ALPHA,ONE,ONE) at b334c2..d6.
  onRenderCompleted clears all channels to0 (literal b331e0) before drawing.
  Thus opaque scenes produce premultiplied RGB and alpha scaled by progress;
  outside the quad is transparent black. The earlier renderer ignored the fade
  and wrote alpha255 even outside the quad.

Raw sources: review-scene-firmware.txt, shader-pointers.txt, shader-data.txt,
shader-source.txt, blend-state.txt and texture-filter.txt (all prefixed
review-scene-). Saved decompiler prototypes are not argument authority; the
instructions explicitly establish registers/constants above.

## Implementation

display v2 now follows the fan triangles, filters four texels at texel-center
coordinates with edge clamping, and applies fade/blend into BGRA output.
The software UNORM conversion rounds to the nearest byte; GPU mediump/filter
precision and exact raster edge coverage have not been measured against vendor
frame sequences. This is not a bit-exact GPU certification.

Opaque slide/fade scenes use a constant fractional offset and a byte-pair
lookup table for linear filtering plus fade. Full opaque fill establishes the
invariant; later draws cannot reduce alpha in this backend. Transparent scenes
and keystone/scale retain the general rasterizer. Zero-progress clears the
frame; completed transforms copy the scene directly. Cross-path tests permit
one byte for differing float arithmetic/UNORM rounding.

ui_fb now paints each dialog-progress callback without relying on highlight,
text-slide or ui.fps timers. Back/Home, stop and suspend cancel its dialog and
clear the transform, preventing delayed completion from reopening a selection.
Display removal clears both service-version aliases before dlclose; removing
ui.anim also clears a frozen transform. Settings pointers are dropped/rebound
with service removal. Failed resume leaves the UI suspended and drops both
display aliases, so later provider events cannot activate it prematurely.

## Checks

Full strict host make check:203 ok, exit0. Focused ASan/UBSan checks pass for
display, navigation, rebinding, text slide and rail highlight. ARM hard-float
display regression binary executes successfully under qemu with the target
rootfs. Tests cover triangle-coordinate differences, filter/fade values,
completion identity, fast/general agreement, frame requests without unrelated
timers, interruption and forced provider unload followed by failed-resume retry.
Logs: review-scene-host-final.txt, sanitizer-final.txt, arm-final-build.txt,
arm-qemu-final.txt and lifecycle-final.txt.

## Actual framebuffer runs

tools/display/scene-probe.c links the real display module's fbdev implementation
with OA_NO_VENDOR solely to suppress HAL initialization/linkage. It uses real
open/ioctl/mmap/msync/pan, no framebuffer wrappers or recording screen. The
probe refuses unknown geometry, snapshots pixels and pan before drawing, and
restores them after normal/errors and handled INT/TERM/HUP signals. It disables region changes,
rotation, mirroring and optional DRM init. It never stops vendor Aurora.

Both runs used fb1:1920x1080, virtual1920x2160,32bpp BGRA, stride7680,
backing16588800bytes, yoffset0. All types0..8 presented five progress steps
(0,.25,.5,.75,1); identity endpoint checks confirmed BGRA/current-frame
readback. Each run restored **16588800 bytes**, yoffset0 and snapshot_match1.

| five-present batch | general first run, ms | final optimized run, ms |
|---|---:|---:|
| identity0 | 175.10 | 196.71 |
| keystone1 | 4166.06 | 2349.00 |
| x-slide2 /3 | 4666.69 /4666.36 | 550.08 /583.51 |
| y-slide4 /5 | 4499.56 /4467.35 | 399.68 /399.32 |
| fade6 | 4599.49 | 399.72 |
| scale7 /bounce8 | 3532.92 /4316.82 | 2150.13 /2482.83 |

These are batch wall times including framebuffer commits, not achieved UI fps
or vendor timing comparisons. The final batch also benefits from the zero/end
shortcuts. General transforms remain slow; even the default slide batch does
not establish the recovered10ms cadence or smooth visual acceptance. Complete
native acceleration/presentation and independent visible frame-sequence checks
remain open. No camera or independent scanout observer was available here.

First probe:99364bytes MD5 3e08400431aa7d59c738a2705ad7fc6d.
Final tested probe:101324bytes MD5 47dcd8ca304e67f42eed48c3a3172b33,
max GLIBC2.17, preserved under openaurora/build-tv-v3/review-scene/.
Foreground upload, separate size/hash verification, execution and cleanup are
logged in review-scene-tv-{upload,verify,runtime,after}.txt and
review-scene-tv-final-{upload,verify,runtime,after}.txt. The intermediate fast
probe was uploaded but replaced before execution; only the final verified
identity above supplied the optimized measurements.

The first status read used two incorrect installed paths and an unqualified
iptables name. review-scene-tv-protection.txt corrects those diagnostic mistakes;
the missing-path errors do not indicate missing installation or protections.
Final reads confirm boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, stock PID1785
and zombie1967, unchanged installed core/hook/hosts MD5s, SSH22/telnet2323,
Samba216.183.117/24REJECT, oa_safe/coredumpnoreset and absent oa_boot.
/tmp/oa-scene-review-20261101 was removed; no scene-probe remained running.
No persistent deployment, recovery change, input capture, audio/CEC change,
service restart or reboot occurred. The matching production16-module manifest
was not rebuilt; these ARM probe/unit artifacts are isolated in its review-scene
subdirectory and do not replace stage.md5 or claim a new full --check.
